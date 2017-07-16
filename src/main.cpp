/*=======================================================================

    main.cpp  -  Copyright (C) 1998 by Don Cross <dcross@intersrv.com>

    Main source file for Sonic/C++ translator.

    Revision history:

1998 September 9 [Don Cross]
    Added function 'SonicGenCleanup', which gets called whenever
    a SonicParseException is caught.  Now, this function simply
    erases the output C++ source file, if it exists.

=======================================================================*/
#include <iomanip.h>
#include <fstream.h>
#include <stdio.h>

#include "scan.h"
#include "parse.h"


void SonicGenCleanup();


int main ( int argc, char *argv[] )
{
    cout << "Sonic/C++ translator - Copyright (C) 1998 by Don Cross <dcross@intersrv.com>\n";
    cout << "Version " << SONIC_VERSION << ", released on " << SONIC_RELEASE_DATE << ".\n";
    cout << "http://www.intersrv.com/~dcross/sonic" << "\n\n";

    if ( argc != 2 )
    {
        cerr << "Use:  SONIC sourceFile\n\n";
        return 1;
    }

    const char *filename = argv[1];
    ifstream input ( filename, ios::nocreate );
    if ( !input )
    {
        cerr << "Error:  Cannot open source file '" << filename << "'" << endl;
        return 1;
    }

    int retcode = 0;
    try
    {
        SonicScanner scanner ( input, filename );
        SonicParse_Program program;
        program.parse ( scanner );
        program.generateCode();
    }
    catch ( const SonicParseException &spe )
    {
        cerr << spe;
        SonicGenCleanup();
        return 1;
    }
    catch ( const char *message )
    {
        cerr << message << endl;
        SonicGenCleanup();
        return 1;
    }
    catch ( ... )
    {
        cerr << "Error:  Unknown exception received!" << endl;
        throw;
    }

    cout << "Translation completed successfully." << endl;
    return 0;
}


void SonicGenCleanup()
{
    // If we get here, it means that the Sonic/C++ translation has failed,
    // most likely due to a syntax error in the source file.  Therefore,
    // it's a good idea to erase the output C++ file to make sure the user
    // doesn't accidentally try to use it.

    const SonicToken &programName = SonicParse_Program::GetCurrentProgramName();
    const char *name = programName.queryToken();
    if ( name && *name )
    {
        char source [256];
        sprintf ( source, "%s.cpp", name );
        remove ( source );
    }
}


/*--- end of file main.cpp ---*/