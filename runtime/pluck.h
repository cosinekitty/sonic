/*============================================================================

    pluck.h

    Defines Sonic import function 'PluckedString'.

============================================================================*/
#ifndef __ddc_sonic_pluck_h
#define __ddc_sonic_pluck_h


//$Sonic:PluckedString(integer,integer)


class i_PluckedString
{
public:
    i_PluckedString(double _freqHz, double _coeff1, double _coeff2);
    ~i_PluckedString();

    void reset(int _numChannels, long _samplingRate);
    double operator()(int channel, long index);
    void operator()(double _freqHz);

protected:
    void freeMemory();

private:
    double * array [MAX_SONIC_CHANNELS];
    int numChannels;
    long samplingRate;
    double freqHz;
    int arraySize;
    long currentSampleIndex;
    int cycle;
    double lastOutput [MAX_SONIC_CHANNELS];
    double coeff1, coeff2;
};


#endif // __ddc_sonic_pluck_h
/*--- end of file pluck.h ---*/