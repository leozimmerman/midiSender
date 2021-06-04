//
//  MidiSenderEditor.h
//  MidiSender
//
//  Created by Leo on 04/06/2021.
//  Copyright © 2021 Oleo Lab. All rights reserved.
//

#pragma once

typedef juce::AudioProcessorValueTreeState::SliderAttachment SliderAttachment;

namespace IDs
{
static juce::String oscPort  { "oscPort" };
static juce::String oscPortName  { "Osc Port" };

static juce::Identifier oscData     { "OSC" };
static juce::Identifier hostAddress { "host" };
static juce::Identifier mainId      { "main" };
}

enum {
    timecodeHeight = 26,
    midiKeyboardHeight = 70,
    oscSectionHeight = 35,
    portSliderWidth = 100,
    maindIdLabelWidth = 100,
    hostLabelWidth = 200,
    vertMargin = 30
};

class SpinLockedPosInfo {
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

class TrackInfoProvider {
public:
    virtual ~TrackInfoProvider() = default;
    virtual SpinLockedPosInfo* getLastPosInfo () = 0;
    virtual juce::AudioPluginInstance::TrackProperties getTrackProperties () = 0;
};


class MidiSenderEditor : public AudioProcessorEditor,
                        private Timer,
                        private Value::Listener,
                        public juce::Label::Listener
{
public:
    MidiSenderEditor (juce::AudioProcessor& processor, juce::AudioProcessorValueTreeState& vts, MidiKeyboardState& ks)
                    : AudioProcessorEditor (processor),
                     midiKeyboard         (ks, MidiKeyboardComponent::horizontalKeyboard),
                     valueTreeState(vts)
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
        setResizable (true, processor.wrapperType != juce::AudioPluginInstance::wrapperType_AudioUnitv3);

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
        if (trackInfoProvider == NULL) return;
        updateTimecodeDisplay (trackInfoProvider->getLastPosInfo()->get());
    }

    void hostMIDIControllerIsAvailable (bool controllerIsAvailable) override {
        midiKeyboard.setVisible (! controllerIsAvailable);
    }

    void updateTrackProperties() {
        if (trackInfoProvider == NULL) return;
        auto trackColour = trackInfoProvider->getTrackProperties().colour;
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
    
    void addTrackInfoProvider(TrackInfoProvider* provider) {
        trackInfoProvider = provider;
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
    TrackInfoProvider* trackInfoProvider;
    
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
