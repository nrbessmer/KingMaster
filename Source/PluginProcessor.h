/*
  ==============================================================================
    King Master - PluginProcessor.h
    (c)(p) 2026 Tully EDM Vibe
  ==============================================================================
*/
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "MasterChain.h"
#include "MasterPresets.h"
#include "TullyUI.h"

class KingMasterProcessor : public juce::AudioProcessor
{
public:
    KingMasterProcessor();
    ~KingMasterProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "King Master"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.1; }

    int getNumPrograms() override { return (int) kmaster::presets().size(); }
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram (int i) override { loadPreset (i); }
    const juce::String getProgramName (int i) override { return juce::isPositiveAndBelow (i, getNumPrograms()) ? kmaster::presets()[(size_t) i].name : ""; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    void loadPreset (int index);
    int  getCurrentPreset() const noexcept { return currentPreset; }

    juce::AudioProcessorValueTreeState apvts;
    kmaster::Chain chain;
    tui::ScopeBuffer scope;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    kmaster::Reader reader;
    int currentPreset = 0;

    // Scratch for a mono output bus (the engines are stereo). Sized in prepareToPlay;
    // only an oversized host block ever grows it.
    std::vector<float> monoScratchBuf;
    float* monoScratch (int n) { if ((int) monoScratchBuf.size() < n) monoScratchBuf.resize ((size_t) n); return monoScratchBuf.data(); }
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KingMasterProcessor)
};
