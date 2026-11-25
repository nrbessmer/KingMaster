/*
  KMDemo: renders the Mister Master 1.5 demo video with the plug-in itself. The real processor masters the mix while the
  real editor is snapshotted 30 times a second with a caption band under it. Frames go to ffmpeg as raw BGRA, the sound
  (exactly what the plug-in puts out) to a WAV.
    KMDemo mix_unmastered.wav outDir     writes outDir/demo_v.mp4 (no sound) and outDir/demo.wav
*/
#include <juce_audio_utils/juce_audio_utils.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <cstdio>

static constexpr int W = 1080, EH = 822, CAP = 98, H = EH + CAP, FPS = 30;
static constexpr double SR = 48000.0;
static constexpr int SPF = (int) (SR / FPS), BS = SPF / 4;

struct Head : juce::AudioPlayHead
{
    double secs = 0, bpm = 124; bool playing = true;
    juce::Optional<PositionInfo> getPosition() const override
    { PositionInfo q; q.setIsPlaying (playing); q.setTimeInSeconds (secs); q.setBpm (bpm); q.setPpqPosition (secs * bpm / 60.0); q.setTimeSignature (TimeSignature { 4, 4 }); return q; }
};
static void setP (KingMasterProcessor& p, const char* id, float v) { auto* q = p.apvts.getParameter (id); q->setValueNotifyingHost (q->convertTo0to1 (v)); }
static void preset (KingMasterProcessor& p, const char* name) { for (int i = 0; i < p.getNumPrograms(); ++i) if (p.getProgramName (i) == name) p.setCurrentProgram (i); }

static void caption (juce::Graphics& g, const juce::String& big, const juce::String& small, float t)
{
    g.setColour (juce::Colour (0xff0b0b14)); g.fillRect (0, EH, W, CAP);
    g.setColour (juce::Colour (0xffffc23d).withAlpha (0.6f)); g.fillRect (0, EH, W, 2);
    const float a = juce::jlimit (0.0f, 1.0f, t / 0.4f);
    g.setColour (juce::Colour (0xffffc23d).withAlpha (a)); g.setFont (juce::Font (juce::FontOptions (31.0f, juce::Font::bold)));
    g.drawFittedText (big, 20, EH + 12, W - 40, 40, juce::Justification::centred, 1);
    g.setColour (juce::Colour (0xffff4fa3).withAlpha (a)); g.setFont (juce::Font (juce::FontOptions (18.5f)));
    g.drawFittedText (small, 20, EH + 56, W - 40, 28, juce::Justification::centred, 1);
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc < 3) { printf ("usage: KMDemo mix.wav outDir\n"); return 2; }
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (juce::File (argv[1])));
    juce::AudioBuffer<float> mix (2, (int) rd->lengthInSamples); rd->read (&mix, 0, mix.getNumSamples(), 0, true, true);
    // the same mix with a squashed, clipped chorus (bars 8-16): what a limiter left on the mix bus does
    juce::AudioBuffer<float> hot (mix); const double bar = 4 * 60.0 / 124.0;
    for (int c = 0; c < 2; ++c) for (int i = 0; i < hot.getNumSamples(); ++i)
    { const double t = i / SR; if (t >= 7 * bar && t < 15 * bar) hot.setSample (c, i, juce::jlimit (-1.0f, 1.0f, hot.getSample (c, i) * juce::Decibels::decibelsToGain (15.0f))); }
    const juce::File out (argv[2]); out.createDirectory();

    const juce::String cmd = "PATH=/opt/homebrew/bin:/usr/local/bin:$PATH ffmpeg -hide_banner -loglevel error -y -f rawvideo -pix_fmt bgra -s " + juce::String (W) + "x" + juce::String (H)
                             + " -r " + juce::String (FPS) + " -i - -c:v libx264 -preset slow -crf 20 -pix_fmt yuv420p \"" + out.getChildFile ("demo_v.mp4").getFullPathName() + "\"";
    FILE* ff = popen (cmd.toRawUTF8(), "w"); if (! ff) { printf ("cannot start ffmpeg\n"); return 1; }
    juce::AudioBuffer<float> all (2, (int) (SR * 160)); all.clear(); int w = 0;
    std::vector<uint8_t> row ((size_t) W * H * 4); juce::AudioBuffer<float> buf (2, BS); juce::MidiBuffer midi;

    auto feed = [&] (KingMasterProcessor& p, Head& h, const juce::AudioBuffer<float>& src, int at)     // one block; the song loops
    {
        for (int c = 0; c < 2; ++c) for (int i = 0; i < BS; ++i) buf.setSample (c, i, src.getSample (c, (at + i) % src.getNumSamples()));
        h.secs = (double) (at % src.getNumSamples()) / SR; p.processBlock (buf, midi);
    };
    auto prime = [&] (KingMasterProcessor& p, Head& h, const juce::AudioBuffer<float>& src, double secs) { for (int at = 0; at < (int) (secs * SR); at += BS) feed (p, h, src, at); };
    auto shot = [&] (KingMasterEditor& ed, const juce::String& big, const juce::String& small, float t, juce::Component* hl)
    {
        juce::Image frame (juce::Image::ARGB, W, H, true, juce::SoftwareImageType());
        {
            juce::Graphics g (frame); g.fillAll (juce::Colour (0xff0b0b14));
            g.drawImageAt (ed.createComponentSnapshot (ed.getLocalBounds(), true, 1.0f), 0, 0);
            if (hl && hl->isVisible())
            {
                auto r = ed.getLocalArea (hl->getParentComponent(), hl->getBounds()).toFloat().expanded (4.0f); const float pl = 0.55f + 0.45f * std::sin (t * 6.0f);
                g.setColour (juce::Colour (0xffffc23d).withAlpha (pl)); g.drawRoundedRectangle (r, 9.0f, 3.0f);
            }
            caption (g, big, small, t);
        }
        juce::Image::BitmapData bd (frame, juce::Image::BitmapData::readOnly);
        for (int y = 0; y < H; ++y) memcpy (row.data() + (size_t) y * W * 4, bd.getLinePointer (y), (size_t) W * 4);
        fwrite (row.data(), 1, row.size(), ff);
    };
    auto scene = [&] (KingMasterProcessor& p, KingMasterEditor& ed, Head& h, const juce::AudioBuffer<float>& src, int& at, double secs, const juce::String& big, const juce::String& small,
                      juce::Component* hl = nullptr, std::function<void()> action = {}, double actAt = 1e9)
    {
        const int start = w; bool done = false;
        for (int f = 0; f < (int) (secs * FPS); ++f)
        {
            const float t = (float) f / FPS; if (! done && t >= actAt && action) { action(); done = true; }
            for (int b = 0; b < SPF / BS; ++b) { feed (p, h, src, at); for (int c = 0; c < 2; ++c) all.copyFrom (c, w, buf, c, 0, BS); w += BS; at += BS; }
            ed.timerCallback(); shot (ed, big, small, t, hl);
        }
        juce::ignoreUnused (start);
    };
    auto fadeIn = [&] (int s) { for (int c = 0; c < 2; ++c) all.applyGainRamp (c, s, 1200, 0.0f, 1.0f); };
    auto fadeOut = [&] { for (int c = 0; c < 2; ++c) all.applyGainRamp (c, w - 2400, 2400, 1.0f, 0.0f); };

    {   // 1-3: a loud master, FIX IT, Apple Music
        KingMasterProcessor p; p.setPlayConfigDetails (2, 2, SR, BS); p.prepareToPlay (SR, BS); Head h; p.setPlayHead (&h); setP (p, "loudness", 2.0f);
        prime (p, h, mix, 60.0); p.chain.resetMeters();
        std::unique_ptr<juce::AudioProcessorEditor> e (p.createEditor()); auto& ed = *static_cast<KingMasterEditor*> (e.get());
        int at = 0; const int s0 = w;
        scene (p, ed, h, mix, at, 17.0, "Mister Master 1.5  -  is it too loud for streaming?", "A HIGH master plays: the check warns by itself and the graph draws the loudness along the song", &ed.stream);
        scene (p, ed, h, mix, at, 10.0, "It says how much each service would turn it down", "Spotify, Apple Music, YouTube, TIDAL, Amazon Music, Deezer  -  and shows where, in time and bars", &ed.graph);
        scene (p, ed, h, mix, at, 27.0, "FIX IT", "Loudness, trim and ceiling are set for you, then Mister Master listens and keeps the master under the line", &ed.stream.fix, [&p] { p.fixForPlatform(); }, 2.0);
        scene (p, ed, h, mix, at, 9.0, "Choose where the song is going", "Every service has its own line: Apple Music plays at -16", &ed.stream, [&p] { setP (p, "platform", 2.0f); }, 1.5);
        scene (p, ed, h, mix, at, 22.0, "7 new streaming presets", "\"Apple Music -16\" sets the level and the check in one click", &ed.presetBar, [&p] { preset (p, "Apple Music -16"); }, 1.5);
        fadeIn (s0); fadeOut();
    }
    {   // 4: a mix that is too loud before mastering
        KingMasterProcessor p; p.setPlayConfigDetails (2, 2, SR, BS); p.prepareToPlay (SR, BS); Head h; p.setPlayHead (&h); preset (p, "Streaming Safe");
        prime (p, h, hot, 60.0); p.chain.resetMeters();
        std::unique_ptr<juce::AudioProcessorEditor> e (p.createEditor()); auto& ed = *static_cast<KingMasterEditor*> (e.get());
        int at = (int) (4 * bar * SR) / BS * BS; const int s0 = w;
        scene (p, ed, h, hot, at, 26.0, "Is your MIX too loud before mastering?", "The amber line is the limit for a mix: the squashed chorus is caught, with its bars, and pink marks show the clipping", &ed.graph);
        scene (p, ed, h, hot, at, 8.0, "Mister Master", "Mac Audio Unit + VST3 + standalone  -  tullyedmvibe.com");
        fadeIn (s0); fadeOut();
    }
    pclose (ff);
    auto wf = out.getChildFile ("demo.wav"); wf.deleteFile(); juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> wr (wav.createWriterFor (new juce::FileOutputStream (wf), SR, 2, 24, {}, 0)); if (wr) wr->writeFromAudioSampleBuffer (all, 0, w);
    printf ("done: %.1f s\n", w / SR);
    return 0;
}
