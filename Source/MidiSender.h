/*
  ==============================================================================

   This file is part of the JUCE examples.
   Copyright (c) 2020 - Raw Material Software Limited

   The code included in this file is provided under the terms of the ISC license
   http://www.isc.org/downloads/software-support-policy/isc-license. Permission
   To use, copy, modify, and/or distribute this software for any purpose with or
   without fee is hereby granted provided that the above copyright notice and
   this permission notice appear in all copies.

   THE SOFTWARE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES,
   WHETHER EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR
   PURPOSE, ARE DISCLAIMED.

  ==============================================================================
*/

#pragma once

#include "OscManager.h"
#include "MidiSenderEditor.h"

class OscSenderAudioProcessor  : public AudioProcessor,
                                 public OscHostListener,
                                 private juce::AudioProcessorValueTreeState::Listener
{
public:
    OscSenderAudioProcessor()
    : AudioProcessor (getBusesProperties()),
    valueTreeState (*this, nullptr, "state", {
        std::make_unique<juce::AudioParameterInt> (IDs::oscPort,
                                                   IDs::oscPortName,
                                                   MIN_OSC_PORT,
                                                   MAX_OSC_PORT,
                                                   DEFAULT_OSC_PORT)
        
    })
    {
        valueTreeState.state.addChild ({ "uiState", { { "width",  400 }, { "height", timecodeHeight + midiKeyboardHeight + oscSectionHeight + vertMargin } }, {} }, -1, nullptr);
        valueTreeState.addParameterListener(IDs::oscPort, this);
    }

    ~OscSenderAudioProcessor() override = default;

    //==============================================================================
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override
    {
        // Only mono/stereo and input/output must have same layout
        const auto& mainOutput = layouts.getMainOutputChannelSet();
        const auto& mainInput  = layouts.getMainInputChannelSet();

        // input and output layout must either be the same or the input must be disabled altogether
        if (! mainInput.isDisabled() && mainInput != mainOutput)
            return false;

        // do not allow disabling the main buses
        if (mainOutput.isDisabled())
            return false;

        // only allow stereo and mono
        if (mainOutput.size() > 2)
            return false;

        return true;
    }
    
    void parameterChanged (const juce::String& param, float value) override {
        if (param == IDs::oscPort) {
            oscPortHasChanged(value);
        }
    }
    
    void oscMainIDHasChanged (juce::String newOscMainID) override {
        oscManager.setMaindId(newOscMainID);
    }

    void oscHostHasChanged (juce::String newOscHostAdress) override {
        oscManager.setOscHost(newOscHostAdress);
    }

    void oscPortHasChanged(int newOscPort) {
        oscManager.setOscPort(newOscPort);
    }

    void prepareToPlay (double newSampleRate, int /*samplesPerBlock*/) override {
        keyboardState.reset();
        reset();
    }

    void releaseResources() override {
        keyboardState.reset();
    }

    void reset() override {}

    //==============================================================================
    void processBlock (AudioBuffer<float>& buffer, MidiBuffer& midiMessages) override
    {
        jassert (! isUsingDoublePrecision());
        process (buffer, midiMessages);
    }

    void processBlock (AudioBuffer<double>& buffer, MidiBuffer& midiMessages) override
    {
        jassert (isUsingDoublePrecision());
        process (buffer, midiMessages);
    }

    //==============================================================================
    bool hasEditor() const override                                   { return true; }

    AudioProcessorEditor* createEditor() override
    {
        editor = new MidiSenderEditor (*this, valueTreeState, keyboardState);
        editor->addOscListener(this);
        return editor;
    }

    //==============================================================================
    const String getName() const override                             { return "MidiSender"; }
    bool acceptsMidi() const override                                 { return true; }
    bool producesMidi() const override                                { return true; }
    double getTailLengthSeconds() const override                      { return 0.0; }

    //==============================================================================
    int getNumPrograms() override                                     { return 0; }
    int getCurrentProgram() override                                  { return 0; }
    void setCurrentProgram (int) override                             {}
    const String getProgramName (int) override                        { return {}; }
    void changeProgramName (int, const String&) override              {}

    //==============================================================================
    void getStateInformation (MemoryBlock& destData) override
    {
        // Store an xml representation of our state.
        if (auto xmlState = valueTreeState.copyState().createXml())
            copyXmlToBinary (*xmlState, destData);
    }

    void setStateInformation (const void* data, int sizeInBytes) override
    {
        // Restore our plug-in's state from the xml representation stored in the above
        // method.
        if (auto xmlState = getXmlFromBinary (data, sizeInBytes)) {
            valueTreeState.replaceState (ValueTree::fromXml (*xmlState));
            editor->updateOscLabelsTexts(true);
        }
            
    }
    
    //==============================================================================
    // this is kept up to date with the midi messages that arrive, and the UI component
    // registers with it so it can represent the incoming messages
    MidiKeyboardState keyboardState;
    AudioProcessorValueTreeState valueTreeState;
    OscManager oscManager;

private:
    MidiSenderEditor* editor;

    template <typename FloatType>
    void process (AudioBuffer<FloatType>& buffer, MidiBuffer& midiMessages) {
        auto numSamples = buffer.getNumSamples();
        for (auto i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
            buffer.clear (i, 0, numSamples);
        keyboardState.processNextMidiBuffer (midiMessages, 0, numSamples, true);
        
        // SEND OSC
        for (const auto metadata : midiMessages) {
            auto message = metadata.getMessage();
            
            const auto timeStamp = metadata.samplePosition;
            int channel = message.getChannel();
            int number = message.getNoteNumber();
            float velocityFloat = message.getFloatVelocity();
            bool isNoteOn = message.isNoteOn();
            
            oscManager.sendNoteBundle(number, velocityFloat, channel, isNoteOn, timeStamp);
        }
    }

    static BusesProperties getBusesProperties()
    {
        return BusesProperties().withInput  ("Input",  AudioChannelSet::stereo(), true)
                                .withOutput ("Output", AudioChannelSet::stereo(), true);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OscSenderAudioProcessor)
};
