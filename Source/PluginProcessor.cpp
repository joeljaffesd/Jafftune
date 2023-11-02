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
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
    //const int numInputChannels = getTotalNumInputChannels();
    auto delayBufferSize = sampleRate * 2.0;
    delayBuffer.setSize(getTotalNumOutputChannels(), (int)delayBufferSize);
    //mSampleRate = sampleRate;
    
    //delayBuffer.setSize(numInputChannels, delayBufferSize);
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

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // This is the place where you'd normally do the guts of your plugin's
    // audio processing...
    // Make sure to reset the state if your inner loop is processing
    // the samples and the outer loop is handling the channels.
    // Alternatively, you can process the samples with the channels
    // interleaved by keeping the same state.
    
    auto bufferSize = buffer.getNumSamples();
    const int delayBufferSize = delayBuffer.getNumSamples();
    
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer (channel);
        
        //const float* bufferData = buffer.getReadPointer(channel);
        //const float* delayBufferData = delayBuffer.getReadPointer(channel);
        
        fillBuffer (channel, bufferSize, delayBufferSize, channelData);
        
        auto readPosition = writePosition - getSampleRate();
        
        if (readPosition < 0)
            readPosition += delayBufferSize;
        
        if (readPosition + bufferSize < delayBufferSize) {
            buffer.addFromWithRamp(channel, 0, delayBuffer.getReadPointer(channel, readPosition), bufferSize, 0.8, 0.8);
        }
        else {
            auto numSamplesToEnd = delayBufferSize - readPosition;
            buffer.addFromWithRamp(channel, 0, delayBuffer.getReadPointer(channel, readPosition), numSamplesToEnd, 0.8, 0.8);
            auto numSamplesAtStart = bufferSize - numSamplesToEnd;
            buffer.addFromWithRamp(channel, numSamplesToEnd, delayBuffer.getReadPointer(channel, 0), numSamplesAtStart, 0.8, 0.8);
        }
        //getFromDelayBuffer(buffer, channel, bufferSize, delayBufferSize, channelData, delayBufferData);
      /*
        //copy data from main buffer to delay buffer -> moved to dedicated function
        if (delayBufferSize > bufferSize + writePosition)
        {
            delayBuffer.copyFromWithRamp(channel, writePosition, channelData, bufferSize, 0.8f, 0.8f); // 0.8 = start and end gain, why not 1.0?
        }
        else {
            auto numSamplesToEnd = delayBufferSize - writePosition;
            
            delayBuffer.copyFromWithRamp(channel, writePosition, channelData, numSamplesToEnd, 0.8f, 0.8f);
            
            auto numSamplesAtStart = bufferSize - numSamplesToEnd;
            
            delayBuffer.copyFromWithRamp(channel, 0, channelData, numSamplesAtStart, 0.8f, 0.8f);
        }
        */
        // ..do something to the data...
    }
    
    writePosition += bufferSize;
    writePosition %= delayBufferSize;
}
void JafftuneAudioProcessor::fillBuffer (int channel, int bufferSize, int delayBufferSize, float* channelData)
{
      //copy data from main buffer to delay buffer -> dedicated function
      if (delayBufferSize > bufferSize + writePosition)
      {
          delayBuffer.copyFromWithRamp(channel, writePosition, channelData, bufferSize, 0.8f, 0.8f); // 0.8 = start and end gain, why not 1.0?
      }
      else {
          auto numSamplesToEnd = delayBufferSize - writePosition;
          
          delayBuffer.copyFromWithRamp(channel, writePosition, channelData, numSamplesToEnd, 0.8f, 0.8f);
          
          auto numSamplesAtStart = bufferSize - numSamplesToEnd;
          
          delayBuffer.copyFromWithRamp(channel, 0, channelData + numSamplesToEnd, numSamplesAtStart, 0.8f, 0.8f);
      }
}
/*
void JafftuneAudioProcessor::getFromDelayBuffer(juce::AudioBuffer<float>& buffer, int channel, const int bufferSize, const int delayBufferSize, const float* channelData, const float* delayBufferData)
{
    int delayTime = 500;
    const int readPosition = static_cast<int> (delayBufferSize + writePosition - (mSampleRate * delayTime /1000)) % delayBufferSize; //make delayTime variable, check if static cast causes issues
    if (delayBufferSize > bufferSize + readPosition)
    {
        buffer.addFrom(channel, 0, delayBufferData + readPosition, bufferSize);
    }
    else {
        const int bufferRemaining = delayBufferSize - readPosition;
        buffer.addFrom(channel, 0, delayBufferData + readPosition, bufferRemaining);
        buffer.addFrom(channel, bufferRemaining, delayBufferData, bufferSize - bufferRemaining);
    }
}
*/
//DBG ("Delay Buffer Size: " << delayBufferSize);
//DBG ("Buffer Size: " << bufferSize);
//DBG ("Write Position: " << writePosition);

//==============================================================================
bool JafftuneAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* JafftuneAudioProcessor::createEditor()
{
    //return new JafftuneAudioProcessorEditor (*this);
    return new juce::GenericAudioProcessorEditor(*this);
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
        
        layout.add(std::make_unique<juce::AudioParameterFloat>("Pitch Ratio",
        "Pitch Ratio",
        juce::NormalisableRange<float>(0.5, 2.f, 0.01, 1.f), 1.f));
                                       // (low, hi, step, skew), default value)
        layout.add(std::make_unique<juce::AudioParameterFloat>("Blend",
        "Blend",
        juce::NormalisableRange<float>(0.f, 100.f, 1.f, 1.f), 50.f));
        
     /*
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
        */
        return layout;
    }

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new JafftuneAudioProcessor();
}
