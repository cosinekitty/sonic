/*=============================================================================

    stmt.cpp  -  Copyright (C) 1998 by Don Cross <cosinekitty@gmail.com>

    This module is responsible for parsing Sonic statement constructs,
    including 'if', 'while', 'repeat', 'return', and assignment statements.

    Revision history:

1998 October 2 [Don Cross]
    Allowed initializer of a 'for' loop to be any statement, 
    not just assignment.

1998 September 30 [Don Cross]
    Added support for 'for' loops.

1998 September 21 [Don Cross]
    The exception thrown at the end of SonicParse_Statement::Parse()
    where stmt==NULL used to report an "internal error".  This has been
    reworded to simply state that a statement was expected, because
    I discovered circumstances where an ordinary syntax error
    can trigger the exception.

=============================================================================*/
#include <iostream>
#include <stdio.h>
#include <string.h>

#include "scan.h"
#include "parse.h"


SonicParse_Statement_Assignment *SonicParse_Statement::ParseAssignment ( 
    SonicScanner &scanner,
    SonicParseContext &px )
{
    SonicToken t, t2;
    scanner.getToken ( t );
    scanner.getToken ( t2 );

    bool isWaveLvalue = false;
    SonicParse_Expression *sampleLimit = 0;
    SonicParse_Expression *indexList=0, *indexTail=0;
    if ( t2 == "[" )
    {
        // Could be array assignment or wave assignment...

        const SonicParse_VarDecl *decl = px.findVar(t);
        const SonicType &type = decl->queryType();
        if ( type == STYPE_ARRAY )
        {
            SonicToken punct;
            for(;;)
            {
                SonicParse_Expression *newIndex = SonicParse_Expression::Parse_term ( scanner, px );
                if ( indexList )
                    indexTail = indexTail->next = newIndex;
                else
                    indexList = indexTail = newIndex;
                scanner.getToken ( punct );
                if ( punct == "]" )
                    break;
                else if ( punct != "," )
                    throw SonicParseException ( "expected ',' or ']'", punct );
            }
        }
        else if ( type == STYPE_WAVE )
        {
            // this is a wave assignment
            isWaveLvalue = true;
            scanner.scanExpected ( "c" );
            scanner.scanExpected ( "," );
            scanner.scanExpected ( "i" );

            scanner.getToken ( t2 );
            if ( t2 == ":" )
                sampleLimit = SonicParse_Expression::Parse_term (scanner, px);
            else
                scanner.pushToken ( t2 );

            scanner.scanExpected ( "]" );
        }
        else
            throw SonicParseException ( "cannot subscript variable of this type", t );
    }
    else
        scanner.pushToken ( t2 );
    
    SonicToken ( op );
    scanner.getToken ( op );
    if ( op != "=" && op != "<<" && op != "+=" && op != "-=" && 
         op != "*=" && op != "/=" && op != "%=" )
    {
        throw SonicParseException ( "invalid assignment operator", op );
    }

    SonicParse_Expression *rvalue = SonicParse_Expression::Parse (scanner, px);

    SonicParse_Lvalue *lvalue = new SonicParse_Lvalue (
        t,
        isWaveLvalue,
        sampleLimit,
        indexList );

    return new SonicParse_Statement_Assignment ( op, lvalue, rvalue );
}



SonicParse_Statement *SonicParse_Statement::Parse ( 
    SonicScanner &scanner,
    SonicParseContext &px )
{
    SonicParse_Statement *stmt = 0;
    SonicToken t, t2;
    scanner.getToken ( t );

    if ( t == "if" )
    {
        scanner.scanExpected ( "(" );
        SonicParse_Expression *condition = SonicParse_Expression::Parse_b0 ( scanner, px );
        scanner.scanExpected ( ")" );
        SonicParse_Statement *ifPart = SonicParse_Statement::Parse ( scanner, px );
        SonicParse_Statement *elsePart = 0;
        scanner.getToken ( t );
        if ( t == "else" )
            elsePart = SonicParse_Statement::Parse ( scanner, px );
        else
            scanner.pushToken ( t );

        stmt = new SonicParse_Statement_If ( condition, ifPart, elsePart );
    }
    else if ( t == "while" )
    {
        scanner.scanExpected ( "(" );
        SonicParse_Expression *condition = SonicParse_Expression::Parse_b0 ( scanner, px );
        scanner.scanExpected ( ")" );
        SonicParse_Statement *loop = SonicParse_Statement::Parse ( scanner, px );
        stmt = new SonicParse_Statement_While ( condition, loop );
    }
    else if ( t == "for" )
    {
        scanner.scanExpected ( "(" );
        SonicParse_Statement *init = SonicParse_Statement::Parse ( scanner, px );
        SonicParse_Expression *condition = SonicParse_Expression::Parse_b0 ( scanner, px );
        scanner.scanExpected ( ";" );
        SonicParse_Statement *update = SonicParse_Statement::ParseAssignment ( scanner, px );
        scanner.scanExpected ( ")" );
        SonicParse_Statement *loop = SonicParse_Statement::Parse ( scanner, px );
        stmt = new SonicParse_Statement_For ( init, condition, update, loop );
    }
    else if ( t == "repeat" )
    {
        scanner.scanExpected ( "(" );
        SonicParse_Expression *count = SonicParse_Expression::Parse_term ( scanner, px );
        scanner.scanExpected ( ")" );
        SonicParse_Statement *loop = SonicParse_Statement::Parse ( scanner, px );
        stmt = new SonicParse_Statement_Repeat ( count, loop );
    }
    else if ( t == "return" )
    {
        scanner.getToken ( t2 );
        SonicParse_Expression *returnValue = 0;
        if ( t2 != ";" )
        {
            scanner.pushToken ( t2 );
            returnValue = SonicParse_Expression::Parse_b0 ( scanner, px );
            scanner.scanExpected ( ";" );
        }
        stmt = new SonicParse_Statement_Return ( t, returnValue );
    }
    else if ( t == "{" )
    {
        SonicParse_Statement *stmtList=0, *stmtTail=0;
        for(;;)
        {
            scanner.getToken ( t );
            if ( t == "}" )
                break;

            scanner.pushToken ( t );
            SonicParse_Statement *newStmt = SonicParse_Statement::Parse ( scanner, px );
            if ( stmtTail )
                stmtTail = stmtTail->next = newStmt;
            else
                stmtTail = stmtList = newStmt;
        }

        stmt = new SonicParse_Statement_Compound ( stmtList );
    }
    else if ( t == ";" )
    {
        stmt = new SonicParse_Statement_Compound ( 0 );
    }
    else if ( t.queryTokenType() == STT_IDENTIFIER )
    {
        // Get one more token to see if this is a wave expression,
        // a function call, or just a simple variable.

        scanner.getToken ( t2 );
        if ( t2 == "(" )
        {
            // Assume this is a function call, not an assignment.

            // Watch this cute trick...
            scanner.pushToken ( t2 );
            scanner.pushToken ( t );
            SonicParse_Expression *expr = SonicParse_Expression::Parse_t3 (scanner, px);

            if ( expr->queryExpressionType() != ETYPE_FUNCTION_CALL )
                throw SonicParseException ( "expected function call", t );

            scanner.scanExpected ( ";" );

            SonicParse_Expression_FunctionCall *call = 
                (SonicParse_Expression_FunctionCall *) expr;

            stmt = new SonicParse_Statement_FunctionCall ( call );
        }
        else 
        {
            scanner.pushToken ( t2 );
            scanner.pushToken ( t );
            stmt = SonicParse_Statement::ParseAssignment ( scanner, px );
            scanner.scanExpected ( ";" );
        }
    }

    if ( !stmt )
        throw SonicParseException ( "expected a statement", t );

    return stmt;
}


SonicParse_Statement_For::~SonicParse_Statement_For()
{
    if ( init )
    {
        delete init;
        init = 0;
    }

    if ( condition )
    {
        delete condition;
        condition = 0;
    }

    if ( update )
    {
        delete update;
        update = 0;
    }
}



/*--- end of file stmt.cpp ---*/