/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
*/
class JafftuneAudioProcessor  : public juce::AudioProcessor
                            #if JucePlugin_Enable_ARA
                             , public juce::AudioProcessorARAExtension
                            #endif
{
public:
    //==============================================================================
    JafftuneAudioProcessor();
    ~JafftuneAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    
    static juce::AudioProcessorValueTreeState::ParameterLayout
    createParameterLayout();
    juce::AudioProcessorValueTreeState apvts {*this, nullptr, "Parameters", createParameterLayout()};
        
private:
    //declare functions
    void fillDelayBuffer (juce::AudioBuffer<float>& buffer, int channel);
    void readFromDelayBuffer (juce::AudioBuffer<float>& buffer, juce::AudioBuffer<float>& delayBuffer, int channel, float delayTime);
    void updateBufferPositions (juce::AudioBuffer<float>& buffer, juce::AudioBuffer<float>& delayBuffer);
    
    /*
    //dbtoa <- unnecessary?
    float dbtoa(float db) {
      return pow(10.0, db / 20.0);
    }
    */
    
    //scale
    float scale(float x, float inMin, float inMax, float outMin, float outMax) {
        // Perform linear mapping based on specified input and output ranges
        float scaledValue = ((x - inMin) / (inMax - inMin)) * (outMax - outMin) + outMin;
        
        return scaledValue;
    }
    
    /*
    Dry:
    scale (100 - rawParameterValue, 0.0f, 100.0f, 0.0f, 1.0f)
     
    Wet:
     scale (100 - rawParameterValue, 0.0f, 100.0f, 0.0f, 1.0f)
    */
    
    //declare buffers
    juce::AudioBuffer<float> delayBuffer;
    juce::AudioBuffer<float> wetBuffer;
    //juce::AudioBuffer<float> phasorBuffer; // <- if writing phasor~ to a buffer
    
    //declare global variables
    int writePosition { 0 };
    float phasorOutput = { 0.0f };
    float delayTime = { 0.0f }; // <- in milliseconds
    float delayWindow = { 22.0f }; // <- in milliseconds
    
    
    //sawtooth oscillator -> replicates "phasor~"
    juce::dsp::Oscillator<float> phasor { [](float x) { return ((x / juce::MathConstants<float>::pi) + 1.0f) / 2.0f; }};
    
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JafftuneAudioProcessor)
};
