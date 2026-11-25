/*
  ==============================================================================
    Mister Master - MasterChain.h
    (c)(p) 2026 Tully EDM Vibe

    Style-based mastering in the spirit of online mastering services:
      Smart EQ (learned tonal match to the chosen style) > style tilt + manual EQ
      > 3-band glue compression (+ de-ess on the top band) > stereo width with
      mono bass > loudness gain (learned to hit the target LUFS) > warmth
      soft-clip > true-peak lookahead limiter > BS.1770 loudness metering.

    LEARN: play the loudest part of the song for ~20 s. Mister Master measures
    the spectrum, band levels and loudness, sets the Smart EQ and compressor
    thresholds, then the gain that lands the master on the target loudness,
    and finally checks the real output and corrects itself.
  ==============================================================================
*/
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "TullyDSP.h"
#include "StreamCheck.h"

namespace kmaster
{
    inline juce::StringArray styleNames() { return { "Warm", "Balanced", "Open" }; }
    inline juce::StringArray loudNames() { return { "Low", "Medium", "High" }; }
    inline float targetLufs (int loud) { return loud == 0 ? -14.0f : loud == 1 ? -11.0f : -8.0f; }

    constexpr int kOct = 9;
    inline const std::array<double, kOct>& octaveCentres() { static const std::array<double, kOct> f { 63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000 }; return f; }
    // Average octave-band balance of commercial masters (dB, relative), per style.
    inline std::array<float, kOct> styleTarget (int style)
    {
        std::array<float, kOct> t { 0.0f, 0.0f, -2.0f, -4.5f, -6.5f, -8.5f, -11.0f, -14.0f, -20.0f };
        const std::array<float, kOct> warm { 1.5f, 1.2f, 0.8f, 0.3f, 0.0f, -0.4f, -0.9f, -1.5f, -2.0f };
        const std::array<float, kOct> open { -1.0f, -0.7f, -0.4f, -0.1f, 0.0f, 0.5f, 1.0f, 1.6f, 2.2f };
        for (int i = 0; i < kOct; ++i) t[(size_t) i] += style == 0 ? warm[(size_t) i] : style == 2 ? open[(size_t) i] : 0.0f;
        return t;
    }

    inline void addParameters (juce::AudioProcessorValueTreeState::ParameterLayout& l)
    {
        using namespace juce;
        auto pct = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + "%"; });
        auto db  = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return (v > 0.04f ? "+" : "") + String (v, 1) + " dB"; });
        auto lu  = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return (v > 0.04f ? "+" : "") + String (v, 1) + " LU"; });
        auto tp  = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " dBTP"; });
        auto f = [&l] (const char* id, const char* name, NormalisableRange<float> r, float def, AudioParameterFloatAttributes a)
        { l.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def, a)); };
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { "style", 1 }, "Style", styleNames(), 1));
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { "loudness", 1 }, "Loudness", loudNames(), 1));
        f ("loud_trim", "Loudness Trim", { -3.0f, 3.0f, 0.1f }, 0.0f, lu);
        f ("smart_eq", "Smart EQ", { 0.0f, 1.0f }, 0.6f, pct);
        f ("eq_low", "Low", { -6.0f, 6.0f }, 0.0f, db);
        f ("eq_mid", "Mid", { -6.0f, 6.0f }, 0.0f, db);
        f ("eq_pres", "Presence", { -6.0f, 6.0f }, 0.0f, db);
        f ("eq_air", "Air", { -6.0f, 6.0f }, 0.0f, db);
        f ("comp", "Compression", { 0.0f, 1.0f }, 0.4f, pct);
        f ("warmth", "Warmth", { 0.0f, 1.0f }, 0.25f, pct);
        f ("deess", "De-ess", { 0.0f, 1.0f }, 0.2f, pct);
        f ("width", "Stereo Width", { 0.0f, 2.0f }, 1.0f, pct);
        l.add (std::make_unique<AudioParameterBool> (ParameterID { "mono_bass", 1 }, "Mono Bass", true));
        f ("ceiling", "Ceiling", { -3.0f, -0.1f, 0.1f }, -1.0f, tp);
        l.add (std::make_unique<AudioParameterBool> (ParameterID { "compare", 1 }, "Compare", false));
        // 1.5: streaming check (display only: it never changes the sound). Appended after the older parameters.
        l.add (std::make_unique<AudioParameterBool> (ParameterID { "stream_check", 1 }, "Streaming Check", true));
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { "platform", 1 }, "Check For", platformNames(), 0));
    }

    struct Settings
    {
        int style = 1, loud = 1; float trim = 0, smartEq = 0.6f, eqLow = 0, eqMid = 0, eqPres = 0, eqAir = 0;
        float comp = 0.4f, warmth = 0.25f, deess = 0.2f, width = 1; bool monoBass = true; float ceiling = -1; bool compare = false;
        bool sameTone (const Settings& o) const
        {
            return style == o.style && smartEq == o.smartEq && eqLow == o.eqLow && eqMid == o.eqMid && eqPres == o.eqPres && eqAir == o.eqAir;
        }
        bool sameLoudnessInputs (const Settings& o) const
        {
            return sameTone (o) && loud == o.loud && trim == o.trim && comp == o.comp && warmth == o.warmth && deess == o.deess
                && width == o.width && monoBass == o.monoBass && ceiling == o.ceiling;
        }
    };

    class Reader
    {
    public:
        void attach (juce::AudioProcessorValueTreeState& s)
        {
            for (auto* id : { "style", "loudness", "loud_trim", "smart_eq", "eq_low", "eq_mid", "eq_pres", "eq_air", "comp", "warmth", "deess", "width", "mono_bass", "ceiling", "compare" })
            { p[id] = s.getRawParameterValue (id); jassert (p[id] != nullptr); }
        }
        Settings read() const
        {
            auto v = [this] (const char* id) { return p.at (id)->load(); };
            Settings s;
            s.style = (int) v ("style"); s.loud = (int) v ("loudness"); s.trim = v ("loud_trim"); s.smartEq = v ("smart_eq");
            s.eqLow = v ("eq_low"); s.eqMid = v ("eq_mid"); s.eqPres = v ("eq_pres"); s.eqAir = v ("eq_air");
            s.comp = v ("comp"); s.warmth = v ("warmth"); s.deess = v ("deess"); s.width = v ("width"); s.monoBass = v ("mono_bass") > 0.5f;
            s.ceiling = v ("ceiling"); s.compare = v ("compare") > 0.5f;
            return s;
        }
    private:
        std::map<std::string, std::atomic<float>*> p;
    };

    // What LEARN found; saved with the session so the master is identical on reload.
    struct Learned
    {
        bool valid = false;
        std::array<float, kOct> octaveDb {};    // measured spectrum (dB, mean-aligned)
        std::array<float, 3> bandDb { -24.0f, -24.0f, -30.0f };
        float inLufs = -16.0f;                   // input loudness of the learned section
        float gainDb = 5.0f;                     // loudness gain (before trim) for `forTarget`
        float forTarget = -11.0f;
        juce::ValueTree toTree() const
        {
            juce::ValueTree t ("LEARNED");
            t.setProperty ("valid", valid, nullptr); t.setProperty ("inLufs", inLufs, nullptr);
            t.setProperty ("gain", gainDb, nullptr); t.setProperty ("target", forTarget, nullptr);
            juce::String o, b;
            for (auto v : octaveDb) o << juce::String (v, 2) << " ";
            for (auto v : bandDb) b << juce::String (v, 2) << " ";
            t.setProperty ("oct", o.trim(), nullptr); t.setProperty ("bands", b.trim(), nullptr);
            return t;
        }
        void fromTree (const juce::ValueTree& t)
        {
            if (! t.isValid()) return;
            valid = (bool) t.getProperty ("valid", false); inLufs = (float) t.getProperty ("inLufs", -16.0f);
            gainDb = (float) t.getProperty ("gain", 5.0f); forTarget = (float) t.getProperty ("target", -11.0f);
            auto o = juce::StringArray::fromTokens (t.getProperty ("oct").toString(), " ", "");
            auto b = juce::StringArray::fromTokens (t.getProperty ("bands").toString(), " ", "");
            for (int i = 0; i < kOct && i < o.size(); ++i) octaveDb[(size_t) i] = o[i].getFloatValue();
            for (int i = 0; i < 3 && i < b.size(); ++i) bandDb[(size_t) i] = b[i].getFloatValue();
        }
    };

    //==========================================================================
    class Chain
    {
    public:
        enum Phase { Idle = 0, Spectrum, Bands, Gain, Check1, Check2, Done };

        void prepare (double sampleRate, int maxBlock)
        {
            sr = sampleRate;
            limiter.prepare (sr, 5.0);
            limiter.setRelease (60.0);
            for (auto* m : { &inMeter, &outMeter, &winIn, &winOut, &winPre, &learnAll, &ckIn, &ckOut }) m->prepare (sr);
            for (auto& d : cmpDelay) d.prepare (limiter.getLatency() + 8);
            for (int i = 0; i < kOct; ++i)
                octBp[(size_t) i].set (tdsp::Biquad::BandPass, sr, octaveCentres()[(size_t) i], 1.41);
            auto lr = [this] (tdsp::Biquad (&f)[2], tdsp::Biquad::Type t, double fc) { f[0].set (t, sr, fc, 0.7071); f[1].set (t, sr, fc, 0.7071); };
            lr (lp1, tdsp::Biquad::LowPass, 150.0); lr (hp1, tdsp::Biquad::HighPass, 150.0);
            lr (lp2, tdsp::Biquad::LowPass, 4000.0); lr (hp2, tdsp::Biquad::HighPass, 4000.0);
            lr (apLp, tdsp::Biquad::LowPass, 4000.0); lr (apHp, tdsp::Biquad::HighPass, 4000.0);
            lr (mLp, tdsp::Biquad::LowPass, 120.0); lr (mHp, tdsp::Biquad::HighPass, 120.0); lr (sHp, tdsp::Biquad::HighPass, 120.0);
            dsDet.set (tdsp::Biquad::HighPass, sr, 6000.0, 0.7071);
            envCoef = tdsp::timeCoef (10.0, sr);
            makeupCoef = tdsp::timeCoef (4000.0, sr);
            dsSm.setTimes (1.0, 60.0, sr);
            gainSmooth.reset (sr, 0.25);
            gainSmooth.setCurrentAndTargetValue (tdsp::dbToGain (currentGainDb()));
            matchSmooth.reset (sr, 0.3);
            scratch.assign ((size_t) juce::jmax (maxBlock, 4096) * 2, 0.0f);
            last = {}; last.style = -1;
            reset();
        }
        int getLatency() const { return limiter.getLatency(); }

        void reset()
        {
            auto rs = [] (tdsp::Biquad* b, int n) { for (int i = 0; i < n; ++i) b[i].reset(); };
            rs (smart.data(), kOct); rs (manual.data(), 6);
            rs (lp1, 2); rs (hp1, 2); rs (lp2, 2); rs (hp2, 2); rs (apLp, 2); rs (apHp, 2); rs (octBp.data(), kOct);
            rs (mLp, 2); rs (mHp, 2); rs (sHp, 2); dsDet.reset();
            for (auto& g : bandGr) g.state = 0; for (auto& e : bandEnv) e = 0; for (auto& m : bandMakeup) m = 0;
            dsSm.state = 0; dsEnvH = dsEnvF = 0;
            limiter.reset();
            for (auto& d : cmpDelay) d.reset();
            tpMax.store (-100.0f);
            resetCheckIn(); resetCheckOut(); clearGraphNow (true, true);
        }

        // ---------------- learning control (message thread)
        void startLearning() { learnRequest.store (true); }
        void restoreLearned (const Learned& l) { const juce::SpinLock::ScopedLockType sl (learnLock); learned = l; pendingRestore = true; }
        Learned getLearned() const { const juce::SpinLock::ScopedLockType sl (learnLock); return learned; }
        void resetMeters() { resetMetersRequest.store (true); }
        void resetOutput() { resetOutputRequest.store (true); }         // the master's integrated loudness and peak only

        std::atomic<int> phase { Idle };
        std::atomic<float> progress { 0.0f };
        std::atomic<float> inLufsS { -100.0f }, outLufsM { -100.0f }, outLufsS { -100.0f }, outLufsI { -100.0f }, tpMax { -100.0f }, limGr { 0.0f };
        std::array<std::atomic<float>, 3> bandGrDb {};
        std::atomic<float> appliedGainDb { 0.0f }, compareGainDb { 0.0f };
        tdsp::Meter meterOut;

        // ---------------- streaming check (measured on the audio thread, read anywhere)
        // ---- loudness along the song (for the graph): 3 s loudness of the mix and of the master at each quarter second
        static constexpr int kGraph = 4800;                 // 20 minutes
        std::array<std::atomic<float>, kGraph> gIn, gOut, gBar; std::array<std::atomic<unsigned char>, kGraph> gClip;
        std::atomic<int> gMax { -1 };
        // from processBlock, before process(): where the song is (secs < 0 = the host does not say), and whether it plays
        void setTransport (double secs, bool playing, double bar, double barsPerSecond) { tpSecs = secs; tpPlaying = playing; tpBar = bar; tpBarRate = barsPerSecond; }
        void clearGraph() { graphClearRequest.store (true); }

        // FIX IT and the streaming presets: after the normal level check, listen to the real master and take off
        // whatever is still over `lufs` (never turns up; stops when you change the loudness yourself)
        void holdUnder (float lufs) { holdLufs.store (lufs); holdPasses.store (0); }
        StreamStats streamStats() const
        {
            StreamStats s;
            s.outLufs = ckOutL.load(); s.outTp = ckOutTp.load(); s.outSecs = ckOutSecs.load();
            s.inLufs = ckInL.load(); s.inTp = ckInTp.load(); s.inSecs = ckInSecs.load(); s.inClips = ckClips.load();
            const int ph = phase.load(); s.learning = ph != Idle && ph != Done; s.recheck = ph == Check1 || ph == Check2;
            return s;
        }

        // EQ curve for display (smart EQ + style + manual), dB
        double eqResponseDb (double f) const
        {
            double m = 0;
            for (auto& b : smart) m += b.magnitudeDb (f, sr);
            for (auto& b : manual) m += b.magnitudeDb (f, sr);
            return m;
        }
        std::array<float, kOct> smartGains() const { std::array<float, kOct> g; for (int i = 0; i < kOct; ++i) g[(size_t) i] = smartDb[(size_t) i].load(); return g; }

        //======================================================================
        void process (float* L, float* R, int n, const Settings& s)
        {
            if ((int) scratch.size() < 2 * n) scratch.assign ((size_t) n * 2, 0.0f);
            handleRequests (s);
            updateFilters (s);
            limiter.setCeilingDb (s.ceiling);

            // style flavours for the dynamics
            const float attMul = s.style == 0 ? 1.3f : s.style == 2 ? 0.7f : 1.0f;
            const bool learningComp = phase.load() == Bands;          // measure band levels with the compressor out of the way
            const float ratio = 1.4f + 1.6f * s.comp;
            const float offs = 4.0f - 10.0f * s.comp;
            const float warm = juce::jmin (1.0f, s.warmth * (s.style == 0 ? 1.3f : s.style == 2 ? 0.8f : 1.0f));
            const float ceilLin = tdsp::dbToGain (s.ceiling);
            for (int b = 0; b < 3; ++b)
            {
                const double att[3] { 30.0, 15.0, 5.0 }, rel[3] { 200.0, 150.0, 100.0 };
                bandGr[b].setTimes (att[b] * attMul, rel[b], sr);
                thr[b] = learnedBand[(size_t) b] + offs;
            }
            gainSmooth.setTargetValue (tdsp::dbToGain (currentGainDb() + s.trim));

            inMeter.process (L, R, n);
            winIn.process (L, R, n);
            measureIn (L, R, n);
            if (fullLearn || phase.load() == Spectrum) learnAll.process (L, R, n);
            if (phase.load() == Spectrum) accumulateSpectrum (L, R, n);

            float* inCopyL = scratch.data(); float* inCopyR = scratch.data() + n;
            std::copy (L, L + n, inCopyL); std::copy (R, R + n, inCopyR);

            float bandMin[3] { 0, 0, 0 };
            for (int i = 0; i < n; ++i)
            {
                float l = L[i], r = R[i];
                for (auto& b : smart)  { l = b.process (l, 0); r = b.process (r, 1); }
                for (auto& b : manual) { l = b.process (l, 0); r = b.process (r, 1); }

                // 3-band split (Linkwitz-Riley 4th order); low band gets the 4 kHz all-pass so the bands sum flat
                float lowL = lp1[1].process (lp1[0].process (l, 0), 0), lowR = lp1[1].process (lp1[0].process (r, 1), 1);
                const float restL = hp1[1].process (hp1[0].process (l, 0), 0), restR = hp1[1].process (hp1[0].process (r, 1), 1);
                float midL = lp2[1].process (lp2[0].process (restL, 0), 0), midR = lp2[1].process (lp2[0].process (restR, 1), 1);
                float hiL = hp2[1].process (hp2[0].process (restL, 0), 0), hiR = hp2[1].process (hp2[0].process (restR, 1), 1);
                lowL = apLp[1].process (apLp[0].process (lowL, 0), 0) + apHp[1].process (apHp[0].process (lowL, 0), 0);
                lowR = apLp[1].process (apLp[0].process (lowR, 1), 1) + apHp[1].process (apHp[0].process (lowR, 1), 1);
                float* bl[3] { &lowL, &midL, &hiL }; float* br[3] { &lowR, &midR, &hiR };

                const bool loudEnough = winPreGate;
                for (int b = 0; b < 3; ++b)
                {
                    const float pw = juce::jmax ((*bl[b]) * (*bl[b]), (*br[b]) * (*br[b]));
                    bandEnv[b] = pw + envCoef * (bandEnv[b] - pw);
                    const float xDb = 10.0f * std::log10 (bandEnv[b] + 1e-12f);
                    if (learningComp) { bandAcc[b] += bandEnv[b]; ++bandAccN[b]; }
                    float gr = 0.0f;
                    if (! learningComp && s.comp > 0.001f) gr = bandGr[b].process (tdsp::compGainDb (xDb, thr[b], ratio, 6.0f));
                    else bandGr[b].state = 0.0f;
                    if (loudEnough) bandMakeup[b] = gr + makeupCoef * (bandMakeup[b] - gr);
                    const float g = tdsp::dbToGain (gr - 0.9f * bandMakeup[b]);
                    *bl[b] *= g; *br[b] *= g;
                    bandMin[b] = juce::jmin (bandMin[b], gr);
                }
                // de-ess: dynamic dip of the top band when it gets harsh
                if (s.deess > 0.001f)
                {
                    const float hd = juce::jmax (std::abs (dsDet.process (l, 0)), std::abs (dsDet.process (r, 1)));
                    const float fd = juce::jmax (std::abs (l), std::abs (r));
                    dsEnvH = hd + envCoef * (dsEnvH - hd); dsEnvF = fd + envCoef * (dsEnvF - fd);
                    const float excess = tdsp::gainToDb (dsEnvH) - tdsp::gainToDb (dsEnvF) + 12.0f;
                    const float g = tdsp::dbToGain (dsSm.process (-juce::jlimit (0.0f, 6.0f, excess) * s.deess));
                    hiL *= g; hiR *= g;
                }
                l = lowL + midL + hiL; r = lowR + midR + hiR;

                // stereo: width on the side signal, bass below 120 Hz folded to mono
                {
                    float m = 0.5f * (l + r);
                    float sd = 0.5f * (l - r);
                    if (s.monoBass)
                    {
                        // Linkwitz-Riley split at 120 Hz: side keeps only the highs, mid gets the matching all-pass
                        m = mLp[1].process (mLp[0].process (m)) + mHp[1].process (mHp[0].process (m));
                        sd = sHp[1].process (sHp[0].process (sd)) * s.width;
                    }
                    else sd *= s.width;
                    l = m + sd; r = m - sd;
                }
                L[i] = l; R[i] = r;
            }
            for (int b = 0; b < 3; ++b) bandGrDb[(size_t) b].store (bandMin[b]);

            winPre.process (L, R, n);   // pre-gain loudness (used while learning)
            {
                const float m = winPre.momentary.load();
                winPreGate = m > -45.0f;
            }

            // loudness gain + warmth soft-clip, then the true-peak limiter
            for (int i = 0; i < n; ++i)
            {
                const float g = gainSmooth.getNextValue();
                float l = L[i] * g, r = R[i] * g;
                if (warm > 0.001f)
                {
                    const float sl = l / ceilLin, sr2 = r / ceilLin;
                    l = ceilLin * (sl + warm * (std::tanh (sl) - sl));
                    r = ceilLin * (sr2 + warm * (std::tanh (sr2) - sr2));
                }
                L[i] = l; R[i] = r;
            }
            limiter.process (L, R, n);
            limGr.store (limiter.gr.load());

            outMeter.process (L, R, n);
            winOut.process (L, R, n);
            {
                tdsp::TruePeak& tp = outTp; float pk = 0;
                for (int i = 0; i < n; ++i) pk = juce::jmax (pk, tp.process (L[i]), tpR.process (R[i]));
                tpMax.store (juce::jmax (tpMax.load(), tdsp::gainToDb (pk)));
                measureOut (L, R, n, pk, s);
                recordGraph (n);
            }
            inLufsS.store (inMeter.shortTerm.load());
            outLufsM.store (outMeter.momentary.load()); outLufsS.store (outMeter.shortTerm.load()); outLufsI.store (outMeter.integrated.load());
            appliedGainDb.store (currentGainDb() + s.trim);

            // compare: original, delayed to line up, matched to the master's loudness
            const float inI = inMeter.integrated.load(), outI = outMeter.integrated.load();
            {
                float pk = 0; for (int i = 0; i < n; ++i) pk = juce::jmax (pk, std::abs (inCopyL[i]), std::abs (inCopyR[i]));
                inPeakDb = juce::jmax (inPeakDb, tdsp::gainToDb (pk));
                float matchDb = inI > -70.0f && outI > -70.0f ? juce::jlimit (-30.0f, 30.0f, outI - inI) : 0.0f;
                matchDb = juce::jmin (matchDb, s.ceiling - inPeakDb);      // headroom: the original never clips
                compareGainDb.store (matchDb);
                matchSmooth.setTargetValue (tdsp::dbToGain (matchDb));
            }
            for (int i = 0; i < n; ++i)
            {
                cmpDelay[0].push (inCopyL[i]); cmpDelay[1].push (inCopyR[i]);
                const float mg = matchSmooth.getNextValue();
                if (s.compare)
                {
                    L[i] = cmpDelay[0].read ((float) getLatency() + 1.0f) * mg;
                    R[i] = cmpDelay[1].read ((float) getLatency() + 1.0f) * mg;
                }
            }
            meterOut.push (L, R, n);

            advanceLearning (s, n);
        }

    private:
        float currentGainDb() const { return learnedGain + (target - learnedFor); }
        float targetGainFor (const Settings& s) { target = targetLufs (s.loud); return currentGainDb() + s.trim; }

        void handleRequests (const Settings& s)
        {
            target = targetLufs (s.loud);
            if (resetMetersRequest.exchange (false)) { resetCheckIn(); resetCheckOut(); clearGraphNow (true, true); inMeter.resetIntegrated(); outMeter.resetIntegrated(); tpMax.store (-100.0f); inPeakDb = -100.0f; }
            if (resetOutputRequest.exchange (false)) { outMeter.resetIntegrated(); tpMax.store (-100.0f); resetCheckOut(); clearGraphNow (false, true); }
            if (pendingRestore.exchange (false))
            {
                const juce::SpinLock::ScopedLockType sl (learnLock);
                if (learned.valid)
                {
                    learnedSpectrum = learned.octaveDb; learnedBand = learned.bandDb; learnedIn = learned.inLufs;
                    learnedGain = learned.gainDb; learnedFor = learned.forTarget; haveSpectrum = true;
                    last.style = -1;   // rebuild smart EQ
                    gainSmooth.setCurrentAndTargetValue (tdsp::dbToGain (targetGainFor (s)));
                    phase.store (Done); progress.store (1.0f);
                    lastLearnSettings = s;
                }
            }
            if (learnRequest.exchange (false)) { resetCheckIn(); beginPhase (Spectrum); }
            // auto-learn on first audio when nothing has been learned yet
            if (phase.load() == Idle && ! autoStarted && winIn.momentary.load() > -45.0f) { autoStarted = true; beginPhase (Spectrum); }
            // settings that change loudness after learning: re-check the output level
            if (phase.load() == Done && ! s.sameLoudnessInputs (lastLearnSettings))
            {
                lastLearnSettings = s;
                settleCountdown = (int) (0.6 * sr);
            }
            if (settleCountdown > 0)
            {
                settleCountdown -= juce::jmax (1, blockGuess);
                if (settleCountdown <= 0 && phase.load() == Done) beginPhase (Check1);
            }
            lastSettings = s;
        }

        void beginPhase (int p)
        {
            phase.store (p); phaseSamples = 0; activeSamples = 0;
            winIn.resetIntegrated(); winOut.resetIntegrated(); winPre.resetIntegrated();
            if (p == Spectrum) { octAcc.fill (0.0); octN = 0; haveSpectrum = false; last.style = -1; learnAll.resetIntegrated(); }
            if (p == Bands) { for (int b = 0; b < 3; ++b) { bandAcc[b] = 0; bandAccN[b] = 0; } }
        }

        // ---- streaming check
        void resetCheckIn() { ckIn.reset(); ckInL.store (-100.0f); ckInTp.store (-100.0f); ckInSecs.store (0.0f); ckClips.store (0); clipRun = 0; }
        void resetCheckOut() { ckOut.reset(); ckOutL.store (-100.0f); ckOutTp.store (-100.0f); ckOutSecs.store (0.0f); ckSkip = (int) (0.3 * sr); if (phase.load() == Done) winIn.resetIntegrated(); }
        void measureIn (const float* L, const float* R, int n)
        {
            ckIn.process (L, R, n);
            float pk = 0;
            for (int i = 0; i < n; ++i)
            {
                pk = juce::jmax (pk, ckTpL.process (L[i]), ckTpR.process (R[i]));
                // a clip = two or more samples in a row at full scale (or beyond it: a hot float bus)
                if (std::abs (L[i]) >= 0.9995f || std::abs (R[i]) >= 0.9995f) { if (++clipRun == 2) { ckClips.store (ckClips.load() + 1); clipNow = true; } }
                else clipRun = 0;
            }
            ckInTp.store (juce::jmax (ckInTp.load(), tdsp::gainToDb (pk)));
            if (ckIn.momentary.load() > -45.0f) ckInSecs.store (ckInSecs.load() + (float) (n / sr));   // only count time with music playing
            ckInL.store (ckIn.integrated.load());
        }
        void measureOut (const float* L, const float* R, int n, float peak, const Settings& s)
        {
            // the numbers belong to the current settings and a finished LEARN: start again whenever either changes
            const int ph = phase.load();
            if (ph != ckPhase || ! s.sameLoudnessInputs (ckSettings)) { if (ph == Done && ckPhase >= Check1) { outMeter.resetIntegrated(); tpMax.store (-100.0f); } ckPhase = ph; ckSettings = s; resetCheckOut(); clearGraphNow (false, true); }
            if (ph != Done || settleCountdown > 0) return;
            if (ckSkip > 0) { ckSkip -= n; return; }           // let the gain settle
            ckOut.process (L, R, n);
            ckOutTp.store (juce::jmax (ckOutTp.load(), tdsp::gainToDb (peak)));
            if (ckOut.momentary.load() > -45.0f) ckOutSecs.store (ckOutSecs.load() + (float) (n / sr));
            ckOutL.store (ckOut.integrated.load());
            const float hold = holdLufs.load();
            if (hold < 0.0f)
            {
                if (std::abs (targetLufs (s.loud) + s.trim - hold) > 0.05f) { if (holdPasses.load() > 0) holdLufs.store (1.0f); }   // changed by hand: leave it alone
                else if (ckOutSecs.load() >= 8.0f && winIn.integrated.load() > -70.0f)
                {
                    // like for like: this stretch of the song against the learned (loudest) one, as the level check does
                    const float over = ckOutL.load() + juce::jlimit (-6.0f, 6.0f, learnedIn - winIn.integrated.load()) - hold; const int pass = holdPasses.load();
                    if (over > 0.25f && pass < 4)
                    {
                        learnedGain = juce::jlimit (-24.0f, 30.0f, learnedGain + (target - learnedFor) - juce::jmin (4.0f, over + 0.1f)); learnedFor = target;
                        holdPasses.store (pass + 1);
                        { const juce::SpinLock::ScopedLockType sl (learnLock); if (learned.valid) { learned.gainDb = learnedGain; learned.forTarget = learnedFor; } }
                        resetCheckOut(); clearGraphNow (false, true); outMeter.resetIntegrated();
                    }
                }
            }
        }

        void clearGraphNow (bool in, bool out)
        {
            for (int i = 0; i < kGraph; ++i)
            {
                if (in) { gIn[(size_t) i].store (kNoData, std::memory_order_relaxed); gBar[(size_t) i].store (0.0f, std::memory_order_relaxed); gClip[(size_t) i].store (0, std::memory_order_relaxed); }
                if (out) gOut[(size_t) i].store (kNoData, std::memory_order_relaxed);
            }
            if (in) { gMax.store (-1); graphRun = 0.0; graphWarm = 0.0; graphLast = -10.0; }
        }
        void recordGraph (int n)
        {
            if (graphClearRequest.exchange (false)) clearGraphNow (true, true);
            const double dt = n / sr; const bool clip = clipNow; clipNow = false;
            const bool host = tpSecs >= 0.0;
            if (host ? ! tpPlaying : winIn.momentary.load() < -60.0f) { graphWarm = 0.0; return; }
            const double now = host ? tpSecs + dt : (graphRun += dt);
            if (now < graphLast - 0.05 || now > graphLast + dt + 0.2) graphWarm = 0.0;      // the playhead jumped: the 3 s window holds another place
            graphLast = now; graphWarm += dt;
            auto slotOf = [] (double t) { return (int) (t / (double) kGraphSlot); };
            if (clip) { const int c = slotOf (now); if (c >= 0 && c < kGraph) { gClip[(size_t) c].store (1, std::memory_order_relaxed); if (c > gMax.load()) gMax.store (c); } }
            if (graphWarm < 1.5) return;
            // 3 s loudness belongs to the middle of its window (a shorter window right after a start or a jump)
            const double span = juce::jmin (3.0, graphWarm); const int c = slotOf (now - 0.5 * span);
            if (c < 0 || c >= kGraph) return;
            const float comp = (float) (10.0 * std::log10 (3.0 / span));                      // the meter divides by 3 s even while it fills
            const float i = inMeter.shortTerm.load(), o = outMeter.shortTerm.load();
            const bool fresh = graphWarm < 3.0;
            if (i > -60.0f) gIn[(size_t) c].store (fresh ? i + comp : i, std::memory_order_relaxed);
            if (o > -60.0f && phase.load() == Done && settleCountdown <= 0 && ckOutSecs.load() > juce::jmin (3.0f, (float) graphWarm) - 0.1f) gOut[(size_t) c].store (fresh ? o + comp : o, std::memory_order_relaxed);
            if (tpBar >= 0.0) gBar[(size_t) c].store ((float) juce::jmax (1.0, std::floor (tpBar - 0.5 * span * tpBarRate) + 1.0), std::memory_order_relaxed);
            if (c > gMax.load()) gMax.store (c);
        }

        void accumulateSpectrum (const float* L, const float* R, int n)
        {
            for (int i = 0; i < n; ++i)
            {
                const float m = 0.5f * (L[i] + R[i]);
                for (int k = 0; k < kOct; ++k) { const float y = octBp[(size_t) k].process (m); octAcc[(size_t) k] += (double) y * y; }
            }
            octN += n;
        }

        void advanceLearning (const Settings& s, int n)
        {
            blockGuess = n;
            const int p = phase.load();
            if (p == Idle || p == Done) return;
            phaseSamples += n;
            if (winIn.momentary.load() > -45.0f) activeSamples += n;      // only count time with music playing
            const double need[] { 0, 6.0, 3.0, 4.0, 4.0, 4.0, 0 };
            const double secs = activeSamples / sr;
            // overall progress for the UI
            double done = 0, total = 0;
            for (int k = Spectrum; k <= Check2; ++k) { total += need[k]; if (k < p) done += need[k]; }
            if (p >= Check1 && ! fullLearn) { progress.store ((float) juce::jlimit (0.0, 1.0, (secs + (p == Check2 ? need[Check1] : 0)) / (need[Check1] + need[Check2]))); }
            else progress.store ((float) juce::jlimit (0.0, 1.0, (done + juce::jmin (secs, need[p])) / total));
            if (secs < need[p]) return;

            if (p == Spectrum)
            {
                fullLearn = true;
                std::array<float, kOct> db {};
                for (int k = 0; k < kOct; ++k) db[(size_t) k] = (float) (10.0 * std::log10 (octAcc[(size_t) k] / juce::jmax<long long> (1, octN) + 1e-14));
                learnedSpectrum = db; haveSpectrum = true; last.style = -1;
                learnedIn = winIn.integrated.load();
                beginPhase (Bands);
            }
            else if (p == Bands)
            {
                for (int b = 0; b < 3; ++b)
                    learnedBand[(size_t) b] = (float) (10.0 * std::log10 (bandAcc[b] / juce::jmax<long long> (1, bandAccN[b]) + 1e-14));
                for (auto& m : bandMakeup) m = 0;
                beginPhase (Gain);
            }
            else if (p == Gain)
            {
                // pre-gain loudness with EQ + compression active -> gain for the target
                const float pre = winPre.integrated.load();
                learnedIn = learnAll.integrated.load();
                const float preForReference = pre + (learnedIn - winIn.integrated.load());
                learnedGain = juce::jlimit (-12.0f, 30.0f, target - preForReference); learnedFor = target;
                beginPhase (Check1);
            }
            else if (p == Check1 || p == Check2)
            {
                // compare real output with the target; normalise for how loud this stretch of the song is
                const float out = winOut.integrated.load(), in = winIn.integrated.load();
                if (fullLearn) learnedIn = learnAll.integrated.load();
                if (out > -70.0f && in > -70.0f)
                {
                    const float err = (target + s.trim) - out + (in - learnedIn);   // estimate for the learned section
                    learnedGain = juce::jlimit (-12.0f, 30.0f, learnedGain + (target - learnedFor) + juce::jlimit (-4.0f, 4.0f, err * (err > 0 ? 1.25f : 1.0f)));
                    learnedFor = target;
                }
                if (p == Check1) beginPhase (Check2);
                else
                {
                    phase.store (Done); progress.store (1.0f); fullLearn = false;
                    lastLearnSettings = s;
                    const juce::SpinLock::ScopedLockType sl (learnLock);
                    learned.valid = true; learned.octaveDb = learnedSpectrum; learned.bandDb = learnedBand;
                    learned.inLufs = learnedIn; learned.gainDb = learnedGain; learned.forTarget = learnedFor;
                }
            }
        }

        void updateFilters (const Settings& s)
        {
            if (! s.sameTone (last) || last.style < 0)
            {
                // smart EQ: move the learned spectrum part-way towards the style's target balance
                std::array<float, kOct> g {};
                if (haveSpectrum)
                {
                    const auto t = styleTarget (s.style);
                    std::array<float, kOct> d {};
                    double mm = 0, mt = 0;
                    for (int k = 2; k <= 6; ++k) { mm += learnedSpectrum[(size_t) k]; mt += t[(size_t) k]; }
                    for (int k = 0; k < kOct; ++k) d[(size_t) k] = (float) ((t[(size_t) k] - mt / 5.0) - (learnedSpectrum[(size_t) k] - mm / 5.0));
                    for (int k = 0; k < kOct; ++k)
                    {
                        const float a = d[(size_t) juce::jmax (0, k - 1)], b = d[(size_t) k], c = d[(size_t) juce::jmin (kOct - 1, k + 1)];
                        float v = (0.25f * a + 0.5f * b + 0.25f * c) * 0.6f;
                        const float maxBoost = k == kOct - 1 || k == 0 ? 2.0f : 4.0f;    // never pump up empty extremes
                        g[(size_t) k] = juce::jlimit (-4.0f, maxBoost, v) * s.smartEq;
                    }
                }
                for (int k = 0; k < kOct; ++k)
                {
                    smartDb[(size_t) k].store (g[(size_t) k]);
                    const double fc = octaveCentres()[(size_t) k];
                    if (k == kOct - 1) smart[(size_t) k].set (tdsp::Biquad::HighShelf, sr, 11000.0, 0.7071, g[(size_t) k]);
                    else if (k == 0)   smart[(size_t) k].set (tdsp::Biquad::LowShelf, sr, 80.0, 0.7071, g[(size_t) k]);
                    else               smart[(size_t) k].set (tdsp::Biquad::Peak, sr, fc, 1.0, g[(size_t) k]);
                }
                // style tilt + manual EQ
                const float tiltLo = s.style == 0 ? 1.2f : s.style == 2 ? -0.5f : 0.0f;
                const float tiltHi = s.style == 0 ? -1.2f : s.style == 2 ? 1.5f : 0.0f;
                manual[0].set (tdsp::Biquad::LowShelf, sr, 110.0, 0.7071, s.eqLow + tiltLo);
                manual[1].set (tdsp::Biquad::Peak, sr, 700.0, 0.7, s.eqMid);
                manual[2].set (tdsp::Biquad::Peak, sr, 3500.0, 0.8, s.eqPres + (s.style == 2 ? 0.5f : 0.0f));
                manual[3].set (tdsp::Biquad::HighShelf, sr, 11000.0, 0.7071, s.eqAir + tiltHi);
                manual[4].set (tdsp::Biquad::HighPass, sr, 20.0, 0.7071);       // sub-sonic clean-up
                manual[5].set (tdsp::Biquad::Peak, sr, 1000.0, 1.0, 0.0);
            }
            last = s;
        }

        double sr = 48000.0;
        Settings last, lastSettings, lastLearnSettings;
        std::array<tdsp::Biquad, kOct> smart; std::array<tdsp::Biquad, 6> manual;
        std::array<std::atomic<float>, kOct> smartDb {};
        tdsp::Biquad lp1[2], hp1[2], lp2[2], hp2[2], apLp[2], apHp[2], mLp[2], mHp[2], sHp[2], dsDet;
        std::array<tdsp::Biquad, kOct> octBp;
        tdsp::GrSmoother bandGr[3], dsSm;
        float bandEnv[3] { 0, 0, 0 }, bandMakeup[3] { 0, 0, 0 }, thr[3] { -20, -20, -26 };
        double bandAcc[3] { 0, 0, 0 }; long long bandAccN[3] { 0, 0, 0 };
        float envCoef = 0, makeupCoef = 0, dsEnvH = 0, dsEnvF = 0;
        tdsp::Limiter limiter;
        tdsp::LoudnessMeter inMeter, outMeter, winIn, winOut, winPre, learnAll;   // learnAll: input over the whole learning run
        tdsp::TruePeak outTp, tpR;
        tdsp::LoudnessMeter ckIn, ckOut; tdsp::TruePeak ckTpL, ckTpR;         // streaming check
        std::atomic<float> ckOutL { -100.0f }, ckOutTp { -100.0f }, ckOutSecs { 0.0f }, ckInL { -100.0f }, ckInTp { -100.0f }, ckInSecs { 0.0f };
        std::atomic<int> ckClips { 0 }, holdPasses { 0 };
        std::atomic<float> holdLufs { 1.0f };                                  // 1 = not holding
        Settings ckSettings; int ckPhase = -1, ckSkip = 0, clipRun = 0; bool clipNow = false;
        double tpSecs = -1.0, tpBar = -1.0, tpBarRate = 0.0, graphRun = 0.0, graphWarm = 0.0, graphLast = -10.0; bool tpPlaying = false;
        std::atomic<bool> graphClearRequest { false };
        tdsp::DelayLine cmpDelay[2];
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> gainSmooth { 1.0f };
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> matchSmooth { 1.0f };
        std::vector<float> scratch;
        bool winPreGate = false;
        float inPeakDb = -100.0f;

        // learning state (audio thread)
        std::array<double, kOct> octAcc {}; long long octN = 0;
        std::array<float, kOct> learnedSpectrum {};
        std::array<float, 3> learnedBand { -24.0f, -24.0f, -30.0f };
        bool haveSpectrum = false, autoStarted = false, fullLearn = false;
        std::atomic<bool> pendingRestore { false };
        float learnedIn = -16.0f, learnedGain = 5.0f, learnedFor = -11.0f, target = -11.0f;
        long long phaseSamples = 0, activeSamples = 0;
        int settleCountdown = 0, blockGuess = 512;
        std::atomic<bool> learnRequest { false }, resetMetersRequest { false }, resetOutputRequest { false };
        mutable juce::SpinLock learnLock;
        Learned learned;
    };
}
