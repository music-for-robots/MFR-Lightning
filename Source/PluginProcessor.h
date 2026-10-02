// MFR-Lightning - Copyright (C) 2026 Music For Robots
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "LightningDSP.h"

class LightningProcessor : public juce::AudioProcessor
{
public:
    LightningProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MFR-Lightning"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.5; }   // the plate's tail

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> gateMeter { 0.0f };

private:
    static constexpr int kOversampleStages = 2;   // 4x
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::dsp::Oversampling<float> oversampling { 2, kOversampleStages,
                                                  juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true };
    light::Chain chain;
    juce::AudioBuffer<float> work;   // ch 0 carries the guitar in; L/R come out (the reverb is stereo)

    std::atomic<float>* pGate; std::atomic<float>* pOdOn; std::atomic<float>* pOdDrive; std::atomic<float>* pOdTone;
    std::atomic<float>* pOdLevel; std::atomic<float>* pAmpOn; std::atomic<float>* pPreamp; std::atomic<float>* pMaster;
    std::atomic<float>* pBass; std::atomic<float>* pMiddle; std::atomic<float>* pTreble; std::atomic<float>* pPresence;
    std::atomic<float>* pReverb; std::atomic<float>* pOutput;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LightningProcessor)
};
