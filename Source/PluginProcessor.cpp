/*
  ==============================================================================
    King Master - PluginProcessor.cpp
    (c)(p) 2026 Tully EDM Vibe
  ==============================================================================
*/
#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout KingMasterProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    kmaster::addParameters (l);
    return l;
}

KingMasterProcessor::KingMasterProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    reader.attach (apvts);
    loadPreset (0);
}

bool KingMasterProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    if (in != out) return false;
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void KingMasterProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    chain.prepare (sampleRate, juce::jmax (1, samplesPerBlock));
    setLatencySamples (chain.getLatency());
}

void KingMasterProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals nd;
    midi.clear();
    const int n = buffer.getNumSamples(), nch = buffer.getNumChannels();
    if (n == 0 || nch == 0) return;
    float* L = buffer.getWritePointer (0);
    float* R = nch > 1 ? buffer.getWritePointer (1) : nullptr;
    juce::HeapBlock<float> tmp;
    if (R == nullptr) { tmp.allocate ((size_t) n, true); R = tmp.get(); std::copy (L, L + n, R); }
    chain.process (L, R, n, reader.read());
    if (nch == 1) for (int i = 0; i < n; ++i) L[i] = 0.5f * (L[i] + R[i]);
    for (int c = 2; c < nch; ++c) buffer.clear (c, 0, n);
    scope.push (L, nch > 1 ? R : L, n);
}

void KingMasterProcessor::loadPreset (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) kmaster::presets().size())) return;
    tsyn::applyPreset (apvts, kmaster::presets()[(size_t) index], { "compare" });
    currentPreset = index;
}

void KingMasterProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto st = apvts.copyState();
    st.setProperty ("preset", currentPreset, nullptr);
    st.removeChild (st.getChildWithName ("LEARNED"), nullptr);
    st.appendChild (chain.getLearned().toTree(), nullptr);
    if (auto xml = st.createXml()) copyXmlToBinary (*xml, dest);
}

void KingMasterProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto st = juce::ValueTree::fromXml (*xml);
            currentPreset = juce::jlimit (0, getNumPrograms() - 1, (int) st.getProperty ("preset", 0));
            kmaster::Learned l; l.fromTree (st.getChildWithName ("LEARNED"));
            st.removeChild (st.getChildWithName ("LEARNED"), nullptr);
            apvts.replaceState (st);
            if (l.valid) chain.restoreLearned (l);
        }
}

juce::AudioProcessorEditor* KingMasterProcessor::createEditor() { return new KingMasterEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new KingMasterProcessor(); }
