/*
  ==============================================================================
    King Master - PluginEditor.cpp
    (c)(p) 2026 Tully EDM Vibe
  ==============================================================================
*/
#include "PluginEditor.h"
#include "BinaryData.h"

namespace Col = tui::Col;
static const juce::Colour gold (0xffffc23d);

//==============================================================================
void LoudnessPanel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xcc0d1014)); g.fillRoundedRectangle (r, 6.0f);
    g.setColour (Col::line); g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
    auto fmt = [] (float v) { return v <= -69.9f ? juce::String ("--.-") : juce::String (v, 1); };
    const float tgt = getTarget ? getTarget() : -11.0f;
    const float I = chain.outLufsI.load(), S = chain.outLufsS.load(), M = chain.outLufsM.load(), tp = chain.tpMax.load();
    auto b = getLocalBounds().reduced (12, 8);
    auto top = b.removeFromTop (22);
    g.setColour (Col::dim); g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText ("INTEGRATED", top, juce::Justification::left);
    g.drawText ("TARGET " + juce::String (tgt, 1) + " LUFS", top, juce::Justification::right);
    auto big = b.removeFromTop (48);
    const bool onTarget = I > -70.0f && std::abs (I - tgt) <= 1.0f;
    g.setColour (I <= -70.0f ? Col::dim : onTarget ? Col::green : gold);
    g.setFont (juce::FontOptions (42.0f, juce::Font::bold));
    const int numW = juce::roundToInt (juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), fmt (I))) + 10;
    g.drawText (fmt (I), big.removeFromLeft (numW), juce::Justification::left);
    g.setFont (juce::FontOptions (16.0f, juce::Font::bold)); g.setColour (Col::text);
    g.drawText ("LUFS", big, juce::Justification::centredLeft);
    b.removeFromTop (6);
    auto row = [&] (const juce::String& cap, const juce::String& val, juce::Colour c)
    {
        auto rr = b.removeFromTop (18);
        g.setColour (Col::dim); g.setFont (juce::FontOptions (11.5f)); g.drawText (cap, rr, juce::Justification::left);
        g.setColour (c); g.setFont (juce::FontOptions (13.0f, juce::Font::bold)); g.drawText (val, rr, juce::Justification::right);
    };
    row ("Short-term (3 s)", fmt (S) + " LUFS", Col::text);
    row ("Momentary (0.4 s)", fmt (M) + " LUFS", Col::text);
    row ("True peak (max)", (tp <= -99.0f ? juce::String ("--.-") : juce::String (tp, 1)) + " dBTP", tp > -0.05f ? Col::pink : Col::text);
    row ("Input short-term", fmt (chain.inLufsS.load()) + " LUFS", Col::dim);
    // short-term bar with target marker
    b.removeFromTop (6);
    auto bar = b.removeFromTop (10).toFloat();
    g.setColour (Col::line); g.fillRoundedRectangle (bar, 3.0f);
    auto xFor = [&] (float l) { return bar.getX() + bar.getWidth() * juce::jlimit (0.0f, 1.0f, (l + 30.0f) / 30.0f); };
    g.setGradientFill (juce::ColourGradient (Col::cyan, bar.getX(), 0, gold, bar.getRight(), 0, false));
    g.fillRoundedRectangle (bar.withRight (xFor (S)), 3.0f);
    g.setColour (Col::text); g.fillRect (xFor (tgt) - 1.0f, bar.getY() - 3, 2.0f, bar.getHeight() + 6);
}

//==============================================================================
LearnPanel::LearnPanel (kmaster::Chain& c) : chain (c)
{
    learn.setColour (juce::TextButton::buttonColourId, gold.withAlpha (0.85f));
    learn.setColour (juce::TextButton::textColourOffId, Col::bg);
    learn.setTooltip ("Play the loudest part of your song and press LEARN. King Master listens for about 20 seconds, then sets the tone match, compression and loudness for you. It also starts by itself the first time it hears audio.");
    learn.onClick = [this] { chain.startLearning(); };
    addAndMakeVisible (learn);
}

void LearnPanel::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().reduced (6, 0); b.removeFromTop (46);
    const int ph = chain.phase.load(); const float pr = chain.progress.load();
    auto bar = b.removeFromTop (10).toFloat();
    g.setColour (Col::line); g.fillRoundedRectangle (bar, 4.0f);
    g.setColour (ph == kmaster::Chain::Done ? Col::green : gold); g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * (ph == kmaster::Chain::Idle ? 0.0f : pr)), 4.0f);
    b.removeFromTop (8);
    juce::String a, c;
    switch (ph)
    {
        case kmaster::Chain::Idle:     a = "Ready"; c = "Press play: King Master starts learning by itself. Use the loudest part of your song."; break;
        case kmaster::Chain::Spectrum: a = "Listening to your tone..."; c = "Keep the loudest part of the song playing."; break;
        case kmaster::Chain::Bands:    a = "Measuring dynamics..."; c = "Keep it playing."; break;
        case kmaster::Chain::Gain:     a = "Setting loudness..."; c = "Almost there."; break;
        case kmaster::Chain::Check1:
        case kmaster::Chain::Check2:   a = "Checking the output level..."; c = "Fine-tuning to hit the target."; break;
        default:                       a = "Mastered for " + juce::String (getTarget ? getTarget() : -11.0f, 1) + " LUFS";
                                       c = "Gain " + juce::String (chain.appliedGainDb.load(), 1) + " dB. Press LEARN again after big mix changes."; break;
    }
    g.setColour (ph == kmaster::Chain::Done ? Col::green : Col::text); g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawFittedText (a, b.removeFromTop (20), juce::Justification::left, 1);
    g.setColour (Col::dim); g.setFont (juce::FontOptions (11.0f));
    g.drawFittedText (c, b, juce::Justification::topLeft, 4);
}

//==============================================================================
void EqView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xcc0d1014)); g.fillRoundedRectangle (r, 6.0f);
    auto xFor = [&] (double f) { return r.getX() + r.getWidth() * (float) (std::log (f / 20.0) / std::log (1000.0)); };
    const float range = 8.0f;
    auto yFor = [&] (double db) { return r.getCentreY() - (float) db / range * r.getHeight() * 0.42f; };
    g.setFont (juce::FontOptions (9.0f));
    for (double f : { 50.0, 100.0, 500.0, 1000.0, 5000.0, 10000.0 })
    {
        g.setColour (Col::line.withAlpha (0.6f)); g.drawVerticalLine ((int) xFor (f), r.getY(), r.getBottom());
        g.setColour (Col::dim); g.drawText (f >= 1000 ? juce::String ((int) (f / 1000)) + "k" : juce::String ((int) f), (int) xFor (f) + 2, (int) r.getBottom() - 12, 30, 10, juce::Justification::left);
    }
    for (float d : { -6.0f, 6.0f }) { g.setColour (Col::line.withAlpha (0.35f)); g.drawHorizontalLine ((int) yFor (d), r.getX(), r.getRight()); }
    g.setColour (Col::line); g.drawHorizontalLine ((int) r.getCentreY(), r.getX(), r.getRight());
    juce::Path p, fill;
    for (int i = 0; i <= 160; ++i)
    {
        const double f = 20.0 * std::pow (1000.0, i / 160.0);
        const float x = xFor (f), y = yFor (juce::jlimit (-range, range, (float) chain.eqResponseDb (f)));
        if (i == 0) { p.startNewSubPath (x, y); fill.startNewSubPath (x, r.getCentreY()); } else p.lineTo (x, y);
        fill.lineTo (x, y);
    }
    fill.lineTo (r.getRight(), r.getCentreY()); fill.closeSubPath();
    g.setColour (gold.withAlpha (0.16f)); g.fillPath (fill);
    g.setColour (gold); g.strokePath (p, juce::PathStrokeType (2.0f));
    g.setColour (Col::line); g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
}

//==============================================================================
KingMasterEditor::KingMasterEditor (KingMasterProcessor& p)
    : AudioProcessorEditor (&p), proc (p), presetBar (kmaster::presets()),
      style (p.apvts, "style", { "WARM: fuller lows, softer highs, a little more tape warmth. Great for hip-hop, R&B, lo-fi and acoustic.",
                                 "BALANCED: neutral, modern and versatile. The safe choice for most songs.",
                                 "OPEN: brighter, airier and more detailed with a tighter low end. Great for EDM, pop and trance." }, gold),
      loudness (p.apvts, "loudness", { "LOW: about -14 LUFS. Matches streaming services, keeps the most dynamics.",
                                       "MEDIUM: about -11 LUFS. Loud and punchy, still dynamic.",
                                       "HIGH: about -8 LUFS. Club / competitive loudness." }, Col::pink),
      learnPanel (p.chain), meter (p.chain), eqView (p.chain),
      monoBass (p.apvts, "mono_bass", "MONO BASS", gold, "Keeps everything below 120 Hz in mono: tighter low end on club systems and vinyl."),
      compare (p.apvts, "compare", "COMPARE", Col::cyan, "Hear your original mix at the same loudness as the master, so you judge the sound, not the volume.")
{
    setLookAndFeel (&lnf);
    logo = juce::ImageCache::getFromMemory (BinaryData::TullyLogo_png, BinaryData::TullyLogo_pngSize);
    productLogo = juce::ImageCache::getFromMemory (BinaryData::KingMasterLogo_png, BinaryData::KingMasterLogo_pngSize);

    presetBar.getCurrent = [this] { return proc.getCurrentPreset(); };
    presetBar.load = [this] (int i) { proc.loadPreset (i); };
    presetBar.sync();

    auto& s = p.apvts;
    auto k = [this, &s] (std::vector<tui::Knob*>& grp, const char* id, const char* cap, juce::Colour c, const char* tip)
    { knobs.push_back (std::make_unique<tui::Knob> (s, id, cap, c, tip)); addAndMakeVisible (*knobs.back()); grp.push_back (knobs.back().get()); };
    k (loudK, "loud_trim", "TRIM", Col::pink, "Fine-tune the loudness target up or down by up to 3 LU.");
    k (tone, "smart_eq", "SMART EQ", gold, "How far the learned tone match moves your mix toward the chosen style.");
    k (tone, "eq_low", "LOW", gold, "Low end (shelf at 110 Hz).");
    k (tone, "eq_mid", "MID", gold, "Body / boxiness around 700 Hz.");
    k (tone, "eq_pres", "PRESENCE", gold, "Vocals, snare and clarity (3.5 kHz).");
    k (tone, "eq_air", "AIR", gold, "Sparkle and openness (shelf at 11 kHz).");
    k (dyn, "comp", "COMPRESSION", Col::pink, "Three-band glue compression. More = denser and more controlled.");
    k (dyn, "warmth", "WARMTH", Col::pink, "Analog-style saturation that rounds off peaks before the limiter.");
    k (dyn, "deess", "DE-ESS", Col::pink, "Softens harsh S sounds and cymbals in the top end.");
    k (stereo, "width", "WIDTH", Col::violet, "Stereo width: 0% = mono, 100% = as mixed, 200% = extra wide.");
    k (out, "ceiling", "CEILING", Col::cyan, "Maximum true-peak level. -1 dBTP is safest for streaming.");

    meter.getTarget = [this] { return kmaster::targetLufs ((int) proc.apvts.getRawParameterValue ("loudness")->load()) + proc.apvts.getRawParameterValue ("loud_trim")->load(); };
    learnPanel.getTarget = meter.getTarget;
    gLow.setTooltip ("Low band compression (below 150 Hz)."); gMid.setTooltip ("Mid band compression."); gHigh.setTooltip ("High band compression (above 4 kHz).");
    gLim.setTooltip ("Limiter gain reduction.");

    for (juce::Component* c : std::initializer_list<juce::Component*> { &presetBar, &links, &style, &loudness, &learnPanel, &meter, &eqView, &gLow, &gMid, &gHigh, &gLim, &monoBass, &compare })
        addAndMakeVisible (c);
    setSize (1080, 680);
    startTimerHz (20);
}

KingMasterEditor::~KingMasterEditor() { stopTimer(); setLookAndFeel (nullptr); }

void KingMasterEditor::timerCallback()
{
    presetBar.sync();
    const int st = (int) proc.apvts.getRawParameterValue ("style")->load(), ld = (int) proc.apvts.getRawParameterValue ("loudness")->load();
    if (st != shownStyle || ld != shownLoud) { shownStyle = st; shownLoud = ld; repaint (rStyle); repaint (rLoud); }
    gLow.setValue (proc.chain.bandGrDb[0].load()); gMid.setValue (proc.chain.bandGrDb[1].load()); gHigh.setValue (proc.chain.bandGrDb[2].load());
    gLim.setValue (proc.chain.limGr.load());
    meter.repaint(); learnPanel.repaint(); eqView.repaint();
}

void KingMasterEditor::paint (juce::Graphics& g)
{
    g.fillAll (Col::bg);
    tui::paintWatermark (g, productLogo, getLocalBounds(), 0.18f);
    tui::paintBrandHeader (g, getWidth(), 76, productLogo, logo, "KING MASTER", "STYLE-BASED MASTERING", gold);
    tui::paintGlassSection (g, rStyle, "STYLE", gold);
    tui::paintGlassSection (g, rLoud, "LOUDNESS", Col::pink);
    tui::paintGlassSection (g, rLearn, "LEARN", gold);
    tui::paintGlassSection (g, rMeter, "LOUDNESS METER", Col::green);
    tui::paintGlassSection (g, rTone, "TONE", gold);
    tui::paintGlassSection (g, rDyn, "DYNAMICS", Col::pink);
    tui::paintGlassSection (g, rStereo, "STEREO", Col::violet);
    tui::paintGlassSection (g, rOut, "OUTPUT", Col::cyan);
    // style / loudness descriptions
    auto desc = [&g] (juce::Rectangle<int> r, const juce::String& t)
    { g.setColour (Col::dim); g.setFont (juce::FontOptions (11.0f)); g.drawFittedText (t, r, juce::Justification::topLeft, 4); };
    const int st = (int) proc.apvts.getRawParameterValue ("style")->load();
    const juce::String sd[] { "Fuller lows, softer highs and more tape warmth. Hip-hop, R&B, lo-fi, acoustic.",
                              "Neutral, modern and versatile. The safe choice for most songs.",
                              "Brighter and airier with a tighter low end. EDM, pop, trance." };
    desc (rStyle.reduced (14, 0).withTrimmedTop (84).withHeight (60), sd[juce::jlimit (0, 2, st)]);
    g.setColour (Col::text.withAlpha (0.5f)); g.setFont (juce::FontOptions (10.5f));
    g.drawFittedText ("Tip: pick the style, set the loudness, then play the loudest part of your song. LEARN does the rest.", rStyle.reduced (14, 0).withTrimmedTop (150).withHeight (60), juce::Justification::topLeft, 4);
    const int ld = (int) proc.apvts.getRawParameterValue ("loudness")->load();
    const juce::String ldd[] { "About -14 LUFS: streaming level, most dynamic.", "About -11 LUFS: loud and punchy.", "About -8 LUFS: club / competitive." };
    desc (rLoud.reduced (14, 0).withTrimmedTop (80).withHeight (40).withTrimmedRight (90), ldd[juce::jlimit (0, 2, ld)]);
}

void KingMasterEditor::resized()
{
    const int W = getWidth();
    presetBar.setBounds (340, 14, 390, 50);
    links.setBounds (W - 210, 26, 200, 36);

    auto b = getLocalBounds().reduced (10); b.removeFromTop (76);
    const int gap = 8;
    auto rowA = b.removeFromTop (228); b.removeFromTop (gap);
    auto rowB = b;
    rStyle = rowA.removeFromLeft (280); rowA.removeFromLeft (gap);
    rLoud = rowA.removeFromLeft (280); rowA.removeFromLeft (gap);
    rMeter = rowA.removeFromRight (250); rowA.removeFromRight (gap);
    rLearn = rowA;

    style.setBounds (rStyle.reduced (10, 0).withTrimmedTop (34).withHeight (40));
    loudness.setBounds (rLoud.reduced (10, 0).withTrimmedTop (34).withHeight (40));
    loudK[0]->setBounds (rLoud.getCentreX() - 42, rLoud.getY() + 118, 84, 100);
    learnPanel.setBounds (rLearn.reduced (6, 0).withTrimmedTop (30).withTrimmedBottom (8));
    meter.setBounds (rMeter.reduced (8).withTrimmedTop (24));

    rTone = rowB.removeFromLeft (420); rowB.removeFromLeft (gap);
    rOut = rowB.removeFromRight (118); rowB.removeFromRight (gap);
    rStereo = rowB.removeFromRight (118); rowB.removeFromRight (gap);
    rDyn = rowB;
    {
        auto a = rTone.reduced (8, 6); a.removeFromTop (24);
        auto knobsRow = a.removeFromBottom (92);
        eqView.setBounds (a.reduced (2, 2));
        const int w = knobsRow.getWidth() / (int) tone.size();
        for (auto* kk : tone) kk->setBounds (knobsRow.removeFromLeft (w).withSizeKeepingCentre (juce::jmin (w, 80), 92));
    }
    {
        auto a = rDyn.reduced (8, 6); a.removeFromTop (24);
        auto meters = a.removeFromRight (128);
        const int mw = meters.getWidth() / 4;
        for (auto* m : { &gLow, &gMid, &gHigh, &gLim }) m->setBounds (meters.removeFromLeft (mw));
        const int w = a.getWidth() / (int) dyn.size();
        for (auto* kk : dyn) kk->setBounds (a.removeFromLeft (w).withSizeKeepingCentre (juce::jmin (w, 84), juce::jmin (a.getHeight(), 110)));
    }
    {
        auto a = rStereo.reduced (8, 6); a.removeFromTop (24);
        monoBass.setBounds (a.removeFromBottom (26));
        stereo[0]->setBounds (a.withSizeKeepingCentre (juce::jmin (a.getWidth(), 90), juce::jmin (a.getHeight(), 110)));
    }
    {
        auto a = rOut.reduced (8, 6); a.removeFromTop (24);
        compare.setBounds (a.removeFromBottom (26));
        out[0]->setBounds (a.withSizeKeepingCentre (juce::jmin (a.getWidth(), 90), juce::jmin (a.getHeight(), 110)));
    }
}
