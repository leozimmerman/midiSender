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

/*******************************************************************************
 The block below describes the properties of this PIP. A PIP is a short snippet
 of code that can be read by the Projucer and used to generate a JUCE project.

 BEGIN_JUCE_PIP_METADATA

 name:                  MidiSender
 version:               1.0.0
 vendor:                Oleo Lab
 website:               http://juce.com
 description:          Midi OSC Sender

 dependencies:          juce_audio_basics, juce_audio_devices, juce_audio_formats,
                        juce_audio_plugin_client, juce_audio_processors,
                        juce_audio_utils, juce_core, juce_data_structures,
                        juce_events, juce_graphics, juce_gui_basics, juce_gui_extra, juce_osc
 exporters:             xcode_mac

 moduleFlags:           JUCE_STRICT_REFCOUNTEDPOINTER=1

 type:                  AudioProcessor
 mainClass:             OscSenderAudioProcessor

 useLocalCopy:          1

 pluginCharacteristics: pluginIsSynth, pluginWantsMidiIn, pluginProducesMidiOut,
                        pluginEditorRequiresKeys
 extraPluginFormats:    AUv3

 END_JUCE_PIP_METADATA

*******************************************************************************/

#pragma once
#include "OscManager.h"

typedef juce::AudioProcessorValueTreeState::SliderAttachment SliderAttachment;

namespace IDs
{
    static juce::String oscPort  { "oscPort" };
    static juce::String oscPortName  { "Osc Port" };

    static juce::Identifier oscData     { "OSC" };
    static juce::Identifier hostAddress { "host" };
    static juce::Identifier mainId      { "main" };
}

enum
{
    timecodeHeight = 26,
    midiKeyboardHeight = 70,
    oscSectionHeight = 35,
    portSliderWidth = 100,
    maindIdLabelWidth = 100,
    hostLabelWidth = 200,
    vertMargin = 30
};

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
        auto editor = new MidiSenderEditor (*this);
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
        if (auto xmlState = getXmlFromBinary (data, sizeInBytes))
            valueTreeState.replaceState (ValueTree::fromXml (*xmlState));
    }

    //==============================================================================
    void updateTrackProperties (const TrackProperties& properties) override
    {
        {
            const ScopedLock sl (trackPropertiesLock);
            trackProperties = properties;
        }

        MessageManager::callAsync ([this]
        {
            if (auto* editor = dynamic_cast<MidiSenderEditor*> (getActiveEditor()))
                 editor->updateTrackProperties();
        });
    }

    TrackProperties getTrackProperties() const
    {
        const ScopedLock sl (trackPropertiesLock);
        return trackProperties;
    }

    class SpinLockedPosInfo
    {
    public:
        SpinLockedPosInfo() { info.resetToDefault(); }

        // Wait-free, but setting new info may fail if the main thread is currently
        // calling `get`. This is unlikely to matter in practice because
        // we'll be calling `set` much more frequently than `get`.
        void set (const AudioPlayHead::CurrentPositionInfo& newInfo)
        {
            const juce::SpinLock::ScopedTryLockType lock (mutex);

            if (lock.isLocked())
                info = newInfo;
        }

        AudioPlayHead::CurrentPositionInfo get() const noexcept
        {
            const juce::SpinLock::ScopedLockType lock (mutex);
            return info;
        }

    private:
        juce::SpinLock mutex;
        AudioPlayHead::CurrentPositionInfo info;
    };

    //==============================================================================
    // this is kept up to date with the midi messages that arrive, and the UI component
    // registers with it so it can represent the incoming messages
    MidiKeyboardState keyboardState;
    // this keeps a copy of the last set of time info that was acquired during an audio
    // callback - the UI component will read this and display it.
    SpinLockedPosInfo lastPosInfo;
    AudioProcessorValueTreeState valueTreeState;
    OscManager oscManager;

private:
    //==============================================================================
    template <typename FloatType>
    void process (AudioBuffer<FloatType>& buffer, MidiBuffer& midiMessages)
    {
        auto numSamples = buffer.getNumSamples();

        for (auto i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
            buffer.clear (i, 0, numSamples);

        keyboardState.processNextMidiBuffer (midiMessages, 0, numSamples, true);

        updateCurrentTimeInfoFromHost();
    }

    CriticalSection trackPropertiesLock;
    TrackProperties trackProperties;

    void updateCurrentTimeInfoFromHost()
    {
        const auto newInfo = [&]
        {
            if (auto* ph = getPlayHead())
            {
                AudioPlayHead::CurrentPositionInfo result;

                if (ph->getCurrentPosition (result))
                    return result;
            }

            // If the host fails to provide the current time, we'll just use default values
            AudioPlayHead::CurrentPositionInfo result;
            result.resetToDefault();
            return result;
        }();

        lastPosInfo.set (newInfo);
    }

    static BusesProperties getBusesProperties()
    {
        return BusesProperties().withInput  ("Input",  AudioChannelSet::stereo(), true)
                                .withOutput ("Output", AudioChannelSet::stereo(), true);
    }
    
    /** This is the editor component that our filter will display. */
    class MidiSenderEditor : public AudioProcessorEditor,
                            private Timer,
                            private Value::Listener,
                            public juce::Label::Listener
    {
    public:
        MidiSenderEditor (OscSenderAudioProcessor& owner)
            : AudioProcessorEditor (owner),
              midiKeyboard         (owner.keyboardState, MidiKeyboardComponent::horizontalKeyboard),
              valueTreeState(owner.valueTreeState)
        {
          
            addAndMakeVisible (midiKeyboard);

            addAndMakeVisible (timecodeDisplayLabel);
            timecodeDisplayLabel.setFont (Font (Font::getDefaultMonospacedFontName(), 15.0f, Font::plain));

            addAndMakeVisible (hostLabel);
            hostLabel.setFont (juce::Font (20.0, juce::Font::bold));
            hostLabel.setComponentID("hostLabel");
            hostLabel.setEditable(true);
            
            hostLabel.setColour (juce::Label::textColourId, juce::Colours::lightgreen);
            hostLabel.setJustificationType (juce::Justification::centredRight);
            hostLabel.addListener(this);
            
            addAndMakeVisible (mainIDLabel);
            mainIDLabel.setComponentID("mainIDLabel");
            mainIDLabel.setFont (juce::Font (20.0, juce::Font::bold));
            mainIDLabel.setEditable(true);
            
            mainIDLabel.setColour (juce::Label::textColourId, juce::Colours::lightblue);
            mainIDLabel.setJustificationType (juce::Justification::centredRight);
            mainIDLabel.addListener(this);
            
            addAndMakeVisible (portSlider);
            portSlider.setSliderStyle(juce::Slider::IncDecButtons);
            portAttachment.reset (new SliderAttachment (valueTreeState, IDs::oscPort, portSlider));
            
            updateOscLabelsTexts(false);
            
            setResizeLimits (400,
                             timecodeHeight + midiKeyboardHeight + oscSectionHeight + vertMargin,
                             1024,
                             700);
            setResizable (true, owner.wrapperType != wrapperType_AudioUnitv3);

            lastUIWidth .referTo (valueTreeState.state.getChildWithName ("uiState").getPropertyAsValue ("width",  nullptr));
            lastUIHeight.referTo (valueTreeState.state.getChildWithName ("uiState").getPropertyAsValue ("height", nullptr));
           
            setSize (lastUIWidth.getValue(), lastUIHeight.getValue());

            lastUIWidth. addListener (this);
            lastUIHeight.addListener (this);

            updateTrackProperties();

            // start a timer which will keep our timecode display updated
            startTimerHz (30);
        }

        ~MidiSenderEditor() override {}

        //==============================================================================
        void paint (Graphics& g) override {
            g.setColour (backgroundColour);
            g.fillAll();
        }

        void resized() override {
            auto r = getLocalBounds(); //.reduced (8);
            
            timecodeDisplayLabel.setBounds (r.removeFromTop (timecodeHeight));
            midiKeyboard.setBounds (r.removeFromTop (midiKeyboardHeight + timecodeHeight));
            
            int spacing = 10;
            int yPos = getHeight() - oscSectionHeight;
            mainIDLabel.setBounds (spacing,
                                   yPos,
                                   maindIdLabelWidth,
                                   oscSectionHeight);
            portSlider.setBounds(getWidth() - portSliderWidth - spacing,
                                 yPos,
                                 portSliderWidth,
                                 oscSectionHeight);
            hostLabel.setBounds (getWidth() - portSliderWidth - hostLabelWidth - spacing*2,
                                 yPos,
                                 hostLabelWidth,
                                 oscSectionHeight);

            lastUIWidth  = getWidth();
            lastUIHeight = getHeight();
        }

        void timerCallback() override {
            updateTimecodeDisplay (getProcessor().lastPosInfo.get());
        }

        void hostMIDIControllerIsAvailable (bool controllerIsAvailable) override {
            midiKeyboard.setVisible (! controllerIsAvailable);
        }

        void updateTrackProperties() {
            auto trackColour = getProcessor().getTrackProperties().colour;
            auto& lf = getLookAndFeel();

            backgroundColour = (trackColour == Colour() ? lf.findColour (ResizableWindow::backgroundColourId)
                                                        : trackColour.withAlpha (1.0f).withBrightness (0.266f));
            repaint();
        }
        
        void labelTextChanged (juce::Label* labelThatHasChanged) override {
            if (labelThatHasChanged->getComponentID() == "hostLabel") {
                setOscIPAdress(labelThatHasChanged->getText());
            } else if (labelThatHasChanged->getComponentID() == "mainIDLabel") {
                setOscMainID(labelThatHasChanged->getText());
            }
        }
        
        void addOscListener(OscHostListener* listener) {
            oscListener = listener;
        }
        
        void updateOscLabelsTexts(bool sendNotification) {
            juce::String hostAddress = DEFAULT_OSC_HOST;
            getLastHostAddress(hostAddress);
            
            juce::String mainId = DEFAULT_OSC_MAIN_ID;
            getLastMainId(mainId);

            auto doSend = sendNotification ? juce::sendNotification : juce::dontSendNotification;
            mainIDLabel.setText (mainId, doSend);
            hostLabel.setText (hostAddress, doSend);
        }

    private:
        MidiKeyboardComponent midiKeyboard;
        juce::AudioProcessorValueTreeState& valueTreeState;
        
        Label timecodeDisplayLabel;
        Colour backgroundColour;
        Value lastUIWidth, lastUIHeight;
        
        juce::Label hostLabel;
        juce::Label mainIDLabel;
        juce::Slider portSlider;
        std::unique_ptr<SliderAttachment> portAttachment;
        
        OscHostListener* oscListener;
        
        OscSenderAudioProcessor& getProcessor() const
        {
            return static_cast<OscSenderAudioProcessor&> (processor);
        }
        
        bool getLastHostAddress(juce::String& address) {
            auto oscNode = valueTreeState.state.getOrCreateChildWithName (IDs::oscData, nullptr);
            if (oscNode.hasProperty (IDs::hostAddress) == false)
                return false;

            address  = oscNode.getProperty (IDs::hostAddress);
            return true;
        }
        
        bool getLastMainId(juce::String& identifier) {
            auto oscNode = valueTreeState.state.getOrCreateChildWithName (IDs::oscData, nullptr);
            if (oscNode.hasProperty (IDs::mainId) == false)
                return false;

            identifier  = oscNode.getProperty (IDs::mainId);
            return true;
        }
        
        void setLastMainId(juce::String mainId) {
            auto oscNode = valueTreeState.state.getOrCreateChildWithName (IDs::oscData, nullptr);
            oscNode.setProperty (IDs::mainId,  mainId,  nullptr);
        }
        
        void setLastHostAddress(juce::String address) {
            auto oscNode = valueTreeState.state.getOrCreateChildWithName (IDs::oscData, nullptr);
            oscNode.setProperty (IDs::hostAddress,  address,  nullptr);
        }
        
        void setOscIPAdress(const juce::String address) {
            if (oscListener != nullptr) {
                oscListener->oscHostHasChanged(address);
                setLastHostAddress(address);
            }
        }

        void setOscMainID(const juce::String mainID) {
            if (oscListener != nullptr) {
                oscListener->oscMainIDHasChanged(mainID);
                setLastMainId(mainID);
            }
        }
        
        // quick-and-dirty function to format a timecode string
        static String timeToTimecodeString (double seconds) {
            auto millisecs = roundToInt (seconds * 1000.0);
            auto absMillisecs = std::abs (millisecs);

            return String::formatted ("%02d:%02d:%02d.%03d",
                                      millisecs / 3600000,
                                      (absMillisecs / 60000) % 60,
                                      (absMillisecs / 1000)  % 60,
                                      absMillisecs % 1000);
        }

        // quick-and-dirty function to format a bars/beats string
        static String quarterNotePositionToBarsBeatsString (double quarterNotes, int numerator, int denominator) {
            if (numerator == 0 || denominator == 0)
                return "1|1|000";

            auto quarterNotesPerBar = (numerator * 4 / denominator);
            auto beats  = (fmod (quarterNotes, quarterNotesPerBar) / quarterNotesPerBar) * numerator;

            auto bar    = ((int) quarterNotes) / quarterNotesPerBar + 1;
            auto beat   = ((int) beats) + 1;
            auto ticks  = ((int) (fmod (beats, 1.0) * 960.0 + 0.5));

            return String::formatted ("%d|%d|%03d", bar, beat, ticks);
        }

        // Updates the text in our position label.
        void updateTimecodeDisplay (AudioPlayHead::CurrentPositionInfo pos) {
            MemoryOutputStream displayText;

            displayText << String (pos.bpm, 2) << " bpm, "
            << pos.timeSigNumerator << '/' << pos.timeSigDenominator
            << "  -  " << timeToTimecodeString (pos.timeInSeconds)
            << "  -  " << quarterNotePositionToBarsBeatsString (pos.ppqPosition,
                                                                pos.timeSigNumerator,
                                                                pos.timeSigDenominator);

            if (pos.isRecording)
                displayText << "  (recording)";
            else if (pos.isPlaying)
                displayText << "  (playing)";

            timecodeDisplayLabel.setText (displayText.toString(), dontSendNotification);
        }

        // called when the stored window size changes
        void valueChanged (Value&) override {
            setSize (lastUIWidth.getValue(), lastUIHeight.getValue());
        }
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OscSenderAudioProcessor)
};
