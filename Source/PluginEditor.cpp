/*
  ==============================================================================
    Mister Master - PluginEditor.cpp
    (c)(p) 2026 Tully EDM Vibe
  ==============================================================================
*/
#include "PluginEditor.h"
#include "TullyLicence.h"
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
    learn.setTooltip ("Play the loudest part of your song and press LEARN. Mister Master listens for about 20 seconds, then sets the tone match, compression and loudness for you. It also starts by itself the first time it hears audio.");
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
        case kmaster::Chain::Idle:     a = "Ready"; c = "Press play: Mister Master starts learning by itself. Use the loudest part of your song."; break;
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
StreamStrip::StreamStrip (KingMasterProcessor& p)
    : proc (p),
      on (p.apvts, "stream_check", "STREAM CHECK", Col::green, "Warns you by itself when the master, or the mix that arrives, is too loud for streaming. It only looks: it never changes the sound."),
      platform (p.apvts, "platform", "Where the song is going. All Streaming uses -14 LUFS and shows what each service would do.")
{
    setTooltip ("How loud your song is for streaming: the integrated loudness and true peak of the master, and the mix before mastering. Play the whole song for the exact figure. Click to measure again. "
                "The references are the loudness figures services commonly publish; they can change.");
    fix.setTooltip ("Sets LOUDNESS, TRIM and CEILING so the master lands on this platform's level.");
    fix.setColour (juce::TextButton::buttonColourId, Col::pink.withAlpha (0.85f));
    fix.onClick = [this] { proc.fixForPlatform(); };
    addAndMakeVisible (on); addAndMakeVisible (platform); addChildComponent (fix);
}

juce::Colour StreamStrip::colour() const
{
    using R = kmaster::StreamReport;
    switch (report.level)
    {
        case R::Ok:   return Col::green;
        case R::Note: return Col::cyan;
        case R::Warn: return Col::amber;
        case R::Bad:  return juce::Colour (0xffff4a5e);
        default:      return Col::dim;
    }
}

void StreamStrip::refresh()
{
    report = proc.streamReport();
    fix.setVisible (report.canFix);
    platform.setEnabled (report.level != kmaster::StreamReport::Off);
    pulse += 0.16f; if (pulse > 6.2831853f) pulse -= 6.2831853f;
    repaint();
}

void StreamStrip::resized()
{
    auto b = getLocalBounds().reduced (10, 8);
    auto left = b.removeFromLeft (150);
    on.setBounds (left.removeFromTop (22)); left.removeFromTop (4); platform.setBounds (left.removeFromTop (22));
    b.removeFromLeft (14);
    fix.setBounds (b.removeFromRight (96).withSizeKeepingCentre (96, 34)); b.removeFromRight (10);
    text = b;
}

void StreamStrip::paint (juce::Graphics& g)
{
    using R = kmaster::StreamReport;
    const auto c = colour(); const bool alarm = report.level == R::Bad || report.level == R::Warn;
    const float glow = alarm ? 0.5f + 0.5f * std::sin (pulse) : 0.0f;
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (juce::Colour (0xcc0d1014)); g.fillRoundedRectangle (r, 8.0f);
    if (alarm) { g.setColour (c.withAlpha (0.10f + 0.12f * glow)); g.fillRoundedRectangle (r, 8.0f); }
    g.setColour (report.level == R::Off || report.level == R::Waiting ? Col::line : c.withAlpha (alarm ? 0.55f + 0.45f * glow : 0.7f));
    g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, alarm ? 2.0f : 1.2f);

    auto b = text; if (fix.isVisible() == false) b = b.withRight (getWidth() - 12);
    // lamp
    auto lamp = b.removeFromLeft (16).withSizeKeepingCentre (12, 12).toFloat();
    g.setColour (c.withAlpha (0.25f)); g.fillEllipse (lamp.expanded (3.0f)); g.setColour (c); g.fillEllipse (lamp);
    b.removeFromLeft (8);
    if (report.level == R::Off)
    {
        g.setColour (Col::dim); g.setFont (juce::FontOptions (13.0f));
        g.drawFittedText ("Streaming check is off.", b, juce::Justification::centredLeft, 1); return;
    }
    const bool chips = ! report.chips.empty() && report.chips.size() > 1;
    auto top = b.removeFromTop (chips ? 18 : 22);
    g.setColour (report.level == R::Waiting ? Col::text : c); g.setFont (juce::FontOptions (15.5f, juce::Font::bold));
    g.drawFittedText (report.headline, top, juce::Justification::centredLeft, 1, 0.75f);
    auto mid = b.removeFromTop (chips ? 14 : 28);
    g.setColour (Col::text.withAlpha (0.82f)); g.setFont (juce::FontOptions (11.5f));
    g.drawFittedText (report.detail, mid, juce::Justification::topLeft, chips ? 1 : 2, chips ? 0.72f : 0.85f);
    if (chips)
    {
        b.removeFromTop (3);
        auto row = b.removeFromTop (17); g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
        const int w = juce::jmin (132, row.getWidth() / (int) report.chips.size());
        for (auto& ch : report.chips)
        {
            auto cell = row.removeFromLeft (w).reduced (2, 0).toFloat();
            const auto cc = ch.over ? (ch.turnDown > 3.0f ? juce::Colour (0xffff4a5e) : Col::amber) : Col::green;
            g.setColour (cc.withAlpha (0.16f)); g.fillRoundedRectangle (cell, 4.0f);
            g.setColour (cc.withAlpha (0.7f)); g.drawRoundedRectangle (cell.reduced (0.5f), 4.0f, 1.0f);
            g.setColour (cc.brighter (0.2f));
            g.drawFittedText (ch.name + (ch.turnDown > 0.05f ? "  -" + juce::String (ch.turnDown, 1) + " dB" : "  OK"), cell.toNearestInt().reduced (4, 0), juce::Justification::centred, 1, 0.7f);
        }
    }
}

//==============================================================================
LoudnessGraph::LoudnessGraph (KingMasterProcessor& p) : proc (p)
{
    setTooltip ("Loudness along the song (3 second loudness). Gold: the master. Blue: your mix before mastering. The green line is the level for the platform you chose, the amber line is how loud a mix may be before mastering; "
                "red and amber show where it is too loud, pink marks on top are where the mix clips. Move the mouse for the time, bar and loudness; double-click to clear.");
}

void LoudnessGraph::refresh()
{
    if (++tick % 3 != 0) return;                        // ~7 times a second is plenty
    auto& c = proc.chain; const int n = juce::jlimit (0, (int) kmaster::Chain::kGraph, c.gMax.load() + 1);
    in.resize ((size_t) n); out.resize ((size_t) n); bars.resize ((size_t) n); clips.assign ((size_t) n, false);
    for (int i = 0; i < n; ++i)
    {
        in[(size_t) i] = c.gIn[(size_t) i].load (std::memory_order_relaxed); out[(size_t) i] = c.gOut[(size_t) i].load (std::memory_order_relaxed);
        bars[(size_t) i] = c.gBar[(size_t) i].load (std::memory_order_relaxed); clips[(size_t) i] = c.gClip[(size_t) i].load (std::memory_order_relaxed) != 0;
    }
    platform = (int) proc.apvts.getRawParameterValue ("platform")->load(); on = proc.apvts.getRawParameterValue ("stream_check")->load() > 0.5f;
    loudOut = on ? kmaster::loudSpans (out, kmaster::masterLimit (platform)) : std::vector<kmaster::LoudSpan>();
    loudIn = on ? kmaster::loudSpans (in, kmaster::kMixHot) : std::vector<kmaster::LoudSpan>();
    where = kmaster::describeSpans (loudOut, out, bars); whereMix = kmaster::describeSpans (loudIn, in, bars);
    repaint();
}

void LoudnessGraph::paint (juce::Graphics& g)
{
    const juce::Colour red (0xffff4a5e), hotMix (0xffffa03c);      // master over the line, mix over its line
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (juce::Colour (0xcc0d1014)); g.fillRoundedRectangle (r, 8.0f);
    g.setColour (Col::line); g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
    auto b = getLocalBounds().reduced (10, 5);
    // title, where it is too loud, legend
    auto top = b.removeFromTop (16);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold)); g.setColour (Col::dim);
    g.drawText ("LOUDNESS ALONG THE SONG", top.removeFromLeft (172), juce::Justification::centredLeft);
    {
        auto lg = top.removeFromRight (150); g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
        g.setColour (gold); g.fillRect (lg.getX(), lg.getCentreY() - 1, 14, 2); g.drawText ("MASTER", lg.getX() + 18, lg.getY(), 52, lg.getHeight(), juce::Justification::centredLeft);
        g.setColour (Col::cyan); g.fillRect (lg.getX() + 76, lg.getCentreY() - 1, 14, 2); g.drawText ("YOUR MIX", lg.getX() + 94, lg.getY(), 56, lg.getHeight(), juce::Justification::centredLeft);
    }
    {
        const juce::String mt = whereMix.isEmpty() ? juce::String() : "MIX too loud " + juce::String (whereMix == "throughout" ? "" : "at ") + whereMix;
        const juce::String at = where.isEmpty() ? juce::String() : "MASTER too loud " + juce::String (where == "throughout" ? "" : "at ") + where;
        auto tr = top.reduced (4, 0); g.setFont (juce::FontOptions (11.5f, juce::Font::bold));
        if (mt.isEmpty() && at.isEmpty())
        { g.setColour (Col::dim); g.drawFittedText (in.empty() ? "Play the song: the lines show where it is too loud." : on ? "Nothing over the lines so far." : "", tr, juce::Justification::centredLeft, 1, 0.75f); }
        else
        {
            if (mt.isNotEmpty())
            {
                const int w = at.isEmpty() ? tr.getWidth() : juce::jmin (tr.getWidth() * 55 / 100, juce::roundToInt (juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), mt)) + 18);
                g.setColour (hotMix); g.drawFittedText (mt, tr.removeFromLeft (w), juce::Justification::centredLeft, 1, 0.7f);
            }
            g.setColour (red); g.drawFittedText (at, tr, juce::Justification::centredLeft, 1, 0.7f);
        }
    }
    auto axis = b.removeFromBottom (12); auto plot = b.withTrimmedTop (3).withTrimmedLeft (30).toFloat();
    const float lo = -30.0f, hi = -3.0f;
    auto yFor = [&] (float l) { return plot.getBottom() - plot.getHeight() * juce::jlimit (0.0f, 1.0f, (l - lo) / (hi - lo)); };
    g.setFont (juce::FontOptions (9.0f));
    for (float l : { -24.0f, -18.0f, -12.0f, -6.0f })
    {
        g.setColour (Col::line.withAlpha (0.45f)); g.drawHorizontalLine ((int) yFor (l), plot.getX(), plot.getRight());
        g.setColour (Col::dim); g.drawText (juce::String ((int) l), 8, (int) yFor (l) - 5, 26, 10, juce::Justification::centredRight);
    }
    // time range: what has been played, at least 30 s
    int first = -1, last = (int) in.size() - 1;
    for (int i = 0; i <= last; ++i) if (in[(size_t) i] > kmaster::kNoData + 1.0f || out[(size_t) i] > kmaster::kNoData + 1.0f) { first = i; break; }
    if (first < 0) first = 0;
    const float t0 = (float) first * kmaster::kGraphSlot, t1 = juce::jmax (t0 + 30.0f, (float) (last + 1) * kmaster::kGraphSlot);
    auto xFor = [&] (float t) { return plot.getX() + plot.getWidth() * (t - t0) / (t1 - t0); };
    // time ticks
    const float span = t1 - t0; const float step = span <= 60 ? 10.0f : span <= 150 ? 20.0f : span <= 330 ? 30.0f : span <= 700 ? 60.0f : 120.0f;
    for (float t = std::ceil (t0 / step) * step; t < t1; t += step)
    {
        g.setColour (Col::line.withAlpha (0.35f)); g.drawVerticalLine ((int) xFor (t), plot.getY(), plot.getBottom());
        g.setColour (Col::dim); g.drawText (kmaster::clock (t), (int) xFor (t) - 18, axis.getY(), 36, axis.getHeight(), juce::Justification::centred);
    }
    const float ref = kmaster::referenceLine (platform), lim = kmaster::masterLimit (platform);
    // red bands: where it is too loud
    for (auto& s : loudIn)  { g.setColour (Col::amber.withAlpha (0.12f)); g.fillRect (juce::Rectangle<float> (xFor (s.start), plot.getY(), juce::jmax (2.0f, xFor (s.end) - xFor (s.start)), plot.getHeight())); }
    for (auto& s : loudOut) { g.setColour (red.withAlpha (0.14f)); g.fillRect (juce::Rectangle<float> (xFor (s.start), plot.getY(), juce::jmax (2.0f, xFor (s.end) - xFor (s.start)), plot.getHeight())); }
    // a curve, drawn red where it is over its limit
    auto curve = [&] (const std::vector<float>& v, juce::Colour col, juce::Colour hotCol, float over, float w)
    {
        juce::Path ok, hot; bool pen = false; float px = 0, py = 0; bool pHot = false;
        for (int i = 0; i < (int) v.size(); ++i)
        {
            if (v[(size_t) i] <= kmaster::kNoData + 1.0f) { pen = false; continue; }
            const float x = xFor (((float) i + 0.5f) * kmaster::kGraphSlot), y = yFor (v[(size_t) i]); const bool h = on && v[(size_t) i] > over;
            if (pen) { auto& p = (h || pHot) ? hot : ok; p.startNewSubPath (px, py); p.lineTo (x, y); }
            px = x; py = y; pHot = h; pen = true;
        }
        g.setColour (col); g.strokePath (ok, juce::PathStrokeType (w)); g.setColour (hotCol); g.strokePath (hot, juce::PathStrokeType (w + 0.8f));
    };
    curve (in, Col::cyan.withAlpha (0.85f), hotMix, kmaster::kMixHot, 1.4f);
    curve (out, gold, red, lim, 2.0f);
    // the limit lines: where the song is going, and how hot a mix may arrive
    if (on)
    {
        { const float my = yFor (kmaster::kMixHot); const float d2[] { 2.0f, 4.0f }; g.setColour (hotMix.withAlpha (0.8f)); g.drawDashedLine (juce::Line<float> (plot.getX(), my, plot.getRight(), my), d2, 2, 1.2f);
          g.setFont (juce::FontOptions (9.5f, juce::Font::bold)); g.setColour (juce::Colour (0xdd0d1014)); g.fillRect ((int) plot.getX() + 2, (int) my - 13, 78, 12); g.setColour (hotMix); g.drawText ("MIX LIMIT " + juce::String (kmaster::kMixHot, 0), (int) plot.getX() + 5, (int) my - 13, 76, 12, juce::Justification::centredLeft); }
        const float y = yFor (ref); const float dash[] { 6.0f, 4.0f };
        g.setColour (Col::green); g.drawDashedLine (juce::Line<float> (plot.getX(), y, plot.getRight(), y), dash, 2, 1.6f);
        const juce::String nm = (platform == kmaster::P_AllStreaming ? juce::String ("STREAMING") : kmaster::platformNames()[platform].toUpperCase()) + " " + juce::String (ref, 0);
        g.setFont (juce::FontOptions (9.5f, juce::Font::bold)); const int tw = 108;
        g.setColour (juce::Colour (0xdd0d1014)); g.fillRect ((int) plot.getRight() - tw, (int) y - 13, tw, 12); g.setColour (Col::green); g.drawText (nm, (int) plot.getRight() - tw, (int) y - 13, tw - 2, 12, juce::Justification::centredRight);
    }
    for (int i = 0; i < (int) clips.size(); ++i) if (clips[(size_t) i]) { g.setColour (Col::pink); g.fillRect (xFor ((float) i * kmaster::kGraphSlot), plot.getY(), juce::jmax (1.5f, xFor (kmaster::kGraphSlot) - xFor (0)), 4.0f); }
    // read-out under the mouse
    if (hoverX >= (int) plot.getX() && hoverX <= (int) plot.getRight())
    {
        const float t = t0 + (t1 - t0) * ((float) hoverX - plot.getX()) / plot.getWidth(); const int i = (int) (t / kmaster::kGraphSlot);
        g.setColour (Col::text.withAlpha (0.5f)); g.drawVerticalLine (hoverX, plot.getY(), plot.getBottom());
        juce::String s = kmaster::clock (t);
        if (i >= 0 && i < (int) in.size())
        {
            if (bars[(size_t) i] > 0.5f) s << "  bar " << (int) bars[(size_t) i];
            if (out[(size_t) i] > kmaster::kNoData + 1.0f) s << "  master " << juce::String (out[(size_t) i], 1);
            if (in[(size_t) i] > kmaster::kNoData + 1.0f) s << "  mix " << juce::String (in[(size_t) i], 1);
            s << " LUFS";
        }
        const int w = 250; const int x = juce::jlimit ((int) plot.getX(), (int) plot.getRight() - w, hoverX - w / 2);
        g.setColour (juce::Colour (0xee14171d)); g.fillRoundedRectangle ((float) x, plot.getBottom() - 19.0f, (float) w, 17.0f, 4.0f);
        g.setColour (Col::text); g.setFont (juce::FontOptions (11.0f, juce::Font::bold)); g.drawFittedText (s, x, (int) plot.getBottom() - 19, w, 17, juce::Justification::centred, 1, 0.8f);
    }
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
      learnPanel (p.chain), meter (p.chain), eqView (p.chain), stream (p), graph (p),
      monoBass (p.apvts, "mono_bass", "MONO BASS", gold, "Keeps everything below 120 Hz in mono: tighter low end on club systems and vinyl."),
      compare (p.apvts, "compare", "COMPARE", Col::cyan, "Hear your original mix at the same loudness as the master, so you judge the sound, not the volume.")
{
    lnf.setAccent (gold);
    setLookAndFeel (&lnf);
    logo = juce::ImageCache::getFromMemory (BinaryData::TullyLogo_png, BinaryData::TullyLogo_pngSize);
    productLogo = juce::ImageCache::getFromMemory (BinaryData::MisterMasterLogo_png, BinaryData::MisterMasterLogo_pngSize);

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

    for (juce::Component* c : std::initializer_list<juce::Component*> { &presetBar, &links, &style, &loudness, &learnPanel, &meter, &eqView, &stream, &graph, &gLow, &gMid, &gHigh, &gLim, &monoBass, &compare })
        addAndMakeVisible (c);
    setSize (1080, 822);
    startTimerHz (20);

    tully::addActivation (*this);   // activation screen until this Mac is licensed
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
    stream.refresh(); graph.refresh();
}

void KingMasterEditor::paint (juce::Graphics& g)
{
    neon::paintBackdrop (g, getLocalBounds(), gold, backdrop);
    tui::paintWatermark (g, productLogo, getLocalBounds(), 0.18f);
    tui::paintBrandHeader (g, getWidth(), 76, productLogo, logo, "MISTER MASTER", "STYLE-BASED MASTERING", gold, presetBar.getX() - 10);
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
    rStream = b.removeFromTop (68); b.removeFromTop (gap);
    stream.setBounds (rStream);
    rGraph = b.removeFromTop (116); b.removeFromTop (gap);
    graph.setBounds (rGraph);
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
