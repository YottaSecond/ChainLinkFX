#include "Effects.h"
#include "portaudio.h"
#include "ChainLinkFX.h"
#include <stdio.h>
#include <stdlib.h>
#define PA_SAMPLE_TYPE paFloat32


Chain* rootChain = NULL;
int newChain(PaDeviceIndex inputDeviceIndex, PaDeviceIndex outputDeviceIndex)
{
	PaError err;
	//malloc memory for new node
	Chain* newChain = malloc(sizeof(Chain));
	//set new final node's next pointer to NULL
	newChain->nextChain = NULL;
	if(rootChain == NULL){
		rootChain = newChain;
	}
	else{
		Chain* iterator = rootChain;
		while(iterator->nextChain != NULL){
			iterator = iterator->nextChain;
		}
		//set final node's next node pointer to new node
		iterator->nextChain = newChain;
	}
	newChain->inputParameters.device = inputDeviceIndex;
	if(newChain->inputParameters.device == paNoDevice){
		fprintf(stderr,"Error: Input device is invalid.\n Maybe the device has been unplugged?");
		goto error;
	}
	const PaDeviceInfo* inputDeviceInfo = Pa_GetDeviceInfo(inputDeviceIndex);
	const PaDeviceInfo* outputDeviceInfo = Pa_GetDeviceInfo(outputDeviceIndex);
	newChain->inputParameters.channelCount = inputDeviceInfo->maxInputChannels;
	newChain->inputParameters.sampleFormat = PA_SAMPLE_TYPE;
	newChain->inputParameters.suggestedLatency = inputDeviceInfo->defaultLowInputLatency;
	newChain->inputParameters.hostApiSpecificStreamInfo = NULL;
	newChain->outputParameters.device = outputDeviceIndex;
	if(newChain->outputParameters.device == paNoDevice){
		fprintf(stderr,"Error: Output device is invalid.\n Maybe the device has been unplugged?");
		goto error;
	}
	newChain->outputParameters.channelCount = outputDeviceInfo->maxOutputChannels;
	newChain->outputParameters.sampleFormat = PA_SAMPLE_TYPE;
	newChain->outputParameters.suggestedLatency = outputDeviceInfo->defaultLowOutputLatency;
	newChain->outputParameters.hostApiSpecificStreamInfo = NULL;
	
	//set root ChainLink to empty effect that does nothing, just for... fun?
	newChain->chainLink = malloc(sizeof(ChainLink));
	newChain->chainLink->effectData = initEmptyEffect();
	newChain->chainLink->effectFunction = &emptyEffect;
	newChain->chainLink->nextLink = NULL;
	newChain->chainLink->numInputChannels = newChain->inputParameters.channelCount;
	newChain->chainLink->numOutputChannels = newChain->outputParameters.channelCount;
	
	err = Pa_OpenStream(
			&(newChain->stream),
			&(newChain->inputParameters),
			&(newChain->outputParameters),
			inputDeviceInfo->defaultSampleRate,	
			paFramesPerBufferUnspecified,
			paClipOff,
			audioCallback,
			newChain->chainLink);
	if(err != paNoError) goto error;
	err = Pa_StartStream(newChain->stream);
	if( err != paNoError ) goto error;
	return 0;
	error:
	fprintf( stderr, "An error occured while using the portaudio stream\n" );
    fprintf( stderr, "Error number: %d\n", err );
    fprintf( stderr, "Error message: %s\n", Pa_GetErrorText( err ) );
    return -1;
}

int audioCallback( const void *inputBuffer, void *outputBuffer,
                         unsigned long framesPerBuffer,
                         const PaStreamCallbackTimeInfo* timeInfo,
                         PaStreamCallbackFlags statusFlags,
                         void *userData )
{
	SAMPLE *out = (SAMPLE*)outputBuffer;
	const SAMPLE *in = (const SAMPLE*)inputBuffer;
	unsigned int i;
	(void) timeInfo;
	(void) statusFlags;
	ChainLink* functionIterator = (ChainLink*) userData;
	int currentChannel;
	if(inputBuffer == NULL)
	{
		for(i=0; i<framesPerBuffer; i++)
		{
			for(currentChannel = 0; currentChannel < functionIterator->numOutputChannels; currentChannel++)
			{
			*out++ = 0;
			}
		}
	}
	else
	{
		for(i = 0; i<framesPerBuffer; i++)
		{
			for(currentChannel = 0; currentChannel < functionIterator->numOutputChannels; currentChannel++)
			{
			functionIterator->effectFunction(in,out,functionIterator);
				while(functionIterator->nextLink != NULL){
					functionIterator = functionIterator->nextLink;
					functionIterator->effectFunction(in,out, functionIterator);
				}
			*out++;
			*in++;
			}
		}
	}
	return paContinue;
}


//check for mem leaks - will this free all chainlinks associated with a chain?
int removeChain(int chainIndex)
{
	if(rootChain == NULL){
		//this totally shouldn't happen but I'll keep it here for debug purposes
		printf("rootChain is null!?\n");
		return -1;
	}
	Chain* chainToRemove = rootChain;
	int i;
	//if root chain: 
	if(chainIndex == 0){
		//if next chain exists:
		if(chainToRemove->nextChain != NULL){
		//set root chain to next chain
			rootChain = chainToRemove->nextChain;
		}
		else{
			rootChain = NULL;
		}
	}
	else{
		Chain* preChainToRemove;
		for(i = 0; i < chainIndex; i++){
			preChainToRemove = chainToRemove;
			chainToRemove = chainToRemove->nextChain;
		}
		preChainToRemove->nextChain = chainToRemove->nextChain;
	}
	if(chainToRemove == NULL){
		//this totally shouldn't happen but I'll keep it here for debug purposes
		printf("chainToRemove was null!\n");
		return -1;
	}
	Pa_StopStream(chainToRemove->stream);
	//this returns 1 when stream is still active
	while(Pa_IsStreamStopped(chainToRemove->stream) != 1){
		//do nothing; block until stream is finished
	}
	free(chainToRemove);
	return 0;
}

int newChainLink(int chainIndex, EffectType effectType)
{
	//malloc the chainLink
	ChainLink* newChainLink = malloc(sizeof(ChainLink));
	//set new final node's next pointer to NULL
	newChainLink->nextLink = NULL;
	//init members
	newChainLink->numInputChannels = rootChain->inputParameters.channelCount;
	newChainLink->numOutputChannels = rootChain->outputParameters.channelCount;
	/*
	switch(effectType){
		case EMPTY:
			newChainLink->effectData = initEmptyEffect;
			newChainLink->effectFunction = &emptyEffect;
		case DELAY:
			newChainLink->effectData = initDelayEffect();
			newChainLink->effectFunction = &delayEffect;
		default: 
			return -1;
	}
	*/
	newChainLink->effectData = initDelayEffect();
	newChainLink->effectFunction = &delayEffect;
	//increment to chain index
	int i;
	Chain* iterator = rootChain;
	
	for(i = 0; i < chainIndex; i++){
		iterator = iterator->nextChain;
	}
	if(iterator == NULL){
		//this is unfortunate
		return -1;
	}
	//append new chainlink to end of chainLink list:
	ChainLink* linkIterator = iterator->chainLink;
	while(linkIterator->nextLink != NULL){
		linkIterator = linkIterator->nextLink;
	}
	//set final node's next node pointer to new node
	linkIterator->nextLink = newChainLink;
	
	return 0;
}

//need to test this really really badly:
int removeChainLink(int chainIndex, int chainLinkIndex)
{
	Chain* chainIterator = rootChain;
	
	int i;
	for(i = 0; i < chainIndex; i++)
	{
		chainIterator = chainIterator->nextChain;
	}

	ChainLink* linkToRemove = chainIterator->chainLink;
	//if we're removing the root node:
	if(chainLinkIndex == 0){
		chainIterator->chainLink = chainIterator->chainLink->nextLink;
		linkToRemove = chainIterator->chainLink;
	}
	//if it is not the root node:
	else{
		ChainLink* prevLink = linkToRemove;
	
		for(i = 0; i < chainLinkIndex; i++)
		{
			prevLink = linkToRemove;
			linkToRemove = linkToRemove->nextLink;
		}
		prevLink->nextLink = linkToRemove->nextLink;
	}
	if(linkToRemove == NULL){
			//that's bad...
			return -1;
	}
	
	//set root chain to next chain	
	free(linkToRemove);
	return 0;
}
