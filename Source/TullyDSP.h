/*
  ==============================================================================
    Tully EDM Vibe - TullyDSP.h
    (c)(p) 2026 Tully EDM Vibe

    Shared effect building blocks for VocalPunch and King Master:
    RBJ biquads, envelope / compressor gain computer, an ITU-R BS.1770-4
    loudness meter (momentary, short-term, gated integrated), a 4x true-peak
    detector and a lookahead brick-wall limiter. Allocation-free after prepare().
  ==============================================================================
*/
#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <array>
#include <vector>
#include <atomic>
#include <complex>

namespace tdsp
{
    constexpr double kPi = 3.14159265358979323846;

    inline float dbToGain (float db) noexcept { return std::pow (10.0f, db * 0.05f); }
    inline float gainToDb (float g, float floorDb = -120.0f) noexcept { return g > 0.0f ? juce::jmax (floorDb, 20.0f * std::log10 (g)) : floorDb; }
    inline float timeCoef (double ms, double sr) noexcept { return ms <= 0.0 ? 0.0f : (float) std::exp (-1.0 / (ms * 0.001 * sr)); }

    //==========================================================================
    // RBJ cookbook biquad, transposed direct form II, double precision state.
    struct Biquad
    {
        enum Type { LowPass, HighPass, BandPass, Peak, LowShelf, HighShelf, AllPass };
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        double z1[2] { 0, 0 }, z2[2] { 0, 0 };

        void set (Type t, double sr, double f, double q, double gainDb = 0.0)
        {
            f = juce::jlimit (5.0, sr * 0.49, f);
            const double A = std::pow (10.0, gainDb / 40.0);
            const double w = 2.0 * kPi * f / sr, cw = std::cos (w), sw = std::sin (w);
            const double alpha = sw / (2.0 * q);
            double B0 = 1, B1 = 0, B2 = 0, A0 = 1, A1 = 0, A2 = 0;
            switch (t)
            {
                case LowPass:  B0 = (1 - cw) / 2; B1 = 1 - cw; B2 = (1 - cw) / 2; A0 = 1 + alpha; A1 = -2 * cw; A2 = 1 - alpha; break;
                case HighPass: B0 = (1 + cw) / 2; B1 = -(1 + cw); B2 = (1 + cw) / 2; A0 = 1 + alpha; A1 = -2 * cw; A2 = 1 - alpha; break;
                case BandPass: B0 = alpha; B1 = 0; B2 = -alpha; A0 = 1 + alpha; A1 = -2 * cw; A2 = 1 - alpha; break;
                case AllPass:  B0 = 1 - alpha; B1 = -2 * cw; B2 = 1 + alpha; A0 = 1 + alpha; A1 = -2 * cw; A2 = 1 - alpha; break;
                case Peak:     B0 = 1 + alpha * A; B1 = -2 * cw; B2 = 1 - alpha * A; A0 = 1 + alpha / A; A1 = -2 * cw; A2 = 1 - alpha / A; break;
                case LowShelf:
                {
                    const double s = 2 * std::sqrt (A) * alpha;
                    B0 = A * ((A + 1) - (A - 1) * cw + s); B1 = 2 * A * ((A - 1) - (A + 1) * cw); B2 = A * ((A + 1) - (A - 1) * cw - s);
                    A0 = (A + 1) + (A - 1) * cw + s; A1 = -2 * ((A - 1) + (A + 1) * cw); A2 = (A + 1) + (A - 1) * cw - s; break;
                }
                case HighShelf:
                {
                    const double s = 2 * std::sqrt (A) * alpha;
                    B0 = A * ((A + 1) + (A - 1) * cw + s); B1 = -2 * A * ((A - 1) + (A + 1) * cw); B2 = A * ((A + 1) + (A - 1) * cw - s);
                    A0 = (A + 1) - (A - 1) * cw + s; A1 = 2 * ((A - 1) - (A + 1) * cw); A2 = (A + 1) - (A - 1) * cw - s; break;
                }
            }
            b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0;
        }
        void setRaw (double nb0, double nb1, double nb2, double na1, double na2) { b0 = nb0; b1 = nb1; b2 = nb2; a1 = na1; a2 = na2; }
        void reset() { z1[0] = z1[1] = z2[0] = z2[1] = 0; }
        inline float process (float x, int ch = 0) noexcept
        {
            const double y = b0 * x + z1[ch];
            z1[ch] = b1 * x - a1 * y + z2[ch];
            z2[ch] = b2 * x - a2 * y;
            return (float) y;
        }
        // magnitude in dB at frequency f (for EQ curve displays)
        double magnitudeDb (double f, double sr) const
        {
            const double w = 2.0 * kPi * f / sr;
            const std::complex<double> z1c = std::polar (1.0, -w), z2c = std::polar (1.0, -2.0 * w);
            const auto num = b0 + b1 * z1c + b2 * z2c, den = 1.0 + a1 * z1c + a2 * z2c;
            return 20.0 * std::log10 (std::max (1e-12, std::abs (num / den)));
        }
    };

    //==========================================================================
    // Compressor gain computer with soft knee; returns gain reduction in dB (<= 0).
    inline float compGainDb (float xDb, float thr, float ratio, float knee) noexcept
    {
        const float d = xDb - thr;
        if (2.0f * d < -knee) return 0.0f;
        if (2.0f * std::abs (d) <= knee) { const float t = d + knee * 0.5f; return (1.0f / ratio - 1.0f) * t * t / (2.0f * knee); }
        return (1.0f / ratio - 1.0f) * d;
    }

    // Attack/release smoother for a gain-reduction value in dB.
    struct GrSmoother
    {
        float att = 0, rel = 0, state = 0;
        void setTimes (double attMs, double relMs, double sr) { att = timeCoef (attMs, sr); rel = timeCoef (relMs, sr); }
        inline float process (float target) noexcept
        {
            const float c = target < state ? att : rel;
            state = target + c * (state - target);
            return state;
        }
    };

    //==========================================================================
    // ITU-R BS.1770-4 / EBU R128 loudness (stereo). Integrated loudness uses a
    // 0.1 LU histogram so it never allocates on the audio thread.
    class LoudnessMeter
    {
    public:
        void prepare (double sampleRate)
        {
            sr = sampleRate;
            {   // stage 1: high shelf (head effects)
                const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
                const double K = std::tan (kPi * f0 / sr), Vh = std::pow (10.0, G / 20.0), Vb = std::pow (Vh, 0.4996667741545416);
                const double a0 = 1.0 + K / Q + K * K;
                shelf.setRaw ((Vh + Vb * K / Q + K * K) / a0, 2.0 * (K * K - Vh) / a0, (Vh - Vb * K / Q + K * K) / a0,
                              2.0 * (K * K - 1.0) / a0, (1.0 - K / Q + K * K) / a0);
            }
            {   // stage 2: RLB high-pass
                const double f0 = 38.13547087602444, Q = 0.5003270373238773;
                const double K = std::tan (kPi * f0 / sr), a0 = 1.0 + K / Q + K * K;
                hp.setRaw (1.0, -2.0, 1.0, 2.0 * (K * K - 1.0) / a0, (1.0 - K / Q + K * K) / a0);
            }
            blockLen = juce::jmax (1, (int) std::lround (sr * 0.1));
            reset();
        }
        void reset()
        {
            shelf.reset(); hp.reset();
            acc = 0; accN = 0; ring.fill (0.0); ringPos = 0; blocksSeen = 0;
            hist.fill (0); histE.fill (0.0);
            momentary.store (-100.0f); shortTerm.store (-100.0f); integrated.store (-100.0f);
        }
        void resetIntegrated() { hist.fill (0); histE.fill (0.0); integrated.store (-100.0f); }

        void process (const float* L, const float* R, int n) noexcept
        {
            for (int i = 0; i < n; ++i)
            {
                const float l = hp.process (shelf.process (L[i], 0), 0);
                const float r = hp.process (shelf.process (R != nullptr ? R[i] : L[i], 1), 1);
                acc += (double) l * l + (double) r * r;
                if (++accN >= blockLen) pushBlock();
            }
        }

        static float toLufs (double meanSquare) noexcept { return meanSquare > 1e-12 ? (float) (-0.691 + 10.0 * std::log10 (meanSquare)) : -100.0f; }

        std::atomic<float> momentary { -100.0f }, shortTerm { -100.0f }, integrated { -100.0f };

    private:
        void pushBlock() noexcept
        {
            ring[(size_t) ringPos] = acc / (double) accN;
            ringPos = (ringPos + 1) % kRing; acc = 0; accN = 0; ++blocksSeen;
            auto meanOf = [this] (int nb)
            {
                double s = 0; const int k = (int) std::min<long long> ((long long) nb, blocksSeen);
                for (int j = 1; j <= k; ++j) s += ring[(size_t) ((ringPos - j + kRing) % kRing)];
                return k > 0 ? s / nb : 0.0;
            };
            const double m = meanOf (4);
            momentary.store (toLufs (m));
            shortTerm.store (toLufs (meanOf (30)));
            if (blocksSeen >= 4)
            {
                const float lk = toLufs (m);
                if (lk > -70.0f)
                {
                    const int b = juce::jlimit (0, kBins - 1, (int) ((lk + 70.0f) * 10.0f));
                    ++hist[(size_t) b]; histE[(size_t) b] += m;
                }
                updateIntegrated();
            }
        }
        void updateIntegrated() noexcept
        {
            long long cnt = 0; double e = 0;
            for (int b = 0; b < kBins; ++b) { cnt += hist[(size_t) b]; e += histE[(size_t) b]; }
            if (cnt == 0) { integrated.store (-100.0f); return; }
            const float rel = toLufs (e / (double) cnt) - 10.0f;
            const int b0 = juce::jlimit (0, kBins, (int) std::ceil ((rel + 70.0f) * 10.0f));
            long long c2 = 0; double e2 = 0;
            for (int b = b0; b < kBins; ++b) { c2 += hist[(size_t) b]; e2 += histE[(size_t) b]; }
            integrated.store (c2 > 0 ? toLufs (e2 / (double) c2) : -100.0f);
        }

        static constexpr int kRing = 32, kBins = 800;
        double sr = 48000.0;
        Biquad shelf, hp;
        double acc = 0; int accN = 0, blockLen = 4800;
        std::array<double, kRing> ring {}; int ringPos = 0; long long blocksSeen = 0;
        std::array<long long, kBins> hist {}; std::array<double, kBins> histE {};
    };

    //==========================================================================
    // 4x oversampled peak estimate (BS.1770 Annex 2 style), 48-tap windowed sinc.
    // process() returns the largest |value| among the 4 interpolated points that
    // lie between the previous and current sample, delayed by kDelay samples.
    class TruePeak
    {
    public:
        static constexpr int kTaps = 12, kPhases = 4, kDelay = kTaps / 2;
        TruePeak()
        {
            for (int p = 0; p < kPhases; ++p)
            {
                double sum = 0;
                for (int t = 0; t < kTaps; ++t)
                {
                    const double x = (double) (t - kTaps / 2 + 1) - (double) p / kPhases;   // distance in input samples
                    const double n = (double) (t * kPhases + p) / (kTaps * kPhases - 1);
                    const double win = 0.42 - 0.5 * std::cos (2 * kPi * n) + 0.08 * std::cos (4 * kPi * n);
                    const double s = std::abs (x) < 1e-9 ? 1.0 : std::sin (kPi * x) / (kPi * x);
                    coef[(size_t) p][(size_t) t] = (float) (s * win); sum += s * win;
                }
                for (int t = 0; t < kTaps; ++t) coef[(size_t) p][(size_t) t] /= (float) sum;
            }
            reset();
        }
        void reset() { hist.fill (0.0f); pos = 0; }
        inline float process (float x) noexcept
        {
            hist[(size_t) pos] = x; pos = (pos + 1) % kTaps;
            float pk = 0.0f;
            for (int p = 0; p < kPhases; ++p)
            {
                float s = 0.0f;
                for (int t = 0; t < kTaps; ++t) s += coef[(size_t) p][(size_t) t] * hist[(size_t) ((pos + t) % kTaps)];
                pk = juce::jmax (pk, std::abs (s));
            }
            return pk;
        }
    private:
        std::array<std::array<float, kTaps>, kPhases> coef {};
        std::array<float, kTaps> hist {};
        int pos = 0;
    };

    //==========================================================================
    // Lookahead brick-wall limiter driven by a true-peak estimate.
    // Gain is the min-hold of the required gain over the lookahead window,
    // released exponentially, then averaged over the same window, so it has
    // fully arrived by the time the peak leaves the delay line.
    class Limiter
    {
    public:
        void prepare (double sampleRate, double lookaheadMs = 5.0)
        {
            sr = sampleRate;
            L = juce::jmax (8, (int) std::lround (lookaheadMs * 0.001 * sr));
            audioDelay = L + TruePeak::kDelay - 3;   // the peak detector looks ~6 samples back; gain window covers [n-L+1, n]
            for (auto& d : delay) d.assign ((size_t) audioDelay, 0.0f);
            hold.assign ((size_t) L, 1.0f); avg.assign ((size_t) L, 1.0f);
            setRelease (80.0);
            reset();
        }
        int getLatency() const noexcept { return audioDelay; }
        void setRelease (double ms) { relCoef = timeCoef (ms, sr); }
        void setCeilingDb (float db) { ceiling = dbToGain (db - 0.15f); }
        void reset()
        {
            for (auto& d : delay) std::fill (d.begin(), d.end(), 0.0f);
            std::fill (hold.begin(), hold.end(), 1.0f); std::fill (avg.begin(), avg.end(), 1.0f);
            tpL.reset(); tpR.reset(); dpos = 0; hpos = 0; apos = 0; avgSum = (double) L; released = 1.0f; minGain = 1.0f;
            gr.store (0.0f);
        }

        void process (float* l, float* r, int n) noexcept
        {
            float blockMin = 1.0f;
            for (int i = 0; i < n; ++i)
            {
                const float pk = juce::jmax (tpL.process (l[i]), tpR.process (r[i]));
                const float need = pk > ceiling ? ceiling / pk : 1.0f;
                // sliding min over L samples (L is small: a few hundred, so a scan is cheap enough)
                hold[(size_t) hpos] = need; hpos = (hpos + 1) % L;
                float h = 1.0f;
                for (int k = 0; k < L; ++k) h = juce::jmin (h, hold[(size_t) k]);
                released = juce::jmin (h, 1.0f - (1.0f - released) * relCoef);   // instant attack, exponential release, never above h
                avgSum -= avg[(size_t) apos]; avg[(size_t) apos] = released; avgSum += released; apos = (apos + 1) % L;
                const float g = juce::jmin (1.0f, (float) (avgSum / (double) L));

                const float dl = delay[0][(size_t) dpos], dr = delay[1][(size_t) dpos];
                delay[0][(size_t) dpos] = l[i]; delay[1][(size_t) dpos] = r[i];
                dpos = (dpos + 1) % audioDelay;
                // final safety: never exceed the ceiling on the sample grid
                l[i] = juce::jlimit (-ceilingHard(), ceilingHard(), dl * g);
                r[i] = juce::jlimit (-ceilingHard(), ceilingHard(), dr * g);
                blockMin = juce::jmin (blockMin, g);
            }
            gr.store (gainToDb (blockMin));
        }
        std::atomic<float> gr { 0.0f };   // last block's max gain reduction (dB, <= 0)

    private:
        float ceilingHard() const noexcept { return ceiling * 1.0177f; }   // +0.15 dB back
        double sr = 48000.0;
        int L = 240, audioDelay = 245, dpos = 0, hpos = 0, apos = 0;
        std::array<std::vector<float>, 2> delay;
        std::vector<float> hold, avg;
        double avgSum = 0;
        float relCoef = 0.99f, released = 1.0f, ceiling = 0.89f, minGain = 1.0f;
        TruePeak tpL, tpR;
    };

    //==========================================================================
    // Simple modulated / fixed delay line with linear interpolation.
    // After push(x), read(d) returns the input from d-1 samples ago (read(1) = newest).
    class DelayLine
    {
    public:
        void prepare (int maxSamples) { buf.assign ((size_t) juce::nextPowerOfTwo (maxSamples + 4), 0.0f); mask = (int) buf.size() - 1; w = 0; }
        void reset() { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }
        inline void push (float x) noexcept { buf[(size_t) w] = x; w = (w + 1) & mask; }
        inline float read (float delaySamples) const noexcept
        {
            const float d = juce::jlimit (1.0f, (float) (mask - 2), delaySamples);
            const int i = (int) d; const float f = d - (float) i;
            const float a = buf[(size_t) ((w - i) & mask)], b = buf[(size_t) ((w - i - 1) & mask)];
            return a + f * (b - a);
        }
    private:
        std::vector<float> buf; int mask = 0, w = 0;
    };

    //==========================================================================
    // Shared meter values written by the audio thread and read by the UI.
    struct Meter
    {
        std::atomic<float> peakL { -100.0f }, peakR { -100.0f };
        void push (const float* l, const float* r, int n) noexcept
        {
            float a = 0, b = 0;
            for (int i = 0; i < n; ++i) { a = juce::jmax (a, std::abs (l[i])); b = juce::jmax (b, std::abs (r[i])); }
            peakL.store (juce::jmax (gainToDb (a), peakL.load() - 1.5f));
            peakR.store (juce::jmax (gainToDb (b), peakR.load() - 1.5f));
        }
    };
}
