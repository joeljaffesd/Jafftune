/*
  ==============================================================================

    JAFFTUNE Real-Time Pitchshifter Powered by Variable-Rate Delay
    Awknowledgments to Karl Yerkes, Miller Puckette, Jazer Sibley-Schwartz, @dude837 on YouTube, @The Audio Programmer on YouTube
    
    YET TO IMPLEMENT:
    -Mono/Stereo mode switching
    -Presets
    -Optimization for use as a Detune (RIP EVH <3)
    -Testing as VST3
 
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
    //initialzie spec
    juce::dsp::ProcessSpec spec;
    spec.maximumBlockSize = samplesPerBlock;
    spec.sampleRate = sampleRate;
    spec.numChannels = getTotalNumOutputChannels();
    
    phasor.prepare (spec); //pass spec to phasor
    phasor.setFrequency( 1000.0f * ((1.0f - pitchRatio) / delayWindow) ); //set initial phasor frequency
    
    reversePhasor.prepare (spec); //pass spec to phasor
    reversePhasor.setFrequency( 1000.0f * ((1.0f - pitchRatio) / delayWindow) ); //set initial phasor frequency

    sinOsc.prepare (spec); //pass spec to sinOsc
    sinOsc.setFrequency( 440.0f ); //set initial sinOsc frequency

    phasorGain.setGainLinear( 1.0f );
    reversePhasorGain.setGainLinear ( 1.0f );
    sinOscGain.setGainLinear( 1.0f );
    
    mDelayLineOne.reset();
    mDelayLineOne.setMaximumDelayInSamples (getSampleRate());
    mDelayLineOne.prepare (spec);
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
    
    phasor.setFrequency ( abs(1000.0f * ((1.0f - pitchRatio) / delayWindow)) );
    reversePhasor.setFrequency ( abs(1000.0f * ((1.0f - pitchRatio) / delayWindow)) );
    
    
    //writes sinOsc to buffer (for testing)
    sinOsc.setFrequency ( 440.0f ); //set runtime sinOsc frequency
    juce::dsp::AudioBlock<float> bufferBlock { buffer };
    //sinOsc.process (juce::dsp::ProcessContextReplacing<float> (bufferBlock));

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());
    
    //Blend control
        float blendFactor = treeState.getRawParameterValue ("Blend")->load();
        float dryGain = scale (100 - blendFactor, 0.0f, 100.0f, 0.0f, 1.0f);
        float wetGain = scale (blendFactor, 0.0f, 100.0f, 0.0f, 1.0f);
        float volFactor = dbtoa(treeState.getRawParameterValue ("Volume")->load());
    
    auto* inputL = buffer.getReadPointer (0);
    auto* inputR = buffer.getReadPointer (1);
    auto* outputL = buffer.getWritePointer (0);
    auto* outputR = buffer.getWritePointer (1);
    //auto* delayTap = delayLine.getWritePointer(channel);
    //auto* phasorPointer = phasorBuffer.getReadPointer(channel);
    
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        float inputSample = inputL[sample] + inputR[sample];
        mDelayLineOne.pushSample(0, inputSample);
        
        if (pitchRatio < 1.0f ) {
            globalPhasorTap = phasor.processSample( 0.0f );
        }
        else if (pitchRatio > 1.0f ){
            globalPhasorTap = reversePhasor.processSample( 0.0f );
        }
        else {
            globalPhasorTap = 0.0f;
        }
        
        float phasorTap = globalPhasorTap;
        
        float delayOne = (phasorTap * msToSamps(delayWindow));
        float delayTwo = (fmod(phasorTap + 0.5f, 1) * msToSamps(delayWindow));
        float filteredDelayOne = onePole(delayOne, lastDelayTimeOne, 0.05f, getSampleRate());
        float filteredDelayTwo = onePole(delayTwo, lastDelayTimeTwo, 0.05f, getSampleRate());
        lastDelayTimeOne = filteredDelayOne;
        lastDelayTimeTwo = filteredDelayTwo;
        float delayTapOne = mDelayLineOne.popSample(0, filteredDelayOne, false); //<- tapout1
        float delayTapTwo = mDelayLineOne.popSample(0, filteredDelayTwo, true); //<- tapout2
        float gainWindowOne = cosf((((phasorTap - 0.5f) / 2.0f)) * 2.0f * pi);
        float gainWindowTwo = cosf(((fmod((phasorTap + 0.5f), 1) - 0.5f) / 2.0f) * 2 * pi);
        
        //auto outputSample = inputSample; //<- bypass
        //float outputSample = (delayTapOne * wetGain * volFactor) + (inputSample * dryGain * volFactor); //<- should work as basic pitchshift with artifacts
        float outputSample = ((((delayTapOne * gainWindowOne) + delayTapTwo * gainWindowTwo ) * wetGain ) + (inputSample * dryGain)) * volFactor;
        outputL[sample] = outputSample;
        outputR[sample] = outputSample;
    }
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
        juce::NormalisableRange<float>(0.5, 2.f, 0.01, 1.f), 1.0));
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
