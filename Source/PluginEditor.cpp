// MFR-Lightning - Copyright (C) 2026 Music For Robots
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PluginEditor.h"

namespace
{
juce::Font font (float size, bool bold = false)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}
} // namespace

//==============================================================================
LightningLookAndFeel::LightningLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, colours::ink);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, colours::light.withAlpha (0.4f));
    setColour (juce::Slider::textBoxBackgroundColourId, colours::panel.darker (0.15f));
    setColour (juce::Label::textColourId, colours::ink);
    setColour (juce::TextEditor::textColourId, colours::ink);
    setColour (juce::TextButton::textColourOffId, colours::ink);
}

void LightningLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                        float startAngle, float endAngle, juce::Slider&)
{
    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (4.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius - 2, radius - 2, 0, startAngle, endAngle, true);
    g.setColour (colours::trackOff);
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path value;
    value.addCentredArc (centre.x, centre.y, radius - 2, radius - 2, 0, startAngle, angle, true);
    g.setColour (colours::rust);
    g.strokePath (value, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float knobR = radius - 8.0f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3434), centre.x, centre.y - knobR,
                                             juce::Colour (0xff060505), centre.x, centre.y + knobR, false));
    g.fillEllipse (juce::Rectangle<float> (knobR * 2, knobR * 2).withCentre (centre));

    juce::Path pointer;
    pointer.addRoundedRectangle (-1.5f, -knobR + 3.0f, 3.0f, knobR * 0.5f, 1.5f);
    g.setColour (colours::ink);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre));
}

void LightningLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    g.setColour (on ? colours::rust : (highlighted ? colours::panel.brighter (0.1f) : colours::panel));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (on ? colours::rust : colours::edge);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    g.setColour (on ? colours::bg : colours::dimInk);
    g.setFont (font (12.0f, true));
    g.drawText (b.getButtonText(), r, juce::Justification::centred);
}

void LightningLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (down ? colours::panel.brighter (0.2f) : highlighted ? colours::panel.brighter (0.1f) : colours::panel);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (colours::edge.brighter (0.3f));
    g.drawRoundedRectangle (r, 4.0f, 1.5f);
}

//==============================================================================
void FineSlider::mouseDown (const juce::MouseEvent& e)
{
    fineDrag = e.mods.isCtrlDown() || e.mods.isShiftDown();
    lastPos = e.position;
    Slider::mouseDown (e);
}

void FineSlider::mouseDrag (const juce::MouseEvent& e)
{
    if (! fineDrag || ! isEnabled()) { Slider::mouseDrag (e); return; }
    const float delta = (e.position.x - lastPos.x) + (lastPos.y - e.position.y);
    lastPos = e.position;
    const double pos = juce::jlimit (0.0, 1.0, valueToProportionOfLength (getValue()) + delta / 2000.0);
    setValue (proportionOfLengthToValue (pos), juce::sendNotificationSync);
}

//==============================================================================
Knob::Knob (juce::AudioProcessorValueTreeState& state, const juce::String& id, const juce::String& title)
    : attachment (state, id, slider)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 16);
    slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    if (auto* p = state.getParameter (id))
        slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
    addAndMakeVisible (slider);

    label.setText (title.toUpperCase(), juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (font (12.0f, true));
    label.setColour (juce::Label::textColourId, colours::ink);
    addAndMakeVisible (label);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (16));
    slider.setBounds (r);
}

//==============================================================================
LightningEditor::LightningEditor (LightningProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      odAttachment (p.apvts, "odon", odToggle), ampAttachment (p.apvts, "ampon", ampToggle)
{
    setLookAndFeel (&lnf);

    addKnob (gateKnobs, "gate", "Gate");
    addKnob (odKnobs, "oddrive", "Drive");
    addKnob (odKnobs, "odtone", "Tone");
    addKnob (odKnobs, "odlevel", "Level");
    addKnob (ampKnobs, "preamp", "Preamp");
    addKnob (ampKnobs, "master", "Master");
    addKnob (eqKnobs, "bass", "Bass");
    addKnob (eqKnobs, "middle", "Middle");
    addKnob (eqKnobs, "treble", "Treble");
    addKnob (eqKnobs, "presence", "Presence");
    addKnob (verbKnobs, "reverb", "Reverb");
    addKnob (levelKnobs, "output", "Output");

    odToggle.setTooltip ("Overdrive pedal in front of the amp (drive low, level high: it tightens rather than fuzzes)");
    ampToggle.setTooltip ("Switch off to use only the gate and overdrive, in front of your own amp sim");
    addAndMakeVisible (odToggle);
    addAndMakeVisible (ampToggle);

    initButton.onClick = [this] { resetToDefaults(); };
    initButton.setTooltip ("Reset every control to its default");
    addAndMakeVisible (initButton);

    setSize (860, 430);
    startTimerHz (20);
}

LightningEditor::~LightningEditor() { setLookAndFeel (nullptr); }

Knob& LightningEditor::addKnob (std::vector<std::unique_ptr<Knob>>& group, const char* id, const char* title)
{
    auto& k = *group.emplace_back (std::make_unique<Knob> (proc.apvts, id, title));
    addAndMakeVisible (k);
    return k;
}

void LightningEditor::resetToDefaults()
{
    for (auto* param : proc.getParameters())
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (param))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->getDefaultValue());
            p->endChangeGesture();
        }
}

void LightningEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (colours::bg.brighter (0.12f), 0, 0, colours::bg.darker (0.4f), 0, (float) getHeight(), false));
    g.fillAll();

    // A thin bolt behind the title.
    juce::Path bolt;
    bolt.startNewSubPath (300, 8); bolt.lineTo (284, 30); bolt.lineTo (296, 30); bolt.lineTo (280, 54);
    bolt.lineTo (306, 24); bolt.lineTo (293, 24); bolt.closeSubPath();
    g.setColour (colours::rust.withAlpha (0.85f));
    g.fillPath (bolt);

    g.setColour (colours::ink);
    g.setFont (font (32.0f, true));
    g.drawText ("MFR-LIGHTNING", 24, 10, 300, 40, juce::Justification::centredLeft);
    g.setColour (colours::dimInk);
    g.setFont (font (12.0f, true));
    g.drawText ("GATE  |  OVERDRIVE  |  BRITISH HEAD  |  4X12  |  PLATE", 320, 14, 460, 18, juce::Justification::centredLeft);
    g.setFont (font (10.0f));
    g.drawText ("v" JucePlugin_VersionString "  |  musicforrobots.com", 320, 31, 300, 14, juce::Justification::centredLeft);

    // The overdrive as a green pedal.
    {
        const auto r = odPanel.toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.3f));
        g.fillRoundedRectangle (r.translated (2, 3), 10.0f);
        g.setColour (colours::pedal);
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (colours::pedal.darker (0.5f));
        g.drawRoundedRectangle (r, 10.0f, 1.5f);
        g.setColour (colours::light);
        g.setFont (font (13.0f, true));
        g.drawText ("OVERDRIVE", odPanel.getX() + 12, odPanel.getY() + 6, 200, 16, juce::Justification::centredLeft);
        const bool on = odToggle.getToggleState();
        const auto led = juce::Rectangle<float> (12, 12).withCentre ({ r.getRight() - 20.0f, r.getY() + 15.0f });
        g.setColour (on ? juce::Colour (0xffff3b30) : juce::Colour (0xff4a2a28));
        g.fillEllipse (led);
    }

    const std::pair<juce::Rectangle<int>, const char*> panels[] = {
        { gatePanel, "GATE" }, { ampPanel, "AMP" }, { eqPanel, "EQ" }, { verbPanel, "PLATE" }, { levelPanel, "OUT" }
    };
    for (auto& [r, name] : panels)
    {
        g.setColour (colours::panel);
        g.fillRoundedRectangle (r.toFloat(), 8.0f);
        g.setColour (colours::edge);
        g.drawRoundedRectangle (r.toFloat(), 8.0f, 1.5f);
        g.setColour (colours::ink);
        g.setFont (font (13.0f, true));
        g.drawText (name, r.getX() + 12, r.getY() + 6, 200, 16, juce::Justification::centredLeft);
    }

    const float gl = juce::jlimit (0.0f, 1.0f, shownGate);
    g.setColour (colours::trackOff);
    g.fillEllipse (gateLed.toFloat());
    g.setColour (colours::rust.withAlpha (gl));
    g.fillEllipse (gateLed.toFloat().reduced (1.0f));

    g.setColour (colours::dimInk);
    g.setFont (font (10.0f));
    g.drawText ("Overdrive: drive low, level high, to tighten an already-cranked head. Double-click or Alt-click resets a knob.",
                24, getHeight() - 26, getWidth() - 48, 14, juce::Justification::centredLeft);
}

void LightningEditor::resized()
{
    const int kw = 86, kh = 108, top = 60, rowH = 160;
    auto row = [&] (std::vector<std::unique_ptr<Knob>>& knobs, juce::Rectangle<int> panel, int leftTrim = 0)
    {
        const int used = kw * (int) knobs.size();
        auto area = panel.withTrimmedTop (32).withTrimmedLeft (leftTrim + (panel.getWidth() - leftTrim - used) / 2);
        for (auto& k : knobs)
            k->setBounds (area.removeFromLeft (kw).withHeight (kh));
    };

    initButton.setBounds (getWidth() - 96, 18, 80, 26);

    gatePanel = { 16, top, kw + 30, rowH };
    odPanel   = { gatePanel.getRight() + 10, top, 3 * kw + 24 + 96, rowH };
    ampPanel  = { odPanel.getRight() + 10, top, getWidth() - odPanel.getRight() - 26, rowH };

    const int top2 = top + rowH + 10, rowH2 = 150;
    eqPanel    = { 16, top2, 4 * kw + 24, rowH2 };
    verbPanel  = { eqPanel.getRight() + 10, top2, (getWidth() - eqPanel.getRight() - 36) / 2, rowH2 };
    levelPanel = { verbPanel.getRight() + 10, top2, getWidth() - verbPanel.getRight() - 26, rowH2 };

    row (gateKnobs, gatePanel);
    gateLed = { gatePanel.getRight() - 24, gatePanel.getY() + 9, 10, 10 };

    auto odKnobArea = odPanel.withTrimmedRight (96);
    row (odKnobs, odKnobArea);
    odToggle.setBounds (odPanel.getRight() - 92, odPanel.getY() + 64, 80, 40);

    auto ampKnobArea = ampPanel.withTrimmedBottom (0);
    row (ampKnobs, ampKnobArea.withTrimmedRight (0));
    ampToggle.setBounds (ampPanel.getRight() - 124, ampPanel.getY() + 5, 112, 22);

    row (eqKnobs, eqPanel);
    row (verbKnobs, verbPanel);
    row (levelKnobs, levelPanel);
}

void LightningEditor::timerCallback()
{
    const float g = proc.gateMeter.load();
    if (std::abs (g - shownGate) > 0.02f)
    {
        shownGate = g;
        repaint (gateLed.expanded (2));
    }
}
