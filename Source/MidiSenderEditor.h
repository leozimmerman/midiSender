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
    timecodeHeight = 0,
    midiKeyboardHeight = 70,
    oscSectionHeight = 35,
    portSliderWidth = 100,
    maindIdLabelWidth = 100,
    hostLabelWidth = 200,
    vertMargin = 10
};

class MidiSenderEditor : public AudioProcessorEditor,
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
    }

    ~MidiSenderEditor() override {}

    //==============================================================================
    void paint (Graphics& g) override {
        g.setColour (backgroundColour);
        g.fillAll();
    }

    void resized() override {
        auto r = getLocalBounds(); //.reduced (8);
        
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

    void hostMIDIControllerIsAvailable (bool controllerIsAvailable) override {
        midiKeyboard.setVisible (! controllerIsAvailable);
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
    
    Colour backgroundColour;
    Value lastUIWidth, lastUIHeight;
    
    juce::Label hostLabel;
    juce::Label mainIDLabel;
    juce::Slider portSlider;
    std::unique_ptr<SliderAttachment> portAttachment;
    
    OscHostListener* oscListener;
    
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
    
    // called when the stored window size changes
    void valueChanged (Value&) override {
        setSize (lastUIWidth.getValue(), lastUIHeight.getValue());
    }
};
