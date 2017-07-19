/*=======================================================================

    main.cpp  -  Copyright (C) 1998 by Don Cross <cosinekitty@gmail.com>

    Main source file for Sonic/C++ translator.

    Revision history:

1998 October 2 [Don Cross]
    Adding suport for multiple source files.

1998 September 9 [Don Cross]
    Added function 'SonicGenCleanup', which gets called whenever
    a SonicParseException is caught.  Now, this function simply
    erases the output C++ source file, if it exists.

=======================================================================*/
#include <iomanip>
#include <iostream>
#include <fstream>
#include <stdio.h>

#include "scan.h"
#include "parse.h"


void SonicGenCleanup();


int main ( int argc, char *argv[] )
{
    std::cout << "Sonic/C++ translator - Copyright (C) 1998 by Don Cross <cosinekitty@gmail.com>\n";
    std::cout << "Version " << SONIC_VERSION << ", released on " << SONIC_RELEASE_DATE << ".\n";
    std::cout << "https://github.com/cosinekitty/sonic" << "\n\n";

    if ( argc < 2 )
    {
        std::cerr << "Use:  SONIC sourcefile [sourcefile...]\n\n";
        return 1;
    }

    int retcode = 0;
    try
    {
        SonicParse_Program program;
        for ( int k=1; k < argc; ++k )
        {
            const char *filename = argv[k];
			std::ifstream input(filename);
            if ( !input )
            {
				std::cerr << "Error:  Cannot open source file '" << filename << "'" << std::endl;
                return 1;
            }
            SonicScanner scanner ( input, filename );
            program.parse ( scanner );
            input.close();
        }

        program.validate();
        program.generateCode();
    }
    catch ( const SonicParseException &spe )
    {
        std::cerr << spe;
        SonicGenCleanup();
        return 1;
    }
    catch ( const char *message )
    {
        std::cerr << message << std::endl;
        SonicGenCleanup();
        return 1;
    }
    catch ( ... )
    {
        std::cerr << "Error:  Unknown exception received!" << std::endl;
        throw;
    }

    std::cout << "Translation completed successfully." << std::endl;
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
