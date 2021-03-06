
#include "PluginProcessor.h"
#include "PluginEditor.h"

PluginProcessor::PluginProcessor()
{
	nSampleRate = 48000;
    nHostBlockSize = FRAME_SIZE;
	ambi_drc_create(&hAmbi);

	ringBufferInputs = new float*[MAX_NUM_CHANNELS];
	for (int i = 0; i < MAX_NUM_CHANNELS; i++)
		ringBufferInputs[i] = new float[FRAME_SIZE];

	ringBufferOutputs = new float*[MAX_NUM_CHANNELS];
	for (int i = 0; i < MAX_NUM_CHANNELS; i++)
		ringBufferOutputs[i] = new float[FRAME_SIZE];
}

PluginProcessor::~PluginProcessor()
{
	ambi_drc_destroy(&hAmbi);

	for (int i = 0; i < MAX_NUM_CHANNELS; ++i)
		delete[] ringBufferInputs[i];
	delete[] ringBufferInputs;

	for (int i = 0; i < MAX_NUM_CHANNELS; ++i)
		delete[] ringBufferOutputs[i];
	delete[] ringBufferOutputs;
}

void PluginProcessor::setParameter (int index, float newValue)
{
	switch (index)
	{ 
		default: break;
	} 
}

void PluginProcessor::setCurrentProgram (int index)
{
}

float PluginProcessor::getParameter (int index)
{
    switch (index)
	{   
		default: return 0.0f;
	}
}

int PluginProcessor::getNumParameters()
{
	return k_NumOfParameters;
}

const String PluginProcessor::getName() const
{
    return JucePlugin_Name;
}

const String PluginProcessor::getParameterName (int index)
{
    switch (index)
	{
		default: return "NULL";
	}
}

const String PluginProcessor::getParameterText(int index)
{
	return String(getParameter(index), 1);    
}

const String PluginProcessor::getInputChannelName (int channelIndex) const
{
    return String (channelIndex + 1);
}

const String PluginProcessor::getOutputChannelName (int channelIndex) const
{
    return String (channelIndex + 1);
}

double PluginProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int PluginProcessor::getNumPrograms()
{
    return 1;
}

int PluginProcessor::getCurrentProgram()
{
    return 0;
}

const String PluginProcessor::getProgramName (int index)
{
    return String::empty;
}


bool PluginProcessor::isInputChannelStereoPair (int index) const
{
    return true;
}

bool PluginProcessor::isOutputChannelStereoPair (int index) const
{
    return true;
}


bool PluginProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool PluginProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool PluginProcessor::silenceInProducesSilenceOut() const
{
    return false;
}

void PluginProcessor::changeProgramName (int index, const String& newName)
{
}

void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
	nHostBlockSize = samplesPerBlock;
	nSampleRate = (int)(sampleRate + 0.5);

	nNumInputs = getNumInputChannels();
    nNumOutputs = getNumOutputChannels();

	setPlayConfigDetails(nNumInputs, nNumOutputs, (double)nSampleRate, nHostBlockSize);
	numChannelsChanged();

	ambi_drc_init(hAmbi, (float)sampleRate);

	for (int i = 0; i < MAX_NUM_CHANNELS; ++i)
		memset(ringBufferInputs[i], 0, FRAME_SIZE*sizeof(float));
	for (int i = 0; i < MAX_NUM_CHANNELS; ++i)
		memset(ringBufferOutputs[i], 0, FRAME_SIZE * sizeof(float));

	isPlaying = false;
}

void PluginProcessor::releaseResources()
{
}

void PluginProcessor::processBlock (AudioSampleBuffer& buffer, MidiBuffer& midiMessages)
{
	int nCurrentBlockSize = buffer.getNumSamples();
	float** bufferData = buffer.getArrayOfWritePointers(); 
	float** outputs = new float*[nNumOutputs];
	for (int i = 0; i < nNumOutputs; i++)
		outputs[i] = new float[FRAME_SIZE]; 
	
    if(nCurrentBlockSize % FRAME_SIZE == 0){ /* divisible by frame size */
        for (int frame = 0; frame < nCurrentBlockSize/FRAME_SIZE; frame++) {
            for (int ch = 0; ch < nNumInputs; ch++)
                for (int i = 0; i < FRAME_SIZE; i++)
                    ringBufferInputs[ch][i] = bufferData[ch][frame*FRAME_SIZE + i];
            
            /* determine if there is actually audio in the damn buffer */
            playHead = getPlayHead();
            bool PlayHeadAvailable = playHead->getCurrentPosition(currentPosition);
            if (PlayHeadAvailable == true)
                isPlaying = currentPosition.isPlaying;
            else
                isPlaying = true;
            if(!isPlaying) /* for DAWs with no transport */
                isPlaying = buffer.getRMSLevel(0, 0, nCurrentBlockSize)>1e-5f ? true : false;
            
            /* perform processing */
            ambi_drc_process(hAmbi, ringBufferInputs, ringBufferOutputs, nNumInputs < nNumOutputs ? nNumInputs : nNumOutputs, FRAME_SIZE, isPlaying);
            
            /* replace buffer with new audio */
            buffer.clear(frame*FRAME_SIZE, FRAME_SIZE);
            for (int ch = 0; ch < nNumOutputs; ch++)
                for (int i = 0; i < FRAME_SIZE; i++)
                    bufferData[ch][frame*FRAME_SIZE + i] = ringBufferOutputs[ch][i];
        }
    }
    else
        buffer.clear();
	
	for (int i = 0; i < nNumOutputs; ++i)
		delete[] outputs[i];
	delete[] outputs;
}

//==============================================================================
bool PluginProcessor::hasEditor() const
{
    return true; 
}

AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor (this);
}

//==============================================================================
void PluginProcessor::getStateInformation (MemoryBlock& destData)
{
	/* Create an outer XML element.. */ 
	XmlElement xml("AMBIDRCAUDIOPLUGINSETTINGS");

	/* add attributes */
	xml.setAttribute("THRESHOLD", ambi_drc_getThreshold(hAmbi));
	xml.setAttribute("RATIO", ambi_drc_getRatio(hAmbi));
	xml.setAttribute("KNEE", ambi_drc_getKnee(hAmbi));
	xml.setAttribute("INGAIN", ambi_drc_getInGain(hAmbi));
    xml.setAttribute("OUTGAIN", ambi_drc_getOutGain(hAmbi));
	xml.setAttribute("ATTACK", ambi_drc_getAttack(hAmbi));
	xml.setAttribute("RELEASE", ambi_drc_getRelease(hAmbi));
    xml.setAttribute("NORM", ambi_drc_getNormType(hAmbi));
    xml.setAttribute("CHORDER", ambi_drc_getChOrder(hAmbi));
    xml.setAttribute("PRESET", ambi_drc_getInputPreset(hAmbi));
 
	copyXmlToBinary(xml, destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
	/* This getXmlFromBinary() function retrieves XML from the binary blob */
	ScopedPointer<XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

	if (xmlState != nullptr) {
		/* make sure that it's actually the correct type of XML object */
		if (xmlState->hasTagName("AMBIDRCAUDIOPLUGINSETTINGS")) {
            
			/* pull attributes */
            if(xmlState->hasAttribute("THRESHOLD"))
                ambi_drc_setThreshold(hAmbi, (float)xmlState->getDoubleAttribute("THRESHOLD", 0.0f));
            if(xmlState->hasAttribute("RATIO"))
                ambi_drc_setRatio(hAmbi, (float)xmlState->getDoubleAttribute("RATIO", 1.0f));
            if(xmlState->hasAttribute("KNEE"))
                ambi_drc_setKnee(hAmbi, (float)xmlState->getDoubleAttribute("KNEE", 0.0f));
            if(xmlState->hasAttribute("INGAIN"))
                ambi_drc_setInGain(hAmbi, (float)xmlState->getDoubleAttribute("INGAIN", 0.0f));
            if(xmlState->hasAttribute("OUTGAIN"))
                ambi_drc_setOutGain(hAmbi, (float)xmlState->getDoubleAttribute("OUTGAIN", 0.0f));
            if(xmlState->hasAttribute("ATTACK"))
                ambi_drc_setAttack(hAmbi, (float)xmlState->getDoubleAttribute("ATTACK", 50.0f));
            if(xmlState->hasAttribute("RELEASE"))
                ambi_drc_setRelease(hAmbi, (float)xmlState->getDoubleAttribute("RELEASE", 100.0f));
            if(xmlState->hasAttribute("NORM"))
                ambi_drc_setNormType(hAmbi, xmlState->getIntAttribute("NORM", 1));
            if(xmlState->hasAttribute("CHORDER"))
                ambi_drc_setChOrder(hAmbi, xmlState->getIntAttribute("CHORDER", 1));
            if(xmlState->hasAttribute("PRESET"))
                ambi_drc_setInputPreset(hAmbi, (INPUT_ORDER)xmlState->getIntAttribute("PRESET", 1));
            
            ambi_drc_refreshSettings(hAmbi);
        } 
	}
}

//==============================================================================
// This creates new instances of the plugin..
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}

