/*=============================================================================

    stmt.cpp  -  Copyright (C) 1998 by Don Cross <cosinekitty@gmail.com>

    This module is responsible for parsing Sonic statement constructs,
    including 'if', 'while', 'repeat', 'return', and assignment statements.

    Revision history:

1998 September 21 [Don Cross]
    The exception thrown at the end of SonicParse_Statement::Parse()
    where stmt==NULL used to report an "internal error".  This has been
    reworded to simply state that a statement was expected, because
    I discovered circumstances where an ordinary syntax error
    can trigger the exception.

=============================================================================*/
#include <iostream.h>
#include <stdio.h>
#include <string.h>

#include "scan.h"
#include "parse.h"


SonicParse_Statement *SonicParse_Statement::Parse ( SonicScanner &scanner )
{
    SonicParse_Statement *stmt = 0;
    SonicToken t, t2;
    scanner.getToken ( t );

    if ( t == "if" )
    {
        scanner.scanExpected ( "(" );
        SonicParse_Expression *condition = SonicParse_Expression::Parse_b0 ( scanner );
        scanner.scanExpected ( ")" );
        SonicParse_Statement *ifPart = SonicParse_Statement::Parse ( scanner );
        SonicParse_Statement *elsePart = 0;
        scanner.getToken ( t );
        if ( t == "else" )
            elsePart = SonicParse_Statement::Parse ( scanner );
        else
            scanner.pushToken ( t );

        stmt = new SonicParse_Statement_If ( condition, ifPart, elsePart );
    }
    else if ( t == "while" )
    {
        scanner.scanExpected ( "(" );
        SonicParse_Expression *condition = SonicParse_Expression::Parse_b0 ( scanner );
        scanner.scanExpected ( ")" );
        SonicParse_Statement *loop = SonicParse_Statement::Parse ( scanner );
        stmt = new SonicParse_Statement_While ( condition, loop );
    }
    else if ( t == "repeat" )
    {
        scanner.scanExpected ( "(" );
        SonicParse_Expression *count = SonicParse_Expression::Parse_term ( scanner );
        scanner.scanExpected ( ")" );
        SonicParse_Statement *loop = SonicParse_Statement::Parse ( scanner );
        stmt = new SonicParse_Statement_Repeat ( count, loop );
    }
    else if ( t == "return" )
    {
        scanner.getToken ( t2 );
        SonicParse_Expression *returnValue = 0;
        if ( t2 != ";" )
        {
            scanner.pushToken ( t2 );
            returnValue = SonicParse_Expression::Parse_b0 ( scanner );
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
            SonicParse_Statement *newStmt = SonicParse_Statement::Parse ( scanner );
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
            SonicParse_Expression *expr = SonicParse_Expression::Parse_t3 (scanner);

            if ( expr->queryExpressionType() != ETYPE_FUNCTION_CALL )
                throw SonicParseException ( "expected function call", t );

            scanner.scanExpected ( ";" );

            SonicParse_Expression_FunctionCall *call = 
                (SonicParse_Expression_FunctionCall *) expr;

            stmt = new SonicParse_Statement_FunctionCall ( call );
        }
        else 
        {
            // this must be some kind of assignment statement

            bool isWaveLvalue = false;
            SonicParse_Expression *sampleLimit = 0;
            if ( t2 == "[" )
            {
                // assume this is an assignment with wave lvalue
                isWaveLvalue = true;
                scanner.scanExpected ( "c" );
                scanner.scanExpected ( "," );
                scanner.scanExpected ( "i" );

                scanner.getToken ( t2 );
                if ( t2 == ":" )
                    sampleLimit = SonicParse_Expression::Parse_term (scanner);
                else
                    scanner.pushToken ( t2 );

                scanner.scanExpected ( "]" );
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

            SonicParse_Expression *rvalue = SonicParse_Expression::Parse (scanner);
            scanner.scanExpected ( ";" );

            SonicParse_Lvalue *lvalue = new SonicParse_Lvalue (
                t,
                isWaveLvalue,
                sampleLimit );

            stmt = new SonicParse_Statement_Assignment ( op, lvalue, rvalue );
        }
    }

    if ( !stmt )
        throw SonicParseException ( "expected a statement", t );

    return stmt;
}


/*--- end of file stmt.cpp ---*/
