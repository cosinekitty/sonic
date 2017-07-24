/*============================================================================

    fftmisc.cpp  -  Don Cross <dcross@intersrv.com>

    http://www.intersrv.com/~dcross/fft.html

    Helper routines for Fast Fourier Transform implementation.
    Contains common code for fft_float() and fft_double().

    Revision history:

1998 September 23 [Don Cross]
    Converted from C to C++ and prepared for use in Sonic runtime library.

1998 September 19 [Don Cross]
    Improved the efficiency of IsPowerOfTwo().
    Updated coding standards.

============================================================================*/

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include <fourier.h>

#define BITS_PER_WORD   (sizeof(unsigned) * 8)


int IsPowerOfTwo(unsigned x)
{
    return (x >= 2) && !(x & (x-1));        // Thanks to 'byang' for this cute trick!
}


unsigned NumberOfBitsNeeded(unsigned PowerOfTwo)
{
    unsigned i;

    if (PowerOfTwo < 2)
    {
        fprintf(
            stderr,
            ">>> Error in fftmisc.c: argument %d to NumberOfBitsNeeded is too small.\n",
            PowerOfTwo);

        exit(1);
    }

    for (i=0; ; i++)
    {
        if (PowerOfTwo & (1 << i))
            return i;
    }
}



unsigned ReverseBits(unsigned index, unsigned NumBits)
{
    unsigned i, rev;

    for (i=rev=0; i < NumBits; i++)
    {
        rev = (rev << 1) | (index & 1);
        index >>= 1;
    }

    return rev;
}


double Index_to_frequency(unsigned NumSamples, unsigned Index)
{
    if (Index >= NumSamples)
        return 0.0;
    else if (Index <= NumSamples/2)
        return (double)Index / (double)NumSamples;

    return -(double)(NumSamples-Index) / (double)NumSamples;
}


/*--- end of file fftmisc.cpp ---*/
