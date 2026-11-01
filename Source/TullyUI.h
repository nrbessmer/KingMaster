/*
  ==============================================================================
    Tully EDM Vibe - TullyUI.h
    (c)(p) 2026 Tully EDM Vibe

    Shared look: colours, knobs, combo boxes, preset bar, scope, the header with
    the Tully EDM Vibe logo + info@tullyedmvibe.com footer, and the full synth
    panel used by Mister Leads and the SOUND page of Mister Arp.
  ==============================================================================
*/
#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "SynthParams.h"

namespace tui
{
    namespace Col
    {
        const juce::Colour bg      { 0xff0a0c10 };
        const juce::Colour panel   { 0xff13171d };
        const juce::Colour panelHi { 0xff1b2129 };
        const juce::Colour line    { 0xff2a323d };
        const juce::Colour text    { 0xffd9e1ea };
        const juce::Colour dim     { 0xff7d8896 };
        const juce::Colour cyan    { 0xff2de2e6 };
        const juce::Colour pink    { 0xffff2d8a };
        const juce::Colour amber   { 0xffffb627 };
        const juce::Colour violet  { 0xff9b6bff };
        const juce::Colour green   { 0xff4be38a };
    }

    using APVTS = juce::AudioProcessorValueTreeState;

    //==========================================================================
    class LnF : public juce::LookAndFeel_V4
    {
    public:
        LnF()
        {
            setColour (juce::ComboBox::backgroundColourId, Col::panelHi);
            setColour (juce::ComboBox::outlineColourId, Col::line);
            setColour (juce::ComboBox::textColourId, Col::text);
            setColour (juce::PopupMenu::backgroundColourId, Col::panelHi);
            setColour (juce::PopupMenu::textColourId, Col::text);
            setColour (juce::PopupMenu::highlightedBackgroundColourId, Col::pink.withAlpha (0.35f));
            setColour (juce::PopupMenu::headerTextColourId, Col::cyan);
            setColour (juce::TextButton::buttonColourId, Col::panelHi);
            setColour (juce::TextButton::buttonOnColourId, Col::pink);
            setColour (juce::TextButton::textColourOffId, Col::text);
            setColour (juce::TextButton::textColourOnId, Col::bg);
            setColour (juce::TooltipWindow::backgroundColourId, Col::panelHi);
            setColour (juce::TooltipWindow::textColourId, Col::text);
            setColour (juce::TooltipWindow::outlineColourId, Col::cyan.withAlpha (0.6f));
            setColour (juce::Label::textColourId, Col::text);
        }

        void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s) override
        {
            const auto c = s.findColour (juce::Slider::rotarySliderFillColourId);
            auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
            const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
            const auto ctr = b.getCentre();
            const float th = juce::jmax (2.5f, r * 0.14f);
            const bool hot = s.isMouseOverOrDragging();
            const bool bip = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
            juce::Path track; track.addCentredArc (ctr.x, ctr.y, r - th, r - th, 0, start, end, true);
            g.setColour (Col::line); g.strokePath (track, juce::PathStrokeType (th, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            const float from = bip ? start + 0.5f * (end - start) : start;
            const float to = start + pos * (end - start);
            juce::Path val; val.addCentredArc (ctr.x, ctr.y, r - th, r - th, 0, juce::jmin (from, to), juce::jmax (from, to), true);
            if (hot) { g.setColour (c.withAlpha (0.25f)); g.strokePath (val, juce::PathStrokeType (th * 2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)); }
            g.setColour (c); g.strokePath (val, juce::PathStrokeType (th, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            const float in = r - th * 2.3f;
            g.setColour (hot ? Col::panelHi.brighter (0.08f) : Col::panelHi); g.fillEllipse (ctr.x - in, ctr.y - in, in * 2, in * 2);
            g.setColour (Col::line); g.drawEllipse (ctr.x - in, ctr.y - in, in * 2, in * 2, 1.0f);
            juce::Path ptr; ptr.addRoundedRectangle (-1.4f, -in + 2.5f, 2.8f, in * 0.55f, 1.4f);
            g.setColour (Col::text); g.fillPath (ptr, juce::AffineTransform::rotation (to).translated (ctr.x, ctr.y));
        }

        void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bgc, bool over, bool down) override
        {
            auto r = b.getLocalBounds().toFloat().reduced (1.0f);
            auto fill = b.getToggleState() ? b.findColour (juce::TextButton::buttonOnColourId) : bgc;
            if (down) fill = fill.brighter (0.2f); else if (over) fill = fill.brighter (0.1f);
            g.setColour (fill); g.fillRoundedRectangle (r, 5.0f);
            g.setColour (over ? Col::cyan.withAlpha (0.7f) : Col::line); g.drawRoundedRectangle (r, 5.0f, 1.0f);
        }

        void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
        {
            auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
            g.setColour (box.findColour (juce::ComboBox::backgroundColourId)); g.fillRoundedRectangle (r, 5.0f);
            g.setColour (box.isMouseOver (true) ? Col::cyan.withAlpha (0.7f) : Col::line); g.drawRoundedRectangle (r, 5.0f, 1.0f);
            juce::Path a; const float ax = (float) w - 11.0f, ay = (float) h * 0.5f;
            a.addTriangle (ax - 3.5f, ay - 2, ax + 3.5f, ay - 2, ax, ay + 2.5f);
            g.setColour (Col::dim); g.fillPath (a);
        }
        void positionComboBoxText (juce::ComboBox& box, juce::Label& l) override
        {
            l.setBounds (4, 0, box.getWidth() - 20, box.getHeight());
            l.setFont (getComboBoxFont (box));
        }
        juce::Font getComboBoxFont (juce::ComboBox& b) override { return juce::Font (juce::FontOptions (b.getHeight() < 22 ? 12.0f : 13.5f)); }
        juce::Font getTextButtonFont (juce::TextButton&, int h) override { return juce::Font (juce::FontOptions (h < 22 ? 11.0f : 12.5f, juce::Font::bold)); }
        juce::Font getPopupMenuFont() override { return juce::Font (juce::FontOptions (14.0f)); }
        juce::Label* createSliderTextBox (juce::Slider& s) override
        {
            auto* l = LookAndFeel_V4::createSliderTextBox (s);
            l->setFont (juce::FontOptions (10.5f));
            l->setColour (juce::Label::textColourId, Col::text);
            l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
            l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
            return l;
        }
    };

    //==========================================================================
    class Knob : public juce::Component
    {
    public:
        Knob (APVTS& s, const juce::String& id, const juce::String& cap, juce::Colour c, const juce::String& tip)
            : caption (cap)
        {
            slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 13);
            slider.setColour (juce::Slider::rotarySliderFillColourId, c);
            slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
            slider.setMouseDragSensitivity (170);
            slider.setTooltip (tip + "  (double-click: default, shift-drag: fine)");
            auto* prm = s.getParameter (id);
            jassert (prm != nullptr);
            slider.setDoubleClickReturnValue (true, (double) s.getParameterRange (id).convertFrom0to1 (prm->getDefaultValue()));
            slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
            slider.setColour (juce::Slider::textBoxTextColourId, Col::text);
            addAndMakeVisible (slider);
            att = std::make_unique<APVTS::SliderAttachment> (s, id, slider);
        }
        void resized() override { auto b = getLocalBounds(); b.removeFromTop (13); slider.setBounds (b); }
        void paint (juce::Graphics& g) override
        {
            g.setColour (slider.isMouseOverOrDragging() ? Col::text : Col::dim);
            g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
            g.drawFittedText (caption, getLocalBounds().removeFromTop (13), juce::Justification::centred, 1, 0.8f);
        }
        juce::Slider slider;
    private:
        juce::String caption;
        std::unique_ptr<APVTS::SliderAttachment> att;
    };

    class Choice : public juce::ComboBox
    {
    public:
        Choice (APVTS& s, const juce::String& id, const juce::String& tip)
        {
            if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (s.getParameter (id))) addItemList (c->choices, 1);
            setTooltip (tip);
            att = std::make_unique<APVTS::ComboBoxAttachment> (s, id, *this);
        }
    private:
        std::unique_ptr<APVTS::ComboBoxAttachment> att;
    };

    class Toggle : public juce::TextButton
    {
    public:
        Toggle (APVTS& s, const juce::String& id, const juce::String& text, juce::Colour on, const juce::String& tip)
            : juce::TextButton (text)
        {
            setClickingTogglesState (true);
            setColour (juce::TextButton::buttonOnColourId, on);
            setTooltip (tip);
            att = std::make_unique<APVTS::ButtonAttachment> (s, id, *this);
        }
    private:
        std::unique_ptr<APVTS::ButtonAttachment> att;
    };

    //==========================================================================
    inline void paintSection (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& t, juce::Colour c)
    {
        g.setColour (Col::panel); g.fillRoundedRectangle (r.toFloat(), 7.0f);
        g.setColour (Col::line); g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 7.0f, 1.0f);
        g.setColour (c); g.fillRoundedRectangle ((float) r.getX() + 10, (float) r.getY() + 9, 4, 12, 2);
        g.setColour (Col::text); g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText (t, r.getX() + 19, r.getY() + 6, 160, 18, juce::Justification::left);
    }

    // Header: logo, product name, TULLY EDM VIBE, subtitle + version. Footer: (c) + email.
    inline void paintHeader (juce::Graphics& g, const juce::Image& logo, const juce::String& title, const juce::String& subtitle, juce::Colour accent)
    {
        if (logo.isValid()) g.drawImage (logo, juce::Rectangle<float> (12, 8, 58, 54), juce::RectanglePlacement::centred);
        g.setColour (Col::text); g.setFont (juce::FontOptions (25.0f, juce::Font::bold));
        g.drawText (title, 80, 10, 300, 30, juce::Justification::left);
        g.setColour (accent); g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText ("TULLY EDM VIBE", 82, 40, 110, 14, juce::Justification::left);
        g.setColour (Col::dim); g.setFont (juce::FontOptions (11.0f));
        g.drawText (subtitle + "   v" + juce::String (JucePlugin_VersionString), 186, 40, 330, 14, juce::Justification::left);
    }
    inline void paintFooter (juce::Graphics& g, juce::Rectangle<int> area)
    {
        g.setColour (Col::dim.withAlpha (0.75f)); g.setFont (juce::FontOptions (10.5f));
        g.drawText ("(c)(p) 2026 Tully EDM Vibe  |  info@tullyedmvibe.com  |  www.tullyedmvibe.com", area, juce::Justification::centredRight);
    }

    //==========================================================================
    // Bass Professor style branding (VocalPunch, King Master):
    // product logo as a faint full-window watermark + header thumbnail, and the
    // Tully EDM Vibe logo next to the (c) line with clickable email + website.
    inline void paintWatermark (juce::Graphics& g, const juce::Image& img, juce::Rectangle<int> area, float alpha)
    {
        if (! img.isValid()) return;
        g.setOpacity (alpha);
        g.drawImage (img, area.toFloat(), juce::RectanglePlacement::centred | juce::RectanglePlacement::fillDestination);
        g.setOpacity (1.0f);
    }

    inline void paintBrandHeader (juce::Graphics& g, int width, int h, const juce::Image& productLogo, const juce::Image& tullyLogo,
                                  const juce::String& title, const juce::String& subtitle, juce::Colour accent)
    {
        g.setColour (Col::panel.withAlpha (0.93f)); g.fillRect (0, 0, width, h);
        g.setColour (accent); g.fillRect (0, h - 2, width, 2);
        const int ls = h - 10;
        if (productLogo.isValid())
            g.drawImageWithin (productLogo, 6, 5, ls, ls, juce::RectanglePlacement::centred, false);
        const int tx = productLogo.isValid() ? ls + 14 : 12;
        g.setColour (accent); g.setFont (juce::FontOptions (25.0f, juce::Font::bold));
        g.drawText (title, tx, 8, 300, 32, juce::Justification::bottomLeft);
        g.setColour (Col::dim); g.setFont (juce::FontOptions (10.5f));
        g.drawText (subtitle + "   v" + juce::String (JucePlugin_VersionString), tx + 1, 42, 300, 16, juce::Justification::left);
        g.setColour (Col::text.withAlpha (0.85f)); g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
        g.drawText ("TULLY EDM VIBE", tx + 1, 56, 200, 12, juce::Justification::left);
        // right: Tully logo + (c)
        const int bw = 200, lh = h - 12;
        if (tullyLogo.isValid())
            g.drawImageWithin (tullyLogo, width - 10 - bw - 6 - lh, 6, lh, lh, juce::RectanglePlacement::centred, false);
        g.setColour (Col::dim); g.setFont (juce::FontOptions (11.0f));
        g.drawText (juce::String::fromUTF8 ("\xc2\xa9\xe2\x84\x97 2026 Tully EDM Vibe"), width - 10 - bw, 8, bw, 16, juce::Justification::right);
    }

    // email + website links, placed under the (c) line by the editor
    class BrandLinks : public juce::Component
    {
    public:
        BrandLinks()
        {
            for (auto* b : { &email, &site })
            {
                b->setFont (juce::FontOptions (11.5f), false, juce::Justification::centredRight);
                b->setColour (juce::HyperlinkButton::textColourId, Col::cyan);
                addAndMakeVisible (b);
            }
            email.setTooltip ("Questions or feedback? Email us.");
            site.setTooltip ("More plugins, manuals and updates.");
        }
        void resized() override { auto b = getLocalBounds(); email.setBounds (b.removeFromTop (b.getHeight() / 2)); site.setBounds (b); }
    private:
        juce::HyperlinkButton email { "info@tullyedmvibe.com", juce::URL ("mailto:info@tullyedmvibe.com") };
        juce::HyperlinkButton site { "www.tullyedmvibe.com", juce::URL ("https://www.tullyedmvibe.com") };
    };

    // translucent section so the watermark shows through (like Bass Professor)
    inline void paintGlassSection (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& t, juce::Colour c)
    {
        auto rf = r.toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.55f)); g.fillRoundedRectangle (rf, 7.0f);
        auto title = rf.withHeight (24.0f);
        g.setGradientFill (juce::ColourGradient (c.withAlpha (0.20f), title.getX(), title.getY(), juce::Colours::transparentBlack, title.getX(), title.getBottom(), false));
        g.fillRoundedRectangle (title, 7.0f);
        g.setColour (Col::line.withAlpha (0.8f)); g.drawRoundedRectangle (rf.reduced (0.5f), 7.0f, 1.0f);
        g.setColour (c); g.fillRoundedRectangle (rf.getX() + 10, rf.getY() + 7, 3.5f, 12, 1.75f);
        g.setColour (Col::text); g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText (t, r.getX() + 19, r.getY() + 4, r.getWidth() - 24, 18, juce::Justification::left);
    }

    // vertical level meter (dB) with peak value text
    class LevelMeter : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        LevelMeter (juce::String cap, juce::Colour c, float lo = -48.0f, float hi = 0.0f, bool fromTop = false)
            : caption (std::move (cap)), colour (c), minDb (lo), maxDb (hi), reduction (fromTop) {}
        void setValue (float db) { if (std::abs (db - value) > 0.05f) { value = db; repaint(); } }
        void paint (juce::Graphics& g) override
        {
            auto b = getLocalBounds().toFloat();
            auto cap = b.removeFromBottom (14);
            auto txt = b.removeFromBottom (14);
            auto bar = b.reduced (b.getWidth() * 0.28f, 2);
            g.setColour (juce::Colour (0xff0d1014)); g.fillRoundedRectangle (bar, 3.0f);
            const float v = juce::jlimit (0.0f, 1.0f, (value - minDb) / (maxDb - minDb));
            juce::Rectangle<float> fill;
            if (reduction) fill = bar.withHeight (bar.getHeight() * (1.0f - v));        // gain reduction grows down from the top
            else           fill = bar.withTrimmedTop (bar.getHeight() * (1.0f - v));
            if (! reduction)
                g.setGradientFill (juce::ColourGradient (Col::pink, bar.getX(), bar.getY(), colour, bar.getX(), bar.getBottom(), false));
            else g.setColour (colour);
            g.fillRoundedRectangle (fill, 3.0f);
            g.setColour (Col::line); g.drawRoundedRectangle (bar, 3.0f, 1.0f);
            g.setColour (Col::text); g.setFont (juce::FontOptions (10.0f));
            g.drawText (value <= minDb + 0.1f ? juce::String ("-inf") : juce::String (value, 1), txt, juce::Justification::centred);
            g.setColour (Col::dim); g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
            g.drawText (caption, cap, juce::Justification::centred);
        }
    private:
        juce::String caption; juce::Colour colour; float minDb, maxDb, value = -100.0f; bool reduction;
    };

    //==========================================================================
    // Preset bar: < [combo] >  + one-line description
    class PresetBar : public juce::Component
    {
    public:
        std::function<int()> getCurrent;
        std::function<void (int)> load;

        PresetBar (const std::vector<tsyn::Preset>& list) : presets (list)
        {
            juce::String last; int id = 1;
            for (auto& p : presets)
            {
                if (last != p.category) { last = p.category; box.addSectionHeading (last); }
                box.addItem (p.name, id++);
            }
            box.onChange = [this] { const int i = box.getSelectedId() - 1; if (i >= 0 && load) { load (i); tip.setText (presets[(size_t) i].tip, juce::dontSendNotification); } };
            box.setTooltip ("Presets. Everything stays adjustable after loading one.");
            prev.setTooltip ("Previous preset"); next.setTooltip ("Next preset");
            prev.onClick = [this] { step (-1); }; next.onClick = [this] { step (1); };
            tip.setFont (juce::FontOptions (11.5f)); tip.setColour (juce::Label::textColourId, Col::dim);
            for (auto* c : std::initializer_list<juce::Component*> { &prev, &box, &next, &tip }) addAndMakeVisible (c);
        }
        void sync()
        {
            if (! getCurrent) return;
            const int c = getCurrent();
            if (box.getSelectedId() != c + 1)
            {
                box.setSelectedId (c + 1, juce::dontSendNotification);
                if (juce::isPositiveAndBelow (c, (int) presets.size())) tip.setText (presets[(size_t) c].tip, juce::dontSendNotification);
            }
        }
        void resized() override
        {
            auto b = getLocalBounds();
            auto row = b.removeFromTop (26);
            prev.setBounds (row.removeFromLeft (26)); row.removeFromLeft (4);
            next.setBounds (row.removeFromRight (26)); row.removeFromRight (4);
            box.setBounds (row);
            tip.setBounds (b.withTrimmedTop (2));
        }
        void step (int d)
        {
            const int n = (int) presets.size();
            const int c = getCurrent ? getCurrent() : 0;
            box.setSelectedId ((c + d + n) % n + 1, juce::sendNotificationSync);
        }
        juce::ComboBox box;
    private:
        const std::vector<tsyn::Preset>& presets;
        juce::TextButton prev { "<" }, next { ">" };
        juce::Label tip;
    };

    //==========================================================================
    // Lock-free scope buffer written by the audio thread
    struct ScopeBuffer
    {
        static constexpr int kSize = 2048;
        std::array<float, kSize> data {};
        std::atomic<int> writePos { 0 };
        void push (const float* L, const float* R, int n) noexcept
        {
            int w = writePos.load (std::memory_order_relaxed);
            for (int i = 0; i < n; ++i) { data[(size_t) w] = 0.5f * (L[i] + R[i]); w = (w + 1) & (kSize - 1); }
            writePos.store (w, std::memory_order_release);
        }
    };

    class Scope : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        explicit Scope (ScopeBuffer& b, juce::Colour c) : buf (b), colour (c) { setTooltip ("Output waveform"); }
        void refresh() { repaint(); }
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            g.setColour (juce::Colour (0xff0d1014)); g.fillRoundedRectangle (r, 6.0f);
            g.setColour (Col::line.withAlpha (0.6f)); g.drawHorizontalLine ((int) r.getCentreY(), r.getX(), r.getRight());
            // trigger on a rising zero crossing so the wave stands still
            const int w = buf.writePos.load (std::memory_order_acquire);
            const int N = 512;
            int start = (w - N - 600 + ScopeBuffer::kSize) & (ScopeBuffer::kSize - 1);
            for (int k = 0; k < 600; ++k)
            {
                const int i0 = (start + k) & (ScopeBuffer::kSize - 1), i1 = (i0 + 1) & (ScopeBuffer::kSize - 1);
                if (buf.data[(size_t) i0] <= 0.0f && buf.data[(size_t) i1] > 0.0f) { start = i1; break; }
            }
            float peak = 0.05f;
            for (int k = 0; k < N; ++k) peak = juce::jmax (peak, std::abs (buf.data[(size_t) ((start + k) & (ScopeBuffer::kSize - 1))]));
            juce::Path p;
            for (int k = 0; k < N; ++k)
            {
                const float v = buf.data[(size_t) ((start + k) & (ScopeBuffer::kSize - 1))] / peak;
                const float x = r.getX() + r.getWidth() * (float) k / (float) (N - 1);
                const float y = r.getCentreY() - v * r.getHeight() * 0.42f;
                if (k == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            g.setColour (colour.withAlpha (0.25f)); g.strokePath (p, juce::PathStrokeType (3.0f));
            g.setColour (colour); g.strokePath (p, juce::PathStrokeType (1.3f));
            g.setColour (Col::line); g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
        }
    private:
        ScopeBuffer& buf;
        juce::Colour colour;
    };

    //==========================================================================
    // Every synth control, in three rows of sections.
    class SynthPanel : public juce::Component
    {
    public:
        explicit SynthPanel (APVTS& s)
            : wave1 (s, "osc1_wave", "Oscillator 1 waveform. SuperSaw = 7 detuned saws (trance/EDM), FM = bell/metallic, Fold = wavefolded sine."),
              wave2 (s, "osc2_wave", "Oscillator 2 waveform."),
              fmRatio (s, "fm_ratio", "FM modulator ratio (for FM waveforms). 2:1 = bright, 3:1 = hollow, 7:1 = bell."),
              subWave (s, "sub_wave", "Sub oscillator shape (one octave below)."),
              fltType (s, "flt_type", "Filter: LP 24 = classic fat ladder, LP 12 = softer, Band = thin/nasal, High = removes lows."),
              lfoShape (s, "lfo_shape", "LFO waveform. S&H = random steps."),
              lfoSync (s, "lfo_sync", "Free = use the RATE knob. A note value locks the LFO to the song tempo."),
              mode (s, "voice_mode", "Poly = chords. Mono = one note, retriggers. Legato = one note, glides between overlapping notes without retriggering (classic lead)."),
              driveType (s, "drive_type", "Distortion after the voices: Soft (warm), Hard (aggressive), Fold (metallic), Crush (bit-crusher)."),
              delayTime (s, "delay_time", "Delay time as a note value at the song tempo (D = dotted, T = triplet).")
        {
            auto k = [&s, this] (const char* id, const char* cap, juce::Colour c, const char* tip)
            { knobs.push_back (std::make_unique<Knob> (s, id, cap, c, tip)); addAndMakeVisible (*knobs.back()); return knobs.back().get(); };

            for (int o = 1; o <= 2; ++o)
            {
                const juce::String p = "osc" + juce::String (o) + "_";
                const auto c = o == 1 ? Col::pink : Col::violet;
                auto& row = o == 1 ? osc1 : osc2;
                row.push_back (k ((p + "oct").toRawUTF8(), "OCT", c, "Octave up/down."));
                row.push_back (k ((p + "semi").toRawUTF8(), "SEMI", c, "Transpose in semitones (+7 = a fifth, +12 = an octave)."));
                row.push_back (k ((p + "fine").toRawUTF8(), "FINE", c, "Fine tune in cents. A few cents against the other oscillator = thick and wide."));
                row.push_back (k ((p + "shape").toRawUTF8(), "SHAPE", c, "Pulse: width. SuperSaw: side saws vs centre. FM: brightness (index). Fold: fold amount."));
                row.push_back (k ((p + "unison").toRawUTF8(), "UNISON", c, "Stacked copies per note (1-7). SuperSaw always uses 7."));
                row.push_back (k ((p + "detune").toRawUTF8(), "DETUNE", c, "How far the unison copies are spread in pitch."));
                row.push_back (k ((p + "level").toRawUTF8(), "LEVEL", c, "Oscillator volume."));
            }
            extra.push_back (k ("sub_level", "SUB", Col::cyan, "Sub oscillator one octave down: weight under the lead."));
            extra.push_back (k ("noise", "NOISE", Col::cyan, "White noise: breath and air."));
            extra.push_back (k ("spread", "SPREAD", Col::cyan, "Stereo width of the unison copies."));

            filter.push_back (k ("cutoff", "CUTOFF", Col::amber, "Filter cutoff frequency."));
            filter.push_back (k ("reso", "RESO", Col::amber, "Resonance: emphasis at the cutoff. High = squelchy/acid."));
            filter.push_back (k ("flt_env", "ENV AMT", Col::amber, "How much the filter envelope opens (+) or closes (-) the filter."));
            filter.push_back (k ("flt_key", "KEY", Col::amber, "Key tracking: higher notes open the filter more."));
            filter.push_back (k ("flt_drive", "DRIVE", Col::amber, "Overdrives the filter input (warm to gritty)."));
            fenv.push_back (k ("flt_a", "A", Col::amber, "Filter envelope attack."));
            fenv.push_back (k ("flt_d", "D", Col::amber, "Filter envelope decay. Short = plucky, long = sweeping."));
            fenv.push_back (k ("flt_s", "S", Col::amber, "Filter envelope sustain level."));
            fenv.push_back (k ("flt_r", "R", Col::amber, "Filter envelope release."));
            aenv.push_back (k ("amp_a", "A", Col::green, "Volume attack. Short = punchy, long = swelling."));
            aenv.push_back (k ("amp_d", "D", Col::green, "Volume decay."));
            aenv.push_back (k ("amp_s", "S", Col::green, "Volume sustain level while the key is held."));
            aenv.push_back (k ("amp_r", "R", Col::green, "Volume release after the key is let go."));

            lfo.push_back (k ("lfo_rate", "RATE", Col::cyan, "LFO speed (when SYNC is Free)."));
            lfo.push_back (k ("lfo_pitch", "VIBRATO", Col::cyan, "LFO to pitch (up to a semitone). The mod wheel adds vibrato too."));
            lfo.push_back (k ("lfo_cutoff", "> FILTER", Col::cyan, "LFO to filter cutoff: wobble and sweeps."));
            lfo.push_back (k ("lfo_amp", "> VOLUME", Col::cyan, "LFO to volume: tremolo / gating."));
            lfo.push_back (k ("lfo_shapemod", "> SHAPE", Col::cyan, "LFO to oscillator SHAPE: PWM, moving supersaws, evolving FM."));
            lfo.push_back (k ("lfo_delay", "DELAY", Col::cyan, "Time before the LFO fades in on each new note: vibrato that arrives late, like a singer."));
            voice.push_back (k ("glide", "GLIDE", Col::pink, "Portamento time between notes."));
            voice.push_back (k ("bend_range", "BEND", Col::pink, "Pitch-bend range in semitones."));
            voice.push_back (k ("vel_sens", "VELOCITY", Col::pink, "How much key velocity changes the volume."));
            fx.push_back (k ("drive", "DRIVE", Col::violet, "Distortion amount (choose the type above)."));
            fx.push_back (k ("chorus", "CHORUS", Col::violet, "Chorus: wider, shimmering."));
            fx.push_back (k ("delay", "DELAY", Col::violet, "Ping-pong delay level."));
            fx.push_back (k ("delay_fb", "FEEDBACK", Col::violet, "Delay repeats."));
            fx.push_back (k ("reverb", "REVERB", Col::violet, "Reverb level."));
            fx.push_back (k ("reverb_size", "SIZE", Col::violet, "Reverb room size."));
            fx.push_back (k ("width", "WIDTH", Col::violet, "Stereo width: 0 = mono, 100% = normal, 200% = extra wide."));
            fx.push_back (k ("volume", "VOLUME", Col::text, "Output volume."));

            for (juce::Component* c : std::initializer_list<juce::Component*> { &wave1, &wave2, &fmRatio, &subWave, &fltType, &lfoShape, &lfoSync, &mode, &driveType, &delayTime })
                addAndMakeVisible (c);
        }

        void paint (juce::Graphics& g) override
        {
            paintSection (g, rOsc1, "OSC 1", Col::pink);
            paintSection (g, rOsc2, "OSC 2", Col::violet);
            paintSection (g, rExtra, "SUB / NOISE", Col::cyan);
            paintSection (g, rFilter, "FILTER", Col::amber);
            paintSection (g, rFenv, "FILTER ENV", Col::amber);
            paintSection (g, rAenv, "AMP ENV", Col::green);
            paintSection (g, rLfo, "LFO", Col::cyan);
            paintSection (g, rVoice, "VOICE", Col::pink);
            paintSection (g, rFx, "FX", Col::violet);
        }

        void resized() override
        {
            auto b = getLocalBounds();
            const int gap = 8;
            const int rowH = (b.getHeight() - 2 * gap) / 3;
            auto r1 = b.removeFromTop (rowH); b.removeFromTop (gap);
            auto r2 = b.removeFromTop (rowH); b.removeFromTop (gap);
            auto r3 = b;

            const int W = r1.getWidth();
            rOsc1 = r1.removeFromLeft ((W - 2 * gap) * 7 / 17); r1.removeFromLeft (gap);
            rOsc2 = r1.removeFromLeft ((W - 2 * gap) * 7 / 17); r1.removeFromLeft (gap);
            rExtra = r1;
            rFilter = r2.removeFromLeft ((W - 2 * gap) * 7 / 17); r2.removeFromLeft (gap);
            rFenv = r2.removeFromLeft ((r2.getWidth() - gap) / 2); r2.removeFromLeft (gap);
            rAenv = r2;
            rLfo = r3.removeFromLeft ((W - 2 * gap) * 6 / 19); r3.removeFromLeft (gap);
            rVoice = r3.removeFromLeft ((W - 2 * gap) * 3 / 19); r3.removeFromLeft (gap);
            rFx = r3;

            auto titleCombo = [] (juce::Rectangle<int> sec, juce::ComboBox& box, int w, int fromRight = 0)
            { box.setBounds (sec.getRight() - 8 - fromRight - w, sec.getY() + 5, w, 20); };
            auto lay = [] (juce::Rectangle<int> sec, const std::vector<Knob*>& ks, int slots = 0)
            {
                auto a = sec.reduced (6, 4); a.removeFromTop (24);
                const int n = slots > 0 ? slots : (int) ks.size();
                const int w = a.getWidth() / juce::jmax (1, n);
                for (auto* k : ks) k->setBounds (a.removeFromLeft (w).withSizeKeepingCentre (juce::jmin (w, 78), a.getHeight()));
            };
            titleCombo (rOsc1, wave1, 110); lay (rOsc1, osc1);
            titleCombo (rOsc2, wave2, 110); titleCombo (rOsc2, fmRatio, 64, 116); lay (rOsc2, osc2);
            titleCombo (rExtra, subWave, 78); lay (rExtra, extra);
            titleCombo (rFilter, fltType, 82); lay (rFilter, filter);
            lay (rFenv, fenv); lay (rAenv, aenv);
            titleCombo (rLfo, lfoSync, 70); titleCombo (rLfo, lfoShape, 82, 76); lay (rLfo, lfo);
            titleCombo (rVoice, mode, 76); lay (rVoice, voice);
            titleCombo (rFx, delayTime, 64); titleCombo (rFx, driveType, 70, 70); lay (rFx, fx);
        }

    private:
        std::vector<std::unique_ptr<Knob>> knobs;
        std::vector<Knob*> osc1, osc2, extra, filter, fenv, aenv, lfo, voice, fx;
        Choice wave1, wave2, fmRatio, subWave, fltType, lfoShape, lfoSync, mode, driveType, delayTime;
        juce::Rectangle<int> rOsc1, rOsc2, rExtra, rFilter, rFenv, rAenv, rLfo, rVoice, rFx;
    };
}
