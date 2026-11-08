/*
  ==============================================================================
    Tully EDM Vibe - TullySynth.h
    (c)(p) 2026 Tully EDM Vibe

    The synth engine shared by Mister Leads and Mister Arp:
      * 8 voices, Poly / Mono / Legato, glide
      * 2 oscillators x up to 7 unison voices (Saw, Square, Pulse, Triangle,
        Sine, SuperSaw, FM, Fold) + sub + noise
      * filter: 24 dB ladder, 12 dB low-pass, band-pass, high-pass, with drive,
        envelope, key tracking and LFO
      * amp + filter ADSR, LFO (5 shapes, free or tempo-synced, fade-in delay)
      * FX: drive (Soft/Hard/Fold/Crush), chorus, ping-pong delay, reverb, width
  ==============================================================================
*/
#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <array>

namespace tsyn
{
   #ifndef TSYN_MAX_VOICES
    #define TSYN_MAX_VOICES 8
   #endif
    static constexpr int   kMaxVoices  = TSYN_MAX_VOICES;   // Stellar Pads builds with 16
    static constexpr int   kMaxUnison  = 7;
    static constexpr float kTwoPi      = 6.283185307179586f;
    static constexpr double kTwoPiD    = 6.283185307179586;

    inline float midiToHz (float n) noexcept { return 440.0f * std::exp2 ((n - 69.0f) / 12.0f); }

    inline float polyBlep (double t, double dt) noexcept
    {
        if (t < dt)       { t /= dt;            return (float) (t + t - t * t - 1.0); }
        if (t > 1.0 - dt) { t = (t - 1.0) / dt; return (float) (t * t + t + t + 1.0); }
        return 0.0f;
    }

    enum Wave { Saw = 0, Square, Pulse, Triangle, Sine, SuperSaw, FM, Fold, kNumWaves };
    inline const juce::StringArray& waveNames()
    {
        static const juce::StringArray n { "Saw", "Square", "Pulse", "Triangle", "Sine", "SuperSaw", "FM", "Fold" };
        return n;
    }
    enum FilterType { LP24 = 0, LP12, BP12, HP12 };
    enum VoiceMode  { Poly = 0, Mono, Legato };
    enum LfoShape   { LSine = 0, LTriangle, LSaw, LSquare, LSampleHold };
    enum DriveType  { DOff = 0, DSoft, DHard, DFold, DCrush };

    //==========================================================================
    struct Params
    {
        int   wave[2]   { SuperSaw, Saw };
        int   octave[2] { 0, 0 };
        float semi[2]   { 0, 0 };        // semitones
        float fine[2]   { 0, 7 };        // cents
        float level[2]  { 0.8f, 0.0f };
        float shape[2]  { 0.5f, 0.5f };  // PW / side mix / FM index / fold
        int   unison[2] { 1, 1 };
        float detune[2] { 0.3f, 0.3f };  // 0..1 (max +-50 cents for plain waves)
        float fmRatio   = 2.0f;
        float subLevel  = 0.0f;
        int   subWave   = 0;             // 0 sine, 1 square
        float noise     = 0.0f;

        int   fltType   = LP24;
        float cutoff    = 6000.0f;       // Hz
        float reso      = 0.2f;          // 0..1
        float fltEnv    = 0.3f;          // -1..1  (x 6 octaves)
        float fltKey    = 0.5f;          // 0..1
        float fltDrive  = 0.1f;          // 0..1

        float aA = 0.003f, aD = 0.3f, aS = 0.8f, aR = 0.25f;  // seconds / level
        float fA = 0.002f, fD = 0.35f, fS = 0.3f, fR = 0.3f;

        int   lfoShape  = LSine;
        float lfoHz     = 5.0f;
        float lfoPitch  = 0.0f;          // 0..1 -> up to 1 semitone
        float lfoCut    = 0.0f;          // 0..1 -> up to 3 octaves
        float lfoAmp    = 0.0f;          // 0..1 tremolo
        float lfoPw     = 0.0f;          // 0..1 shape modulation
        float lfoDelay  = 0.0f;          // seconds before the LFO fades in

        int   mode      = Poly;
        float glide     = 0.0f;          // seconds
        float bendRange = 2.0f;          // semitones
        float velSens   = 0.5f;
        float spread    = 0.6f;          // unison stereo spread
        float volume    = 0.7f;          // linear
    };

    struct FxParams
    {
        int   driveType   = DOff;
        float drive       = 0.0f;   // 0..1
        float chorusMix   = 0.0f;
        float chorusRate  = 0.6f;   // Hz
        float delayMix    = 0.0f;
        float delaySec    = 0.375f;
        float delayFb     = 0.35f;
        float reverbMix   = 0.0f;
        float reverbSize  = 0.6f;
        float width       = 1.0f;   // 0..2 (M/S)
    };

    //==========================================================================
    struct Adsr
    {
        enum Stage { Idle, Attack, Decay, Sustain, Release };
        Stage stage = Idle;
        float value = 0.0f;
        float a = 0.01f, d = 0.2f, s = 0.8f, r = 0.2f;   // seconds / level
        double sr = 44100.0;

        void set (float A, float D, float S, float R) noexcept { a = A; d = D; s = S; r = R; }
        void noteOn() noexcept  { stage = Attack; }                       // starts from the current value: no click on retrigger
        void noteOff() noexcept { if (stage != Idle) stage = Release; }
        void kill() noexcept    { stage = Idle; value = 0.0f; }
        bool active() const noexcept { return stage != Idle; }

        float next() noexcept
        {
            switch (stage)
            {
                case Attack:
                {
                    value += 1.0f / (juce::jmax (0.0005f, a) * (float) sr);
                    if (value >= 1.0f) { value = 1.0f; stage = Decay; }
                    break;
                }
                case Decay:
                {
                    const float c = std::exp (-1.0f / (juce::jmax (0.002f, d) * 0.3f * (float) sr));
                    value = s + (value - s) * c;
                    if (std::abs (value - s) < 1.0e-4f) { value = s; stage = Sustain; }
                    break;
                }
                case Sustain: value = s; break;
                case Release:
                {
                    const float c = std::exp (-1.0f / (juce::jmax (0.003f, r) * 0.3f * (float) sr));
                    value *= c;
                    if (value < 1.0e-5f) { value = 0.0f; stage = Idle; }
                    break;
                }
                case Idle: default: value = 0.0f; break;
            }
            return value;
        }
    };

    //==========================================================================
    // TPT 4-pole ladder (Zavalishin) with tanh input saturation
    struct Ladder
    {
        float s[4] {};
        void reset() noexcept { for (auto& v : s) v = 0.0f; }
        float process (float x, float g, float k) noexcept
        {
            const float G = g / (1.0f + g);
            const float G2 = G * G, G3 = G2 * G, G4 = G3 * G, om = 1.0f - G;
            const float S = G3 * om * s[0] + G2 * om * s[1] + G * om * s[2] + om * s[3];
            const float y4 = (G4 * x + S) / (1.0f + k * G4);
            float u = std::tanh (x * (1.0f + 0.35f * k) - k * y4);
            for (int i = 0; i < 4; ++i) { const float v = (u - s[i]) * G; const float y = v + s[i]; s[i] = y + v; u = y; }
            return u;
        }
    };

    // TPT state-variable filter (Simper)
    struct Svf
    {
        float ic1 = 0, ic2 = 0;
        void reset() noexcept { ic1 = ic2 = 0; }
        float process (float x, float g, float k, int type) noexcept
        {
            const float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
            const float v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0f * v1 - ic1; ic2 = 2.0f * v2 - ic2;
            if (type == BP12) return v1 * k;            // unity-ish peak
            if (type == HP12) return x - k * v1 - v2;
            return v2;
        }
    };

    //==========================================================================
    // Unison detune layout (JP-8000 style, symmetric), gains and pans for n voices
    struct UnisonLayout
    {
        float off[kMaxUnison] {}, gain[kMaxUnison] {}, pan[kMaxUnison] {};
        int n = 1;
        void build (int voices, bool superSaw, float sideMix) noexcept
        {
            n = juce::jlimit (1, kMaxUnison, voices);
            static const float jp[7] = { -1.0f, -0.62f, -0.31f, 0.0f, 0.29f, 0.61f, 1.0f };
            float sum = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                float o;
                if (n == 1) o = 0.0f;
                else if (n == 7) o = jp[i];
                else o = -1.0f + 2.0f * (float) i / (float) (n - 1);
                off[i] = o;
                const bool centre = std::abs (o) < 1.0e-3f;
                float g = 1.0f;
                if (superSaw && n > 1) g = centre ? 1.0f - 0.55f * sideMix : 0.35f + 0.65f * sideMix;
                gain[i] = g; sum += g * g;
                pan[i] = (n == 1) ? 0.0f : ((i % 2 == 0 ? -1.0f : 1.0f) * std::abs (o));
            }
            const float norm = 1.0f / std::sqrt (juce::jmax (1.0e-6f, sum));
            for (int i = 0; i < n; ++i) gain[i] *= norm;
        }
    };

    //==========================================================================
    class Voice
    {
    public:
        void prepare (double sampleRate) noexcept
        {
            sr = sampleRate; amp.sr = flt.sr = sampleRate;
            reset();
        }
        void reset() noexcept
        {
            amp.kill(); flt.kill();
            for (auto& l : ladder) l.reset();
            for (auto& s : svf) s.reset();
            note = -1; age = 0;
        }
        bool isActive() const noexcept { return amp.active(); }
        bool isReleasing() const noexcept { return amp.stage == Adsr::Release; }

        void start (int midiNote, float velocity, float fromPitch, bool retrigger, juce::Random& rnd) noexcept
        {
            note = midiNote; vel = velocity; target = (float) midiNote;
            pitch = fromPitch < 0.0f ? (float) midiNote : fromPitch;
            if (! amp.active())
            {
                // fresh voice: randomise unison phases (a stack of saws in phase sounds like one loud saw)
                for (int o = 0; o < 2; ++o)
                    for (int u = 0; u < kMaxUnison; ++u) { ph[o][u] = rnd.nextDouble(); fm[o][u] = 0.0; }
                subPh = 0.0;
                for (auto& l : ladder) l.reset();
                for (auto& s : svf) s.reset();
                lfoFade = 0.0f;
            }
            if (retrigger || ! amp.active()) { amp.noteOn(); flt.noteOn(); lfoFade = 0.0f; }
            released = false; age = 0;
        }
        void glideTo (int midiNote, float velocity) noexcept { note = midiNote; target = (float) midiNote; vel = juce::jmax (vel * 0.6f, velocity); }
        void release() noexcept { amp.noteOff(); flt.noteOff(); released = true; }
        void kill() noexcept { amp.kill(); flt.kill(); note = -1; }

        // render this voice into L/R (added)
        void render (float* L, float* R, int n, const Params& p, float bend, float lfoVal, const UnisonLayout* lay, float modWheel)
        {
            amp.set (p.aA, p.aD, p.aS, p.aR);
            flt.set (p.fA, p.fD, p.fS, p.fR);
            const float glideCoef = p.glide > 0.0005f ? std::exp (-1.0f / (p.glide * 0.25f * (float) sr)) : 0.0f;
            const float velGain = 1.0f - p.velSens + p.velSens * vel;
            const float lfoFadeInc = p.lfoDelay > 0.001f ? 1.0f / (p.lfoDelay * (float) sr) : 1.0f;
            const float k24 = 3.9f * juce::jlimit (0.0f, 1.0f, p.reso);
            const float kSvf = 2.0f - 1.94f * juce::jlimit (0.0f, 1.0f, p.reso);
            const float drive = 1.0f + 5.0f * p.fltDrive;
            const float driveComp = 1.0f / std::sqrt (drive);

            constexpr int kChunk = 16;
            for (int base = 0; base < n; base += kChunk)
            {
                const int m = juce::jmin (kChunk, n - base);
                // ---- control rate
                lfoFade = juce::jmin (1.0f, lfoFade + lfoFadeInc * (float) m);
                const float vib = (p.lfoPitch + 0.6f * modWheel) * lfoVal * lfoFade;          // semitones
                const float notePitch = pitch + bend + vib;
                float dts[2][kMaxUnison]; float dtMod[2][kMaxUnison];
                for (int o = 0; o < 2; ++o)
                {
                    const float basePitch = notePitch + 12.0f * (float) p.octave[o] + p.semi[o] + p.fine[o] * 0.01f;
                    const bool ss = p.wave[o] == SuperSaw;
                    const float maxCents = ss ? 90.0f : 50.0f;
                    const float det = p.detune[o] * p.detune[o] * maxCents * 0.01f;   // semitones
                    for (int u = 0; u < lay[o].n; ++u)
                    {
                        const float hz = midiToHz (basePitch + det * lay[o].off[u]);
                        dts[o][u] = juce::jlimit (0.0f, 0.49f, hz / (float) sr);
                        dtMod[o][u] = dts[o][u] * p.fmRatio;
                    }
                }
                const float subDt = juce::jlimit (0.0f, 0.49f, midiToHz (notePitch - 12.0f) / (float) sr);

                // filter cutoff (per chunk), envelope value from the start of the chunk
                const float envNow = flt.value;
                const float keyOct = p.fltKey * (pitch - 60.0f) / 12.0f;
                const float cutOct = std::log2 (juce::jmax (20.0f, p.cutoff)) + keyOct + p.fltEnv * 6.0f * envNow
                                   + p.lfoCut * 3.0f * lfoVal * lfoFade;
                const float fc = juce::jlimit (20.0f, (float) sr * 0.45f, std::exp2 (cutOct));
                const float g = std::tan (juce::MathConstants<float>::pi * fc / (float) sr);
                const float shapeMod = p.lfoPw * 0.45f * lfoVal * lfoFade;
                const float trem = 1.0f - p.lfoAmp * 0.5f * (1.0f + lfoVal) * lfoFade;

                for (int i = 0; i < m; ++i)
                {
                    if (glideCoef > 0.0f) pitch = target + (pitch - target) * glideCoef; else pitch = target;
                    float l = 0.0f, r = 0.0f;
                    for (int o = 0; o < 2; ++o)
                    {
                        if (p.level[o] <= 0.0005f) continue;
                        const int w = p.wave[o];
                        const float shp = juce::jlimit (0.0f, 1.0f, p.shape[o] + shapeMod);
                        float ol = 0.0f, orr = 0.0f;
                        for (int u = 0; u < lay[o].n; ++u)
                        {
                            const double dt = dts[o][u];
                            double& t = ph[o][u];
                            float y;
                            switch (w)
                            {
                                case Saw: case SuperSaw: y = (float) (2.0 * t - 1.0) - polyBlep (t, dt); break;
                                case Square: case Pulse:
                                {
                                    const double pw = w == Square ? 0.5 : 0.05 + 0.45 * shp;
                                    y = t < pw ? 1.0f : -1.0f;
                                    y += polyBlep (t, dt);
                                    double t2 = t + (1.0 - pw); if (t2 >= 1.0) t2 -= 1.0;
                                    y -= polyBlep (t2, dt);
                                    y -= (float) (2.0 * pw - 1.0);       // remove the DC of narrow pulses (mean = 2pw - 1)
                                    break;
                                }
                                case Triangle: y = (float) (t < 0.5 ? 4.0 * t - 1.0 : 3.0 - 4.0 * t); break;
                                case Sine: y = std::sin (kTwoPi * (float) t); break;
                                case FM:
                                {
                                    double& mp = fm[o][u];
                                    // index falls off with pitch so high notes do not alias into noise
                                    const float idx = shp * 6.0f * juce::jmin (1.0f, 0.04f / juce::jmax (1.0e-5f, (float) dt * p.fmRatio));
                                    y = std::sin (kTwoPi * (float) t + idx * std::sin (kTwoPi * (float) mp));
                                    mp += dtMod[o][u]; if (mp >= 1.0) mp -= 1.0;
                                    break;
                                }
                                case Fold: default:
                                {
                                    const float s = std::sin (kTwoPi * (float) t) * (1.0f + 4.0f * shp);
                                    y = std::sin (s * 1.5707963f);
                                    break;
                                }
                            }
                            t += dt; if (t >= 1.0) t -= 1.0;
                            const float gg = lay[o].gain[u];
                            const float pn = lay[o].pan[u] * p.spread;
                            ol  += y * gg * (1.0f - pn) ;
                            orr += y * gg * (1.0f + pn);
                        }
                        l += ol * p.level[o]; r += orr * p.level[o];
                    }
                    if (p.subLevel > 0.0005f)
                    {
                        const float s = p.subWave == 0 ? std::sin (kTwoPi * (float) subPh) : (subPh < 0.5 ? 0.7f : -0.7f);
                        subPh += subDt; if (subPh >= 1.0) subPh -= 1.0;
                        l += s * p.subLevel; r += s * p.subLevel;
                    }
                    if (p.noise > 0.0005f)
                    {
                        noiseSeed = noiseSeed * 1664525u + 1013904223u;
                        const float nz = ((float) (noiseSeed >> 9) / 4194304.0f - 1.0f) * p.noise * 0.6f;
                        l += nz; r += nz;
                    }
                    // filter
                    l *= drive; r *= drive;
                    if (p.fltType == LP24) { l = ladder[0].process (l, g, k24); r = ladder[1].process (r, g, k24); }
                    else { l = svf[0].process (std::tanh (l), g, kSvf, p.fltType); r = svf[1].process (std::tanh (r), g, kSvf, p.fltType); }
                    l *= driveComp; r *= driveComp;

                    flt.next();
                    const float a = amp.next() * velGain * trem;
                    L[base + i] += l * a;
                    R[base + i] += r * a;
                }
            }
            age += n;
            if (! amp.active()) note = -1;
        }

        int note = -1;
        float vel = 0.8f, pitch = 60.0f, target = 60.0f;
        bool released = false;
        juce::int64 age = 0;
        Adsr amp, flt;

    private:
        double sr = 44100.0;
        double ph[2][kMaxUnison] {}, fm[2][kMaxUnison] {};
        double subPh = 0.0;
        uint32_t noiseSeed = 22222u;
        float lfoFade = 0.0f;
        Ladder ladder[2];
        Svf svf[2];
    };

    //==========================================================================
    class FxChain
    {
    public:
        void prepare (double sampleRate, int maxBlock)
        {
            sr = sampleRate;
            juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) juce::jmax (1, maxBlock), 2 };
            chorus.prepare (spec); chorus.reset();
            chorus.setCentreDelay (7.0f); chorus.setDepth (0.25f); chorus.setFeedback (0.0f); chorus.setMix (1.0f);
            reverb.setSampleRate (sampleRate); reverb.reset();
            const int maxDelay = (int) (sampleRate * 2.1) + 8;
            dl.assign ((size_t) maxDelay, 0.0f); dr.assign ((size_t) maxDelay, 0.0f); dpos = 0;
            dryL.resize ((size_t) juce::jmax (1, maxBlock)); dryR.resize ((size_t) juce::jmax (1, maxBlock));
            delaySmoothed.reset (sampleRate, 0.12); delaySmoothed.setCurrentAndTargetValue (0.375f * (float) sampleRate);
            lpL = lpR = 0.0f; crushL = crushR = 0.0f; crushCount = 0;
        }
        void reset() { chorus.reset(); reverb.reset(); std::fill (dl.begin(), dl.end(), 0.0f); std::fill (dr.begin(), dr.end(), 0.0f); }

        void process (float* L, float* R, int n, const FxParams& f)
        {
            // ---- drive
            if (f.driveType != DOff && f.drive > 0.001f)
            {
                const float gIn = 1.0f + 12.0f * f.drive * f.drive;
                const float comp = 1.0f / (1.0f + 1.6f * f.drive);
                if (f.driveType == DCrush)
                {
                    const int hold = 1 + (int) (f.drive * 7.0f);
                    const float steps = std::exp2 (12.0f - 9.0f * f.drive);
                    for (int i = 0; i < n; ++i)
                    {
                        if (crushCount++ % hold == 0) { crushL = std::round (L[i] * steps) / steps; crushR = std::round (R[i] * steps) / steps; }
                        L[i] = crushL; R[i] = crushR;
                    }
                }
                else
                    for (int i = 0; i < n; ++i)
                    {
                        L[i] = shapeDrive (L[i] * gIn, f.driveType) * comp;
                        R[i] = shapeDrive (R[i] * gIn, f.driveType) * comp;
                    }
            }
            // ---- chorus (wet/dry here so the chorus itself stays at 100% wet)
            if (f.chorusMix > 0.001f && (int) dryL.size() >= n)
            {
                std::copy (L, L + n, dryL.begin()); std::copy (R, R + n, dryR.begin());
                chorus.setRate (f.chorusRate);
                float* ch[2] = { L, R };
                juce::dsp::AudioBlock<float> blk (ch, 2, (size_t) n);
                chorus.process (juce::dsp::ProcessContextReplacing<float> (blk));
                for (int i = 0; i < n; ++i)
                {
                    L[i] = dryL[(size_t) i] * (1.0f - 0.5f * f.chorusMix) + L[i] * f.chorusMix;
                    R[i] = dryR[(size_t) i] * (1.0f - 0.5f * f.chorusMix) + R[i] * f.chorusMix;
                }
            }
            // ---- ping-pong delay with damped feedback
            if (f.delayMix > 0.001f)
            {
                const int D = (int) dl.size();
                delaySmoothed.setTargetValue (juce::jlimit (1.0f, (float) (D - 4), f.delaySec * (float) sr));
                const float fb = juce::jlimit (0.0f, 0.92f, f.delayFb);
                for (int i = 0; i < n; ++i)
                {
                    const float d = delaySmoothed.getNextValue();
                    float rp = (float) dpos - d; while (rp < 0) rp += (float) D;
                    const int i0 = (int) rp; const float fr = rp - (float) i0; const int i1 = (i0 + 1) % D;
                    const float yl = dl[(size_t) i0] + fr * (dl[(size_t) i1] - dl[(size_t) i0]);
                    const float yr = dr[(size_t) i0] + fr * (dr[(size_t) i1] - dr[(size_t) i0]);
                    lpL += 0.35f * (yl - lpL); lpR += 0.35f * (yr - lpR);
                    const float inMono = 0.5f * (L[i] + R[i]);
                    dl[(size_t) dpos] = inMono + lpR * fb;     // cross feedback = ping-pong
                    dr[(size_t) dpos] = lpL * fb;
                    dpos = (dpos + 1) % D;
                    L[i] += yl * f.delayMix; R[i] += yr * f.delayMix;
                }
            }
            // ---- reverb
            if (f.reverbMix > 0.001f)
            {
                juce::Reverb::Parameters rp;
                rp.roomSize = 0.4f + 0.58f * f.reverbSize; rp.damping = 0.45f; rp.width = 1.0f;
                rp.wetLevel = f.reverbMix * 0.6f; rp.dryLevel = 1.0f - 0.35f * f.reverbMix; rp.freezeMode = 0.0f;
                reverb.setParameters (rp);
                reverb.processStereo (L, R, n);
            }
            // ---- width (M/S)
            if (std::abs (f.width - 1.0f) > 0.001f)
                for (int i = 0; i < n; ++i)
                {
                    const float m = 0.5f * (L[i] + R[i]), s = 0.5f * (L[i] - R[i]) * f.width;
                    L[i] = m + s; R[i] = m - s;
                }
        }

    private:
        static float shapeDrive (float x, int t) noexcept
        {
            switch (t)
            {
                case DSoft: return std::tanh (x);
                case DHard: return juce::jlimit (-0.8f, 0.8f, x) * 1.2f;
                case DFold: { float y = x; for (int k = 0; k < 4 && std::abs (y) > 1.0f; ++k) y = y > 1.0f ? 2.0f - y : -2.0f - y; return y; }
                default: return x;
            }
        }
        double sr = 44100.0;
        juce::dsp::Chorus<float> chorus;
        juce::Reverb reverb;
        std::vector<float> dl, dr, dryL, dryR;
        int dpos = 0;
        float lpL = 0, lpR = 0, crushL = 0, crushR = 0; int crushCount = 0;
        juce::SmoothedValue<float> delaySmoothed;
    };

    //==========================================================================
    // Voice allocation, note stack, pitch bend, mod wheel, sustain pedal, LFO.
    class Engine
    {
    public:
        void prepare (double sampleRate, int maxBlock)
        {
            sr = sampleRate;
            maxBlockSize = juce::jmax (1, maxBlock);
            for (auto& v : voices) v.prepare (sampleRate);
            fx.prepare (sampleRate, maxBlock);
            scratchL.assign ((size_t) juce::jmax (1, maxBlock), 0.0f); scratchR.assign ((size_t) juce::jmax (1, maxBlock), 0.0f);
            numHeld = 0; sustainDown = false; lastPitch = -1.0f;
            lfoPhase = 0.0; shValue = 0.0f;
            outSmooth.reset (sampleRate, 0.03); outSmooth.setCurrentAndTargetValue (prm.volume);
        }
        void allNotesOff()
        {
            for (auto& v : voices) v.kill();
            numHeld = 0; sustainDown = false; for (auto& s : sustained) s = false;
        }

        void setParams (const Params& p, const FxParams& f) noexcept { prm = p; fxp = f; }
        const Params& params() const noexcept { return prm; }

        // host sync for the LFO: beatsPerCycle > 0 locks the phase to the song position
        void setLfoSync (bool synced, double ppqAtBlockStart, double beatsPerCycle) noexcept
        {
            if (synced && beatsPerCycle > 0.0)
            {
                const double ph = ppqAtBlockStart / beatsPerCycle;
                lfoPhase = ph - std::floor (ph);
            }
        }

        void handleMidi (const juce::MidiMessage& m)
        {
            if (m.isNoteOn()) noteOn (m.getNoteNumber(), m.getFloatVelocity());
            else if (m.isNoteOff()) noteOff (m.getNoteNumber());
            else if (m.isPitchWheel()) bend = (float) (m.getPitchWheelValue() - 8192) / 8192.0f;
            else if (m.isControllerOfType (1)) modWheel = (float) m.getControllerValue() / 127.0f;
            else if (m.isSustainPedalOn()) sustainDown = true;
            else if (m.isSustainPedalOff())
            {
                sustainDown = false;
                for (int nn = 0; nn < 128; ++nn) if (sustained[nn]) { sustained[nn] = false; releaseNote (nn); }
            }
            else if (m.isAllNotesOff() || m.isAllSoundOff()) allNotesOff();
        }

        void render (float* L, float* R, int n)
        {
            // Hosts may send more samples than prepareToPlay announced. The FX chain's
            // juce::dsp::Chorus has per-block buffers sized to that maximum and would
            // write past them, so render oversized blocks in announced-size pieces.
            if (n > maxBlockSize)
            {
                for (int done = 0; done < n; done += maxBlockSize)
                    render (L + done, R + done, juce::jmin (maxBlockSize, n - done));
                return;
            }
            std::fill (L, L + n, 0.0f); std::fill (R, R + n, 0.0f);
            if (n <= 0) return;
            UnisonLayout lay[2];
            for (int o = 0; o < 2; ++o)
            {
                const bool ss = prm.wave[o] == SuperSaw;
                lay[o].build (ss ? juce::jmax (prm.unison[o], 7) : prm.unison[o], ss, prm.shape[o]);
            }
            const float bendSemis = bend * prm.bendRange;
            // 32-sample sub-blocks: the LFO (vibrato, filter wobble) moves smoothly at any host block size
            constexpr int kSub = 32;
            for (int done = 0; done < n; done += kSub)
            {
                const int m = juce::jmin (kSub, n - done);
                const float lfo = lfoValue();
                lfoPhase += prm.lfoHz * (double) m / sr;
                if (lfoPhase >= 1.0) { lfoPhase -= std::floor (lfoPhase); shValue = rnd.nextFloat() * 2.0f - 1.0f; }
                for (auto& v : voices)
                    if (v.isActive()) v.render (L + done, R + done, m, prm, bendSemis, lfo, lay, modWheel);
            }
            // headroom: unison stacks and chords add up
            fx.process (L, R, n, fxp);
            outSmooth.setTargetValue (prm.volume);
            for (int i = 0; i < n; ++i)
            {
                const float g = outSmooth.getNextValue() * 0.5f;
                L[i] = softLimit (L[i] * g); R[i] = softLimit (R[i] * g);
            }
        }

        int activeVoices() const noexcept { int c = 0; for (auto& v : voices) if (v.isActive()) ++c; return c; }
        int monoNote() const noexcept { return voices[0].note; }
        float currentLfo() const noexcept { return lfoValue(); }
        FxChain fx;

    private:
        static float softLimit (float x) noexcept
        {
            const float a = std::abs (x);
            if (a < 0.9f) return x;
            const float y = 0.9f + 0.1f * std::tanh ((a - 0.9f) / 0.1f);
            return x < 0 ? -y : y;
        }

        float lfoValue() const noexcept
        {
            const float t = (float) lfoPhase;
            switch (prm.lfoShape)
            {
                case LTriangle:   return t < 0.5f ? 4.0f * t - 1.0f : 3.0f - 4.0f * t;
                case LSaw:        return 1.0f - 2.0f * t;
                case LSquare:     return t < 0.5f ? 1.0f : -1.0f;
                case LSampleHold: return shValue;
                case LSine: default: return std::sin (kTwoPi * t);
            }
        }

        void noteOn (int nn, float v)
        {
            sustained[nn] = false;
            pushHeld (nn);
            const float from = (prm.glide > 0.0005f && lastPitch >= 0.0f) ? lastPitch : -1.0f;
            if (prm.mode == Poly)
            {
                Voice* target = nullptr;
                for (auto& vc : voices) if (vc.note == nn && vc.isActive()) { target = &vc; break; }   // same note: reuse
                if (target == nullptr) for (auto& vc : voices) if (! vc.isActive()) { target = &vc; break; }
                if (target == nullptr)
                {
                    // steal: prefer the oldest releasing voice, then the oldest held one
                    juce::int64 best = -1;
                    for (auto& vc : voices) if (vc.isReleasing() && vc.age > best) { best = vc.age; target = &vc; }
                    if (target == nullptr) for (auto& vc : voices) if (vc.age > best) { best = vc.age; target = &vc; }
                }
                target->start (nn, v, from, true, rnd);
            }
            else
            {
                Voice& mv = voices[0];
                const bool overlapping = mv.isActive() && ! mv.released;
                if (prm.mode == Legato && overlapping)
                    mv.glideTo (nn, v);   // no retrigger, glide only between overlapping notes
                else
                {
                    const float monoFrom = (prm.mode == Mono && prm.glide > 0.0005f && mv.isActive()) ? mv.pitch : from;
                    mv.start (nn, v, prm.mode == Legato ? -1.0f : monoFrom, true, rnd);
                }
            }
            lastPitch = (float) nn;
        }

        void noteOff (int nn)
        {
            if (sustainDown) { sustained[nn] = true; return; }
            releaseNote (nn);
        }

        void releaseNote (int nn)
        {
            removeHeld (nn);
            if (prm.mode == Poly)
            {
                for (auto& vc : voices) if (vc.note == nn && vc.isActive() && ! vc.released) vc.release();
                return;
            }
            Voice& mv = voices[0];
            if (mv.note != nn) return;
            if (numHeld > 0)
            {
                // fall back to the last still-held note (classic mono-synth priority)
                const int back = held[numHeld - 1];
                if (prm.mode == Legato) mv.glideTo (back, mv.vel);
                else mv.start (back, mv.vel, prm.glide > 0.0005f ? mv.pitch : -1.0f, true, rnd);
                lastPitch = (float) back;
            }
            else mv.release();
        }

        void pushHeld (int nn) { removeHeld (nn); if (numHeld < 32) held[numHeld++] = nn; }
        void removeHeld (int nn)
        {
            for (int i = 0; i < numHeld; ++i)
                if (held[i] == nn) { for (int j = i; j < numHeld - 1; ++j) held[j] = held[j + 1]; --numHeld; return; }
        }

        double sr = 44100.0;
        int maxBlockSize = 512;
        Params prm; FxParams fxp;
        std::array<Voice, kMaxVoices> voices;
        int held[32] {}; int numHeld = 0;
        bool sustained[128] {}; bool sustainDown = false;
        float bend = 0.0f, modWheel = 0.0f, lastPitch = -1.0f;
        double lfoPhase = 0.0; float shValue = 0.0f;
        juce::Random rnd { 12345 };
        std::vector<float> scratchL, scratchR;
        juce::SmoothedValue<float> outSmooth;
    };
}
