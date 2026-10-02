// MFR-Lightning - Copyright (C) 2026 Music For Robots
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
using FloatParam = juce::AudioParameterFloat;
using Attrs = juce::AudioParameterFloatAttributes;

std::unique_ptr<FloatParam> tenScale (const juce::String& id, const juce::String& name, float def)
{
    return std::make_unique<FloatParam> (juce::ParameterID { id, 1 }, name, juce::NormalisableRange<float> (0.0f, 10.0f), def,
                                         Attrs().withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1); }));
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout LightningProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const light::Params d;

    layout.add (std::make_unique<FloatParam> (juce::ParameterID { "gate", 1 }, "Gate",
                                              juce::NormalisableRange<float> (-90.0f, -30.0f), d.gateDb,
                                              Attrs().withStringFromValueFunction ([] (float v, int)
                                              { return v <= -89.5f ? juce::String ("Off") : juce::String (v, 0) + " dB"; })));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "odon", 1 }, "Overdrive On", d.odOn));
    layout.add (tenScale ("oddrive", "Overdrive Drive", d.odDrive * 10.0f));
    layout.add (tenScale ("odtone", "Overdrive Tone", d.odTone * 10.0f));
    layout.add (tenScale ("odlevel", "Overdrive Level", d.odLevel * 10.0f));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "ampon", 1 }, "Amp + Cab On", d.ampOn));
    layout.add (tenScale ("preamp", "Preamp", d.preamp));
    layout.add (tenScale ("master", "Master", d.master));
    layout.add (tenScale ("bass", "Bass", d.bass));
    layout.add (tenScale ("middle", "Middle", d.middle));
    layout.add (tenScale ("treble", "Treble", d.treble));
    layout.add (tenScale ("presence", "Presence", d.presence));
    layout.add (tenScale ("reverb", "Reverb", d.reverb * 10.0f));
    layout.add (std::make_unique<FloatParam> (juce::ParameterID { "output", 1 }, "Output",
                                              juce::NormalisableRange<float> (-36.0f, 12.0f), d.outputDb,
                                              Attrs().withStringFromValueFunction ([] (float v, int)
                                              { return (v > 0.05f ? "+" : "") + juce::String (v, 1) + " dB"; })));
    return layout;
}

LightningProcessor::LightningProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    pGate = get ("gate"); pOdOn = get ("odon"); pOdDrive = get ("oddrive"); pOdTone = get ("odtone");
    pOdLevel = get ("odlevel"); pAmpOn = get ("ampon"); pPreamp = get ("preamp"); pMaster = get ("master");
    pBass = get ("bass"); pMiddle = get ("middle"); pTreble = get ("treble"); pPresence = get ("presence");
    pReverb = get ("reverb"); pOutput = get ("output");
}

bool LightningProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    const bool inOk = in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
    return inOk && out == juce::AudioChannelSet::stereo();
}

void LightningProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    work.setSize (2, samplesPerBlock);
    oversampling.reset();
    oversampling.initProcessing ((size_t) samplesPerBlock);
    chain.prepare (sampleRate * (1 << kOversampleStages));
    setLatencySamples (juce::roundToInt (oversampling.getLatencyInSamples()));
}

void LightningProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels();
    if (work.getNumSamples() < n)
        work.setSize (2, n, false, false, true);

    work.clear();
    for (int ch = 0; ch < numIn; ++ch)
        work.addFrom (0, 0, buffer, ch, 0, n, 1.0f / (float) numIn);

    light::Params p;
    p.gateDb = pGate->load();
    p.odOn = pOdOn->load() > 0.5f;
    p.odDrive = pOdDrive->load() / 10.0f; p.odTone = pOdTone->load() / 10.0f; p.odLevel = pOdLevel->load() / 10.0f;
    p.ampOn = pAmpOn->load() > 0.5f;
    p.preamp = pPreamp->load(); p.master = pMaster->load();
    p.bass = pBass->load(); p.middle = pMiddle->load(); p.treble = pTreble->load(); p.presence = pPresence->load();
    p.reverb = pReverb->load() / 10.0f;
    p.outputDb = pOutput->load();
    chain.setParams (p);

    juce::dsp::AudioBlock<float> block (work.getArrayOfWritePointers(), 2, (size_t) n);
    auto up = oversampling.processSamplesUp (block);
    float* l = up.getChannelPointer (0);
    float* r = up.getChannelPointer (1);
    for (size_t i = 0; i < up.getNumSamples(); ++i)
        chain.process (l[i], l[i], r[i]);
    oversampling.processSamplesDown (block);

    buffer.copyFrom (0, 0, work, 0, 0, n);
    buffer.copyFrom (1, 0, work, 1, 0, n);
    gateMeter = chain.getGate();
}

void LightningProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void LightningProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* LightningProcessor::createEditor() { return new LightningEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new LightningProcessor(); }
