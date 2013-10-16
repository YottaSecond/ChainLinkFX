#include <portaudio.h>
#include "Effects.h"

#ifndef CHAINLINKFX_H
#define CHAINLINKFX_H

//effects may need to know the sample rate and input output channel numbers
int sampleRate;
int numInputChannels, numOutputChannels;
/* allocates memory for a new Chain, initializes a stream
 */
int newChain(PaDeviceIndex inputDeviceIndex, PaDeviceIndex outputDeviceIndex);

/* removes the specific chain from the link, frees the memory associated 
 * with it, stops the stream
 */
int removeChain(int chainIndex);
int newChainLink(int chainIndex, int effectType);
int removeChainLink(int chainIndex, int chainLinkIndex);
int audioCallback(const void *inputBuffer, void *outputBuffer,
				unsigned long framesPerBuffer,
				const PaStreamCallbackTimeInfo* timeInfo,
				PaStreamCallbackFlags statusFlags,
				void *userData);
int setParameter(int chainIndex, int effectIndex, int parameterIndex, int value);

#endif /* CHAINLINKFX_H */
