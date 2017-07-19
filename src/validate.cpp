/*===========================================================================

    validate.cpp  -  Copyright (C) 1998 by Don Cross <cosinekitty@gmail.com>

    This code is responsible for doing post-parse validation of a 
    Sonic program.  It also has certain critical side effects such as
    figuring out data types of expressions and functions, based on
    information gleaned during the parse phase.  This allows one
    function to call another function which is defined later in the 
    source file without requiring a forward declaration mechanism in
    the language.

    Revision history:

1998 September 23 [Don Cross]
    Adding support for passing function arguments by reference.
    Therefore, I'm adding code to verify that reference arguments
    are truly lvalues and that the types are identical (not just compatible).
    Adding code to validate the new 'fft' pseudo-function.

1998 September 19 [Don Cross]
    Added code to SonicParse_Expression_WaveExpr::validate()
    that ensures both channel term and index term are of
    numeric type.

1998 September 16 [Don Cross]
    The member function SonicParse_VarDecl::validate() was not being 
    called, and even worse, it didn't exist!  I have fixed both
    problems.  Now the validator will catch attempts to initialize
    a variable with an expression of an incompatible type.

    Fixed another problem:  should not allow assignment operators 
    like '+=' and '<<' to assign to boolean variables.  Now '=' is 
    the only operator allowed for a boolean lvalue.

    Yet another problem:  operands of comparison operators must be
    of compatible types, and may not be of type 'wave'.

===========================================================================*/
#include <iostream>
#include <stdio.h>

#include "scan.h"
#include "parse.h"


void SonicParse_Expression::ValidateList ( 
    SonicParse_Expression *exprList,
    SonicParse_Program &program, 
    SonicParse_Function *func )
{
    while ( exprList )
    {
        exprList->validate ( program, func );
        exprList = exprList->next;
    }
}


void SonicParse_Statement_Compound::validate ( 
    SonicParse_Program &prog, 
    SonicParse_Function *func )
{
    for ( SonicParse_Statement *stmt = compound; stmt; stmt = stmt->next )
        stmt->validate ( prog, func );
}


void SonicParse_Statement_FunctionCall::validate ( 
    SonicParse_Program &program, 
    SonicParse_Function *func )
{
    call->validate ( program, func );
}


void SonicParse_Statement_If::validate ( 
    SonicParse_Program &program, 
    SonicParse_Function *func )
{
    condition->validate ( program, func );

    if ( condition->determineType() != STYPE_BOOLEAN )
        throw SonicParseException ( "argument to 'if' must be boolean type", condition->getFirstToken() );

    ifPart->validate ( program, func );
    if ( elsePart )
        elsePart->validate ( program, func );
}


void SonicParse_Statement_While::validate ( 
    SonicParse_Program &program, 
    SonicParse_Function *func )
{
    condition->validate ( program, func );
    loop->validate ( program, func );

    if ( condition->determineType() != STYPE_BOOLEAN )
        throw SonicParseException ( "argument to 'while' must be boolean type", condition->getFirstToken() );
}


void SonicParse_Statement_Repeat::validate (
    SonicParse_Program &program,
    SonicParse_Function *func )
{
    count->validate ( program, func );
    loop->validate ( program, func );

    if ( !count->canConvertTo(STYPE_INTEGER) )
        throw SonicParseException ( "cannot convert 'repeat' argument to integer type", count->getFirstToken() );
}


void SonicParse_Statement_Return::validate ( 
    SonicParse_Program &program, 
    SonicParse_Function *func )
{
    if ( !func )
        throw SonicParseException ( "internal error: func==NULL in SonicParse_Statement_Return::validate", returnToken );

    if ( returnValue )
    {
        returnValue->validate ( program, func );
        const SonicType source = returnValue->determineType();
        const SonicType target = func->queryReturnType();
        if ( !CanConvertTo(source,target) )
            throw SonicParseException ( "cannot convert return value to return type", returnValue->getFirstToken() );
    }
    else
    {
        if ( func->queryReturnType() != STYPE_VOID )
            throw SonicParseException ( "this function must return a value", returnToken );
    }
}


void SonicParse_Statement_Assignment::validate ( 
    SonicParse_Program &program, 
    SonicParse_Function *func )
{
    lvalue->validate ( program, func );
    rvalue->validate ( program, func );

    SonicType ltype = lvalue->determineType ( program, func );
    if ( !rvalue->canConvertTo (ltype) )
        throw SonicParseException ( "cannot convert expression to type on left side of '='", rvalue->getFirstToken() );

    if ( ltype == STYPE_BOOLEAN )
    {
        if ( op != "=" )
            throw SonicParseException ( "assignment operator not allowed for boolean on left", op );
    }
}


void SonicParse_VarDecl::validate ( 
    SonicParse_Program &prog, 
    SonicParse_Function *func )
{
    if ( init )
    {
        // multiple expressions can occur in initializers for import function (constructors)
        for ( SonicParse_Expression *ip = init; ip; ip = ip->queryNext() )
            ip->validate ( prog, func );

        if ( type != STYPE_IMPORT )
        {
            if ( !init->canConvertTo (type) )
            {
                throw SonicParseException ( 
                    "cannot convert initializer expression to variable type", 
                    init->getFirstToken() );
            }
        }
    }
}


void SonicParse_Function::validate ( SonicParse_Program &prog )
{
    // First, make sure the function name is unique...

    SonicParse_Function *progBody = prog.queryProgramBody();
    if ( progBody != this )
    {
        if ( progBody->queryName() == queryName() )
            throw SonicParseException ( "function name conflicts with program name", queryName() );
    }

    for ( SonicParse_Function *fp = next; fp; fp = fp->next )
    {
        if ( fp != this )
        {
            if ( fp->queryName() == queryName() )
                throw SonicParseException ( "function name already defined", fp->queryName() );
        }
    }

    for ( SonicParse_VarDecl *vp = parmList; vp; vp = vp->next )
    {
        validateUniqueSymbol ( prog, vp->queryName() );
        vp->validate ( prog, this );
    }

    for ( SonicParse_VarDecl *vp = varList; vp; vp = vp->next )
    {
        validateUniqueSymbol ( prog, vp->queryName() );
        vp->validate ( prog, this );
    }

    for ( SonicParse_Statement *sp = statementList; sp; sp = sp->next )
        sp->validate ( prog, this );
}


void SonicParse_Function::validateUniqueSymbol ( 
    SonicParse_Program &prog, 
    const SonicToken &name )
{
    int numFound = countInstances(name) + prog.countInstances(name);

    for ( SonicParse_Function *fp = prog.queryFunctionList(); fp; fp = fp->next )
    {
        if ( fp->name == name )
            ++numFound;
    }

    for ( SonicParse_Function *fp = prog.queryImportList(); fp; fp = fp->next )
    {
        if ( fp->name == name )
            ++numFound;
    }

	SonicParse_Function *fp = prog.queryProgramBody();
    if ( fp->name == name )
        ++numFound;

    if ( numFound == 0 )
        throw SonicParseException ( "symbol not defined", name );
    else if ( numFound > 1 )
        throw SonicParseException ( "symbol defined more than once", name );
}


int SonicParse_Expression_Vector::queryNumChannels() const
{
    int count = 0;

    for ( SonicParse_Expression *ep = exprList; ep; ep = ep->queryNext() )
        ++count;

    return count;
}


void SonicParse_Expression_Vector::validate ( 
    SonicParse_Program &program, 
    SonicParse_Function *func )
{
    int numComponents = 0;
    const int numChannels = program.queryNumChannels();
    for ( SonicParse_Expression *ep = exprList; ep; ep = ep->queryNext() )
    {
        if ( ++numComponents > numChannels )
            throw SonicParseException ( "too many vector components", ep->getFirstToken() );

        ep->validate ( program, func );
        SonicType etype = ep->determineType();
        if ( etype != STYPE_INTEGER && etype != STYPE_REAL )
            throw SonicParseException ( "vector component expression must have numeric type", ep->getFirstToken() );
    }

    if ( numComponents < numChannels )
        throw SonicParseException ( "too few vector components", lbrace );
}


void SonicParse_Expression_BinaryBoolOp::validate ( 
    SonicParse_Program &program, 
    SonicParse_Function *func )
{
    lchild->validate ( program, func );
    rchild->validate ( program, func );

    SonicType ltype = lchild->determineType();
    SonicType rtype = rchild->determineType();

    if ( requiresBooleanOperands )
    {
        if ( ltype != STYPE_BOOLEAN )
            throw SonicParseException ( "left operand must have boolean type", op );

        if ( rtype != STYPE_BOOLEAN )
            throw SonicParseException ( "right operand must have boolean type", op );
    }
    else
    {
        if ( ltype == STYPE_WAVE )
            throw SonicParseException ( "left operand may not be of type 'wave'", op );

        if ( rtype == STYPE_WAVE )
            throw SonicParseException ( "right operand may not be of type 'wave'", op );

        if  ( !CanConvertTo(rtype,ltype) )
            throw SonicParseException ( "operands of comparison have incompatible types", op );
    }
}


void SonicParse_Expression_WaveExpr::validate ( 
    SonicParse_Program &program, 
    SonicParse_Function *func )
{
    cterm->validate ( program, func );
    if ( !cterm->canConvertTo (STYPE_INTEGER) )
        throw SonicParseException ( "channel term must be of numeric type", cterm->getFirstToken() );

    iterm->validate ( program, func );
    if ( !iterm->canConvertTo (STYPE_REAL) )
        throw SonicParseException ( "index term must be of numeric type", iterm->getFirstToken() );
}


void SonicParse_Expression_FunctionCall::validate ( 
    SonicParse_Program &program, 
    SonicParse_Function *func )
{
    if ( isIntrinsic() )
    {
        // For now, all intrinsic functions accept real parameters...
        // The number of parameters was checked when we created this object.

        for ( SonicParse_Expression *ep = parmList; ep; ep = ep->queryNext() )
        {
            ep->validate ( program, func );

            if ( !ep->canConvertTo ( STYPE_REAL ) )
            {
                throw SonicParseException (
                    "cannot convert intrinsic function parameter to type 'real'",
                    ep->getFirstToken() );
            }
        }

        type = STYPE_REAL;  // all intrinsics return 'real' for now.
    }
    else
    {
        SonicParse_Function *called = program.findImportVar ( name, func );
        if ( called )
        {
            // allow any number of parameters to import functions

            for ( SonicParse_Expression *ep = parmList; ep; ep = ep->queryNext() )
                ep->validate ( program, func );

            type = STYPE_REAL;  // assume that all import functions return real
            ftype = SFT_IMPORT;
        }
        else
        {
            called = program.findFunction ( name );
            type = called->queryReturnType();

            // match the parameters in the call to those in the function's prototype.

            SonicParse_VarDecl *vp = called->queryParmList();
            for ( SonicParse_Expression *ep = parmList; ep; ep = ep->queryNext() )
            {
                if ( !vp )
                    throw SonicParseException ( "too many parameters to function", name );

                ep->validate ( program, func );

                if ( vp->queryType().isReference() )
                {
                    // Make sure the parameter is a valid lvalue.
                    if ( ep->queryExpressionType() != ETYPE_VARIABLE )
                    {
                        throw SonicParseException (
                            "Must pass a variable as reference argument to function",
                            ep->getFirstToken() );
                    }

                    // The types must be identical, not just compatible...

                    if ( ep->determineType() != vp->queryType() )
                    {
                        throw SonicParseException (
                            "Variable type does not match function argument type",
                            ep->getFirstToken() );
                    }
                }
                else if ( !ep->canConvertTo ( vp->queryType() ) )
                {
                    throw SonicParseException ( 
                        "cannot convert expression to function parameter type",
                        ep->getFirstToken() );
                }

                vp = vp->queryNext();
            }

            if ( vp )
                throw SonicParseException ( "not enough parameters to function", name );
        }
    }
}


void SonicParse_Expression_Builtin::validate ( 
    SonicParse_Program &program, 
    SonicParse_Function *func )
{
}


void SonicParse_Expression_Variable::validate ( 
    SonicParse_Program &program, 
    SonicParse_Function *func )
{
    SonicParse_VarDecl *decl = program.findSymbol ( name, func, true );
    type = decl->queryType();
}


void SonicParse_Expression_Negate::validate ( 
    SonicParse_Program &program, 
    SonicParse_Function *func )
{
    child->validate ( program, func );
    SonicType ctype = child->determineType();
    if ( ctype != STYPE_REAL && ctype != STYPE_INTEGER )
        throw SonicParseException ( "operand of unary '-' must have numeric type", op );
}


void SonicParse_Expression_Not::validate ( 
    SonicParse_Program &program, 
    SonicParse_Function *func )
{
    child->validate ( program, func );
    SonicType ctype = child->determineType();
    if ( ctype != STYPE_BOOLEAN )
        throw SonicParseException ( "operand of '!' must have boolean type", op );
}


void SonicParse_Expression_BinaryMathOp::validate ( 
    SonicParse_Program &program, 
    SonicParse_Function *func )
{
    lchild->validate ( program, func );
    rchild->validate ( program, func );

    SonicType ltype = lchild->determineType();
    if ( ltype != STYPE_REAL && ltype != STYPE_INTEGER )
        throw SonicParseException ( "left operand must have numeric type", op );

    SonicType rtype = rchild->determineType();
    if ( rtype != STYPE_REAL && rtype != STYPE_INTEGER )
        throw SonicParseException ( "right operand must have numeric type", op );
}


SonicType SonicParse_Lvalue::determineType ( 
    SonicParse_Program &prog,
    SonicParse_Function *func ) const
{
    if ( isWave )
        return STYPE_VECTOR;

    SonicParse_VarDecl *decl = prog.findSymbol ( varName, func, true );
    SonicType type = decl->queryType();
    return type;
}


void SonicParse_Lvalue::validate ( 
    SonicParse_Program &prog, 
    SonicParse_Function *func )
{
    SonicParse_VarDecl *decl = prog.findSymbol ( varName, func, true );
    if ( isWave )
    {
        if ( sampleLimit )
        {
            sampleLimit->validate ( prog, func );
            SonicType sltype = sampleLimit->determineType();
            if ( sltype != STYPE_REAL && sltype != STYPE_INTEGER )
                throw SonicParseException ( "sample limit expression must have numeric type", sampleLimit->getFirstToken() );
        }

        if ( decl->queryType() != STYPE_WAVE )
            throw SonicParseException ( "subscript '[]' allowed only on variable of wave type", varName );
    }
}


void SonicParse_Expression_FFT::validate (
    SonicParse_Program &prog,
    SonicParse_Function *func )
{
    input->validate ( prog, func );
    fftSize->validate ( prog, func );
    freqShift->validate ( prog, func );

    if ( !input->canConvertTo (STYPE_REAL) )
        throw SonicParseException ( "cannot convert fft input expression to type 'real'", input->getFirstToken() );

    if ( !fftSize->canConvertTo (STYPE_INTEGER) )
        throw SonicParseException ( "cannot convert fft size expression to type 'integer'", fftSize->getFirstToken() );

    // Make sure that 'funcName' is really the name of a user-defined function
    // with the following prototype:
    // function xxx ( real, real&, real& );

    SonicParse_Function *xferFunc = prog.findFunction ( funcName );
    if ( !xferFunc )
        throw SonicParseException ( "symbol not defined or is not a function", funcName );

    if ( xferFunc->numParameters() != 3 )
        throw SonicParseException ( "fft transfer function must accept 3 parameters", funcName );

    if ( xferFunc->queryReturnType() != STYPE_VOID )
        throw SonicParseException ( "fft transfer function must not return a value", funcName );

    SonicParse_VarDecl *parmList = xferFunc->queryParmList();
    if ( parmList->queryType() != STYPE_REAL || parmList->queryType().isReference() )
        throw SonicParseException ( "first parm of transfer function must be of type 'real'", funcName );

    parmList = parmList->queryNext();
    if ( parmList->queryType() != STYPE_REAL || !parmList->queryType().isReference() )
        throw SonicParseException ( "second parm of transfer function must be of type 'real &'", funcName );

    parmList = parmList->queryNext();
    if ( parmList->queryType() != STYPE_REAL || !parmList->queryType().isReference() )
        throw SonicParseException ( "third parm of transfer function must be of type 'real &'", funcName );

    if ( !freqShift->canConvertTo (STYPE_REAL) )
        throw SonicParseException ( "cannot convert fft frequency shift expression to type 'real'", freqShift->getFirstToken() );
}


void SonicParse_Expression_IIR::validate (
    SonicParse_Program &prog,
    SonicParse_Function *func )
{
    for ( SonicParse_Expression *ep = xCoeffList; ep; ep = ep->queryNext() )
    {
        ep->validate ( prog, func );
        if ( !ep->canConvertTo (STYPE_REAL) )
            throw SonicParseException ( "cannot convert filter x-coefficient to type 'real'", ep->getFirstToken() );
    }

    for ( SonicParse_Expression *ep = yCoeffList; ep; ep = ep->queryNext() )
    {
        ep->validate ( prog, func );
        if ( !ep->canConvertTo (STYPE_REAL) )
            throw SonicParseException ( "cannot convert filter y-coefficient to type 'real'", ep->getFirstToken() );
    }

    filterInput->validate ( prog, func );
    if ( !filterInput->canConvertTo (STYPE_REAL) )
        throw SonicParseException ( "cannot convert filter input expression to type 'real'", filterInput->getFirstToken() );
}


void SonicParse_Expression_Sinewave::validate (
    SonicParse_Program &prog,
    SonicParse_Function *func )
{
    amplitude->validate ( prog, func );
    if ( !amplitude->canConvertTo (STYPE_REAL) )
        throw SonicParseException ( "cannot convert amplitude expression to type 'real'", amplitude->getFirstToken() );

    frequencyHz->validate ( prog, func );
    if ( !frequencyHz->canConvertTo (STYPE_REAL) )
        throw SonicParseException ( "cannot convert frequency expression to type 'real'", frequencyHz->getFirstToken() );

    phaseDeg->validate ( prog, func );
    if ( !phaseDeg->canConvertTo (STYPE_REAL) )
        throw SonicParseException ( "cannot convert phase expression to type 'real'", phaseDeg->getFirstToken() );
}


void SonicParse_Expression_Sawtooth::validate ( 
    SonicParse_Program &prog, 
    SonicParse_Function *func )
{
    frequencyHz->validate ( prog, func );
    if ( !frequencyHz->canConvertTo (STYPE_REAL) )
        throw SonicParseException ( "cannot convert frequency expression to type 'real'", frequencyHz->getFirstToken() );
}


/*--- end of file validate.cpp ---*/
