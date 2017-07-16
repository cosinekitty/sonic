/*==========================================================================

    sonic.h  -  Copyright (C) 1998 by Don Cross <dcross@intersrv.com>

    This header file is included by Sonic/C++ source files.
    It represents the run-time library for the Sonic language in C++ form.

    Revision history:

1998 September 23 [Don Cross]
    Added new intrinsic functions:  square, cube, quart, recip, dB.

1998 September 19 [Don Cross]
    Added member function SonicWave::interp().
    This supports the interpolate feature in Sonic v 0.901.

==========================================================================*/
#ifndef __ddc_sonic_runtime
#define __ddc_sonic_runtime

class WaveFile;


const int MAX_SONIC_CHANNELS = 64;


enum SonicWaveMode
{
    SWM_UNDEFINED,
    SWM_CLOSED,
    SWM_MODIFY,
    SWM_WRITE,
    SWM_READ,
    SWM_PREMODIFY
};


double ScanReal    ( const char *varname, const char *vstring );
long   ScanInteger ( const char *varname, const char *vstring );
int    ScanBoolean ( const char *varname, const char *vstring );


class SonicWave
{
public:
    SonicWave (
        const char *_filename,
        const char *_varname,
        long _requiredSamplingRate,
        int _requiredNumChannels );

    ~SonicWave();

    void openForRead();
    void openForWrite();
    void openForAppend();
    void openForModify();

    long queryNumSamples() const { return inNumSamples; }
    void read ( double sample[] );
    void write ( const double sample[] );
    double fetch ( int c, long i, int &countdown );
    double interp ( int c, double i, int &countdown );
    double queryMaxValue() const { return double(maxValue); }

    void close();
    void convertToWav ( const char *outWavFilename );   // ... but only if necessary

    static void EraseAllTempFiles();

protected:
    void determineNumSamples();

private:
    static int NextTempTag;     // used to generate temporary filenames

private:
    char *varname;  // sonic variable name for this 'wave' instance

    // 'inWave' and 'inFile' are mutually exclusive (the non-NULL one is read from)
    char *inFilename;
    WaveFile *inWave;
    FILE *inFile;
    long inNumSamples;

    char *outFilename;
    FILE *outFile;
    float maxValue;

    SonicWaveMode mode;

    long requiredSamplingRate;
    int  requiredNumChannels;

    int eof_flag;
    long samplesWritten;

    float *outBuffer;
    int outBufferSize;
    int outBufferPos;
    int dataIn_OutBuffer;

    short *inWaveBuffer;
    float *inBuffer;
    int   inBufferSize;         // number of data (not samples) in inBuffer
    int   dataIn_InBuffer;
    long  inBufferBaseIndex;    // sample index at beginning of inBuffer
    long  nextReadIndex;
};


typedef void (* Sonic_TransferFunction) ( double f, double &zr, double &zi );


class Sonic_FFT_Filter
{
public:
    Sonic_FFT_Filter ( 
        int _numChannels, 
        long _samplingRate,
        long _fftSize, 
        Sonic_TransferFunction _xfer,
        double _freqShift );         

    ~Sonic_FFT_Filter();

    double filter ( int channel, double value );

private:
    int   numChannels;
    long  samplingRate;
    long  fftSize;
    long  halfSize;
    long  binShift;
    int   index;
    Sonic_TransferFunction   xfer;
    double  **inBuffer;
    double  **outBuffer1;
    double  **outBuffer2;
    double  envinit[2];
    double  envmix[2];
    double  envelope[3];        // cosine envelope generator buffer
    double  envelope_coeff;     // cosine generator coefficient
    double  *freqReal;
    double  *freqImag;
    double  *timeImag;
};


// intrinsic functions for the Sonic language...

double Sonic_Noise ( double amplitude );
inline double Sonic_Square ( double x )  { return x * x; }
inline double Sonic_Cube   ( double x )  { return x * x * x; }
inline double Sonic_Quart  ( double x )  { double x2=x*x; return x2*x2; }
inline double Sonic_Recip  ( double x )  { return 1.0 / x; }
inline double Sonic_dB     ( double x )  { return pow ( 10.0, x/20.0 ); }

#endif // __ddc_sonic_runtime
/*--- end of file sonic.h ---*/