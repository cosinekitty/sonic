/*========================================================================

    func.cpp  -  Copyright (C) 1998 by Don Cross <dcross@intersrv.com>

    This module contains code to parse Sonic function/program bodies.

    Revision history:

1998 September 23 [Don Cross]
    Added support for passing parameters by reference.

1998 September 21 [Don Cross]
    Factored out variable declaration parser code out of 
    SonicParse_Function::Parse().  Now it is a separate static
    member function SonicParse_VarDecl::ParseVarList().
    This is so that the same code can be called to implement
    the new feature: global variables.

========================================================================*/
#include <iostream.h>
#include <stdio.h>

#include "scan.h"
#include "parse.h"


SonicType ParseType ( SonicScanner &scanner, SonicParse_Program &prog )
{
    SonicType type = STYPE_UNDEFINED;
    SonicToken t;
    scanner.getToken ( t );

    if ( t == "integer" )
        type = STYPE_INTEGER;
    else if ( t == "real" )
        type = STYPE_REAL;
    else if ( t == "boolean" )
        type = STYPE_BOOLEAN;
    else if ( t == "wave" )
        type = STYPE_WAVE;
    else
    {
        // See if this is an imported type...
        SonicParse_Function *import = prog.findImportType ( t );
        if ( import )
            type = &import->queryName();
        else
            throw SonicParseException ( "expected data type", t );
    }

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

    for ( vp = varList; vp; vp = vp->next )
    {
        if ( vp->queryName() == symbol )
            return vp;
    }

    vp = prog.findGlobalVar ( symbol );
    if ( vp )
        return vp;

    if ( forceFind )
        throw SonicParseException ( "undefined symbol", symbol );

    return 0;
}


int SonicParse_Function::countInstances ( const SonicToken &name ) const
{
    int count = 0;
    for ( SonicParse_VarDecl *vp = varList; vp; vp = vp->next )
    {
        if ( vp->queryName() == name )
            ++count;
    }

    for ( vp = parmList; vp; vp = vp->next )
    {
        if ( vp->queryName() == name )
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
    SonicParse_VarDecl * &varList,
    SonicParse_Program &prog,
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
                initializer = SonicParse_Expression::Parse ( scanner );
            else if ( t == "(" )
            {
                scanner.getToken ( t );
                if ( t != ")" )
                {
                    scanner.pushToken ( t );
                    SonicParse_Expression *tail = 0;
                    for(;;)
                    {
                        SonicParse_Expression *parm = SonicParse_Expression::Parse_b0 ( scanner );
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
                isGlobal );

            if ( varTail )
                varTail = varTail->next = newVar;
            else
                varList = varTail = newVar;

            if ( !thisVarList )
                thisVarList = newVar;

            scanner.getToken ( t );
            if ( t == ":" )
                break;
            else if ( t != "," )
                throw SonicParseException ( "expected ',' or ':'", t );
        }

        SonicType varListType = ParseType (scanner, prog);
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
    SonicParse_Program &prog )
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

        SonicType parmType = ParseType ( scanner, prog );

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
            false );

        if ( parmList )
            parmTail = parmTail->next = newParm;
        else
            parmList = parmTail = newParm;

        scanner.getToken ( t );
        if ( t != "," )
            scanner.pushToken ( t );
    }

    SonicType returnType = STYPE_VOID;
    scanner.getToken ( t );
    if ( t == ":" )
        returnType = ParseType (scanner, prog);
    else
        scanner.pushToken ( t );

    scanner.scanExpected ( "{" );

    SonicParse_VarDecl *varList = 0;
    SonicParse_VarDecl::ParseVarList ( scanner, varList, prog, false );

    SonicParse_Statement *statementList=0, *statementTail=0;

    for(;;)
    {
        scanner.getToken ( t );
        if ( t == "}" )
            break;

        scanner.pushToken ( t );
        SonicParse_Statement *newStatement = SonicParse_Statement::Parse ( scanner );
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
        prog );

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

    for ( vp = varList; vp; vp = vp->next )
        vp->modifyResetFlag ( false );

    prog.clearAllResetFlags();
}


//----------------------------------------------------------------------------


SonicParse_VarDecl::SonicParse_VarDecl ( 
    const SonicToken &_name,
    SonicType _type,
    SonicParse_Expression *_init,
    bool _isGlobal ):
        next ( 0 ),
        name ( _name ),
        type ( _type ),
        init ( _init ),
        resetFlag ( false ),
        isGlobal ( _isGlobal )
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