// KMTest: automated checks for Mister Master.
// Usage: KMTest mix_unmastered.wav outDir
#include <juce_audio_utils/juce_audio_utils.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <cstdio>

static int fails = 0, passes = 0;
static void check (bool ok, const juce::String& what) { printf ("%s  %s\n", ok ? "PASS" : "FAIL", what.toRawUTF8()); (ok ? passes : fails)++; }
static juce::AudioBuffer<float> load (const juce::File& f)
{
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (f));
    juce::AudioBuffer<float> b (2, (int) r->lengthInSamples);
    r->read (&b, 0, (int) r->lengthInSamples, 0, true, true);
    return b;
}
static void setP (KingMasterProcessor& p, const char* id, float v) { auto* q = p.apvts.getParameter (id); q->setValueNotifyingHost (q->convertTo0to1 (v)); }
static void prep (KingMasterProcessor& p, double sr, int block) { p.setPlayConfigDetails (2, 2, sr, block); p.prepareToPlay (sr, block); }
// process `src` looped `passes` times; returns the output of the last pass
static juce::AudioBuffer<float> run (KingMasterProcessor& p, const juce::AudioBuffer<float>& src, int passesN, int block = 512)
{
    juce::AudioBuffer<float> out (2, src.getNumSamples());
    juce::MidiBuffer m;
    for (int ps = 0; ps < passesN; ++ps)
        for (int s = 0; s < src.getNumSamples(); s += block)
        {
            const int k = juce::jmin (block, src.getNumSamples() - s);
            juce::AudioBuffer<float> b (2, k);
            b.copyFrom (0, 0, src, 0, s, k); b.copyFrom (1, 0, src, 1, s, k);
            p.processBlock (b, m);
            if (ps == passesN - 1) { out.copyFrom (0, s, b, 0, 0, k); out.copyFrom (1, s, b, 1, 0, k); }
        }
    return out;
}
static float lufs (const juce::AudioBuffer<float>& b, double sr, int skip = 0)
{
    tdsp::LoudnessMeter m; m.prepare (sr);
    m.process (b.getReadPointer (0) + skip, b.getReadPointer (1) + skip, b.getNumSamples() - skip);
    return m.integrated.load();
}
static float truePeakDb (const juce::AudioBuffer<float>& b)
{
    // independent 8x check with a long windowed-sinc (not the plugin's own detector)
    float pk = 0; const int taps = 64, os = 8;
    for (int c = 0; c < 2; ++c)
    {
        const float* x = b.getReadPointer (c);
        for (int i = taps; i < b.getNumSamples() - taps; ++i)
        {
            pk = std::max (pk, std::abs (x[i]));
            if (std::abs (x[i]) < 0.5f && std::abs (x[i + 1]) < 0.5f) continue;   // only look near loud samples
            for (int f = 1; f < os; ++f)
            {
                const double fr = (double) f / os; double s = 0;
                for (int t = -taps / 2 + 1; t <= taps / 2; ++t)
                {
                    const double d = t - fr, w = 0.5 + 0.5 * std::cos (tdsp::kPi * d / (taps / 2 + 1));
                    s += x[i + t] * std::sin (tdsp::kPi * d) / (tdsp::kPi * d) * w;
                }
                pk = std::max (pk, (float) std::abs (s));
            }
        }
    }
    return tdsp::gainToDb (pk);
}
static double bandDb (const juce::AudioBuffer<float>& b, double sr, double lo, double hi)
{
    tdsp::Biquad h1, h2, l1, l2; h1.set (tdsp::Biquad::HighPass, sr, lo, 0.707); h2 = h1; l1.set (tdsp::Biquad::LowPass, sr, hi, 0.707); l2 = l1;
    double e = 0;
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const float m = 0.5f * (b.getSample (0, i) + b.getSample (1, i));
        const float y = l2.process (l1.process (h2.process (h1.process (m))));
        e += (double) y * y;
    }
    return 10.0 * std::log10 (e / b.getNumSamples() + 1e-20);
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const auto mix = load (juce::File (argv[1]));
    const juce::File outDir (argv[2]); outDir.createDirectory();
    const double SR = 48000.0;

    // ---- meter + true peak calibration
    {
        juce::AudioBuffer<float> s (2, (int) SR * 5);
        const float a = tdsp::dbToGain (-23.0f);
        for (int i = 0; i < s.getNumSamples(); ++i) { const float v = a * std::sin (2.0f * juce::MathConstants<float>::pi * 1000.0f * (float) i / (float) SR); s.setSample (0, i, v); s.setSample (1, i, v); }
        check (std::abs (lufs (s, SR) + 23.0f) < 0.1f, "EBU 1 kHz stereo sine at -23 dBFS reads " + juce::String (lufs (s, SR), 2) + " LUFS (-23.0)");
        check (std::abs (lufs (mix, SR) + 20.0f) < 0.15f, "test mix reads " + juce::String (lufs (mix, SR), 2) + " LUFS (pyloudnorm: -20.00)");
        tdsp::TruePeak tp; float pk = 0;
        for (int i = 0; i < 4800; ++i) pk = std::max (pk, tp.process (std::sin ((float) (tdsp::kPi / 2 * i + tdsp::kPi / 4))));
        check (std::abs (tdsp::gainToDb (pk)) < 0.25f, "true-peak finds the 0 dBTP peak between -3 dB samples (" + juce::String (tdsp::gainToDb (pk), 2) + " dB)");
    }

    // ---- learn hits each loudness target
    for (int loud = 0; loud < 3; ++loud)
    {
        KingMasterProcessor p; prep (p, SR, 512);
        setP (p, "loudness", (float) loud);
        auto o = run (p, mix, 2);
        const float L = lufs (o, SR), tgt = kmaster::targetLufs (loud), tp = truePeakDb (o);
        check (p.chain.phase.load() == kmaster::Chain::Done, kmaster::loudNames()[loud] + ": learning finished within one play-through");
        check (std::abs (L - tgt) <= 1.0f, kmaster::loudNames()[loud] + ": master at " + juce::String (L, 2) + " LUFS (target " + juce::String (tgt, 0) + " +-1)");
        check (tp <= -0.95f, kmaster::loudNames()[loud] + ": true peak " + juce::String (tp, 2) + " dBTP (ceiling -1.0)");
        if (loud == 1)
        {
            juce::WavAudioFormat wav; auto wf = outDir.getChildFile ("mastered_medium.wav"); wf.deleteFile();
            std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (wf), SR, 2, 24, {}, 0));
            w->writeFromAudioSampleBuffer (o, 0, o.getNumSamples());
        }
    }

    // ---- changing loudness after learning re-targets by itself
    {
        KingMasterProcessor p; prep (p, SR, 512);
        run (p, mix, 1);
        setP (p, "loudness", 2.0f);
        auto o = run (p, mix, 1);
        auto o2 = run (p, mix, 1);
        check (std::abs (lufs (o2, SR) + 8.0f) <= 1.0f, "Medium -> High after learning: " + juce::String (lufs (o2, SR), 2) + " LUFS (-8 +-1)");
        setP (p, "loud_trim", -2.0f);
        auto o3 = run (p, mix, 1);
        check (std::abs (lufs (o3, SR) + 10.0f) <= 1.0f, "High with -2 LU trim: " + juce::String (lufs (o3, SR), 2) + " LUFS (-10 +-1)");
    }

    // ---- styles change the tone in the right direction (same loudness)
    {
        double tilt[3];
        for (int st = 0; st < 3; ++st)
        {
            KingMasterProcessor p; prep (p, SR, 512); setP (p, "style", (float) st);
            auto o = run (p, mix, 2);
            tilt[st] = bandDb (o, SR, 4000, 16000) - bandDb (o, SR, 30, 200);
            printf ("   %-8s  highs-minus-lows %.2f dB, %.2f LUFS\n", kmaster::styleNames()[st].toRawUTF8(), tilt[st], lufs (o, SR));
        }
        check (tilt[2] - tilt[1] > 1.0 && tilt[1] - tilt[0] > 1.0, "Open is brighter than Balanced, Balanced brighter than Warm");
    }

    // ---- smart EQ reacts to the mix: dark mix -> top lifted, bright mix -> top cut
    {
        auto filtered = [&] (bool dark)
        {
            juce::AudioBuffer<float> b (mix);
            tdsp::Biquad f; f.set (dark ? tdsp::Biquad::HighShelf : tdsp::Biquad::HighShelf, SR, 3000.0, 0.7, dark ? -8.0 : 8.0);
            for (int c = 0; c < 2; ++c) for (int i = 0; i < b.getNumSamples(); ++i) b.setSample (c, i, f.process (b.getSample (c, i), c));
            return b;
        };
        KingMasterProcessor a; prep (a, SR, 512); run (a, filtered (true), 1);
        KingMasterProcessor c; prep (c, SR, 512); run (c, filtered (false), 1);
        const auto ga = a.chain.smartGains(), gc = c.chain.smartGains();
        check (ga[6] + ga[7] > 2.0f, "Smart EQ lifts the top of a dark mix (4k/8k: +" + juce::String (ga[6], 1) + "/+" + juce::String (ga[7], 1) + " dB)");
        check (gc[6] + gc[7] < -2.0f, "Smart EQ tames the top of a harsh mix (4k/8k: " + juce::String (gc[6], 1) + "/" + juce::String (gc[7], 1) + " dB)");
    }

    // ---- mono bass, width
    {
        juce::AudioBuffer<float> s (2, (int) SR * 4);
        for (int i = 0; i < s.getNumSamples(); ++i)
        {
            const float t = (float) i / (float) SR;
            const float lo = 0.1f * std::sin (2.0f * juce::MathConstants<float>::pi * 50.0f * t), hi = 0.05f * std::sin (2.0f * juce::MathConstants<float>::pi * 2000.0f * t);
            s.setSample (0, i, lo + hi + 0.1f * std::sin (2.0f * juce::MathConstants<float>::pi * 100.0f * t)); s.setSample (1, i, -lo - hi + 0.1f * std::sin (2.0f * juce::MathConstants<float>::pi * 100.0f * t));
        }
        auto sideLowDb = [&] (const juce::AudioBuffer<float>& b)
        {
            juce::AudioBuffer<float> sd (2, b.getNumSamples());
            for (int i = 0; i < b.getNumSamples(); ++i) { const float v = 0.5f * (b.getSample (0, i) - b.getSample (1, i)); sd.setSample (0, i, v); sd.setSample (1, i, v); }
            return bandDb (sd, SR, 20, 80);
        };
        KingMasterProcessor a; prep (a, SR, 512); setP (a, "mono_bass", 0.0f); setP (a, "warmth", 0.0f); auto oa = run (a, s, 2);
        KingMasterProcessor b; prep (b, SR, 512); setP (b, "mono_bass", 1.0f); setP (b, "warmth", 0.0f); auto ob = run (b, s, 2);
        const double d = sideLowDb (oa) - sideLowDb (ob);
        check (d > 15.0, "MONO BASS removes 50 Hz side content by " + juce::String (d, 1) + " dB (> 15)");
        KingMasterProcessor c; prep (c, SR, 512); setP (c, "width", 0.0f); auto oc = run (c, s, 1);
        float diff = 0; for (int i = 2000; i < oc.getNumSamples(); ++i) diff = std::max (diff, std::abs (oc.getSample (0, i) - oc.getSample (1, i)));
        check (diff < 1e-4f, "WIDTH 0% gives a mono master");
    }

    // ---- latency and compare
    {
        KingMasterProcessor p; prep (p, SR, 512);
        check (p.getLatencySamples() == p.chain.getLatency() && p.getLatencySamples() > 0, "latency reported to the host: " + juce::String (p.getLatencySamples()) + " samples");
        run (p, mix, 1);
        setP (p, "compare", 1.0f);
        auto o = run (p, mix, 1);
        // compare output is the original, delayed by the latency, at matched loudness
        double xy = 0, xx = 0, yy = 0; const int lat = p.getLatencySamples();
        for (int i = lat + 48000; i < mix.getNumSamples() - 1000; ++i) { const double x = mix.getSample (0, i - lat), y = o.getSample (0, i); xy += x * y; xx += x * x; yy += y * y; }
        check (xy / std::sqrt (xx * yy) > 0.999, "COMPARE plays the original, time-aligned with the master");
        setP (p, "compare", 0.0f);
        auto m = run (p, mix, 1);
        const float inPk = tdsp::gainToDb (std::max (mix.getMagnitude (0, 0, mix.getNumSamples()), mix.getMagnitude (1, 0, mix.getNumSamples())));
        const float expect = std::min (lufs (m, SR), lufs (mix, SR) + (-1.0f - inPk));
        check (std::abs (lufs (o, SR) - expect) < 1.0f, "COMPARE loudness-matched up to the headroom (" + juce::String (lufs (o, SR), 1) + " LUFS, expected " + juce::String (expect, 1) + ", master " + juce::String (lufs (m, SR), 1) + ")");
        check (o.getMagnitude (0, o.getNumSamples()) < 1.0f, "COMPARE never clips (peak " + juce::String (tdsp::gainToDb (o.getMagnitude (0, o.getNumSamples())), 2) + " dBFS)");
        // impulse latency check on the limiter itself
        tdsp::Limiter lim; lim.prepare (SR); lim.setCeilingDb (-1.0f);
        std::vector<float> l (4000, 0.0f), r (4000, 0.0f); l[100] = 0.1f; r[100] = 0.1f;
        lim.process (l.data(), r.data(), 4000);
        int at = 0; for (int i = 0; i < 4000; ++i) if (std::abs (l[(size_t) i]) > 0.05f) at = i;
        check (at - 100 == lim.getLatency(), "limiter delay equals reported latency (" + juce::String (at - 100) + ")");
    }

    // ---- limiter holds the ceiling on brutal input
    {
        juce::AudioBuffer<float> hot (mix); hot.applyGain (tdsp::dbToGain (18.0f));
        tdsp::Limiter lim; lim.prepare (SR); lim.setCeilingDb (-1.0f);
        lim.process (hot.getWritePointer (0), hot.getWritePointer (1), hot.getNumSamples());
        const float tp = truePeakDb (hot);
        check (tp <= -0.95f, "limiter with +18 dB of overdrive: true peak " + juce::String (tp, 2) + " dBTP (ceiling -1)");
    }

    // ---- loud, drum-heavy material: sharp kicks, snares and 16th hats, already at about -13 LUFS (1.4.1: the limiter
    //      let such a mix through at up to +1 dBTP with the ceiling at -1)
    {
        juce::AudioBuffer<float> drums (2, (int) SR * 12); drums.clear();
        juce::Random rnd (1234); const int n16 = (int) (SR * 60.0 / 128.0 / 4.0);
        for (int k = 0; k * n16 < drums.getNumSamples(); ++k)
        {
            const int at = k * n16; const bool kick = k % 4 == 0, snare = k % 8 == 4; const float pan = k % 2 ? 0.8f : 1.0f;
            for (int i = 0; i < n16 * 2 && at + i < drums.getNumSamples(); ++i)
            {
                const float t = (float) i / (float) SR; float x = 0;
                if (kick) x += 0.75f * std::sin (6.2831853f * (46.0f * t + 3.0f * (1.0f - std::exp (-t * 38.0f)))) * std::exp (-t * 7.0f) + 0.35f * (rnd.nextFloat() * 2 - 1) * std::exp (-t * 900.0f);
                if (snare) x += 0.55f * (rnd.nextFloat() * 2 - 1) * std::exp (-t * 22.0f);
                float hat = 0.0f; for (float f : { 11500.0f, 14300.0f, 17600.0f, 19900.0f }) hat += std::sin (6.2831853f * f * t + f);
                x += 0.10f * hat * std::exp (-t * 60.0f) + 0.12f * std::sin (6.2831853f * 55.0f * ((float) (at + i) / (float) SR));
                drums.addSample (0, at + i, x); drums.addSample (1, at + i, x * pan);
            }
        }
        printf ("      drum mix: %.1f LUFS, peak %.1f dBFS\n", lufs (drums, SR), tdsp::gainToDb (drums.getMagnitude (0, drums.getNumSamples())));
        for (int loud = 0; loud < 3; ++loud)
        {
            KingMasterProcessor p; prep (p, SR, 512); setP (p, "loudness", (float) loud);
            auto o = run (p, drums, 2); const float tp = truePeakDb (o);
            check (tp <= -0.95f, "drum mix, " + kmaster::loudNames()[loud] + ": true peak " + juce::String (tp, 2) + " dBTP (ceiling -1.0)");
        }
        for (float gain : { 6.0f, 18.0f })
        {
            juce::AudioBuffer<float> hot (drums); hot.applyGain (tdsp::dbToGain (gain));
            tdsp::Limiter lim; lim.prepare (SR); lim.setCeilingDb (-1.0f);
            lim.process (hot.getWritePointer (0), hot.getWritePointer (1), hot.getNumSamples());
            const float tp = truePeakDb (hot);
            check (tp <= -0.95f, "drum mix +" + juce::String (gain, 0) + " dB into the limiter: true peak " + juce::String (tp, 2) + " dBTP (ceiling -1)");
        }
    }

    // ---- save / restore keeps the learned master
    {
        KingMasterProcessor a; prep (a, SR, 512); setP (a, "style", 2.0f); run (a, mix, 2);
        const float ga = a.chain.appliedGainDb.load();
        juce::MemoryBlock mb; a.getStateInformation (mb);
        KingMasterProcessor b; b.setStateInformation (mb.getData(), (int) mb.getSize()); prep (b, SR, 512);
        auto ob = run (b, mix, 1);
        auto oa = run (a, mix, 1);
        check (b.chain.phase.load() == kmaster::Chain::Done && std::abs (b.chain.appliedGainDb.load() - ga) < 0.05f, "restored session keeps the learned gain (" + juce::String (ga, 2) + " dB) without re-learning");
        check (std::abs (lufs (oa, SR) - lufs (ob, SR)) < 0.2f, "restored session sounds the same (" + juce::String (lufs (oa, SR), 2) + " vs " + juce::String (lufs (ob, SR), 2) + " LUFS)");
    }

    // ---- streaming check (1.5): the verdicts themselves
    {
        using namespace kmaster; using R = StreamReport;
        auto st = [] (float lufs, float tp, float secs = 30.0f) { StreamStats s; s.outLufs = lufs; s.outTp = tp; s.outSecs = secs; s.inLufs = -20.0f; s.inTp = -6.0f; s.inSecs = secs; return s; };
        auto lv = [&] (int pf, float lufs, float tp) { return checkStreaming (true, pf, st (lufs, tp)); };
        check (checkStreaming (false, 0, st (-6, 0)).level == R::Off, "check off: no warning");
        check (checkStreaming (true, 0, st (-8, -1, 2.0f)).level == R::Waiting, "under 5 s of music: waits");
        check (lv (P_AllStreaming, -14.0f, -1.0f).level == R::Ok, "-14 LUFS / -1 dBTP is ready for streaming");
        check (lv (P_AllStreaming, -12.4f, -1.0f).level == R::Warn && lv (P_AllStreaming, -8.0f, -1.0f).level == R::Bad, "-12.4 LUFS warns, -8 LUFS is a red warning");
        auto bad = lv (P_AllStreaming, -8.0f, -1.0f);
        check (std::abs (bad.turnDown - 6.0f) < 0.05f && bad.headline.contains ("TOO LOUD FOR STREAMING") && bad.headline.contains ("6.0 dB") && bad.canFix, "says how much it is turned down: " + bad.headline);
        bool apple = false, deezer = false; for (auto& c : bad.chips) { apple |= c.name == "Apple Music" && std::abs (c.turnDown - 8.0f) < 0.05f; deezer |= c.name == "Deezer" && std::abs (c.turnDown - 7.0f) < 0.05f; }
        check (bad.chips.size() == 6 && apple && deezer, "six services, each with its own turn-down (Apple -8.0, Deezer -7.0)");
        check (lv (P_AppleMusic, -14.0f, -1.0f).level == R::Warn && lv (P_AppleMusic, -16.0f, -1.0f).level == R::Ok, "Apple Music: -14 warns, -16 is ready");
        check (lv (P_AllStreaming, -14.0f, -0.4f).tpOver && lv (P_AllStreaming, -14.0f, 0.3f).level == R::Bad, "true peak over the limit warns, over 0 is red");
        check (lv (P_Amazon, -14.0f, -1.0f).tpOver && ! lv (P_Amazon, -14.0f, -2.0f).tpOver, "Amazon Music wants -2 dBTP");
        check (lv (P_Spotify, -10.0f, -1.0f).tpOver && ! lv (P_Spotify, -14.0f, -1.0f).tpOver, "Spotify wants -2 dBTP only when louder than -14");
        check (lv (P_AllStreaming, -20.0f, -3.0f).level == R::Note, "a quiet master is a note, not a warning");
        check (lv (P_Club, -8.0f, -0.3f).level == R::Ok && lv (P_Club, -4.5f, -0.3f).level == R::Warn && lv (P_Club, -14.0f, -1.0f).level == R::Note, "club: -8 is ready, -4.5 too loud, -14 quiet");
        check (lv (P_Podcast, -16.0f, -1.5f).level == R::Ok && lv (P_Podcast, -11.0f, -1.0f).level == R::Bad, "podcast: -16 ready, -11 too loud");
        { auto h = st (-14.0f, -1.0f); h.inLufs = -9.5f; auto r = checkStreaming (true, 0, h); check (r.mixTooHot && r.level == R::Warn && r.headline.contains ("YOUR MIX IS TOO LOUD"), "a loud mix is caught even when the master is fine: " + r.headline);
          h.inLufs = -6.0f; h.inClips = 12; r = checkStreaming (true, 0, h); check (r.mixClips && r.level == R::Bad && r.headline.contains ("CLIPS"), "a clipping mix: " + r.headline);
          StreamStats w; w.inLufs = -6.0f; w.inSecs = 20; w.learning = true; r = checkStreaming (true, 0, w); check (r.level == R::Bad, "a squashed mix warns during LEARN, before any master exists"); }
        for (int pf = 0; pf < P_Count; ++pf) { auto f = fixFor (pf); const float l = targetLufs (f.loudness) + f.trim; auto r = lv (pf, l, f.ceiling); check (r.level == R::Ok, "FIX IT lands " + platformNames()[pf] + " on a clean verdict (" + juce::String (l, 0) + " LUFS, " + juce::String (f.ceiling, 1) + " dBTP)"); }
    }
    // ---- streaming check on real audio
    {
        using R = kmaster::StreamReport;
        KingMasterProcessor p; prep (p, SR, 512); setP (p, "loudness", 2.0f);
        run (p, mix, 3); auto r = p.streamReport();
        check (r.level == R::Bad && r.tooLoud && r.turnDown > 4.5f && r.turnDown < 7.5f, "HIGH master: " + r.headline);
        p.fixForPlatform(); run (p, mix, 1);
        run (p, mix, 2); r = p.streamReport();
        check (r.level == R::Ok && ! r.canFix && std::abs (p.chain.streamStats().outLufs + 14.0f) <= 0.9f, "after FIX IT: " + r.headline);
        const auto s0 = p.chain.streamStats(); setP (p, "loud_trim", 1.0f); { juce::AudioBuffer<float> one (2, 512); one.clear(); juce::MidiBuffer m; p.processBlock (one, m); }
        check (s0.outSecs > 5.0f && p.chain.streamStats().outSecs < 0.1f, "changing the loudness starts the measurement again");

        KingMasterProcessor am; prep (am, SR, 512); for (int i = 0; i < am.getNumPrograms(); ++i) if (am.getProgramName (i) == "Apple Music -16") am.setCurrentProgram (i);
        check ((int) am.apvts.getRawParameterValue ("platform")->load() == kmaster::P_AppleMusic, "the Apple Music preset points the check at Apple Music");
        run (am, mix, 3); r = am.streamReport(); check (r.level == R::Ok && std::abs (am.chain.streamStats().outLufs + 16.0f) <= 0.9f, "Apple Music preset: " + r.headline);
        am.setCurrentProgram (0); check ((int) am.apvts.getRawParameterValue ("platform")->load() == kmaster::P_AppleMusic, "an ordinary preset leaves your platform alone");
        setP (am, "stream_check", 0.0f); am.setCurrentProgram (4); check (am.apvts.getRawParameterValue ("stream_check")->load() < 0.5f, "a preset never switches the check back on"); check (am.streamReport().level == R::Off, "switched off: silent");

        juce::AudioBuffer<float> hot (mix); hot.applyGain (tdsp::dbToGain (16.0f));
        for (int c = 0; c < 2; ++c) for (int i = 0; i < hot.getNumSamples(); ++i) hot.setSample (c, i, juce::jlimit (-1.0f, 1.0f, hot.getSample (c, i)));
        KingMasterProcessor h; prep (h, SR, 512); setP (h, "loudness", 0.0f); run (h, hot, 2); r = h.streamReport();
        check (r.mixTooHot && r.mixClips && r.level == R::Bad, "a squashed, clipped mix: " + r.headline + " | " + r.detail);
        KingMasterProcessor q; prep (q, SR, 512); run (q, mix, 2); r = q.streamReport();
        check (! r.mixTooHot && ! r.mixClips, "the normal test mix (-20 LUFS) is not called too loud");

        // display only: the sound is identical with the check on, off, or pointed anywhere
        KingMasterProcessor a, b; prep (a, SR, 512); prep (b, SR, 512); setP (b, "stream_check", 0.0f); setP (b, "platform", 8.0f);
        auto oa = run (a, mix, 1), ob = run (b, mix, 1); float diff = 0; for (int c = 0; c < 2; ++c) for (int i = 0; i < oa.getNumSamples(); ++i) diff = std::max (diff, std::abs (oa.getSample (c, i) - ob.getSample (c, i)));
        check (diff == 0.0f, "the check never changes the sound (max difference " + juce::String (diff, 9) + ")");

        // a session saved by 1.4 has neither parameter: the check is on, for all streaming, and everything else is as saved
        KingMasterProcessor o; setP (o, "style", 2.0f); setP (o, "ceiling", -0.5f); juce::MemoryBlock mb; o.getStateInformation (mb);
        if (auto xml = juce::AudioProcessor::getXmlFromBinary (mb.getData(), (int) mb.getSize()))
        {
            for (auto* id : { "stream_check", "platform" }) if (auto* e = xml->getChildByAttribute ("id", id)) xml->removeChildElement (e, true);
            juce::MemoryBlock m2; juce::AudioProcessor::copyXmlToBinary (*xml, m2);
            KingMasterProcessor n; setP (n, "platform", 3.0f); setP (n, "stream_check", 0.0f); n.setStateInformation (m2.getData(), (int) m2.getSize());
            check (n.apvts.getRawParameterValue ("stream_check")->load() > 0.5f && (int) n.apvts.getRawParameterValue ("platform")->load() == 0
                   && (int) n.apvts.getRawParameterValue ("style")->load() == 2 && std::abs (n.apvts.getRawParameterValue ("ceiling")->load() + 0.5f) < 0.01f, "a 1.4 session opens with the check on and its own settings");
        }
        const auto t0 = juce::Time::getMillisecondCounterHiRes(); KingMasterProcessor c; prep (c, SR, 512); run (c, mix, 2); const double ms = juce::Time::getMillisecondCounterHiRes() - t0, secs = 2.0 * mix.getNumSamples() / SR;
        check (ms / 1000.0 / secs < 0.10, "CPU " + juce::String (100.0 * ms / 1000.0 / secs, 1) + "% of real time");
    }
    // ---- the graph: where in the song it is too loud (1.5)
    {
        using namespace kmaster;
        std::vector<float> v (400, -14.0f), bars (400); for (int i = 0; i < 400; ++i) bars[(size_t) i] = (float) (i / 8 + 1);
        for (int i = 80; i < 140; ++i) v[(size_t) i] = -8.0f; for (int i = 200; i < 202; ++i) v[(size_t) i] = -8.0f;
        for (int i = 240; i < 260; ++i) v[(size_t) i] = -9.0f; for (int i = 264; i < 280; ++i) v[(size_t) i] = -9.5f; v[300] = kNoData;
        auto sp = loudSpans (v, -13.0f);
        check (sp.size() == 2 && std::abs (sp[0].start - 20.0f) < 0.01f && std::abs (sp[0].end - 35.0f) < 0.01f && std::abs (sp[1].start - 60.0f) < 0.01f && std::abs (sp[1].end - 70.0f) < 0.01f, "loud stretches found, blips ignored, short gaps bridged");
        check (describeSpans (sp, v, bars) == "0:20-0:35 (bars 11-18), 1:00-1:10 (bars 31-35)", "said in time and bars: " + describeSpans (sp, v, bars));
        std::vector<float> all (200, -6.0f); check (describeSpans (loudSpans (all, -13.0f), all, {}) == "throughout", "loud everywhere says throughout");
        check (loudSpans (std::vector<float> (100, -20.0f), -13.0f).empty(), "a quiet song has no red");

        struct Head : juce::AudioPlayHead
        {
            double secs = 0, bpm = 120; bool playing = true;
            juce::Optional<PositionInfo> getPosition() const override { PositionInfo q; q.setIsPlaying (playing); q.setTimeInSeconds (secs); q.setBpm (bpm); q.setPpqPosition (secs * bpm / 60.0); q.setTimeSignature (TimeSignature { 4, 4 }); return q; }
        };
        // 60 s: the mix, with a section at 20-35 s that is 13 dB louder (a squashed chorus)
        juce::AudioBuffer<float> song (2, (int) SR * 60); const float base = tdsp::dbToGain (-1.0f);
        for (int c = 0; c < 2; ++c) for (int i = 0; i < song.getNumSamples(); ++i)
        { const double t = i / SR; const float x = mix.getSample (c, i % mix.getNumSamples()) * base; song.setSample (c, i, t >= 20.0 && t < 35.0 ? juce::jlimit (-1.0f, 1.0f, x * tdsp::dbToGain (16.0f)) : x); }
        auto play = [&] (KingMasterProcessor& p, Head& h, double from, double to)
        {
            juce::MidiBuffer m;
            for (int at = (int) (from * SR); at < (int) (to * SR); at += 512)
            { const int k = juce::jmin (512, song.getNumSamples() - at); if (k <= 0) break; juce::AudioBuffer<float> b (2, k); b.copyFrom (0, 0, song, 0, at, k); b.copyFrom (1, 0, song, 1, at, k); h.secs = at / SR; p.processBlock (b, m); }
        };
        auto curve = [] (KingMasterProcessor& p, bool out) { std::vector<float> r; for (int i = 0; i <= p.chain.gMax.load(); ++i) r.push_back (out ? p.chain.gOut[(size_t) i].load() : p.chain.gIn[(size_t) i].load()); return r; };
        auto barv = [] (KingMasterProcessor& p) { std::vector<float> r; for (int i = 0; i <= p.chain.gMax.load(); ++i) r.push_back (p.chain.gBar[(size_t) i].load()); return r; };
        KingMasterProcessor p; Head h; prep (p, SR, 512); p.setPlayHead (&h); setP (p, "loudness", 1.0f);
        play (p, h, 0, 60);
        auto in = curve (p, false); auto li = loudSpans (in, kMixHot);
        check (li.size() == 1 && std::abs (li[0].start - 20.0f) < 1.6f && std::abs (li[0].end - 35.0f) < 1.6f, "the squashed chorus is found at " + describeSpans (li, in, barv (p)) + " (truth 0:20-0:35, bars 11-18)");
        bool clipIn = false, clipOut = false; for (int i = 0; i <= p.chain.gMax.load(); ++i) if (p.chain.gClip[(size_t) i].load()) { const float t = i * kGraphSlot; ((t >= 19.5f && t <= 35.5f) ? clipIn : clipOut) = true; }
        check (clipIn && ! clipOut, "clip marks sit on the clipped section only");
        play (p, h, 0, 60);                                  // LEARN is done now: the master line fills
        auto out = curve (p, true); int have = 0; for (auto x : out) if (x > kNoData + 1) ++have;
        auto lo = loudSpans (out, masterLimit (P_AllStreaming));
        bool covers = false; for (auto& x : lo) covers |= x.start < 24.0f && x.end > 31.0f;
        check (have > 180 && covers, "the master line shows the chorus over the streaming line at " + describeSpans (lo, out, barv (p)));
        const auto before = curve (p, false);
        play (p, h, 50, 54); play (p, h, 5, 9);              // jump about: nothing may smear into the wrong place
        auto after = curve (p, false); float worst = 0; for (size_t i = 0; i < juce::jmin (before.size(), after.size()); ++i) if (before[i] > kNoData + 1 && after[i] > kNoData + 1) worst = std::max (worst, std::abs (before[i] - after[i]));
        check (worst < 4.0f && loudSpans (after, kMixHot).size() == 1, "jumping the playhead does not smear the graph (largest change " + juce::String (worst, 1) + " LU)");
        h.playing = false; const int m0 = p.chain.gMax.load(); play (p, h, 55, 58); check (p.chain.gMax.load() == m0, "stopped transport records nothing");
        h.playing = true; setP (p, "loudness", 0.0f); play (p, h, 0, 0.1); int left = 0; for (auto x : curve (p, true)) if (x > kNoData + 1) ++left; check (left == 0, "changing the loudness clears the master line, and keeps the mix line (" + juce::String (loudSpans (curve (p, false), kMixHot).size()) + " span)");
        p.chain.clearGraph(); play (p, h, 0, 0.1); check (p.chain.gMax.load() < 2, "cleared");
        KingMasterProcessor q; prep (q, SR, 512); run (q, mix, 2); check (q.chain.gMax.load() > 250, "no host time (standalone): the graph runs on its own clock");
        // the picture with the graph in use
        KingMasterProcessor g; Head gh; prep (g, SR, 512); g.setPlayHead (&gh); setP (g, "loudness", 2.0f); play (g, gh, 0, 60); play (g, gh, 0, 60);
        std::unique_ptr<juce::AudioProcessorEditor> ed (g.createEditor()); auto* ke = static_cast<KingMasterEditor*> (ed.get()); for (int i = 0; i < 12; ++i) ke->timerCallback();
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f); juce::File f = outDir.getChildFile ("mm_graph.png"); f.deleteFile(); { juce::FileOutputStream fos (f); juce::PNGImageFormat().writeImageToStream (img, fos); }
        check (ke->graph.where.isNotEmpty() && ke->graph.whereMix.contains ("0:"), "the editor names the places: MASTER " + ke->graph.where + " | MIX " + ke->graph.whereMix);
    }
    {
        int n = (int) kmaster::presets().size(); check (n == 46, "46 presets (" + juce::String (n) + ")");
        check (juce::String (kmaster::presets()[0].name) == "Balanced Master" && juce::String (kmaster::presets()[38].name) == "Podcast Voice", "the 39 older presets keep their places");
    }

    // ---- every preset at 44.1 / 96 kHz: finite, under the ceiling
    {
        int bad = 0; juce::String why;
        for (int i = 0; i < (int) kmaster::presets().size(); ++i)
            for (double sr : { 44100.0, 96000.0 })
            {
                KingMasterProcessor p; p.setCurrentProgram (i); prep (p, sr, sr > 50000 ? 1024 : 256);
                auto o = run (p, mix, 1, sr > 50000 ? 1024 : 256);   // (file is 48k; resampling is irrelevant for safety checks)
                bool fin = true; for (int c = 0; c < 2; ++c) for (int k = 0; k < o.getNumSamples(); ++k) fin &= std::isfinite (o.getSample (c, k));
                const float ceil = p.apvts.getRawParameterValue ("ceiling")->load();
                if (! fin || o.getMagnitude (0, o.getNumSamples()) > tdsp::dbToGain (ceil) * 1.001f) { ++bad; why << p.getProgramName (i) << "@" << sr << " "; }
            }
        check (bad == 0, "all " + juce::String ((int) kmaster::presets().size()) + " presets at 44.1 + 96 kHz: finite and under their ceiling " + why);
    }

    // ---- editor, in each state of the streaming check
    {
        auto shot = [&] (KingMasterProcessor& p, const char* name)
        {
            std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
            auto* ke = static_cast<KingMasterEditor*> (ed.get()); for (int i = 0; i < 10; ++i) ke->timerCallback();
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            juce::File f = outDir.getChildFile (name); f.deleteFile();
            juce::FileOutputStream fos (f); juce::PNGImageFormat().writeImageToStream (img, fos);
            return img.getWidth() == 1080 && img.getHeight() == 822;
        };
        KingMasterProcessor ok; prep (ok, SR, 512); for (int i = 0; i < ok.getNumPrograms(); ++i) if (ok.getProgramName (i) == "Streaming Safe") ok.setCurrentProgram (i); run (ok, mix, 3);
        check (shot (ok, "mistermaster_ui.png"), "editor renders: " + ok.streamReport().headline);
        KingMasterProcessor loud; prep (loud, SR, 512); setP (loud, "loudness", 2.0f); run (loud, mix, 3); check (shot (loud, "mm_loud.png"), "editor renders: " + loud.streamReport().headline);
        juce::AudioBuffer<float> hot (mix); hot.applyGain (tdsp::dbToGain (16.0f)); for (int c = 0; c < 2; ++c) for (int i = 0; i < hot.getNumSamples(); ++i) hot.setSample (c, i, juce::jlimit (-1.0f, 1.0f, hot.getSample (c, i)));
        KingMasterProcessor h; prep (h, SR, 512); setP (h, "loudness", 0.0f); run (h, hot, 3); check (shot (h, "mm_hotmix.png"), "editor renders: " + h.streamReport().headline);
        KingMasterProcessor sp; prep (sp, SR, 512); setP (sp, "platform", 2.0f); run (sp, mix, 3); check (shot (sp, "mm_apple.png"), "editor renders: " + sp.streamReport().headline);
        KingMasterProcessor idle; prep (idle, SR, 512); check (shot (idle, "mm_idle.png"), "editor renders: " + idle.streamReport().headline);
        KingMasterProcessor off; prep (off, SR, 512); setP (off, "stream_check", 0.0f); check (shot (off, "mm_off.png"), "editor renders with the check off");
    }

    printf ("\n%d passed, %d failed\n", passes, fails);
    printf (fails == 0 ? "ALL TESTS PASSED\n" : "SOME TESTS FAILED\n");
    return fails == 0 ? 0 : 1;
}
