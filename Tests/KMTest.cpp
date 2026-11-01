// KMTest: automated checks for King Master.
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
        check (tp <= -0.8f, kmaster::loudNames()[loud] + ": true peak " + juce::String (tp, 2) + " dBTP (ceiling -1.0)");
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
        check (tp <= -0.8f, "limiter with +18 dB of overdrive: true peak " + juce::String (tp, 2) + " dBTP (ceiling -1)");
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

    // ---- editor
    {
        KingMasterProcessor p; prep (p, SR, 512); run (p, mix, 1);
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        static_cast<KingMasterEditor*> (ed.get())->timerCallback();
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::File f = outDir.getChildFile ("kingmaster_ui.png"); f.deleteFile();
        { juce::FileOutputStream fos (f); juce::PNGImageFormat().writeImageToStream (img, fos); }
        check (img.getWidth() == 1080, "editor renders");
    }

    printf ("\n%d passed, %d failed\n", passes, fails);
    printf (fails == 0 ? "ALL TESTS PASSED\n" : "SOME TESTS FAILED\n");
    return fails == 0 ? 0 : 1;
}
