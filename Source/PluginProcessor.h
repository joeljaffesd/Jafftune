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
    juce::AudioProcessorValueTreeState treeState {*this, nullptr, "Parameters", createParameterLayout()};
        
private:
    //declare functions
    void fillDelayBuffer (juce::AudioBuffer<float>& buffer, int channel);
    
    void updateBufferPositions (juce::AudioBuffer<float>& buffer, juce::AudioBuffer<float>& delayBuffer);
    
    float scale(float x, float inMin, float inMax, float outMin, float outMax) {
        // Perform linear mapping based on specified input and output ranges
        float scaledValue = ((x - inMin) / (inMax - inMin)) * (outMax - outMin) + outMin;
        
        return scaledValue;
    }
    
    float msToSamps(float valueInMs) {
        return valueInMs * (getSampleRate() / 1000);
    }
    
    float dbtoa(float valueIndB) {
        return std::pow(10.0, valueIndB / 20.0);
    }
    
    //onePole from https://www.musicdsp.org/en/latest/Filters/257-1-pole-lpf-for-smooth-parameter-changes.html
    float onePole (float inputSample, float pastSample, float smoothingTimeInMs, int sampleRate) {
        const int twoPi = 2 * juce::MathConstants<float>::pi;
        float a = exp( -twoPi / (smoothingTimeInMs * 0.001f * sampleRate));
        float b = 1.0f - a;
        float outputSample = (inputSample * b) + (pastSample * a);
        return outputSample;
    };
    
    //initialize buffers
    //juce::AudioBuffer<float> phasorBuffer;
    juce::AudioBuffer<float> delayLine;
    juce::AudioBuffer<float> wetBuffer;
    
    //initialzie mDelayLine
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> mDelayLineOne { static_cast<int>(getSampleRate())};
    
    //initialzie mDelayLineCopy
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> mDelayLineTwo { static_cast<int>(getSampleRate())};
    
    //initialzie oscillator gains
    juce::dsp::Gain<float> phasorGain;
    juce::dsp::Gain<float> sinOscGain;
   
    //declare global variables
    const float pi = {juce::MathConstants<float>::pi};
    int writePosition = { 0 };
    float delayTime = { 0.0f };
    float delayWindow = { 22.0f };
    float pitchRatio = { 1.0f };
    float globalPhasorTap = { 0.0f };

    //sawtooth oscillator -> replicates "phasor~"
    juce::dsp::Oscillator<float> phasor { [](float x) { return ((x / juce::MathConstants<float>::pi) + 1.0f) / 2.0f; }};
    
    //initialize sinOsc (for testing pitchshift)
    juce::dsp::Oscillator<float> sinOsc { [](float x) { return std::sin (x); }};
    
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JafftuneAudioProcessor)
};
