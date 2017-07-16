/*=============================================================================

    prog.cpp  -  Copyright (C) 1998 by Don Cross <dcross@intersrv.com>

    This module implements the class SonicParse_Program, which represents
    the entire parsed input Sonic source code.  This class represents the
    high-level interface for translating Sonic into C++.

    Revision history:

1998 September 19 [Don Cross]
    Added SonicParse_Program::interpolateFlag and interpolateFlag_explicit.
    These allow the programmer to specify whether samples should be fetched
    from the nearest sample index or should be interpolated linearly 
    between the nearest two samples.
    Deleted bitsPerSample and bitsPerSample_explicit.
    Was not checking the values of the ..._explicit members, which means
    that the programmer was able to define them more than once.  Now that
    is fixed so that an error occurs if any of 'r', 'm', or 'interpolate'
    is defined more than once.

=============================================================================*/
#include <fstream.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "scan.h"
#include "parse.h"


SonicParse_Program::SonicParse_Program():
    samplingRate ( 44100 ),
    samplingRate_explicit ( false ),
    numChannels ( 2 ),
    numChannels_explicit ( false ),
    interpolateFlag ( true ),
    interpolateFlag_explicit ( false ),
    programBody ( 0 ),
    functionBodyList ( 0 ),
    importList ( 0 ),
    globalVars ( 0 )
{
}


SonicParse_Program::~SonicParse_Program()
{
    if ( programBody )
    {
        delete programBody;
        programBody = 0;
    }

    if ( functionBodyList )
    {
        delete functionBodyList;
        functionBodyList = 0;
    }

    if ( globalVars )
    {
        delete globalVars;
        globalVars = 0;
    }
}


SonicParse_Function *SonicParse_Program::findFunction ( const SonicToken &funcName ) const
{
    for ( SonicParse_Function *fp = functionBodyList; fp; fp = fp->next )
    {
        if ( fp->queryName() == funcName )
            return fp;
    }

    if ( programBody && programBody->queryName() ==  funcName )
        return programBody;

    throw SonicParseException ( "undefined function", funcName );
    return 0;
}


SonicParse_Function *SonicParse_Program::findImportVar ( 
    const SonicToken &varname, 
    SonicParse_Function *enclosingFunc ) const
{
    SonicParse_VarDecl *vp = findSymbol ( varname, enclosingFunc, false );
    if ( vp )
    {
        if ( vp->queryType() == STYPE_IMPORT )
        {
            const SonicToken *iname = vp->queryType().queryImportName();
            if ( !iname )
                throw SonicParseException ( "internal error: import name not found!", varname );

            return findImportType ( *iname );
        }
        else
            throw SonicParseException ( "this variable is not an import function", varname );
    }

    return 0;
}


void SonicParse_Program::clearAllResetFlags()
{
    for ( SonicParse_VarDecl *vp = globalVars; vp; vp = vp->queryNext() )
        vp->modifyResetFlag ( false );
}


SonicParse_Function *SonicParse_Program::findImportType ( const SonicToken &importName ) const
{
    for ( SonicParse_Function *fp = importList; fp; fp = fp->next )
    {
        if ( fp->queryName() == importName )
            return fp;
    }

    return 0;
}


SonicParse_VarDecl *SonicParse_Program::findGlobalVar ( const SonicToken &name ) const
{
    for ( SonicParse_VarDecl *vp = globalVars; vp; vp = vp->queryNext() )
    {
        if ( vp->queryName() == name )
            return vp;
    }

    return 0;
}


SonicParse_VarDecl *SonicParse_Program::findSymbol ( 
    const SonicToken &name, 
    SonicParse_Function *encloser,
    bool forceFind ) const
{
    SonicParse_VarDecl *vp = 0;
    if ( encloser )
        vp = encloser->findSymbol ( name, forceFind );
    else
        vp = findGlobalVar ( name );

    if ( forceFind && !vp )
        throw SonicParseException ( "symbol not declared", name );

    return vp;
}


int SonicParse_Program::countInstances ( const SonicToken &globalVarName ) const
{
    int count = 0;
    for ( SonicParse_VarDecl *vp = globalVars; vp; vp = vp->queryNext() )
    {
        if ( vp->queryName() == globalVarName )
            ++count;
    }

    return count;
}


void SonicParse_Program::parse ( SonicScanner &scanner )
{
    if ( programBody )
        throw SonicParseException ( "internal error: program body already defined" );

    SonicToken t;
    SonicParse_Function *functionBodyTail = 0;

    while ( scanner.getToken ( t, false ) )
    {
        if ( t.queryTokenType() == STT_BUILTIN )
        {
            SonicToken v;
            if ( t == "r" || t == "m" )
            {
                const char *fussy = "expected positive integer constant";
                scanner.scanExpected ( "=" );
                scanner.getToken ( v );
                if ( v.queryTokenType() != STT_CONSTANT )
                    throw SonicParseException ( fussy, v );
                
                const char *vstring = v.queryToken();
                if ( strchr(vstring,'.') || strchr(vstring,'e') || strchr(vstring,'E') )
                    throw SonicParseException ( fussy, v );

                const long value = atol(vstring);
                if ( value <= 0 )
                    throw SonicParseException ( fussy, v );

                scanner.scanExpected ( ";" );

                if ( t == "r" )
                {
                    if ( samplingRate_explicit )
                        throw SonicParseException ( "value for 'r' has already been defined in program", t );

                    samplingRate = value;
                    samplingRate_explicit = true;
                }
                else  // t must hold "m" to get here
                {
                    if ( value > MAX_SONIC_CHANNELS )
                    {
                        char temp [128];
                        sprintf ( temp, "Maximum allowed number of channels is %d", MAX_SONIC_CHANNELS );
                        throw SonicParseException ( temp, t );
                    }

                    if ( numChannels_explicit )
                        throw SonicParseException ( "value for 'm' has already been defined in program", t );

                    numChannels = value;
                    numChannels_explicit = true;                    
                }
            }
            else if ( t == "interpolate" )
            {
                scanner.scanExpected ( "=" );
                scanner.getToken ( v );
                scanner.scanExpected ( ";" );

                if ( interpolateFlag_explicit )
                    throw SonicParseException ( "value for 'interpolate' has already been defined in program", t );

                if ( v == "true" )
                    interpolateFlag = true;
                else if ( v == "false" )
                    interpolateFlag = false;
                else
                    throw SonicParseException ( "expected 'true' or 'false'", v );

                interpolateFlag_explicit = true;
            }
            else
                throw ( "cannot assign a value to this built-in symbol", t );
        }
        else if ( t == "program" || t == "function" )
        {
            scanner.pushToken ( t );
            SonicParse_Function *body = SonicParse_Function::Parse ( scanner, *this );
            if ( body->queryIsProgramBody() )
            {
                if ( programBody )
                    throw SonicParseException ( "program body already defined", t );

                programBody = body;
                SonicParse_Program::SaveCurrentProgramName ( body->queryName() );
            }
            else
            {
                if ( functionBodyTail )
                    functionBodyTail = functionBodyTail->next = body;
                else
                    functionBodyTail = functionBodyList = body;
            }
        }
        else if ( t == "import" )
        {
            SonicParse_Function *tempList=0, *tempTail=0;

            for(;;)
            {
                SonicToken name;
                scanner.getToken ( name );
                if ( name.queryTokenType() != STT_IDENTIFIER )
                    throw SonicParseException ( "expected imported C++ class name", name );

                SonicParse_Function *newImport = new SonicParse_Function (
                    name,
                    false,
                    SonicType (STYPE_REAL),
                    0,
                    0,
                    0,
                    *this );

                if ( tempTail )
                    tempTail = tempTail->next = newImport;
                else
                    tempList = tempTail = newImport;

                scanner.getToken ( t );
                if ( t == "from" )
                {
                    SonicToken header;
                    scanner.getToken ( header );
                    if ( header.queryTokenType() != STT_STRING )
                        throw SonicParseException ( "expected C++ header filename inside double quotes", header );

                    scanner.scanExpected ( ";" );

                    for ( SonicParse_Function *fp = tempList; fp; fp = fp->next )
                        fp->importHeader = header;

                    tempTail->next = importList;
                    importList = tempList;
                    break;
                }
                else if ( t != "," )
                    throw SonicParseException ( "expected ',' or 'from'", t );
            }
        }
        else if ( t == "var" )
        {
            scanner.pushToken ( t );
            SonicParse_VarDecl::ParseVarList ( scanner, globalVars, *this, true );
        }
        else
            throw SonicParseException ( "expected 'program', 'function', 'var', 'import', or constant definition", t );
    }

    validate();
}


void SonicParse_Program::validate()
{
    if ( !programBody )
        throw SonicParseException ( "code contains no program body" );

    programBody->validate ( *this );

    for ( SonicParse_Function *fp = functionBodyList; fp; fp = fp->next )
        fp->validate ( *this );

    for ( SonicParse_VarDecl *vp = globalVars; vp; vp = vp->queryNext() )
    {
        int numInstances = countInstances ( vp->queryName() );
        if ( numInstances < 1 )
            throw SonicParseException ( "internal error: cannot locate global variable", vp->queryName() );
        else if ( numInstances > 1 )
            throw SonicParseException ( "global variable declared more than once", vp->queryName() );

        vp->validate ( *this, 0 );
    }
}


void SonicParse_Program::generateCode()
{
    if ( !programBody )
        throw SonicParseException ( "Internal error: no program body defined!" );

    char temp [512];
    char cppFilename [256];
    sprintf ( cppFilename, "%s.cpp", programBody->queryName().queryToken() );
    ofstream o ( cppFilename );
    if ( !o )
    {
        sprintf ( temp, "Cannot open file '%s' for write.", cppFilename );
        throw SonicParseException ( temp );
    }

    time_t now;
    time(&now);

    o << "// " << cppFilename << "  -  generated by Sonic/C++ translator v " << SONIC_VERSION << ".\n";
    o << "// Translator written by Don Cross <dcross@intersrv.com>\n";
    o << "// For more info about Sonic, see the following web site:\n";
    o << "// http://www.intersrv.com/~dcross/sonic/\n\n";
    o << "// This file created: " << ctime(&now) << "\n";
    o << "// Standard includes...\n";
    o << "#include <stdio.h>\n";
    o << "#include <iostream.h>\n";
    o << "#include <stdlib.h>\n";
    o << "#include <string.h>\n";
    o << "#include <math.h>\n";
    o << "\n// Sonic-specific includes...\n";
    o << "#include \"sonic.h\"\n";
    genImportIncludes ( o );
    o << "\n\n";
    o << "const long    SamplingRate     =  " << samplingRate << ";\n";
    o << "const double  SampleTime       =  1.0 / double(SamplingRate);\n";
    o << "const int     NumChannels      =  " << numChannels << ";\n";
    o << "const int     InterpolateFlag  =  " << (interpolateFlag ? 1 : 0) << ";\n";
    o << "\n";
    o << "const double pi = 4.0 * atan(1.0);\n";
    o << "const double e  = exp(1.0);\n\n";

    Sonic_CodeGenContext x (this);
    genFunctionPrototypes (o, x);
    genGlobalVariables (o, x);
    genMain (o, x);
    genProgramFunction (o, x);
    genFunctions (o, x);

    o << "\n\n/*---  end of file " << cppFilename << "  ---*/\n";
    o.close();
}


void SonicParse_Program::genImportIncludes ( ostream &o )
{
    for ( SonicParse_Function *fp = importList; fp; fp = fp->next )
    {
        bool repeat = false;
        for ( SonicParse_Function *xp = fp->next; !repeat && xp; xp = xp->next )
            if ( fp->importHeader == xp->importHeader )
                repeat = true;

        if ( !repeat )
            o << "#include \"" << fp->importHeader.queryToken() << "\"\n";
    }
}


void SonicParse_Program::genFunctionPrototypes ( ostream &o, Sonic_CodeGenContext &x )
{
    programBody->generatePrototype ( o, x );
    o << ";\n\n";

    for ( SonicParse_Function *fp = functionBodyList; fp; fp = fp->next )
    {
        fp->generatePrototype ( o, x );
        o << ";\n\n";
    }
}


void SonicParse_Program::genGlobalVariables ( ostream &o, Sonic_CodeGenContext &x )
{
    if ( globalVars )
    {
        o << "// global variables...\n\n";
        for ( SonicParse_VarDecl *vp = globalVars; vp; vp = vp->queryNext() )
        {
            vp->generateCode ( o, x );
            o << ";\n";
        }

        o << "\n";
    }   
}


void SonicParse_Program::genMain ( ostream &o, Sonic_CodeGenContext &x )
{
    o << "\n";
    o << "int main ( int argc, char *argv[] )\n";
    o << "{\n";
    x.pushIndent();

    // generate code to verify the number of command-line arguments...

    int numProgramParms = programBody->numParameters();
    
    o << "    if ( argc != " << (1 + numProgramParms) << " )\n";
    o << "    {\n";
    o << "        cerr << \"Use:  " << programBody->queryName().queryToken();

    for ( SonicParse_VarDecl *pp = programBody->queryParmList(); pp; pp = pp->queryNext() )
    {
        o << " " << pp->queryName().queryToken();
    }

    o << "\" << endl << endl;\n";
    o << "        return 1;\n";
    o << "    }\n\n";

    // generate code to extract program arguments from argv, argc...

    int argc = 0;
    for ( pp = programBody->queryParmList(); pp; pp = pp->queryNext() )
    {
        ++argc;
        x.indent ( o );

        switch ( pp->queryType().queryTypeClass() )
        {
            case STYPE_INTEGER:     o << "long ";       break;
            case STYPE_REAL:        o << "double ";     break;
            case STYPE_BOOLEAN:     o << "int ";        break;
            case STYPE_WAVE:        o << "SonicWave ";  break;

            case STYPE_IMPORT:
                throw SonicParseException ( "cannot pass import type to program", pp->queryName() );

            default:
                throw SonicParseException ( "internal error: invalid program argument type", pp->queryName() );
        }

        const char *pname = pp->queryName().queryToken();
        o << LOCAL_SYMBOL_PREFIX << pname;

        switch ( pp->queryType().queryTypeClass() )
        {
            case STYPE_INTEGER:     
                o << " = ScanInteger ( \"" << pname << "\", argv[" << argc << "] );\n";
                break;

            case STYPE_REAL:
                o << " = ScanReal ( \"" << pname << "\", argv[" << argc << "] );\n";
                break;

            case STYPE_BOOLEAN:
                o << " = ScanBoolean ( \"" << pname << "\", argv[" << argc << "] );\n";
                break;

            case STYPE_WAVE:
                o << " ( argv[" << argc << "], ";
                o << '"' << pname << '"';
                o << ", SamplingRate, NumChannels );\n";
        }
    }

    // generate code to call the program function...

    x.indent ( o );
    o << FUNCTION_PREFIX << programBody->queryName().queryToken() << " ( ";

    for ( pp = programBody->queryParmList(); pp; pp = pp->queryNext() )
    {
        o << LOCAL_SYMBOL_PREFIX << pp->queryName().queryToken();

        if ( pp->queryNext() )
            o << ", ";
    }

    o << " );\n\n";

    // generate code to convert all float files to permanent WAV files...

    argc = 0;
    for ( pp = programBody->queryParmList(); pp; pp = pp->queryNext() )
    {
        ++argc;
        if ( pp->queryType() == STYPE_WAVE )
        {
            o << "    " << LOCAL_SYMBOL_PREFIX << pp->queryName().queryToken();
            o << ".convertToWav ( argv[" << argc << "] );\n";
        }
    }

    o << "    SonicWave::EraseAllTempFiles();\n";
    o << "    return 0;\n";
    x.popIndent();

    o << "}\n\n";
}


void SonicParse_Program::genProgramFunction ( ostream &o, Sonic_CodeGenContext &x )
{   
    programBody->generateCode ( o, x );
}


void SonicParse_Program::genFunctions ( ostream &o, Sonic_CodeGenContext &x )
{
    for ( SonicParse_Function *fp = functionBodyList; fp; fp = fp->next )
        fp->generateCode ( o, x );
}


SonicToken SonicParse_Program::CurrentProgramName;


void SonicParse_Program::SaveCurrentProgramName ( const SonicToken &name )
{
    CurrentProgramName = name;
}


const SonicToken & SonicParse_Program::GetCurrentProgramName()
{
    return CurrentProgramName;
}


/*--- end of file prog.cpp ---*/