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
    delayLine.setSize(getTotalNumOutputChannels(), (int)delayBufferSize);
    
    auto phasorBufferSize = samplesPerBlock;
    phasorBuffer.setSize(getTotalNumOutputChannels(), (int)phasorBufferSize);
    
    auto wetBufferSize = samplesPerBlock;
    wetBuffer.setSize(getTotalNumInputChannels(), (int)wetBufferSize);
        
    //initialzie spec
    juce::dsp::ProcessSpec spec;
    spec.maximumBlockSize = samplesPerBlock;
    spec.sampleRate = sampleRate;
    spec.numChannels = getTotalNumOutputChannels();
    
    phasor.prepare (spec); //pass spec to phasor
    phasor.setFrequency( 1000.0f * ((1.0f - pitchRatio) / delayWindow) ); //set initial phasor frequency

    sinOsc.prepare (spec); //pass spec to sinOsc
    sinOsc.setFrequency( 440.0f ); //set initial sinOsc frequency

    phasorGain.setGainLinear( 1.0f );
    sinOscGain.setGainLinear( 1.0f );
    
    mDelayLine.reset();
    mDelayLine.setMaximumDelayInSamples (getSampleRate());
    mDelayLine.prepare (spec);
    
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
    
    //set phasor~ frequency based on pitchRatio
    float pitchRatio = treeState.getRawParameterValue ("Pitch Ratio")->load();
    
    phasor.setFrequency ( 1000.0f * ((1.0f - pitchRatio) / delayWindow) ); //set runtime phasor frequency
    
    juce::dsp::AudioBlock<float> phasorBlock { phasorBuffer };
    
    phasor.process (juce::dsp::ProcessContextReplacing<float> (phasorBlock));
    
    sinOsc.setFrequency ( 440.0f ); //set runtime phasor frequency
    
    juce::dsp::AudioBlock<float> bufferBlock { buffer };
    
    sinOsc.process (juce::dsp::ProcessContextReplacing<float> (bufferBlock));

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());
    
    //Blend control
        float blendFactor = treeState.getRawParameterValue ("Blend")->load();
        float dryGain = scale (100 - blendFactor, 0.0f, 100.0f, 0.0f, 1.0f);
        float wetGain = scale (blendFactor, 0.0f, 100.0f, 0.0f, 1.0f);
        float volFactor = dbtoa(treeState.getRawParameterValue ("Volume")->load());

    //write and read from delayBuffer
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        //write from main buffer to delay buffer
        fillDelayBuffer (buffer, channel);
        
        auto* input = buffer.getWritePointer (channel);
        auto* output = wetBuffer.getWritePointer (channel);
        //auto* delayTap = delayLine.getWritePointer(channel);
        
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            //float inputSample = input[sample];
            
            //delayLine class solution <- sounds like fm, some gain issue (try volume -5, blend 99 to hear)
            mDelayLine.pushSample(channel, input[sample]);
            float phasorTap = phasor.processSample(0.0f); // fixed!
            int delayInSamples = static_cast<int>(fmod(phasorTap, 1) * msToSamps(delayWindow));
            //int delayInSamples = getSampleRate();
            //int delayInSamples = static_cast<int>(phasorTap * msToSamps(delayWindow));
            float delayTapOne = mDelayLine.popSample(channel, delayInSamples, true); //<- tapout1
            
            //DBG("delayTest: " + juce::String(phasorTap));
    
            /*//custom circular buffer solution <- also sounds like fm, not as smooth as delayLine class?
            float phasorTap = phasorBuffer.getSample(channel, sample);
            //int readPositionOne = writePosition - static_cast<int>(fmod(phasorTap, 1) * msToSamps(delayWindow));
            int readPositionOne = writePosition - getSampleRate();
            //int readPositionTwo = writePosition - static_cast<int>(fmod(phasorTap + 0.5f, 1) * msToSamps(delayWindow));
            
            float delayTapOne = 0.0f;
            
            if (readPositionOne < 0)
                readPositionOne += delayLine.getNumSamples();
            {
                delayTapOne = delayLine.getSample(channel, readPositionOne);
            }
            */
            
            //float gainWindowOne = cos((((fmod(phasorTap, 1) - 0.5f) / 2.0f)) * 2.0f * pi);
            //float gainWindowTwo = cos((((fmod(phasorTap + 0.5f, 1) - 0.5f) / 2.0f)) * 2.0f * pi);
            
            //auto outputSample = delayLine.getSample(channel, delayLine.getWritePointer(channel)[sample] - 1); //<- shows delay buffer is flawed
            //auto outputSample = ((delayTapOne * gainWindowOne) + (delayTapTwo * gainWindowTwo));
            auto outputSample = delayTapOne; //<- should work as basic pitchshift with artifacts
            //auto outputSample = inputSample; //<- bypass
            output[sample] = outputSample;
        }
        
        //read from wetBuffer into buffer
        buffer.copyFromWithRamp (channel, 0, buffer.getReadPointer(channel), buffer.getNumSamples(), dryGain * volFactor, dryGain * volFactor);
        
        buffer.addFromWithRamp (channel, 0, wetBuffer.getReadPointer(channel), buffer.getNumSamples(), wetGain * volFactor, wetGain * volFactor);
        
    }
     
    updateBufferPositions (buffer, delayLine);
   
}

void JafftuneAudioProcessor::fillDelayBuffer (juce::AudioBuffer<float>& buffer, int channel)
{
      //initialize variables
      float gain = 1.0f;
      auto bufferSize = buffer.getNumSamples();
      auto delayBufferSize = delayLine.getNumSamples();
      
      //Check to see if main buffer copies to delay buffer without needing to wrap
      if (delayBufferSize > bufferSize + writePosition)
      {
          //copy main buffer to delay buffer
          delayLine.copyFromWithRamp(channel, writePosition, buffer.getWritePointer(channel), bufferSize, gain, gain); // 0.8 = start and end gain, why not 1.0?
      }
      //if not
      else {
          //determine how much space is left at the end of the delay buffer
          auto numSamplesToEnd = delayBufferSize - writePosition;
          
          //copy that amount of content to the end
          delayLine.copyFromWithRamp(channel, writePosition, buffer.getWritePointer(channel), numSamplesToEnd, gain, gain);
          
          //calculate how much content remains to be copied
          auto numSamplesAtStart = bufferSize - numSamplesToEnd;
          
          //copy remainging amount to beginning of delay buffer
          delayLine.copyFromWithRamp(channel, 0, buffer.getWritePointer(channel) + numSamplesToEnd, numSamplesAtStart, gain, gain);
      }
}

void JafftuneAudioProcessor::updateBufferPositions (juce::AudioBuffer<float>& buffer, juce::AudioBuffer<float>& delayLine)
{
    auto bufferSize = buffer.getNumSamples();
    auto delayBufferSize = delayLine.getNumSamples();
    
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
        
        //adds parameter for blending pitshifted signal with input signal
        layout.add(std::make_unique<juce::AudioParameterFloat>("Volume",
        "Volume",
        juce::NormalisableRange<float>(-60.0f, 0.0f, 1.f, 1.f), -60.0f));
        
        //adds binary option for Stereo and Mono modes (not implemented)
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
