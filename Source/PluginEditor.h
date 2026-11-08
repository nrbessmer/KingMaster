/*
  ==============================================================================
    King Master - PluginEditor.h
    (c)(p) 2026 Tully EDM Vibe
  ==============================================================================
*/
#pragma once
#include "PluginProcessor.h"

// Row of radio buttons bound to a choice parameter (STYLE, LOUDNESS)
class SegButtons : public juce::Component
{
public:
    SegButtons (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::StringArray& tips, juce::Colour on)
    {
        auto* p = dynamic_cast<juce::AudioParameterChoice*> (s.getParameter (id));
        jassert (p != nullptr);
        for (int i = 0; i < p->choices.size(); ++i)
        {
            auto* b = buttons.add (new juce::TextButton (p->choices[i].toUpperCase()));
            b->setClickingTogglesState (false);
            b->setColour (juce::TextButton::buttonOnColourId, on);
            b->setTooltip (tips[i]);
            b->onClick = [this, i] { att->setValueAsCompleteGesture ((float) i); };
            addAndMakeVisible (b);
        }
        att = std::make_unique<juce::ParameterAttachment> (*p, [this] (float v) { for (int i = 0; i < buttons.size(); ++i) buttons[i]->setToggleState (i == juce::roundToInt (v), juce::dontSendNotification); });
        att->sendInitialUpdate();
    }
    void resized() override
    {
        auto b = getLocalBounds(); const int w = b.getWidth() / juce::jmax (1, buttons.size());
        for (auto* x : buttons) x->setBounds (b.removeFromLeft (w).reduced (3, 0));
    }
private:
    juce::OwnedArray<juce::TextButton> buttons;
    std::unique_ptr<juce::ParameterAttachment> att;
};

// Big loudness readout: integrated / short-term / momentary LUFS, true peak, limiter
class LoudnessPanel : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit LoudnessPanel (kmaster::Chain& c) : chain (c) { setTooltip ("Loudness of the master (EBU R128 / ITU BS.1770). Click to reset the integrated reading and peak hold."); }
    std::function<float()> getTarget;
    void mouseDown (const juce::MouseEvent&) override { chain.resetMeters(); }
    void paint (juce::Graphics& g) override;
private:
    kmaster::Chain& chain;
};

class LearnPanel : public juce::Component
{
public:
    explicit LearnPanel (kmaster::Chain& c);
    void paint (juce::Graphics& g) override;
    void resized() override { auto b = getLocalBounds(); learn.setBounds (b.removeFromTop (40).reduced (6, 2)); }
    std::function<float()> getTarget;
private:
    kmaster::Chain& chain;
    juce::TextButton learn { "LEARN" };
};

class EqView : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit EqView (kmaster::Chain& c) : chain (c) { setTooltip ("Final tone curve: Smart EQ (learned from your song) + style + your EQ moves."); }
    void paint (juce::Graphics& g) override;
private:
    kmaster::Chain& chain;
};

class KingMasterEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit KingMasterEditor (KingMasterProcessor&);
    ~KingMasterEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

    KingMasterProcessor& proc;
    tui::LnF lnf;
    neon::BackdropCache backdrop;
    juce::Image logo, productLogo;
    tui::PresetBar presetBar;
    tui::BrandLinks links;
    SegButtons style, loudness;
    LearnPanel learnPanel;
    LoudnessPanel meter;
    EqView eqView;
    tui::LevelMeter gLow { "LOW", tui::Col::amber, -8.0f, 0.0f, true }, gMid { "MID", tui::Col::amber, -8.0f, 0.0f, true },
                    gHigh { "HIGH", tui::Col::amber, -8.0f, 0.0f, true }, gLim { "LIMIT", tui::Col::pink, -12.0f, 0.0f, true };
    std::vector<std::unique_ptr<tui::Knob>> knobs;
    std::vector<tui::Knob*> loudK, tone, dyn, stereo, out;
    tui::Toggle monoBass, compare;
    juce::TooltipWindow tooltips { this, 500 };
private:
    int shownStyle = -1, shownLoud = -1;
    juce::Rectangle<int> rStyle, rLoud, rLearn, rMeter, rTone, rDyn, rStereo, rOut;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KingMasterEditor)
};
