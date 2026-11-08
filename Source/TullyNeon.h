/*
  ==============================================================================
    Tully EDM Vibe - TullyNeon.h
    (c)(p) 2026 Tully EDM Vibe

    The shared "neon" look for every Tully EDM Vibe plugin: one family (brand pink
    + cyan on a deep night-blue backdrop), each product with its own accent colour.

      * neon::LookAndFeel   glowing knobs with tick rings, neon buttons / toggles /
                            combo boxes / sliders / popup menus / tooltips
      * neon::paintBackdrop night gradient + perspective grid + scanlines + vignette
                            (rendered once into a cached image per size)
      * neon::glassPanel    translucent panel with a neon edge and a lit title bar
      * neon::glowText      text with a soft neon bloom
      * neon::Header        brand header: logo, glowing product name, accent rule

    Header-only, JUCE modules only, no fonts or images of its own. Drawing cost is
    kept low: the backdrop is cached, glows are 2-3 extra strokes, nothing animates
    unless the plugin already repaints (meters, scopes).
  ==============================================================================
*/
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace neon
{
    namespace Pal
    {
        const juce::Colour night0 { 0xff07060f };   // backdrop, top
        const juce::Colour night1 { 0xff120a24 };   // backdrop, bottom (violet tint)
        const juce::Colour glass  { 0xff141024 };   // panels
        const juce::Colour glassHi{ 0xff1e1834 };   // controls
        const juce::Colour edge   { 0xff2f2850 };   // hairlines
        const juce::Colour text   { 0xffeef0ff };
        const juce::Colour dim    { 0xff8e8aae };
        const juce::Colour pink   { 0xffff2d8a };   // Tully EDM Vibe brand
        const juce::Colour cyan   { 0xff22e6f2 };   // Tully EDM Vibe brand
        const juce::Colour amber  { 0xffffb627 };
        const juce::Colour violet { 0xffa36bff };
        const juce::Colour green  { 0xff3dffa2 };
        const juce::Colour gold   { 0xffffd25a };
    }

    //==========================================================================
    // soft bloom: the same path stroked wide and faint, then narrower and brighter
    inline void strokeGlow (juce::Graphics& g, const juce::Path& p, juce::Colour c, float width, float glow = 1.0f)
    {
        using PST = juce::PathStrokeType;
        g.setColour (c.withAlpha (0.10f * glow)); g.strokePath (p, PST (width * 3.6f, PST::curved, PST::rounded));
        g.setColour (c.withAlpha (0.22f * glow)); g.strokePath (p, PST (width * 2.0f, PST::curved, PST::rounded));
        g.setColour (c);                          g.strokePath (p, PST (width,        PST::curved, PST::rounded));
        g.setColour (juce::Colours::white.withAlpha (0.35f)); g.strokePath (p, PST (juce::jmax (0.8f, width * 0.35f), PST::curved, PST::rounded));
    }

    inline void glowText (juce::Graphics& g, const juce::String& t, juce::Rectangle<float> r, juce::Colour c,
                          float size, juce::Justification j = juce::Justification::centredLeft, bool bold = true, float glow = 1.0f)
    {
        juce::Font f (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
        f.setExtraKerningFactor (0.06f);
        g.setFont (f);
        for (auto [d, a] : { std::pair<float, float> { 2.2f, 0.10f }, { 1.2f, 0.18f } })
        {
            g.setColour (c.withAlpha (a * glow));
            for (auto [dx, dy] : { std::pair<float, float> { -d, 0 }, { d, 0 }, { 0, -d }, { 0, d } })
                g.drawText (t, r.translated (dx, dy), j, false);
        }
        g.setColour (c.interpolatedWith (juce::Colours::white, 0.35f));
        g.drawText (t, r, j, false);
    }

    //==========================================================================
    // Backdrop, cached per size + accent
    class BackdropCache
    {
    public:
        const juce::Image& get (int w, int h, juce::Colour accent)
        {
            if (img.isValid() && img.getWidth() == w && img.getHeight() == h && accent == lastAccent) return img;
            lastAccent = accent;
            img = juce::Image (juce::Image::RGB, juce::jmax (1, w), juce::jmax (1, h), true);
            juce::Graphics g (img);
            const float W = (float) w, H = (float) h;
            g.setGradientFill (juce::ColourGradient (Pal::night0, 0, 0, Pal::night1, 0, H, false));
            g.fillAll();
            // accent haze from the top and brand haze from the bottom corners
            {
                juce::ColourGradient top (accent.withAlpha (0.16f), W * 0.5f, -H * 0.15f, juce::Colours::transparentBlack, W * 0.5f, H * 0.55f, true);
                g.setGradientFill (top); g.fillRect (0.0f, 0.0f, W, H);
                juce::ColourGradient bl (Pal::pink.withAlpha (0.10f), 0, H, juce::Colours::transparentBlack, W * 0.45f, H * 0.55f, true);
                g.setGradientFill (bl); g.fillRect (0.0f, 0.0f, W, H);
                juce::ColourGradient br (Pal::cyan.withAlpha (0.09f), W, H, juce::Colours::transparentBlack, W * 0.55f, H * 0.55f, true);
                g.setGradientFill (br); g.fillRect (0.0f, 0.0f, W, H);
            }
            // perspective "synth grid" in the lower third
            {
                const float horizon = H * 0.62f, vx = W * 0.5f;
                juce::Graphics::ScopedSaveState ss (g);
                g.reduceClipRegion (0, (int) horizon, w, h - (int) horizon);
                for (int i = -24; i <= 24; ++i)
                {
                    const float x = vx + (float) i * W / 12.0f;
                    g.setColour (accent.withAlpha (0.055f));
                    g.drawLine (vx, horizon, x, H * 1.4f, 1.0f);
                }
                for (int k = 1; k < 14; ++k)
                {
                    const float y = horizon + (H - horizon) * std::pow ((float) k / 13.0f, 1.8f);
                    g.setColour (accent.withAlpha (0.03f + 0.05f * (float) k / 13.0f));
                    g.drawHorizontalLine ((int) y, 0.0f, W);
                }
            }
            // scanlines
            g.setColour (juce::Colours::black.withAlpha (0.10f));
            for (int y = 0; y < h; y += 3) g.drawHorizontalLine (y, 0.0f, W);
            // sparse stars
            juce::Random r (42);
            for (int i = 0; i < (w * h) / 9000; ++i)
            {
                const float x = r.nextFloat() * W, y = r.nextFloat() * H * 0.6f, s = 0.6f + r.nextFloat() * 1.1f;
                g.setColour (juce::Colours::white.withAlpha (0.08f + 0.22f * r.nextFloat()));
                g.fillEllipse (x, y, s, s);
            }
            // vignette
            juce::ColourGradient vig (juce::Colours::transparentBlack, W * 0.5f, H * 0.5f, juce::Colours::black.withAlpha (0.55f), 0, 0, true);
            g.setGradientFill (vig); g.fillRect (0.0f, 0.0f, W, H);
            return img;
        }
    private:
        juce::Image img; juce::Colour lastAccent;
    };

    inline void paintBackdrop (juce::Graphics& g, juce::Rectangle<int> area, juce::Colour accent, BackdropCache& cache)
    {
        g.drawImageAt (cache.get (area.getWidth(), area.getHeight(), accent), area.getX(), area.getY());
    }

    //==========================================================================
    // translucent panel with a neon edge; optional lit title strip
    inline void glassPanel (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour accent,
                            const juce::String& title = {}, float radius = 9.0f, float alpha = 0.72f)
    {
        g.setColour (Pal::glass.withAlpha (alpha)); g.fillRoundedRectangle (r, radius);
        // inner top sheen
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.045f), r.getX(), r.getY(),
                                                 juce::Colours::transparentWhite, r.getX(), r.getY() + juce::jmin (40.0f, r.getHeight()), false));
        g.fillRoundedRectangle (r, radius);
        // edge: faint all round, accent glow along the top
        g.setColour (Pal::edge.withAlpha (0.9f)); g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
        juce::Path topEdge;
        topEdge.startNewSubPath (r.getX() + radius, r.getY() + 0.5f);
        topEdge.lineTo (r.getRight() - radius, r.getY() + 0.5f);
        strokeGlow (g, topEdge, accent.withAlpha (0.85f), 1.2f, 0.8f);
        if (title.isNotEmpty())
        {
            g.setColour (accent); g.fillRoundedRectangle (r.getX() + 11, r.getY() + 9, 3.5f, 13, 1.75f);
            g.setColour (accent.withAlpha (0.25f)); g.fillRoundedRectangle (r.getX() + 9, r.getY() + 7, 7.5f, 17, 3.0f);
            juce::Font f (juce::FontOptions (12.0f, juce::Font::bold)); f.setExtraKerningFactor (0.12f);
            g.setFont (f); g.setColour (Pal::text);
            g.drawText (title.toUpperCase(), (int) r.getX() + 21, (int) r.getY() + 6, (int) r.getWidth() - 26, 18, juce::Justification::left);
        }
    }

    // brand header strip: logo, glowing product name, small caps line, accent rule
    // textRight: x where header text must stop (e.g. a preset bar sits there); 0 = area's right edge
    inline void paintHeader (juce::Graphics& g, juce::Rectangle<int> area, const juce::Image& logo,
                             const juce::String& title, const juce::String& subtitle, juce::Colour accent, int textRight = 0)
    {
        auto r = area.toFloat();
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.55f), 0, r.getY(),
                                                 juce::Colours::black.withAlpha (0.15f), 0, r.getBottom(), false));
        g.fillRect (r);
        juce::Path rule; rule.startNewSubPath (r.getX(), r.getBottom() - 1); rule.lineTo (r.getRight(), r.getBottom() - 1);
        g.setGradientFill (juce::ColourGradient (Pal::pink, r.getX(), 0, Pal::cyan, r.getRight(), 0, false));
        g.fillRect (r.getX(), r.getBottom() - 2.0f, r.getWidth(), 2.0f);
        g.setColour (accent.withAlpha (0.18f)); g.fillRect (r.getX(), r.getBottom() - 6.0f, r.getWidth(), 4.0f);
        const float h = r.getHeight();
        float tx = r.getX() + 14;
        if (logo.isValid())
        {
            const float ls = h - 12;
            g.drawImageWithin (logo, (int) tx, (int) (r.getY() + 6), (int) ls, (int) ls, juce::RectanglePlacement::centred);
            tx += ls + 12;
        }
        glowText (g, title.toUpperCase(), { tx, r.getY() + h * 0.10f, 420.0f, h * 0.50f }, accent, juce::jmin (28.0f, h * 0.42f));
        juce::Font sf (juce::FontOptions (10.5f, juce::Font::bold)); sf.setExtraKerningFactor (0.18f);
        g.setFont (sf); g.setColour (Pal::pink);
        g.drawText ("TULLY EDM VIBE", (int) tx + 1, (int) (r.getY() + h * 0.62f), 120, 14, juce::Justification::left);
        g.setColour (Pal::dim); g.setFont (juce::FontOptions (10.5f));
        if (subtitle.isNotEmpty())
        {
            const int sx = (int) tx + 122;
            const int sw = juce::jmin (360, (textRight > 0 ? textRight : area.getRight()) - sx);
            if (sw > 30)
                g.drawText (subtitle, sx, (int) (r.getY() + h * 0.62f), sw, 14, juce::Justification::left, true);
        }
    }

    //==========================================================================
    class LookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        explicit LookAndFeel (juce::Colour accentColour = Pal::cyan) { setAccent (accentColour); }

        // the product's own colour: value arcs, focus edges, highlights
        void setAccent (juce::Colour accentColour)
        {
            using namespace juce;
            accent = accentColour;
            setColourScheme ({ Pal::night0, Pal::glass, Pal::glassHi, Pal::edge, Pal::text,
                               accent.withAlpha (0.6f), Colours::white, accent, Pal::text });
            setColour (Slider::rotarySliderFillColourId, accent);
            setColour (Slider::rotarySliderOutlineColourId, Pal::edge);
            setColour (Slider::thumbColourId, accent);
            setColour (Slider::trackColourId, accent);
            setColour (Slider::backgroundColourId, Pal::glassHi);
            setColour (Slider::textBoxTextColourId, Pal::text);
            setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
            setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
            setColour (ComboBox::backgroundColourId, Pal::glassHi);
            setColour (ComboBox::outlineColourId, Pal::edge);
            setColour (ComboBox::textColourId, Pal::text);
            setColour (ComboBox::arrowColourId, accent);
            setColour (PopupMenu::backgroundColourId, Pal::glass);
            setColour (PopupMenu::textColourId, Pal::text);
            setColour (PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.28f));
            setColour (PopupMenu::highlightedTextColourId, Colours::white);
            setColour (PopupMenu::headerTextColourId, Pal::cyan);
            setColour (TextButton::buttonColourId, Pal::glassHi);
            setColour (TextButton::buttonOnColourId, accent);
            setColour (TextButton::textColourOffId, Pal::text);
            setColour (TextButton::textColourOnId, Pal::night0);
            setColour (ToggleButton::textColourId, Pal::text);
            setColour (ToggleButton::tickColourId, accent);
            setColour (ToggleButton::tickDisabledColourId, Pal::dim);
            setColour (TooltipWindow::backgroundColourId, Pal::glass);
            setColour (TooltipWindow::textColourId, Pal::text);
            setColour (TooltipWindow::outlineColourId, accent.withAlpha (0.7f));
            setColour (Label::textColourId, Pal::text);
            setColour (TextEditor::backgroundColourId, Pal::glass.withAlpha (0.85f));
            setColour (TextEditor::outlineColourId, Pal::edge);
            setColour (TextEditor::focusedOutlineColourId, accent);
            setColour (TextEditor::textColourId, Pal::text);
            setColour (TextEditor::highlightColourId, accent.withAlpha (0.35f));
            setColour (CaretComponent::caretColourId, accent);
            setColour (ScrollBar::thumbColourId, accent.withAlpha (0.5f));
            setColour (HyperlinkButton::textColourId, Pal::cyan);
            setColour (ProgressBar::foregroundColourId, accent);
            setColour (ProgressBar::backgroundColourId, Pal::glassHi);
        }

        juce::Colour accent;

        //----------------------------------------------------------------------
        void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                               float start, float end, juce::Slider& s) override
        {
            const auto c = s.findColour (juce::Slider::rotarySliderFillColourId);
            auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
            const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
            if (r < 6.0f) return;
            const auto ctr = b.getCentre();
            const bool hot = s.isMouseOverOrDragging();
            const bool enabled = s.isEnabled();
            const float th = juce::jmax (2.2f, r * 0.11f);
            const float arcR = r - th * 1.2f;
            const bool bip = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
            const float to = start + pos * (end - start);
            const float from = bip ? start + 0.5f * (end - start) : start;

            // tick ring
            const int ticks = r > 22.0f ? 21 : 11;
            for (int i = 0; i < ticks; ++i)
            {
                const float a = start + (end - start) * (float) i / (float) (ticks - 1);
                const bool lit = enabled && ((a >= juce::jmin (from, to) - 1.0e-3f) && (a <= juce::jmax (from, to) + 1.0e-3f));
                const float r0 = r - th * 0.15f, r1 = r + (i % 5 == 0 ? 1.5f : 0.2f);
                g.setColour (lit ? c.withAlpha (0.85f) : Pal::edge.brighter (0.2f));
                g.drawLine (ctr.x + r0 * std::sin (a), ctr.y - r0 * std::cos (a),
                            ctr.x + r1 * std::sin (a), ctr.y - r1 * std::cos (a), i % 5 == 0 ? 1.4f : 1.0f);
            }
            // track
            juce::Path track; track.addCentredArc (ctr.x, ctr.y, arcR, arcR, 0, start, end, true);
            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.strokePath (track, juce::PathStrokeType (th + 2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (Pal::edge);
            g.strokePath (track, juce::PathStrokeType (th, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            // value arc with bloom
            if (std::abs (to - from) > 0.002f)
            {
                juce::Path val; val.addCentredArc (ctr.x, ctr.y, arcR, arcR, 0, juce::jmin (from, to), juce::jmax (from, to), true);
                strokeGlow (g, val, enabled ? c : Pal::dim, th, hot ? 1.5f : 1.0f);
            }
            // cap: dark metal with a lit rim
            const float capR = arcR - th * 1.35f;
            if (capR > 3.0f)
            {
                auto capB = juce::Rectangle<float> (capR * 2, capR * 2).withCentre (ctr);
                g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillEllipse (capB.translated (0, 2.0f));
                g.setGradientFill (juce::ColourGradient (Pal::glassHi.brighter (hot ? 0.35f : 0.22f), ctr.x, capB.getY(),
                                                         Pal::night0, ctr.x, capB.getBottom(), false));
                g.fillEllipse (capB);
                g.setColour (c.withAlpha (hot ? 0.75f : 0.35f)); g.drawEllipse (capB.reduced (0.5f), 1.0f);
                // pointer
                juce::Path ptr;
                ptr.addRoundedRectangle (-1.5f, -capR + 2.5f, 3.0f, capR * 0.62f, 1.5f);
                const auto tf = juce::AffineTransform::rotation (to).translated (ctr.x, ctr.y);
                g.setColour (c.withAlpha (0.35f)); g.fillPath (ptr, juce::AffineTransform::scale (2.2f, 1.0f).followedBy (tf));
                g.setColour (enabled ? juce::Colours::white : Pal::dim); g.fillPath (ptr, tf);
            }
        }

        void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                               juce::Slider::SliderStyle style, juce::Slider& s) override
        {
            if (style != juce::Slider::LinearHorizontal && style != juce::Slider::LinearVertical
                && style != juce::Slider::LinearBar && style != juce::Slider::LinearBarVertical)
            {
                LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, minPos, maxPos, style, s);
                return;
            }
            const auto c = s.findColour (juce::Slider::trackColourId);
            const bool vert = style == juce::Slider::LinearVertical || style == juce::Slider::LinearBarVertical;
            auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
            const float th = 4.0f;
            juce::Point<float> a, b, p;
            if (vert) { a = { r.getCentreX(), r.getBottom() - 4 }; b = { r.getCentreX(), r.getY() + 4 }; p = { r.getCentreX(), pos }; }
            else      { a = { r.getX() + 4, r.getCentreY() };      b = { r.getRight() - 4, r.getCentreY() }; p = { pos, r.getCentreY() }; }
            juce::Path track; track.startNewSubPath (a); track.lineTo (b);
            g.setColour (juce::Colours::black.withAlpha (0.5f));
            g.strokePath (track, juce::PathStrokeType (th + 2, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (Pal::edge);
            g.strokePath (track, juce::PathStrokeType (th, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            juce::Path val; val.startNewSubPath (a); val.lineTo (p);
            strokeGlow (g, val, c, th, s.isMouseOverOrDragging() ? 1.5f : 1.0f);
            const float tr = 7.0f;
            g.setColour (c.withAlpha (0.3f)); g.fillEllipse (juce::Rectangle<float> (tr * 3.0f, tr * 3.0f).withCentre (p));
            g.setColour (juce::Colours::white); g.fillEllipse (juce::Rectangle<float> (tr * 1.6f, tr * 1.6f).withCentre (p));
            g.setColour (c); g.drawEllipse (juce::Rectangle<float> (tr * 1.6f, tr * 1.6f).withCentre (p), 1.5f);
        }

        //----------------------------------------------------------------------
        void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bgc, bool over, bool down) override
        {
            auto r = b.getLocalBounds().toFloat().reduced (1.5f);
            const float rad = juce::jmin (7.0f, r.getHeight() * 0.35f);
            const bool on = b.getToggleState();
            const auto onCol = b.findColour (juce::TextButton::buttonOnColourId);
            if (on)
            {
                g.setColour (onCol.withAlpha (0.25f)); g.fillRoundedRectangle (r.expanded (2.0f), rad + 2.0f);
                g.setGradientFill (juce::ColourGradient (onCol.brighter (0.25f), 0, r.getY(), onCol.darker (0.25f), 0, r.getBottom(), false));
                g.fillRoundedRectangle (r, rad);
            }
            else
            {
                auto base = bgc.isTransparent() ? Pal::glassHi : bgc;
                g.setGradientFill (juce::ColourGradient (base.brighter (over ? 0.18f : 0.08f), 0, r.getY(),
                                                         base.darker (0.35f), 0, r.getBottom(), false));
                g.fillRoundedRectangle (r, rad);
            }
            if (down) { g.setColour (juce::Colours::white.withAlpha (0.12f)); g.fillRoundedRectangle (r, rad); }
            const auto edgeCol = on ? onCol.brighter (0.4f) : (over ? accent : Pal::edge.brighter (0.15f));
            g.setColour (edgeCol.withAlpha (on || over ? 0.95f : 0.8f)); g.drawRoundedRectangle (r, rad, 1.2f);
            if (over && ! on) { g.setColour (accent.withAlpha (0.18f)); g.drawRoundedRectangle (r.expanded (1.5f), rad + 1.5f, 2.0f); }
        }

        juce::Font getTextButtonFont (juce::TextButton&, int h) override
        {
            juce::Font f (juce::FontOptions (h < 22 ? 11.0f : 12.5f, juce::Font::bold)); f.setExtraKerningFactor (0.08f); return f;
        }

        void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool) override
        {
            auto r = b.getLocalBounds().toFloat();
            const float s = juce::jmin (16.0f, r.getHeight() - 4.0f);
            auto box = juce::Rectangle<float> (s * 1.7f, s).withY (r.getCentreY() - s * 0.5f).withX (r.getX() + 2);
            const bool on = b.getToggleState();
            const auto c = b.findColour (juce::ToggleButton::tickColourId);
            g.setColour (on ? c.withAlpha (0.35f) : Pal::glassHi); g.fillRoundedRectangle (box, s * 0.5f);
            g.setColour (on ? c : (over ? accent.withAlpha (0.7f) : Pal::edge.brighter (0.2f))); g.drawRoundedRectangle (box, s * 0.5f, 1.2f);
            auto knob = juce::Rectangle<float> (s - 4, s - 4).withCentre ({ on ? box.getRight() - s * 0.5f : box.getX() + s * 0.5f, box.getCentreY() });
            if (on) { g.setColour (c.withAlpha (0.35f)); g.fillEllipse (knob.expanded (3)); }
            g.setColour (on ? juce::Colours::white : Pal::dim); g.fillEllipse (knob);
            g.setColour (b.findColour (juce::ToggleButton::textColourId).withMultipliedAlpha (b.isEnabled() ? 1.0f : 0.5f));
            g.setFont (juce::FontOptions (juce::jmin (13.0f, r.getHeight() * 0.62f)));
            g.drawFittedText (b.getButtonText(), r.withTrimmedLeft (box.getRight() + 6).toNearestInt(), juce::Justification::centredLeft, 2);
        }

        //----------------------------------------------------------------------
        void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
        {
            auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.75f);
            const bool over = box.isMouseOver (true);
            g.setGradientFill (juce::ColourGradient (box.findColour (juce::ComboBox::backgroundColourId).brighter (0.08f), 0, 0,
                                                     Pal::night0, 0, (float) h, false));
            g.fillRoundedRectangle (r, 6.0f);
            g.setColour (over ? accent : Pal::edge.brighter (0.2f)); g.drawRoundedRectangle (r, 6.0f, 1.1f);
            juce::Path a; const float ax = (float) w - 12.0f, ay = (float) h * 0.5f;
            a.startNewSubPath (ax - 4, ay - 2); a.lineTo (ax, ay + 2.5f); a.lineTo (ax + 4, ay - 2);
            g.setColour (box.findColour (juce::ComboBox::arrowColourId));
            g.strokePath (a, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        void positionComboBoxText (juce::ComboBox& box, juce::Label& l) override
        {
            l.setBounds (6, 0, box.getWidth() - 24, box.getHeight());
            l.setFont (getComboBoxFont (box));
        }
        juce::Font getComboBoxFont (juce::ComboBox& b) override { return juce::Font (juce::FontOptions (b.getHeight() < 22 ? 12.0f : 13.5f)); }

        void drawPopupMenuBackground (juce::Graphics& g, int w, int h) override
        {
            g.fillAll (Pal::glass);
            g.setColour (accent.withAlpha (0.6f)); g.drawRect (0, 0, w, h, 1);
        }
        juce::Font getPopupMenuFont() override { return juce::Font (juce::FontOptions (14.0f)); }

        void drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h) override
        {
            auto r = juce::Rectangle<float> ((float) w, (float) h);
            g.setColour (Pal::glass); g.fillRoundedRectangle (r, 5.0f);
            g.setColour (accent.withAlpha (0.8f)); g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
            juce::AttributedString s; s.setJustification (juce::Justification::centredLeft);
            s.append (text, juce::Font (juce::FontOptions (13.0f)), Pal::text);
            juce::TextLayout tl; tl.createLayoutWithBalancedLineLengths (s, (float) w - 16.0f);
            tl.draw (g, r.reduced (8.0f, 4.0f));
        }

        juce::Label* createSliderTextBox (juce::Slider& s) override
        {
            auto* l = LookAndFeel_V4::createSliderTextBox (s);
            l->setFont (juce::FontOptions (10.5f, juce::Font::bold));
            l->setColour (juce::Label::textColourId, Pal::text.withAlpha (0.9f));
            l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
            l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
            return l;
        }

        void drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int w, int h, bool vertical,
                            int thumbStart, int thumbSize, bool over, bool) override
        {
            auto t = vertical ? juce::Rectangle<float> ((float) x + 2, (float) thumbStart, (float) w - 4, (float) thumbSize)
                              : juce::Rectangle<float> ((float) thumbStart, (float) y + 2, (float) thumbSize, (float) h - 4);
            g.setColour (accent.withAlpha (over ? 0.7f : 0.4f)); g.fillRoundedRectangle (t, 3.0f);
        }
    };
}
