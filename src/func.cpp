/*========================================================================

    func.cpp  -  Copyright (C) 1998 by Don Cross <cosinekitty@gmail.com>

    This module contains code to parse Sonic function/program bodies.

    Revision history:

1998 October 1 [Don Cross]
    Now allow '?' to be first array dimension in function parameters.

1998 September 28 [Don Cross]
    Adding support for array types.

1998 September 23 [Don Cross]
    Added support for passing parameters by reference.

1998 September 21 [Don Cross]
    Factored out variable declaration parser code out of 
    SonicParse_Function::Parse().  Now it is a separate static
    member function SonicParse_VarDecl::ParseVarList().
    This is so that the same code can be called to implement
    the new feature: global variables.

========================================================================*/
#include <iostream>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "scan.h"
#include "parse.h"


bool IsPositiveIntegerConstant ( const char *s )
{
    if ( strchr(s,'e') || strchr(s,'E') || strchr(s,'.') )
        return false;

    if ( s[0] == '-' )
        return false;

    return true;
}


SonicType ParseType ( SonicScanner &scanner, SonicParseContext &px )
{
    SonicType type = STYPE_UNDEFINED;
    SonicToken t;
    scanner.getToken ( t );

    bool arrayAllowed = true;

    if ( t == "integer" )
        type = STYPE_INTEGER;
    else if ( t == "real" )
        type = STYPE_REAL;
    else if ( t == "boolean" )
        type = STYPE_BOOLEAN;
    else if ( t == "wave" )
    {
        type = STYPE_WAVE;
        arrayAllowed = false;
    }
    else
    {
        arrayAllowed = false;

        // See if this is an imported type...
        SonicParse_Function *import = px.prog.findImportType ( t );
        if ( import )
            type = &import->queryName();
        else
            throw SonicParseException ( "expected data type", t );
    }

    SonicToken lbracket;
    scanner.getToken ( lbracket );
    if ( lbracket == "[" )
    {
        SonicToken dim, punct;
        int dimArray [MAX_SONIC_ARRAY_DIMENSIONS];
        int dimCount = 0;

        for(;;)
        {
            scanner.getToken ( dim );
            if ( dimCount >= MAX_SONIC_ARRAY_DIMENSIONS )
                throw SonicParseException ( "too many array dimensions", dim );

            if ( dim == "?" )
            {
                if ( !px.insideFuncParms )
                    throw SonicParseException ( "may use '?' as array dimension only in function parameters", dim );

                if ( dimCount > 0 )
                    throw SonicParseException ( "may use '?' only as first dimension of array", dim );

                dimArray[dimCount] = 0;
            }
            else
            {
                dimArray[dimCount] = atoi ( dim.queryToken() );

                if ( dim.queryTokenType() != STT_CONSTANT || 
                     !IsPositiveIntegerConstant(dim.queryToken()) ||
                     dimArray[dimCount] < 1 )
                {
                    throw SonicParseException ( 
                        "array dimension must be positive integer constant", 
                        dim );
                }
            }

            ++dimCount;

            scanner.getToken ( punct );
            if ( punct == "]" )
                break;
            else if ( punct != "," )
                throw SonicParseException ( "expected ',' or ']'", punct );
        }

        type = SonicType ( dimCount, dimArray, type.queryTypeClass() );
    }
    else
        scanner.pushToken ( lbracket );

    if ( type == STYPE_UNDEFINED )
        throw SonicParseException ( "internal error: could not parse type", t );

    return type;
}


SonicParse_Function::SonicParse_Function ( 
    const SonicToken &_name,
    bool _isProgramBody,
    SonicType _returnType,
    SonicParse_VarDecl *_parmList,
    SonicParse_VarDecl *_varList,
    SonicParse_Statement *_statementList,
    SonicParse_Program &_prog ):
        name ( _name ),
        isProgramBody ( _isProgramBody ),
        next ( 0 ),
        returnType ( _returnType ),
        parmList ( _parmList ),
        varList ( _varList ),
        statementList ( _statementList ),
        prog ( _prog )
{
}


SonicParse_Function::~SonicParse_Function()
{
    if ( parmList )
    {
        delete parmList;
        parmList = 0;
    }

    if ( varList )
    {
        delete varList;
        varList = 0;
    }

    if ( next )
    {
        delete next;    // results in recursive destructor call
        next = 0;
    }
}


SonicParse_VarDecl *SonicParse_Function::findSymbol ( 
    const SonicToken &symbol,
    bool forceFind ) const
{
    for ( SonicParse_VarDecl *vp = parmList; vp; vp = vp->next )
    {
        if ( vp->queryName() == symbol )
            return vp;
    }

    for ( SonicParse_VarDecl *vp = varList; vp; vp = vp->next )
    {
        if ( vp->queryName() == symbol )
            return vp;
    }

	SonicParse_VarDecl *vp = prog.findGlobalVar ( symbol );
    if ( vp )
        return vp;

    if ( forceFind )
        throw SonicParseException ( "undefined symbol", symbol );

    return 0;
}


int SonicParse_Function::countInstances ( const SonicToken &otherName ) const
{
    int count = 0;
    for ( SonicParse_VarDecl *vp = varList; vp; vp = vp->next )
    {
        if ( vp->queryName() == otherName )
            ++count;
    }

    for ( SonicParse_VarDecl *vp = parmList; vp; vp = vp->next )
    {
        if ( vp->queryName() == otherName )
            ++count;
    }

    return count;
}


bool IsPseudoFunction ( const SonicToken &name )
{
    return 
        name == "sinewave" ||
        name == "sawtooth" ||
        name == "fft" ||
        name == "iir";
}


void SonicParse_VarDecl::ParseVarList (
    SonicScanner &scanner,
    SonicParseContext &px,
    SonicParse_VarDecl * &varList,
    bool isGlobal )
{
    SonicParse_VarDecl *varTail = varList;
    if ( varTail )
    {
        while ( varTail->next )
            varTail = varTail->next;
    }

    SonicToken t;
    for(;;)
    {
        bool foundToken = scanner.getToken ( t, !isGlobal );
        if ( !foundToken )
            break;

        if ( t != "var" )
        {
            scanner.pushToken ( t );
            break;
        }

        SonicParse_VarDecl *thisVarList = 0;

        for(;;)
        {       
            SonicToken varName;
            scanner.getToken ( varName );
            if ( varName.queryTokenType() != STT_IDENTIFIER )
                throw SonicParseException ( "Expected variable name", varName );

            if ( FindIntrinsic ( varName.queryToken() ) )
                throw SonicParseException ( "variable name conflicts with intrinsic function", varName );

            if ( IsPseudoFunction ( varName ) )
                throw SonicParseException ( "variable name conflicts with pseudo-function", varName );

            SonicParse_Expression *initializer = 0;
            scanner.getToken ( t );
            if ( t == "=" )
                initializer = SonicParse_Expression::Parse ( scanner, px );
            else if ( t == "(" )
            {
                scanner.getToken ( t );
                if ( t != ")" )
                {
                    scanner.pushToken ( t );
                    SonicParse_Expression *tail = 0;
                    for(;;)
                    {
                        SonicParse_Expression *parm = SonicParse_Expression::Parse_b0 ( scanner, px );
                        if ( tail )
                            tail = tail->next = parm;
                        else
                            initializer = tail = parm;

                        scanner.getToken ( t );
                        if ( t == ")" )
                            break;
                        else if ( t != "," )
                            throw SonicParseException ( "expected ')' or ','", t );
                    }
                }
            }
            else
                scanner.pushToken ( t );

            SonicParse_VarDecl *newVar = new SonicParse_VarDecl ( 
                varName,
                STYPE_UNDEFINED,
                initializer,
                isGlobal,
                false );

            if ( varTail )
                varTail = varTail->next = newVar;
            else
            {
                varList = varTail = newVar;
                if ( !isGlobal && !px.localVars )
                    px.localVars = varList;
            }

            if ( !thisVarList )
                thisVarList = newVar;

            scanner.getToken ( t );
            if ( t == ":" )
                break;
            else if ( t != "," )
                throw SonicParseException ( "expected ',' or ':'", t );
        }

        SonicType varListType = ParseType (scanner, px);
        while ( thisVarList )
        {
            thisVarList->type = varListType;
            thisVarList = thisVarList->next;
        }

        scanner.scanExpected ( ";" );
    }
}


SonicParse_Function *SonicParse_Function::Parse ( 
    SonicScanner &scanner, 
    SonicParseContext &px )
{
    char temp [256];

    SonicToken t;
    if ( !scanner.getToken ( t, false ) )
        return 0;   // marks end of source code

    const bool isProgramBody = (t == "program");
    if ( !isProgramBody && t!="function" )
        throw SonicParseException ( "Expected 'program' or 'function'", t );

    SonicToken funcName;
    scanner.getToken ( funcName );
    if ( funcName.queryTokenType() != STT_IDENTIFIER )
    {
        sprintf ( temp, "Expected %s name", isProgramBody ? "program" : "function" );
        throw SonicParseException ( temp, funcName );
    }

    if ( FindIntrinsic ( funcName.queryToken() ) )
        throw SonicParseException ( "name conflicts with intrinsic function", funcName );

    if ( IsPseudoFunction ( funcName ) )
        throw SonicParseException ( "name conflicts with pseudo-function", funcName );

    scanner.scanExpected ( "(" );

    SonicParse_VarDecl *parmList=0, *parmTail=0;
    SonicToken parmName;
    for(;;)
    {
        scanner.getToken ( parmName );
        if ( parmName == ")" )
            break;

        if ( parmName.queryTokenType() != STT_IDENTIFIER )
            throw SonicParseException ( "Expected parameter name or ')'", parmName );

        if ( FindIntrinsic ( parmName.queryToken() ) )
            throw SonicParseException ( "name conflicts with intrinsic function", parmName );

        if ( IsPseudoFunction ( parmName ) )
            throw SonicParseException ( "name conflicts with pseudo-function", parmName );

        scanner.scanExpected ( ":" );

        px.insideFuncParms = true;
        SonicType parmType = ParseType ( scanner, px );
        px.insideFuncParms = false;

        // Check for '&', indicating that the parameter is passed by reference 
        // instead of by value.

        SonicToken amp;
        scanner.getToken ( amp );
        if ( amp == "&" )
            parmType.setReferenceFlag (true);
        else
            scanner.pushToken ( amp );

        SonicParse_VarDecl *newParm = new SonicParse_VarDecl ( 
            parmName,
            parmType,
            0,
            false,
            true );

        if ( parmList )
            parmTail = parmTail->next = newParm;
        else
            px.localParms = parmList = parmTail = newParm;

        scanner.getToken ( t );
        if ( t != "," )
            scanner.pushToken ( t );
    }

    SonicType returnType = STYPE_VOID;
    scanner.getToken ( t );
    if ( t == ":" )
        returnType = ParseType (scanner, px);
    else
        scanner.pushToken ( t );

    scanner.scanExpected ( "{" );

    SonicParse_VarDecl *varList = 0;
    SonicParse_VarDecl::ParseVarList ( scanner, px, varList, false );

    SonicParse_Statement *statementList=0, *statementTail=0;

    for(;;)
    {
        scanner.getToken ( t );
        if ( t == "}" )
            break;

        scanner.pushToken ( t );
        SonicParse_Statement *newStatement = SonicParse_Statement::Parse ( scanner, px );
        if ( statementTail )
            statementTail = statementTail->next = newStatement;
        else
            statementTail = statementList = newStatement;
    }

    SonicParse_Function *func = new SonicParse_Function (
        funcName,
        isProgramBody,
        returnType,
        parmList,
        varList,
        statementList,
        px.prog );

    px.localVars = 0;
    px.localParms = 0;
    return func;
}


int SonicParse_Function::numParameters() const
{
    int count = 0;
    
    for ( SonicParse_VarDecl *pp = parmList; pp; pp = pp->next )
        ++count;

    return count;
}


void SonicParse_Function::clearAllResetFlags()
{
    for ( SonicParse_VarDecl *vp = parmList; vp; vp = vp->queryNext() )
        vp->modifyResetFlag ( false );

    for ( SonicParse_VarDecl *vp = varList; vp; vp = vp->next )
        vp->modifyResetFlag ( false );

    prog.clearAllResetFlags();
}


//----------------------------------------------------------------------------


SonicParse_VarDecl::SonicParse_VarDecl ( 
    const SonicToken &_name,
    SonicType _type,
    SonicParse_Expression *_init,
    bool _isGlobal,
    bool _isFunctionParm ):
        next ( 0 ),
        name ( _name ),
        type ( _type ),
        init ( _init ),
        resetFlag ( false ),
        isGlobal ( _isGlobal ),
        isFunctionParm ( _isFunctionParm )
{
}


SonicParse_VarDecl::~SonicParse_VarDecl()
{
    if ( init )
    {
        delete init;
        init = 0;
    }

    if ( next )
    {
        delete next;
        next = 0;
    }
}


/*--- end of file func.cpp ---*/
