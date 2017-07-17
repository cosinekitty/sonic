/*==========================================================================

    sonic.cpp  -  Copyright (C) 1998 by Don Cross <dcross@intersrv.com>

    Runtime library for Sonic/C++ programs.

    Revision history:

1998 September 23 [Don Cross]
    Adding support for fft filters.

==========================================================================*/
#include <stdio.h>
#include <io.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include "sonic.h"
#include "riff.h"
#include "copystr.h"
#include "fourier.h"


double ScanReal ( const char *varname, const char *vstring )
{
    double x = 0;
    if ( sscanf ( vstring, "%lf", &x ) != 1 )
    {
        fprintf ( stderr, "Error:  Cannot convert '%s' to real value for variable '%s'\n",
            vstring,
            varname );

        exit(1);
    }

    return x;
}


long ScanInteger ( const char *varname, const char *vstring )
{
    long x = 0;
    if ( sscanf ( vstring, "%ld", &x ) != 1 )
    {
        fprintf ( stderr, "Error:  Cannot convert '%s' to integer value for variable '%s'\n",
            vstring,
            varname );

        exit(1);
    }

    return x;
}


int ScanBoolean ( const char *varname, const char *vstring )
{
    int x = 0;
    if ( strcmp(vstring,"true") == 0 )
        x = 1;
    else if ( strcmp(vstring,"false") == 0 )
        x = 0;
    else
    {
        fprintf ( stderr, "Error:  Cannot convert '%s' to boolean value for variable '%s'\n",
            vstring,
            varname );

        exit(1);
    }

    return x;
}



//--------------------------------------------------------------------------

int SonicWave::NextTempTag = 0;

SonicWave::SonicWave (
    const char *_filename,
    const char *_varname,
    long _requiredSamplingRate,
    int _requiredNumChannels ):
        varname ( DDC_CopyString(_varname) ),
        inFilename ( DDC_CopyString(_filename) ),
        inWave ( 0 ),
        inFile ( 0 ),
        inNumSamples ( 0 ),
        outFilename ( 0 ),
        outFile ( 0 ),
        maxValue ( float(0) ),
        mode ( SWM_CLOSED ),
        requiredSamplingRate ( _requiredSamplingRate ),
        requiredNumChannels ( _requiredNumChannels ),
        eof_flag ( 0 ),
        outBuffer ( 0 ),
        outBufferSize ( _requiredNumChannels * _requiredSamplingRate * 5 ),
        outBufferPos ( 0 ),
        samplesWritten ( 0 ),
        dataIn_OutBuffer ( 0 ),
        inBuffer ( 0 ),
        inBufferSize ( _requiredNumChannels * 256 ),
        inBufferBaseIndex ( 0 ),
        dataIn_InBuffer ( 0 ),
        nextReadIndex ( 0 )
{
    outBuffer = new float [outBufferSize];
    inBuffer = new float [inBufferSize];
    inWaveBuffer = new short [inBufferSize];

    if ( !varname || !inFilename || !outBuffer || !inBuffer || !inWaveBuffer )
    {
        fprintf ( stderr, "Out of memory creating Sonic variable '%s'\n", _varname );
        exit(1);
    }

    if ( requiredNumChannels < 1 || requiredNumChannels > MAX_SONIC_CHANNELS )
    {
        fprintf ( stderr,
            "Invalid number of channels %d creating Sonic variable '%s'\n",
            requiredNumChannels );

        exit(1);
    }

    determineNumSamples();
}


SonicWave::~SonicWave()
{
    close();

    if ( outBuffer )
    {
        delete[] outBuffer;
        outBuffer = 0;
    }

    if ( inBuffer )
    {
        delete[] inBuffer;
        inBuffer = 0;
    }

    if ( inWaveBuffer )
    {
        delete[] inWaveBuffer;
        inWaveBuffer = 0;
    }

    inBufferSize = 0;
    outBufferSize = outBufferPos = 0;

    DDC_DeleteString (varname);
    DDC_DeleteString (inFilename);
    DDC_DeleteString (outFilename);
}


void SonicWave::determineNumSamples()
{
    // Try to determine number of samples, but don't fuss if not possible.

    if ( mode != SWM_CLOSED )
        return;

    inNumSamples = 0;

    if ( inFilename[0] == '\0' )
        return;

    char peek[4] = { 0, 0, 0, 0 };
    FILE *temp = fopen ( inFilename, "rb" );
    if ( !temp )
        return;

    long fsize = filelength ( fileno(temp) );
    fread ( peek, 1, 4, temp );
    fclose (temp);

    if ( memcmp(peek,"RIFF",4) == 0 )
    {
        WaveFile tempWave;
        DDCRET rc = tempWave.OpenForRead ( inFilename );
        if ( rc != DDC_SUCCESS )
            return;

        inNumSamples = tempWave.NumSamples();
        tempWave.Close();
    }
    else
    {
        inNumSamples = (fsize/sizeof(float) - 1) / requiredNumChannels;
    }
}


void SonicWave::openForRead()
{
    samplesWritten = 0;
    dataIn_OutBuffer = 0;
    dataIn_InBuffer = 0;
    inBufferBaseIndex = 0;
    nextReadIndex = 0;

    if ( mode != SWM_CLOSED )
    {
        fprintf ( stderr, "Error:  tried to open non-closed variable '%s' for read\n", varname );
        exit(1);
    }

    mode = SWM_UNDEFINED;

    // First try to open the file as a WAV file...

    inWave = new WaveFile;
    if ( !inWave )
    {
        fprintf ( stderr, "Out of memory trying to open Sonic variable '%s' for read\n", varname );
        exit(1);
    }

    if ( !inFilename )
    {
        fprintf ( stderr, "Error: tried to open '%s' with undefined input filename.\n", varname );
        exit(1);
    }

    char peek[4] = { 0, 0, 0, 0 };
    FILE *temp = fopen ( inFilename, "rb" );
    if ( !temp )
    {
        fprintf ( stderr, "Error:  variable '%s' cannot open file '%s' for read\n",
            varname,
            inFilename );

        exit(1);
    }

    fread ( peek, 1, 4, temp );
    fclose (temp);

    if ( memcmp ( peek, "RIFF", 4 ) == 0 )
    {
        DDCRET rc = inWave->OpenForRead ( inFilename );
        if ( rc != DDC_SUCCESS )
        {
            fprintf ( stderr, "Error:  variable '%s' cannot open WAV file '%s' for read\n",
                varname,
                inFilename );

            exit(1);
        }

        if ( inWave->BitsPerSample() != 16 )
        {
            fprintf ( stderr, "Error:  variable '%s' WAV file must be 16-bit.\n", varname );
            exit(1);
        }

        if ( inWave->NumChannels() != requiredNumChannels )
        {
            fprintf ( stderr, "Error:  variable '%s' must have %d channel%s.\n",
                varname,
                requiredNumChannels,
                (requiredNumChannels == 1) ? "" : "s" );

            exit(1);
        }

        if ( long(inWave->SamplingRate()) != requiredSamplingRate )
        {
            fprintf ( stderr, "Error: variable '%s' must have sampling rate = %ld.\n",
                varname,
                requiredSamplingRate );

            exit(1);
        }

        maxValue = float(1);
        inNumSamples = inWave->NumSamples();
    }
    else
    {
        // Maybe the file exists, but it just isn't a WAV file.
        // Try opening as a float file.

        delete inWave;
        inWave = 0;
        inFile = fopen ( inFilename, "rb" );
        if ( !inFile )
        {
            fprintf ( stderr, "Error:  Cannot open variable '%s' file '%s' for read\n",
                varname,
                inFilename );

            exit(1);
        }

        if ( fread ( &maxValue, sizeof(float), 1, inFile ) != 1 )
        {
            fprintf ( stderr, "Error:  Invalid file '%s' trying to open variable '%s' for read.\n",
                inFilename,
                varname );

            exit(1);
        }

        if ( maxValue < 1.0e-30 )
            maxValue = float(1);

        long fsize = filelength ( fileno(inFile) );
        inNumSamples = (fsize/sizeof(float) - 1) / requiredNumChannels;
    }

    mode = SWM_READ;
    eof_flag = 0;
}


void SonicWave::openForWrite()
{
    samplesWritten = 0;
    dataIn_OutBuffer = 0;

    if ( mode != SWM_CLOSED && mode != SWM_PREMODIFY )
    {
        fprintf ( stderr,
            "Error:  Attempt to open non-closed variable '%s' for write/modify\n",
            varname );

        exit(1);
    }

    char tempFilename [256];
    sprintf ( tempFilename, "s$%d.tmp", NextTempTag++ );
    outFilename = DDC_CopyString(tempFilename);
    if ( !outFilename )
    {
        fprintf ( stderr,
            "Error:  Out of memory opening output file '%s' for variable '%s'\n",
            tempFilename,
            varname );

        exit(1);
    }

    outFile = fopen ( outFilename, "w+b" );
    if ( !outFile )
    {
        fprintf ( stderr,
            "Error:  Cannot open output file '%s' for variable '%s'\n",
            outFilename,
            varname );

        exit(1);
    }

    maxValue = float(0);
    if ( fwrite ( &maxValue, sizeof(float), 1, outFile ) != 1 )
    {
        fprintf ( stderr,
            "Error:  Cannot initialize output file '%s' for variable '%s'\n",
            outFilename,
            varname );

        exit(1);
    }

    mode = SWM_WRITE;
}


void SonicWave::openForAppend()
{
    samplesWritten = 0;
    dataIn_OutBuffer = 0;

    if ( mode != SWM_CLOSED && mode != SWM_PREMODIFY )
    {
        fprintf ( stderr,
            "Error:  Attempt to open non-closed variable '%s' for write/modify\n",
            varname );

        exit(1);
    }

    if ( !inFilename )
    {
        fprintf ( stderr, "Cannot append to variable '%s':  filename unknown\n", varname );
        exit(1);
    }

    DDC_DeleteString ( outFilename );
    outFilename = inFilename;
    inFilename = 0;

    outFile = fopen ( outFilename, "r+b" );
    if ( !outFile )
    {
        openForWrite();
        return;
    }

    maxValue = float(0);
    if ( fread ( &maxValue, sizeof(float), 1, outFile ) != 1 )
    {
        fprintf ( stderr,
            "Error:  Cannot initialize append file '%s' for variable '%s'\n",
            outFilename,
            varname );

        exit(1);
    }

    if ( memcmp(&maxValue,"RIFF",4) == 0 )
    {
        fprintf ( stderr,
            "Error:  var='%s' ... appending directly to WAV file not yet supported!\n",
            varname );

        exit(1);
    }

    if ( fseek ( outFile, 0, SEEK_END ) )
    {
        fprintf ( stderr,
            "Error:  Could not seek to end of file '%s' for append variable '%s'\n",
            outFilename,
            varname );

        exit(1);
    }

    mode = SWM_WRITE;
}



void SonicWave::openForModify()
{
    openForRead();
    mode = SWM_PREMODIFY;
    openForWrite();
    mode = SWM_MODIFY;
}


void SonicWave::read ( double sample[] )
{
    if ( mode != SWM_READ && mode != SWM_MODIFY )
    {
        fprintf ( stderr, "Error:  Attempt to read from improperly opened variable '%s'\n", varname );
        exit(1);
    }

    long pastLastIndex = inBufferBaseIndex + dataIn_InBuffer/requiredNumChannels;
    if ( nextReadIndex >= inBufferBaseIndex && nextReadIndex < pastLastIndex )
    {
        int p = requiredNumChannels * (nextReadIndex - inBufferBaseIndex);

        for ( int c=0; c < requiredNumChannels; ++c )
            sample[c] = double ( inBuffer[p++] );

        ++nextReadIndex;
        return;
    }

    if ( inWave )
    {
        DDCRET rc = DDC_FAILURE;

        if ( !eof_flag )
        {
            inBufferBaseIndex = nextReadIndex;

            dataIn_InBuffer = inBufferSize;
            int dataRemaining =
                requiredNumChannels * (inNumSamples - nextReadIndex);

            if ( dataIn_InBuffer > dataRemaining )
                dataIn_InBuffer = dataRemaining;

            if ( dataIn_InBuffer > 0 )
                rc = inWave->ReadData ( inWaveBuffer, dataIn_InBuffer );
        }

        if ( rc == DDC_SUCCESS )
        {
            for ( int i=0; i < dataIn_InBuffer; ++i )
                inBuffer[i] = float ( inWaveBuffer[i] / 32768.0 );

            for ( int c=0; c < requiredNumChannels; ++c )
                sample[c] = float ( inBuffer[c] );
        }
        else
        {
            eof_flag = 1;
            for ( int c=0; c < requiredNumChannels; ++c )
                sample[c] = double(0);
        }

        ++nextReadIndex;
    }
    else if ( inFile )
    {
        if ( !eof_flag )
        {
            inBufferBaseIndex = nextReadIndex;

            dataIn_InBuffer = fread (
                inBuffer, sizeof(float), inBufferSize, inFile );

            eof_flag = (dataIn_InBuffer < inBufferSize);

            if ( eof_flag )
            {
                for ( int c=0; c < requiredNumChannels; ++c )
                    sample[c] = double(0);
            }
            else
            {
                for ( int c=0; c < requiredNumChannels; ++c )
                    sample[c] = inBuffer[c];
            }
        }
        else
        {
            for ( int c=0; c < requiredNumChannels; ++c )
                sample[c] = double(0);
        }

        ++nextReadIndex;
    }
    else
    {
        fprintf ( stderr, "Internal error:  variable '%s' not opened for read!\n", varname );
        exit(1);
    }
}


void SonicWave::write ( const double sample[] )
{
    if ( mode != SWM_WRITE && mode != SWM_MODIFY )
    {
        fprintf ( stderr, "Error:  Attempt to write to improperly opened variable '%s'\n", varname );
        exit(1);
    }

    if ( !outFile )
    {
        fprintf ( stderr, "Internal error:  Output file not open for variable '%s'\n", varname );
        exit(1);
    }

    for ( int c=0; c < requiredNumChannels; ++c )
    {
        float value = outBuffer[outBufferPos] = float(sample[c]);
        if ( value < 0 )
            value = -value;

        if ( value > maxValue )
            maxValue = value;

        if ( ++outBufferPos >= outBufferSize )
        {
            int numWritten = fwrite ( outBuffer, sizeof(float), outBufferSize, outFile );
            if ( numWritten != outBufferSize )
            {
                fprintf ( stderr, "Error writing variable '%s' data to file '%s'.  (disk full?)\n",
                    varname,
                    outFilename );

                exit(1);
            }

            outBufferPos = 0;
        }

        if ( dataIn_OutBuffer < outBufferSize )
            ++dataIn_OutBuffer;
    }

    ++samplesWritten;
}


double SonicWave::interp ( int c, double i, int &countdown )
{
    int tempCountdown = 2;
    long ibase = long(i);
    double y1 = fetch ( c, ibase,   tempCountdown );
    double y2 = fetch ( c, 1+ibase, tempCountdown );

    if ( tempCountdown != 2 )
        --countdown;

    double frac = i - double(ibase);
    double y = y1*(1-frac) + y2*frac;
    return y;
}


double SonicWave::fetch ( int c, long i, int &countdown )
{
    if ( mode == SWM_WRITE )
    {
        if ( i >= samplesWritten || i < 0 )
        {
            --countdown;
            return double(0);
        }

        long numDataBack = (samplesWritten - i) * requiredNumChannels;
        if ( numDataBack <= dataIn_OutBuffer )
        {
            int index = (outBufferSize + outBufferPos - numDataBack) % outBufferSize;
            return outBuffer[index + c];
        }
        else
        {
            long currentPos = ftell(outFile);
            long backward = sizeof(float) * (1 + i*requiredNumChannels + c);
            if ( fseek ( outFile, backward, SEEK_SET ) )
            {
                fprintf ( stderr,
                    "Error:  Could not seek backward to sample %ld for variable '%s' file '%s'\n",
                    i,
                    varname,
                    outFilename );

                exit(1);
            }

            float temp = float(0);
            if ( fread ( &temp, sizeof(float), 1, outFile ) != 1 )
            {
                fprintf ( stderr,
                    "Error:  Could not read backward sample %ld from variable '%s' file '%s'\n",
                    i,
                    varname,
                    outFilename );

                exit(1);
            }

            fseek ( outFile, currentPos, SEEK_SET );
            return double(temp);
        }
    }
    else if ( mode != SWM_READ && mode != SWM_MODIFY )
    {
        fprintf ( stderr, "Error:  Tried to fetch sample from improperly opened variable '%s'\n", varname );
        exit(1);
    }

    if ( i >= inNumSamples || i < 0 )
    {
        --countdown;
        return double(0);
    }

    long pastLastIndex = inBufferBaseIndex + dataIn_InBuffer/requiredNumChannels;

    if ( i >= inBufferBaseIndex && i < pastLastIndex )
    {
        int p = requiredNumChannels * (i - inBufferBaseIndex);
        return inBuffer[p+c];
    }

    nextReadIndex = i;

    if ( inWave )
    {
        DDCRET rc = inWave->SeekToSample ( i );
        if ( rc != DDC_SUCCESS )
        {
            fprintf ( stderr, "Error seeking to sample %ld in WAV file '%s' for variable '%s'\n",
                i,
                inFilename,
                varname );

            exit(1);
        }
    }
    else if ( inFile )
    {
        if ( fseek ( inFile, sizeof(float) * (i*requiredNumChannels + 1), SEEK_SET ) == EOF )
        {
            fprintf ( stderr, "Error performing seek to sample %ld in float file '%s' for variable '%s'\n",
                i,
                inFilename,
                varname );

            exit(1);
        }
    }
    else
    {
        fprintf ( stderr, "Error:  tried to fetch sample %ld for improperly opened variable '%s'\n",
            i,
            varname );

        exit(1);
    }

    double sample [MAX_SONIC_CHANNELS];
    read ( sample );

    return sample[c];
}


void SonicWave::close()
{
    if ( outFile )
    {
        if ( outBufferPos > 0 )
        {
            int numWritten = fwrite ( outBuffer, sizeof(float), outBufferPos, outFile );
            if ( numWritten != outBufferPos )
            {
                fprintf ( stderr, "Error flushing last output buffer for variable '%s'\n",
                    varname );

                exit(1);
            }
        }

        fflush ( outFile );
        if ( fseek ( outFile, 0, SEEK_SET ) )
        {
            fprintf ( stderr, "Error seeking to beginning of output file '%s' for variable '%s'\n",
                outFilename,
                varname );

            fprintf ( stderr, "(Trying to backpatch maxValue = %f)\n", maxValue );
            exit(1);
        }

        if ( fwrite ( &maxValue, sizeof(float), 1, outFile ) != 1 )
        {
            fprintf ( stderr, "Error writing maxValue=%f to beginning of file '%s' for variable '%s'\n",
                maxValue,
                outFilename,
                varname );

            exit(1);
        }

        fclose ( outFile );
        outFile = 0;
    }

    outBufferPos = 0;

    if ( inWave )
    {
        inWave->Close();
        delete inWave;
        inWave = 0;
    }

    if ( inFile )
    {
        fclose ( inFile );
        inFile = 0;
        if ( mode == SWM_MODIFY )
            remove ( inFilename );
    }

    if ( mode == SWM_WRITE || mode == SWM_MODIFY )
    {
        // prepare to read from data just written...

        DDC_DeleteString ( inFilename );
        inFilename = outFilename;
        outFilename = 0;

        inNumSamples = samplesWritten;
    }

    mode = SWM_CLOSED;
    eof_flag = 0;
}


void SonicWave::convertToWav ( const char *outWaveFilename )
{
    openForRead();

    if ( !inWave )
    {
        // Only convert to WAV file if it isn't a WAV file already.

        WaveFile outWave;
        DDCRET rc = outWave.OpenForWrite (
            outWaveFilename,
            requiredSamplingRate,
            16,
            requiredNumChannels );

        if ( rc != DDC_SUCCESS )
        {
            fprintf ( stderr,
                "Error:  Cannot open permanent output WAV file '%s' for variable '%s'",
                outWaveFilename,
                varname );

            exit(1);
        }

        const double scale = 32000.0 / maxValue;
        const int bufferSize = 512;
        int numDataRemaining = inNumSamples * requiredNumChannels;
        float inBuffer [bufferSize];
        INT16 outBuffer [bufferSize];

        while ( numDataRemaining > 0 )
        {
            int dataToRead = bufferSize;
            if ( dataToRead > numDataRemaining )
                dataToRead = numDataRemaining;

            int numRead = (int) fread ( inBuffer, sizeof(float), dataToRead, inFile );
            if ( numRead != dataToRead )
            {
                fprintf ( stderr,
                    "Error reading from file '%s' while converting variable '%s' to WAV file\n",
                    inFilename,
                    varname );

                exit(1);
            }

            for ( int i=0; i < dataToRead; ++i )
                outBuffer[i] = INT16 ( inBuffer[i] * scale );

            DDCRET rc = outWave.WriteData ( outBuffer, dataToRead );
            if ( rc != DDC_SUCCESS )
            {
                fprintf ( stderr,
                    "Error writing to WAV file '%s' while converting variable '%s'\n",
                    outWaveFilename,
                    varname );

                exit(1);
            }

            numDataRemaining -= dataToRead;
        }

        outWave.Close();
    }

    close();
}


void SonicWave::EraseAllTempFiles()
{
    char filename [256];
    for ( int i=0; i < NextTempTag; i++ )
    {
        sprintf ( filename, "s$%d.tmp", i );
        remove ( filename );
    }
}


//------------------------------------------------------------------------------


double Sonic_Noise ( double amplitude )
{
    static unsigned long array[] = 
    {
        0x3847a384,     
        0x56af9029,     
        0xc3852109, 
        0x01835567,
        0x58927374,
        0x77733935,
        0xabcdef09,
        0x19258761,
        0x58585716,
        0xd08f0ea0,
        0x44a5face,
        0xc0feeba6,
        0x67860a38,
        0x45871265,
        0x9fbc0e38,
        0x35175722,
        0x45787162
    };

    const int ARRAY_SIZE = sizeof(array) / sizeof(array[0]);
    static int index = 0;
    static int firstTime = 1;
    if ( firstTime )
    {
        firstTime = 0;
        unsigned long now = time(0);
        array[0] ^= (now << 24) | (now >> 8);
        array[1] += (array[0] >> 16) * (now & 0xffff);

        for ( int k=0; k < 8; ++k )
        {
            for ( int i=0; i < ARRAY_SIZE; ++i )
            {
                unsigned long rot =
                    array[(i+13)%ARRAY_SIZE];

                array[i] += 
                    (array[(i+7)%ARRAY_SIZE] ^ array[(i+22)%ARRAY_SIZE]) +
                    ((rot << 3) | (rot >> 29));
            }
        }
    }

    if ( ++index >= ARRAY_SIZE )
        index = 0;

    unsigned long a1 = array [(index +  7) % ARRAY_SIZE];
    unsigned long a2 = array [(index +  8) % ARRAY_SIZE];
    unsigned long a3 = array [(index + 13) % ARRAY_SIZE];
    unsigned long a4 = array [(index +  4) % ARRAY_SIZE];
    unsigned long a5 = array [(index + 10) % ARRAY_SIZE];

    unsigned long x = array[index] += 
        (a4 ^ a3) + 
        ((a5 << 3) | (a5 >> 29)) +
        ((a1 & 0xffff) * ((a2 >> 3) & 0xffff));

    double r = double(x) / 2147483648.0;
    return amplitude * (1.0 - r);
}


//------------------------------------------------------------------------------


static const double Pi = 4.0 * atan(1.0);


Sonic_FFT_Filter::Sonic_FFT_Filter ( 
    int _numChannels, 
    long _samplingRate,
    long _fftSize, 
    Sonic_TransferFunction _xfer,
    double _freqShift ):
        numChannels ( _numChannels ),
        samplingRate ( _samplingRate ),
        fftSize ( _fftSize ),
        halfSize ( fftSize / 2 ),
        index ( fftSize / 2 ),
        xfer ( _xfer )
{
    if ( !xfer )
    {
        fprintf ( stderr, "Error:  FFT filter transfer function is NULL!\n\n" );
        exit(1);
    }

    if ( !IsPowerOfTwo(fftSize) )
    {
        fprintf ( stderr, "Error:  FFT filter size %ld is not a power of two.\n\n", fftSize );
        exit(1);
    }

    double xBinShift = _freqShift * double(fftSize) / double(samplingRate);
    if ( xBinShift <= double(-halfSize) || xBinShift >= double(halfSize) )
        binShift = halfSize;
    else
        binShift = long(floor(0.5 + xBinShift));

    binShift &= ~1;   // force to be even number of frequency bins

    double radiansPerSample = Pi / double(halfSize - 1);
    envinit[0] = envelope[0] = cos ( -2.0 * radiansPerSample );
    envinit[1] = envelope[1] = cos ( -radiansPerSample );
    envelope_coeff = 2.0 * envelope[1];
    envelope[2] = double(0);
    envmix[0] = envmix[1] = double(0);

    const char *memerr = "Out of memory constructing FFT filter\n\n";
    inBuffer    =  new double * [numChannels];
    outBuffer1  =  new double * [numChannels];
    outBuffer2  =  new double * [numChannels];

    freqReal = new double [fftSize];
    freqImag = new double [fftSize];
    timeImag = new double [fftSize];

    if ( !inBuffer || !outBuffer1 || !outBuffer2 || !freqReal || !freqImag || !timeImag )
    {
        fprintf ( stderr, memerr );
        exit(1);
    }

    for ( int c=0; c < numChannels; ++c )
    {
        inBuffer[c]   =  new double [fftSize];
        outBuffer1[c] =  new double [fftSize];
        outBuffer2[c] =  new double [fftSize];
        if ( !inBuffer[c] || !outBuffer1[c] || !outBuffer2[c] )
        {
            fprintf ( stderr, memerr );
            exit(1);
        }

        for ( int i=0; i < fftSize; ++i )
            inBuffer[c][i] = outBuffer1[c][i] = outBuffer2[c][i] = double(0);
    }

    for ( int i=0; i < fftSize; ++i )
        freqReal[i] = freqImag[i] = timeImag[i] = double(0);
}


Sonic_FFT_Filter::~Sonic_FFT_Filter()
{
    if ( inBuffer )
    {
        for ( int c=0; c < numChannels; ++c )
            delete[] inBuffer[c];

        delete[] inBuffer;
        inBuffer = 0;
    }   

    if ( outBuffer1 )
    {
        for ( int c=0; c < numChannels; ++c )
            delete[] outBuffer1[c];

        delete[] outBuffer1;
        outBuffer1 = 0;
    }   

    if ( outBuffer2 )
    {
        for ( int c=0; c < numChannels; ++c )
            delete[] outBuffer2[c];

        delete[] outBuffer2;
        outBuffer2 = 0;
    }

    if ( freqReal )
    {
        delete[] freqReal;
        freqReal = 0;
    }

    if ( freqImag )
    {
        delete[] freqImag;
        freqImag = 0;
    }

    if ( timeImag )
    {
        delete[] timeImag;
        timeImag = 0;
    }
}


double Sonic_FFT_Filter::filter ( 
    int channel, 
    double value )
{
    if ( index >= fftSize )
    {
        // We've hit the trigger point!

        double **swap = outBuffer1;     // swap old buffer with new buffer
        outBuffer1 = outBuffer2;
        outBuffer2 = swap;

        double delta_freq = samplingRate * Index_to_frequency(fftSize,1);
        for ( int c=0; c < numChannels; ++c )
        {
            fft_double ( fftSize, 0, inBuffer[c], 0, freqReal, freqImag );   // forward FFT

            double freq = double(0);
            for ( int i=0; i <= halfSize; ++i )
            {
                (*xfer) ( freq, freqReal[i], freqImag[i] );
                freq += delta_freq;
            }

            if ( binShift > 0 )
            {
                // scoot frequencies up...

                for ( int i=halfSize; i >= binShift; --i )
                {
                    freqReal[i] = freqReal[i-binShift];
                    freqImag[i] = freqImag[i-binShift];
                }

                for ( int i=halfSize; i >= 0; --i )
                    freqReal[i] = freqImag[i] = double(0);
            }
            else if ( binShift < 0 )
            {
                // scoot frequencies down...

                long limit = halfSize + binShift;
                for ( int i=0; i <= limit; ++i )
                {
                    freqReal[i] = freqReal[i-binShift];
                    freqImag[i] = freqImag[i-binShift];
                }

                for ( int i=0; i <= halfSize; ++i )
                    freqReal[i] = freqImag[i] = double(0);
            }

            // Because we are dealing with real-valued time signals,
            // we know that the negative frequency components are just
            // the complex conjugates of the positive frequency components.

            for ( int i = halfSize + 1; i < fftSize; ++i )
            {
                freqReal[i] =  freqReal[fftSize - i];
                freqImag[i] = -freqImag[fftSize - i];
            }

            fft_double ( fftSize, 1, freqReal, freqImag, outBuffer2[c], timeImag );   // inverse FFT

            for ( int i=0; i < halfSize; ++i )
                inBuffer[c][i] = inBuffer[c][i + halfSize];
        }       

        index = halfSize;
        envelope[0] = envinit[0];
        envelope[1] = envinit[1];
    }

    inBuffer[channel][index] = value;

    if ( channel == 0 )
    {
        envelope[2] = envelope_coeff*envelope[1] - envelope[0];
        envelope[0] = envelope[1];
        envelope[1] = envelope[2];
        envmix[0] = (1.0 + envelope[2]) / 2.0;
        envmix[1] = 1.0 - envmix[0];
    }

    double mix = 
        envmix[0] * outBuffer1[channel][index] +
        envmix[1] * outBuffer2[channel][index - halfSize];

    if ( channel == numChannels-1 )
        ++index;

    return mix;
}


/*--- end of file sonic.cpp ---*/