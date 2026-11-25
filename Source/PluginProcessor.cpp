/*
  ==============================================================================
    Mister Master - PluginProcessor.cpp
    (c)(p) 2026 Tully EDM Vibe
  ==============================================================================
*/
#include "PluginProcessor.h"
#include "TullyLicence.h"
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
    tully::licence ("King Master");   // purchase activation (TullyLicence.h)

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
    monoScratchBuf.assign ((size_t) juce::jmax (1, samplesPerBlock) * 2, 0.0f);
    chain.prepare (sampleRate, juce::jmax (1, samplesPerBlock));
    setLatencySamples (chain.getLatency());
}

void KingMasterProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    if (! tully::licence().ok()) return;   // not activated yet: the sound passes through untouched

    juce::ScopedNoDenormals nd;
    midi.clear();
    const int n = buffer.getNumSamples(), nch = buffer.getNumChannels();
    if (n == 0 || nch == 0) return;
    float* L = buffer.getWritePointer (0);
    float* R = nch > 1 ? buffer.getWritePointer (1) : nullptr;
    if (R == nullptr) { R = monoScratch (n); std::copy (L, L + n, R); }   // preallocated: no audio-thread malloc
    {
        double secs = -1.0, bar = -1.0, rate = 0.0; bool playing = false;
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition())
            {
                playing = pos->getIsPlaying();
                if (auto t = pos->getTimeInSeconds()) secs = *t;
                else if (auto ts = pos->getTimeInSamples()) secs = (double) *ts / getSampleRate();
                if (auto q = pos->getPpqPosition())
                {
                    const auto sig = pos->getTimeSignature().orFallback (juce::AudioPlayHead::TimeSignature { 4, 4 });
                    const double qPerBar = juce::jmax (0.25, 4.0 * sig.numerator / juce::jmax (1, sig.denominator));
                    bar = juce::jmax (0.0, *q / qPerBar); rate = pos->getBpm().orFallback (120.0) / 60.0 / qPerBar;
                }
            }
        chain.setTransport (secs, playing, bar, rate);
    }
    chain.process (L, R, n, reader.read());
    if (nch == 1) for (int i = 0; i < n; ++i) L[i] = 0.5f * (L[i] + R[i]);
    for (int c = 2; c < nch; ++c) buffer.clear (c, 0, n);
    scope.push (L, nch > 1 ? R : L, n);
}

void KingMasterProcessor::loadPreset (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) kmaster::presets().size())) return;
    // a preset never switches the streaming check off, and only the streaming presets change what it checks for
    juce::StringArray keep { "compare", "stream_check" };
    const auto& pr = kmaster::presets()[(size_t) index];
    if (! juce::String (pr.settings).contains ("platform=")) keep.add ("platform");
    tsyn::applyPreset (apvts, pr, keep);
    if (! keep.contains ("platform"))
        chain.holdUnder (kmaster::targetLufs ((int) apvts.getRawParameterValue ("loudness")->load()) + apvts.getRawParameterValue ("loud_trim")->load());
    currentPreset = index;
}

kmaster::StreamReport KingMasterProcessor::streamReport() const
{
    return kmaster::checkStreaming (apvts.getRawParameterValue ("stream_check")->load() > 0.5f,
                                    (int) apvts.getRawParameterValue ("platform")->load(), chain.streamStats());
}

void KingMasterProcessor::fixForPlatform()
{
    const auto f = kmaster::fixFor ((int) apvts.getRawParameterValue ("platform")->load());
    auto set = [this] (const char* id, float v) { auto* p = apvts.getParameter (id); p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (v)); p->endChangeGesture(); };
    set ("loudness", (float) f.loudness); set ("loud_trim", f.trim); set ("ceiling", f.ceiling);
    chain.holdUnder (kmaster::targetLufs (f.loudness) + f.trim);
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
