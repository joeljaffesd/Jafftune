/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
JafftuneAudioProcessor::JafftuneAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
}

JafftuneAudioProcessor::~JafftuneAudioProcessor()
{
}

//==============================================================================
const juce::String JafftuneAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool JafftuneAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool JafftuneAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool JafftuneAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double JafftuneAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int JafftuneAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int JafftuneAudioProcessor::getCurrentProgram()
{
    return 0;
}

void JafftuneAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String JafftuneAudioProcessor::getProgramName (int index)
{
    return {};
}

void JafftuneAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void JafftuneAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    //initialize delayBufferSize
    auto delayBufferSize = sampleRate * 2.0;
    delayBuffer.setSize(getTotalNumOutputChannels(), (int)delayBufferSize);
    
    auto wetBufferSize = samplesPerBlock;
    wetBuffer.setSize(getTotalNumInputChannels(), (int)wetBufferSize);
    
    auto wetBufferCopySize = samplesPerBlock;
    wetBufferCopy.setSize(getTotalNumInputChannels(), (int)wetBufferCopySize);
    
    auto wetBufferMixSize = samplesPerBlock;
    wetBufferMix.setSize(getTotalNumInputChannels(), (int)wetBufferMixSize);
    
    /* if using phasorBuffer
    //initialize phasorBufferSize // <- if writing phasor~ to a buffer
    auto phasorBufferSize = samplesPerBlock;
    phasorBuffer.setSize(getTotalNumInputChannels(), (int)phasorBufferSize);
    */
    
    //initialzie spec
    juce::dsp::ProcessSpec spec;
    spec.maximumBlockSize = samplesPerBlock;
    spec.sampleRate = sampleRate;
    spec.numChannels = getTotalNumOutputChannels();
    
    phasor.prepare (spec); //pass spec to phasor
    phasor.setFrequency( 1.0f ); //set initial phasor frequency

}

void JafftuneAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool JafftuneAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void JafftuneAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());
    
    //set phasor~ frequency based on pitchRatio
    float pitchRatio = treeState.getRawParameterValue ("Pitch Ratio")->load();
    //delayWindow = // <- resolve to adjustable parameter
    phasor.setFrequency ( 1000.0f * ((1.0f - pitchRatio) / delayWindowOne) ); //set runtime phasor frequency
    
    /* Attempting to avoid using processSample
     
    juce::dsp::AudioBlock<float> phasorBlock { phasorBuffer };
     
    phasor.process (juce::dsp::ProcessContextReplacing<float> (phasorBlock));
    
    for (int sampleIndex = 0; sampleIndex < buffer.getNumSamples(); ++sampleIndex)
    {
        phasorOutput = phasorBuffer.getSample (0, sampleIndex);
        delayTime = delayWindow * phasorOutput;
    }
    */
    
    //Set phasorOutput equal to phasor~ output, set delayTime to delayWindow * phasorOutput
        for (int sampleIndex = 0; sampleIndex < buffer.getNumSamples(); ++sampleIndex)
        {
            phasorOutput = phasor.processSample(0.0f);
            
            delayTimeOne = delayWindowOne * phasorOutput;
            
            wetGainOne = std::cos (2 * juce::MathConstants<float>::pi * (phasorOutput - 0.5f) / 2.0f); // <- modulating wetGainOne
            
            delayTimeTwo = delayWindowOne * std::fmodf ((phasorOutput + 0.5f), 1.0f);
            
            wetGainTwo = std::cos (2 * juce::MathConstants<float>::pi * (std::fmodf ((phasorOutput + 0.5f), 1.0f) - 0.5f) / 2.0f); // <- modulating wetGainTwo
        }
    
    
    //write and read from delayBuffer
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        //write from main buffer to delay buffer
        fillDelayBuffer (buffer, channel);
    
        //read from delay buffer into wetBuffer
        readFromDelayBuffer (wetBuffer, delayBuffer, channel, delayTimeOne, wetGainOne);
        
        //read from delay buffer into wetBufferCopy
        readFromDelayBuffer (wetBufferCopy, delayBuffer, channel, delayTimeTwo, wetGainTwo);
        
        //read from wetBuffer into wetBufferMix
        wetBufferMix.copyFrom (channel, 0, wetBuffer.getReadPointer(channel), buffer.getNumSamples());
        
        //read from wetBufferCopt into wetBufferMix
        wetBufferMix.addFrom (channel, 0, wetBufferCopy.getReadPointer(channel), buffer.getNumSamples());
        
        
        //Blend control
        float blendFactor = treeState.getRawParameterValue ("Blend")->load();
        float dryGain = scale (100 - blendFactor, 0.0f, 100.0f, 0.0f, 1.0f);
        float wetGain = scale (blendFactor, 0.0f, 100.0f, 0.0f, 1.0f);
        
        //buffer passthru
        buffer.copyFromWithRamp (channel, 0, buffer.getReadPointer(channel), buffer.getNumSamples(), dryGain, dryGain);
        
        //read from wetBufferMix into main buffer
        buffer.addFromWithRamp (channel, 0, wetBufferMix.getReadPointer(channel), buffer.getNumSamples(), wetGain, wetGain);

    }
    
    updateBufferPositions (buffer, delayBuffer);
    
    /*
    //DBG printer
       static juce::Time lastDebugPrintTime = juce::Time::getCurrentTime();
           juce::Time currentTime = juce::Time::getCurrentTime();
           if (currentTime - lastDebugPrintTime >= juce::RelativeTime::milliseconds(2000))
           {
               //Blend control
               float blendFactor = apvts.getRawParameterValue ("Blend")->load();
               float dryGain = scale (100 - blendFactor, 0.0f, 100.0f, 0.0f, 1.0f);
               float wetGain = scale (blendFactor, 0.0f, 100.0f, 0.0f, 1.0f);
               
               // Print debug message
               DBG("Blend: " + juce::String(blendFactor));
               DBG("dryGain: " + juce::String(dryGain));
               DBG("wetGain: " + juce::String(wetGain));
               
               // Update the last print time
               lastDebugPrintTime = currentTime;
           }
     */
    
}
void JafftuneAudioProcessor::fillDelayBuffer (juce::AudioBuffer<float>& buffer, int channel)
{
      //initialize variables
      float gain = 1.0f;
      auto bufferSize = buffer.getNumSamples();
      auto delayBufferSize = delayBuffer.getNumSamples();
      
      //Check to see if main buffer copies to delay buffer without needing to wrap
      if (delayBufferSize > bufferSize + writePosition)
      {
          //copy main buffer to delay buffer
          delayBuffer.copyFromWithRamp(channel, writePosition, buffer.getWritePointer(channel), bufferSize, gain, gain); // 0.8 = start and end gain, why not 1.0?
      }
      //if not
      else {
          //determine how much space is left at the end of the delay buffer
          auto numSamplesToEnd = delayBufferSize - writePosition;
          
          //copy that amount of content to the end
          delayBuffer.copyFromWithRamp(channel, writePosition, buffer.getWritePointer(channel), numSamplesToEnd, gain, gain);
          
          //calculate how much content remains to be copied
          auto numSamplesAtStart = bufferSize - numSamplesToEnd;
          
          //copy remainging amount to beginning of delay buffer
          delayBuffer.copyFromWithRamp(channel, 0, buffer.getWritePointer(channel) + numSamplesToEnd, numSamplesAtStart, gain, gain);
      }
}

void JafftuneAudioProcessor::readFromDelayBuffer (juce::AudioBuffer<float>& wetBuffer, juce::AudioBuffer<float>& delayBuffer, int channel, float delayTime, float gain)
{
    //adds delay buffer data to wetBuffer <- formerly main buffer
    
    //initialize variables
    //float gain = 1.0f;
    auto wetBufferSize = wetBuffer.getNumSamples();
    auto delayBufferSize = delayBuffer.getNumSamples();
    //float delayTime = 1500.0f; // <- should variable be declared inside the function?
    
    //create parameter for delay time
    auto readPosition = writePosition - (delayTime * (getSampleRate() / 1000));
    
    if (readPosition < 0)
        readPosition += delayBufferSize;
    
    if (readPosition + wetBufferSize < delayBufferSize) {
        wetBuffer.copyFromWithRamp(channel, 0, delayBuffer.getReadPointer(channel, readPosition), wetBufferSize, gain, gain);
    }
    else {
        auto numSamplesToEnd = delayBufferSize - readPosition;
        wetBuffer.addFromWithRamp(channel, 0, delayBuffer.getReadPointer(channel, readPosition), numSamplesToEnd, gain, gain);
        auto numSamplesAtStart = wetBufferSize - numSamplesToEnd;
        wetBuffer.addFromWithRamp(channel, numSamplesToEnd, delayBuffer.getReadPointer(channel, 0), numSamplesAtStart, gain, gain);
    }
}

void JafftuneAudioProcessor::updateBufferPositions (juce::AudioBuffer<float>& buffer, juce::AudioBuffer<float>& delayBuffer)
{
    auto bufferSize = buffer.getNumSamples();
    auto delayBufferSize = delayBuffer.getNumSamples();
    
    writePosition += bufferSize;
    writePosition %= delayBufferSize;
}

//==============================================================================
bool JafftuneAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* JafftuneAudioProcessor::createEditor()
{
    //return new JafftuneAudioProcessorEditor (*this);
    return new juce::GenericAudioProcessorEditor (*this);
}

//==============================================================================
void JafftuneAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void JafftuneAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//Add sliders to layout below

juce::AudioProcessorValueTreeState::ParameterLayout
    JafftuneAudioProcessor::createParameterLayout()
{
        juce::AudioProcessorValueTreeState::ParameterLayout layout;
        
        //adds parameter for controlling ratio of outpitch to inpitch
        layout.add(std::make_unique<juce::AudioParameterFloat>("Pitch Ratio",
        "Pitch Ratio",
        juce::NormalisableRange<float>(0.5, 2.f, 0.01, 1.f), 0.94));
                                       // (low, hi, step, skew), default value)
        
        //adds parameter for blending pitshifted signal with input signal
        layout.add(std::make_unique<juce::AudioParameterFloat>("Blend",
        "Blend",
        juce::NormalisableRange<float>(0.f, 100.f, 1.f, 1.f), 0.0f));
        
        //adds binary option for Stereo and Mono modes
        juce::StringArray stringArray;
        for( int i = 0; i < 2; ++i )
        {
            juce::String str;
            if (i == 0) {
                str << "Mono (Blend)";
            }
            else {
                str << "Stereo (Dry L Wet R)";
            }
            stringArray.add(str);
        }
        
        layout.add(std::make_unique<juce::AudioParameterChoice>("Operation Mode", "Operation Mode", stringArray, 0));
        
        return layout;
    }

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new JafftuneAudioProcessor();
}
