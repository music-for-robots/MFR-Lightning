// MFR-Lightning - Copyright (C) 2026 Music For Robots
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

namespace colours
{
const juce::Colour bg      { 0xff0b1633 };   // night sky
const juce::Colour panel   { 0xff12244d };
const juce::Colour edge    { 0xff2d4f9a };
const juce::Colour ink     { 0xffeaf2ff };
const juce::Colour dimInk  { 0xff93a9d6 };
const juce::Colour light   { 0xfff4f8ff };
const juce::Colour trackOff{ 0xff1d3366 };
const juce::Colour rust    { 0xff5cc8ff };   // electric blue accent
const juce::Colour pedal   { 0xff2f7d46 };   // green overdrive box
} // namespace colours

class LightningLookAndFeel : public juce::LookAndFeel_V4
{
public:
    LightningLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
};

class FineSlider : public juce::Slider
{
public:
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
private:
    bool fineDrag = false;
    juce::Point<float> lastPos;
};

class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& title);
    void resized() override;
private:
    FineSlider slider;
    juce::Label label;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};

class LightningEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit LightningEditor (LightningProcessor&);
    ~LightningEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    Knob& addKnob (std::vector<std::unique_ptr<Knob>>&, const char* id, const char* title);
    void resetToDefaults();

    LightningProcessor& proc;
    LightningLookAndFeel lnf;

    std::vector<std::unique_ptr<Knob>> gateKnobs, odKnobs, ampKnobs, eqKnobs, verbKnobs, levelKnobs;
    juce::ToggleButton odToggle { "ON" }, ampToggle { "AMP + CAB" };
    juce::AudioProcessorValueTreeState::ButtonAttachment odAttachment, ampAttachment;
    juce::TextButton initButton { "INIT" };
    juce::Rectangle<int> gatePanel, odPanel, ampPanel, eqPanel, verbPanel, levelPanel, gateLed;
    float shownGate = -1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LightningEditor)
};
