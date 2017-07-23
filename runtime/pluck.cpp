/*============================================================================

    pluck.cpp

    Defines Sonic import function 'PluckedString'.
    This is an implementation of the
    Karplus-Strong Plucked String Algorithm.  See following references:

http://ccrma-www.stanford.edu/CCRMA/Software/clm/compmus/clm-tutorials/pm.html
http://www.ece.cmu.edu/~kb/paper.ciarm95.html
http://sunsite.ubc.ca/WorldSoundscapeProject/reson.html
http://www.hut.fi/Yksikot/Akustiikka/stringmodels/

    Keywords: Karplus-Strong, waveguide resonators, string models

============================================================================*/
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "sonic.h"
#include "pluck.h"


i_PluckedString::i_PluckedString(double _freqHz, double _coeff1, double _coeff2):
    numChannels(0),
    samplingRate(0),
    freqHz(_freqHz),
    arraySize(0),
    currentSampleIndex(0),
    cycle(0),
    coeff1(_coeff1),
    coeff2(_coeff2)
{
    if (freqHz <= 0.0)
    {
        fprintf(stderr,
                "Error in PluckedString constructor:  freqHz = %lf is invalid.\n",
                freqHz);

        exit(1);
    }

    for (int i=0; i < MAX_SONIC_CHANNELS; ++i)
        array[i] = 0;
}


i_PluckedString::~i_PluckedString()
{
    freeMemory();
}


void i_PluckedString::operator()(double _freqHz)
{
    if (_freqHz <= 0.0)
    {
        fprintf(stderr,
                "Error in PluckedString constructor:  freqHz = %lf is invalid.\n",
                _freqHz);

        exit(1);
    }

    freqHz = _freqHz;
}


void i_PluckedString::reset(int _numChannels, long _samplingRate)
{
    if (freqHz >= _samplingRate / 2.0)
    {
        fprintf(stderr,
                "i_PluckedString::reset():  freqHz = %lf exceeds Nyquist frequency.\n",
                freqHz);

        exit(1);
    }

    freeMemory();

    numChannels = _numChannels;
    samplingRate = _samplingRate;

    arraySize = int (double(samplingRate) / freqHz);
    for (int c=0; c < numChannels; ++c)
    {
        lastOutput[c] = double(0);
        double *ptr = array[c] = new double [arraySize];
        if (!ptr)
        {
            fprintf(stderr, "Error:  Out of memory in PluckedString::reset()\n");
            exit(1);
        }

        for (int i=0; i < arraySize; ++i)
            *ptr++ = Sonic_Noise(1.0);
    }

    currentSampleIndex = 0;
    cycle = 0;
}


double i_PluckedString::operator()(int channel, long index)
{
    if (index < 0)
        return 0;       // allow delay

    if (index != currentSampleIndex)
    {
        // advance the state of the simulation
        currentSampleIndex = index;

        if (++cycle >= arraySize)
            cycle = 0;

        for (int c=0; c < numChannels; ++c)
        {
            double current = array[c][cycle];
            array[c][cycle] = coeff1*lastOutput[c] + coeff2*current;
            lastOutput[c] = current;
        }
    }

    return array[channel][cycle];
}


void i_PluckedString::freeMemory()
{
    for (int i=0; i < MAX_SONIC_CHANNELS; ++i)
    {
        if (array[i])
        {
            delete[] array[i];
            array[i] = 0;
        }
    }
}


/*--- end of file pluck.h ---*/