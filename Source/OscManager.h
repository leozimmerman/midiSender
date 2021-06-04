
#pragma once

#define DEFAULT_OSC_HOST "127.0.0.1"
#define DEFAULT_OSC_PORT 9001
#define DEFAULT_OSC_MAIN_ID "trackId"
#define MIN_OSC_PORT 1
#define MAX_OSC_PORT 65535

class OscManager {
public:
    
    OscManager() {
        _oscHost = DEFAULT_OSC_HOST;
        _oscPort = DEFAULT_OSC_PORT;
        _mainID = DEFAULT_OSC_MAIN_ID;
        _isConnected = false;
        connect();
    }
    
    void setMaindId(juce::String mainId) {
        _mainID = mainId;
    }
    
    void setOscPort(int port) {
        _oscPort = port;
        connect();
    }
    
    void setOscHost(juce::String hostAdress) {
        _oscHost = hostAdress;
        connect();
    }
    
    void connect() {
        connect(_oscHost, _oscPort);
    }
    
    void connect(const juce::String& targetHostName, int targetPortNumber) {
        _isConnected = false;
        oscSender.disconnect();
        
        _isConnected = oscSender.connect (targetHostName, targetPortNumber);
        if (! _isConnected) {
            juce::Logger::outputDebugString(&"Error: could not connect to UDP port:" [ targetPortNumber]);
        }
    }
    
    void sendValue(float value, juce::String name) {
        if (!_isConnected) return;
        juce::String root = "/" + _mainID;
        juce::String address = root + "/" + name;
        juce::OSCAddressPattern addressPattern = juce::OSCAddressPattern(address);
        oscSender.send(addressPattern, value);
    }
    
    void sendNoteBundle(int noteNumber, float velocity, int channel, bool noteOn, int timeStamp) {
        if (!_isConnected) return;
        juce::String root = "/" + _mainID;
        juce::String mainName = "midiNote";
        juce::String identifier = juce::String(noteNumber);
        juce::String mainAddress = root + "/" + mainName + "/" + identifier + "/";
        
        OSCBundle bundle = OSCBundle();
        
        juce::OSCAddressPattern noteNumberAddress = juce::OSCAddressPattern(mainAddress + "number");
        OSCMessage noteNumberMessage = OSCMessage(noteNumberAddress, noteNumber);
        bundle.addElement(noteNumberMessage);
        
        juce::OSCAddressPattern velocityAddress = juce::OSCAddressPattern(mainAddress + "velocity");
        OSCMessage velocityMessage = OSCMessage(velocityAddress, velocity);
        bundle.addElement(velocityMessage);
        
        juce::OSCAddressPattern onOffAddress = juce::OSCAddressPattern(mainAddress + "onOff");
        OSCMessage onOffMessage = OSCMessage(onOffAddress, noteOn);
        bundle.addElement(onOffMessage);
        
        juce::OSCAddressPattern channelAddress = juce::OSCAddressPattern(mainAddress + "channel");
        OSCMessage channelMessage = OSCMessage(channelAddress, channel);
        bundle.addElement(channelMessage);
        
        oscSender.send(bundle);
    }
    
private:
    juce::OSCSender oscSender;
    juce::String _oscHost;
    juce::String _mainID;
    int _oscPort;
    bool _isConnected;
};

class OscHostListener
{
public:
    virtual ~OscHostListener() = default;
    virtual void oscHostHasChanged (juce::String newOscHostAdress) = 0;
    virtual void oscMainIDHasChanged (juce::String newOscMainID) = 0;
};

