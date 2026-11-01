/*
  ==============================================================================
    Tully EDM Vibe - SynthParams.h
    (c)(p) 2026 Tully EDM Vibe

    Parameter layout for the shared synth engine, reading parameters into the
    engine's Params/FxParams, and text presets ("cutoff=1200 osc1_wave=FM ...").
  ==============================================================================
*/
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "TullySynth.h"

namespace tsyn
{
    // note-value choices shared by LFO sync, delay time and the arpeggiator
    struct NoteValue { const char* name; double beats; };
    inline const std::vector<NoteValue>& noteValues()
    {
        static const std::vector<NoteValue> v = {
            { "1/32", 0.125 }, { "1/16T", 1.0 / 6.0 }, { "1/16", 0.25 }, { "1/16D", 0.375 },
            { "1/8T", 1.0 / 3.0 }, { "1/8", 0.5 }, { "1/8D", 0.75 }, { "1/4T", 2.0 / 3.0 },
            { "1/4", 1.0 }, { "1/4D", 1.5 }, { "1/2", 2.0 }, { "1/1", 4.0 }, { "2/1", 8.0 }, { "4/1", 16.0 } };
        return v;
    }
    inline juce::StringArray noteValueNames (bool withOff, int maxIndex = 99)
    {
        juce::StringArray s; if (withOff) s.add ("Free");
        for (int i = 0; i < (int) noteValues().size() && i <= maxIndex; ++i) s.add (noteValues()[(size_t) i].name);
        return s;
    }

    inline const std::vector<float>& fmRatios() { static const std::vector<float> r { 0.5f, 1.0f, 1.5f, 2.0f, 3.0f, 4.0f, 5.0f, 7.0f }; return r; }

    //==========================================================================
    inline void addSynthParameters (juce::AudioProcessorValueTreeState::ParameterLayout& l)
    {
        using namespace juce;
        auto pct  = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + "%"; });
        auto bip  = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return (v > 0 ? "+" : "") + String (roundToInt (v * 100.0f)) + "%"; });
        auto hz   = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return v >= 1000.0f ? String (v / 1000.0f, 1) + " kHz" : String (v < 10 ? v : (float) roundToInt (v), v < 10 ? 2 : 0) + " Hz"; });
        auto sec  = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return v < 1.0f ? String (roundToInt (v * 1000.0f)) + " ms" : String (v, 2) + " s"; });
        auto st   = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return (v > 0 ? "+" : "") + String (roundToInt (v)) + " st"; });
        auto ct   = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return (v > 0 ? "+" : "") + String (roundToInt (v)) + " ct"; });
        auto db   = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " dB"; });
        auto skew = [] (float lo, float hi, float centre) { NormalisableRange<float> r (lo, hi); r.setSkewForCentre (centre); return r; };
        auto F = [&l] (const char* id, const char* name, NormalisableRange<float> r, float def, AudioParameterFloatAttributes a)
                 { l.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def, a)); };
        auto C = [&l] (const char* id, const char* name, const StringArray& c, int def)
                 { l.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, c, def)); };
        auto I = [&l] (const char* id, const char* name, int lo, int hi, int def)
                 { l.add (std::make_unique<AudioParameterInt> (ParameterID { id, 1 }, name, lo, hi, def)); };

        for (int o = 1; o <= 2; ++o)
        {
            const String p = "osc" + String (o) + "_", n = "Osc " + String (o) + " ";
            C ((p + "wave").toRawUTF8(), (n + "Wave").toRawUTF8(), waveNames(), o == 1 ? SuperSaw : Saw);
            I ((p + "oct").toRawUTF8(), (n + "Octave").toRawUTF8(), -2, 2, 0);
            F ((p + "semi").toRawUTF8(), (n + "Semitone").toRawUTF8(), NormalisableRange<float> (-12.0f, 12.0f, 1.0f), 0.0f, st);
            F ((p + "fine").toRawUTF8(), (n + "Fine").toRawUTF8(), NormalisableRange<float> (-100.0f, 100.0f), o == 1 ? 0.0f : 7.0f, ct);
            F ((p + "level").toRawUTF8(), (n + "Level").toRawUTF8(), NormalisableRange<float> (0.0f, 1.0f), o == 1 ? 0.8f : 0.0f, pct);
            F ((p + "shape").toRawUTF8(), (n + "Shape").toRawUTF8(), NormalisableRange<float> (0.0f, 1.0f), 0.5f, pct);
            I ((p + "unison").toRawUTF8(), (n + "Unison").toRawUTF8(), 1, 7, 1);
            F ((p + "detune").toRawUTF8(), (n + "Detune").toRawUTF8(), NormalisableRange<float> (0.0f, 1.0f), 0.3f, pct);
        }
        StringArray ratios; for (auto r : fmRatios()) ratios.add (String (r, r == std::floor (r) ? 0 : 1) + ":1");
        C ("fm_ratio", "FM Ratio", ratios, 3);
        F ("sub_level", "Sub Level", NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct);
        C ("sub_wave", "Sub Wave", { "Sine", "Square" }, 0);
        F ("noise", "Noise", NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct);

        C ("flt_type", "Filter Type", { "LP 24", "LP 12", "Band", "High" }, 0);
        F ("cutoff", "Cutoff", skew (30.0f, 20000.0f, 1000.0f), 6000.0f, hz);
        F ("reso", "Resonance", NormalisableRange<float> (0.0f, 1.0f), 0.2f, pct);
        F ("flt_env", "Filter Env", NormalisableRange<float> (-1.0f, 1.0f), 0.3f, bip);
        F ("flt_key", "Key Track", NormalisableRange<float> (0.0f, 1.0f), 0.5f, pct);
        F ("flt_drive", "Filter Drive", NormalisableRange<float> (0.0f, 1.0f), 0.1f, pct);

        F ("amp_a", "Amp Attack",  skew (0.0005f, 5.0f, 0.15f), 0.003f, sec);
        F ("amp_d", "Amp Decay",   skew (0.005f, 8.0f, 0.5f), 0.3f, sec);
        F ("amp_s", "Amp Sustain", NormalisableRange<float> (0.0f, 1.0f), 0.8f, pct);
        F ("amp_r", "Amp Release", skew (0.005f, 8.0f, 0.5f), 0.25f, sec);
        F ("flt_a", "Filter Attack",  skew (0.0005f, 5.0f, 0.15f), 0.002f, sec);
        F ("flt_d", "Filter Decay",   skew (0.005f, 8.0f, 0.5f), 0.35f, sec);
        F ("flt_s", "Filter Sustain", NormalisableRange<float> (0.0f, 1.0f), 0.3f, pct);
        F ("flt_r", "Filter Release", skew (0.005f, 8.0f, 0.5f), 0.3f, sec);

        C ("lfo_shape", "LFO Shape", { "Sine", "Triangle", "Saw", "Square", "S&H" }, 0);
        F ("lfo_rate", "LFO Rate", skew (0.05f, 20.0f, 3.0f), 5.0f, hz);
        C ("lfo_sync", "LFO Sync", noteValueNames (true), 0);
        F ("lfo_pitch", "Vibrato", NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct);
        F ("lfo_cutoff", "LFO > Filter", NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct);
        F ("lfo_amp", "LFO > Volume", NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct);
        F ("lfo_shapemod", "LFO > Shape", NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct);
        F ("lfo_delay", "LFO Delay", skew (0.0f, 3.0f, 0.5f), 0.0f, sec);

        C ("voice_mode", "Voice Mode", { "Poly", "Mono", "Legato" }, 0);
        F ("glide", "Glide", skew (0.0f, 2.0f, 0.15f), 0.0f, sec);
        I ("bend_range", "Bend Range", 1, 24, 2);
        F ("vel_sens", "Velocity", NormalisableRange<float> (0.0f, 1.0f), 0.5f, pct);
        F ("spread", "Unison Spread", NormalisableRange<float> (0.0f, 1.0f), 0.6f, pct);

        C ("drive_type", "Drive Type", { "Off", "Soft", "Hard", "Fold", "Crush" }, 0);
        F ("drive", "Drive", NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct);
        F ("chorus", "Chorus", NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct);
        F ("chorus_rate", "Chorus Rate", skew (0.05f, 5.0f, 0.6f), 0.6f, hz);
        F ("delay", "Delay", NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct);
        C ("delay_time", "Delay Time", noteValueNames (false, 11), 6);
        F ("delay_fb", "Delay Feedback", NormalisableRange<float> (0.0f, 0.9f), 0.35f, pct);
        F ("reverb", "Reverb", NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct);
        F ("reverb_size", "Reverb Size", NormalisableRange<float> (0.0f, 1.0f), 0.6f, pct);
        F ("width", "Width", NormalisableRange<float> (0.0f, 2.0f), 1.0f, pct);
        F ("volume", "Volume", NormalisableRange<float> (-24.0f, 6.0f), -3.0f, db);
    }

    //==========================================================================
    // Cached raw-parameter pointers -> engine settings, once per block.
    class ParamReader
    {
    public:
        void attach (juce::AudioProcessorValueTreeState& s)
        {
            auto g = [&s] (const juce::String& id) { auto* p = s.getRawParameterValue (id); jassert (p != nullptr); return p; };
            for (int o = 0; o < 2; ++o)
            {
                const juce::String p = "osc" + juce::String (o + 1) + "_";
                wave[o] = g (p + "wave"); oct[o] = g (p + "oct"); semi[o] = g (p + "semi"); fine[o] = g (p + "fine");
                level[o] = g (p + "level"); shape[o] = g (p + "shape"); uni[o] = g (p + "unison"); det[o] = g (p + "detune");
            }
            fmRatio = g ("fm_ratio"); sub = g ("sub_level"); subWave = g ("sub_wave"); noise = g ("noise");
            fType = g ("flt_type"); cut = g ("cutoff"); reso = g ("reso"); fEnv = g ("flt_env"); fKey = g ("flt_key"); fDrive = g ("flt_drive");
            aA = g ("amp_a"); aD = g ("amp_d"); aS = g ("amp_s"); aR = g ("amp_r");
            fA = g ("flt_a"); fD = g ("flt_d"); fS = g ("flt_s"); fR = g ("flt_r");
            lShape = g ("lfo_shape"); lRate = g ("lfo_rate"); lSync = g ("lfo_sync"); lPitch = g ("lfo_pitch"); lCut = g ("lfo_cutoff");
            lAmp = g ("lfo_amp"); lPw = g ("lfo_shapemod"); lDelay = g ("lfo_delay");
            mode = g ("voice_mode"); glide = g ("glide"); bendR = g ("bend_range"); velS = g ("vel_sens"); spread = g ("spread");
            dType = g ("drive_type"); drive = g ("drive"); chorus = g ("chorus"); chRate = g ("chorus_rate");
            delay = g ("delay"); dTime = g ("delay_time"); dFb = g ("delay_fb"); rev = g ("reverb"); revSize = g ("reverb_size");
            width = g ("width"); vol = g ("volume");
        }

        // bpm is needed for synced LFO and delay; returns LFO beats-per-cycle (0 = free)
        double read (Params& p, FxParams& f, double bpm) const
        {
            for (int o = 0; o < 2; ++o)
            {
                p.wave[o] = (int) wave[o]->load(); p.octave[o] = (int) oct[o]->load(); p.semi[o] = semi[o]->load(); p.fine[o] = fine[o]->load();
                p.level[o] = level[o]->load(); p.shape[o] = shape[o]->load(); p.unison[o] = (int) uni[o]->load(); p.detune[o] = det[o]->load();
            }
            p.fmRatio = fmRatios()[(size_t) juce::jlimit (0, (int) fmRatios().size() - 1, (int) fmRatio->load())];
            p.subLevel = sub->load(); p.subWave = (int) subWave->load(); p.noise = noise->load();
            p.fltType = (int) fType->load(); p.cutoff = cut->load(); p.reso = reso->load(); p.fltEnv = fEnv->load(); p.fltKey = fKey->load(); p.fltDrive = fDrive->load();
            p.aA = aA->load(); p.aD = aD->load(); p.aS = aS->load(); p.aR = aR->load();
            p.fA = fA->load(); p.fD = fD->load(); p.fS = fS->load(); p.fR = fR->load();
            p.lfoShape = (int) lShape->load(); p.lfoPitch = lPitch->load(); p.lfoCut = lCut->load(); p.lfoAmp = lAmp->load(); p.lfoPw = lPw->load(); p.lfoDelay = lDelay->load();
            const int sync = (int) lSync->load();
            const double safeBpm = juce::jlimit (30.0, 300.0, bpm);
            double beatsPerCycle = 0.0;
            if (sync > 0) { beatsPerCycle = noteValues()[(size_t) juce::jlimit (0, (int) noteValues().size() - 1, sync - 1)].beats; p.lfoHz = (float) (safeBpm / 60.0 / beatsPerCycle); }
            else p.lfoHz = lRate->load();
            p.mode = (int) mode->load(); p.glide = glide->load(); p.bendRange = bendR->load(); p.velSens = velS->load(); p.spread = spread->load();
            p.volume = juce::Decibels::decibelsToGain (vol->load());

            f.driveType = (int) dType->load(); f.drive = drive->load(); f.chorusMix = chorus->load(); f.chorusRate = chRate->load();
            f.delayMix = delay->load(); f.delayFb = dFb->load(); f.reverbMix = rev->load(); f.reverbSize = revSize->load(); f.width = width->load();
            const double beats = noteValues()[(size_t) juce::jlimit (0, 11, (int) dTime->load())].beats;
            f.delaySec = (float) juce::jmin (2.0, beats * 60.0 / safeBpm);
            return beatsPerCycle;
        }

    private:
        std::atomic<float>* wave[2] {}; std::atomic<float>* oct[2] {}; std::atomic<float>* semi[2] {}; std::atomic<float>* fine[2] {};
        std::atomic<float>* level[2] {}; std::atomic<float>* shape[2] {}; std::atomic<float>* uni[2] {}; std::atomic<float>* det[2] {};
        std::atomic<float>* fmRatio {}; std::atomic<float>* sub {}; std::atomic<float>* subWave {}; std::atomic<float>* noise {};
        std::atomic<float>* fType {}; std::atomic<float>* cut {}; std::atomic<float>* reso {}; std::atomic<float>* fEnv {}; std::atomic<float>* fKey {}; std::atomic<float>* fDrive {};
        std::atomic<float>* aA {}; std::atomic<float>* aD {}; std::atomic<float>* aS {}; std::atomic<float>* aR {};
        std::atomic<float>* fA {}; std::atomic<float>* fD {}; std::atomic<float>* fS {}; std::atomic<float>* fR {};
        std::atomic<float>* lShape {}; std::atomic<float>* lRate {}; std::atomic<float>* lSync {}; std::atomic<float>* lPitch {}; std::atomic<float>* lCut {};
        std::atomic<float>* lAmp {}; std::atomic<float>* lPw {}; std::atomic<float>* lDelay {};
        std::atomic<float>* mode {}; std::atomic<float>* glide {}; std::atomic<float>* bendR {}; std::atomic<float>* velS {}; std::atomic<float>* spread {};
        std::atomic<float>* dType {}; std::atomic<float>* drive {}; std::atomic<float>* chorus {}; std::atomic<float>* chRate {};
        std::atomic<float>* delay {}; std::atomic<float>* dTime {}; std::atomic<float>* dFb {}; std::atomic<float>* rev {}; std::atomic<float>* revSize {};
        std::atomic<float>* width {}; std::atomic<float>* vol {};
    };

    //==========================================================================
    // Text presets. Unlisted parameters return to their defaults, so every preset is complete.
    struct Preset { const char* name; const char* category; const char* tip; const char* settings; };

    // Applies `settings`; keys that are not parameters are returned for the caller (e.g. arp steps).
    inline juce::StringPairArray applyPreset (juce::AudioProcessorValueTreeState& s, const Preset& pr, const juce::StringArray& keepIds = {})
    {
        juce::StringPairArray extra;
        juce::StringPairArray kv;
        for (auto& tok : juce::StringArray::fromTokens (pr.settings, " ", ""))
        {
            if (tok.isEmpty() || ! tok.containsChar ('=')) continue;
            kv.set (tok.upToFirstOccurrenceOf ("=", false, false).trim(), tok.fromFirstOccurrenceOf ("=", false, false).trim());
        }
        for (auto* p : s.processor.getParameters())
        {
            auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p);
            if (rp == nullptr || keepIds.contains (rp->getParameterID())) continue;
            const auto id = rp->getParameterID();
            float norm = rp->getDefaultValue();
            if (kv.containsKey (id))
            {
                const auto v = kv[id];
                if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (rp))
                {
                    int idx = c->choices.indexOf (v.replaceCharacter ('_', ' '), true);
                    if (idx < 0) idx = v.getIntValue();
                    norm = rp->convertTo0to1 ((float) juce::jlimit (0, c->choices.size() - 1, idx));
                }
                else if (auto* b = dynamic_cast<juce::AudioParameterBool*> (rp))
                    norm = (v == "1" || v.equalsIgnoreCase ("on") || v.equalsIgnoreCase ("true")) ? 1.0f : 0.0f;
                else
                    norm = rp->convertTo0to1 (rp->getNormalisableRange().snapToLegalValue (v.getFloatValue()));
            }
            rp->beginChangeGesture(); rp->setValueNotifyingHost (norm); rp->endChangeGesture();
        }
        for (auto& k : kv.getAllKeys())
            if (s.getParameter (k) == nullptr) extra.set (k, kv[k]);
        return extra;
    }
}
