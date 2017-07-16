/*==========================================================================

    codegen.cpp  -  Copyright (C) 1998 by Don Cross <dcross@intersrv.com>

    This module is responsible for generating output C++ source code
    once Sonic source code has been parsed and validated without finding
    any errors.  Note that this code generator does look for certain
    kinds of errors which may be missed by the parse and validate 
    phases, resulting in throwing a SonicParseException.

    Revision history:

1998 September 23 [Don Cross]
    Added code generation for new 'fft' pseudo-function.

1998 September 21 [Don Cross]
    Fixed a bug in which a non-import variable could have more than one
    initializer expression enclosed inside parentheses.

1998 September 19 [Don Cross]
    Added the ability to call the new member function SonicWave::interp().
    This allows a non-integer sample index to cause linear interpolation
    between the bounding integer samples, instead of just being truncated
    to the floor of the index.

1998 September 14 [Don Cross]
    Fixed a bug:  code was generating the strings 'true' and 'false' for
    the boolean constants.  Fixing this to output '1' and '0' instead.

1998 September 10 [Don Cross]
    Modified code generator so that '.reset()' call for import function
    objects is called only once per assignment, and only called at all
    when the assignment is a wave assignment.

==========================================================================*/
#include <iostream.h>
#include <stdio.h>

#include "scan.h"
#include "parse.h"


//-------------------------------------------------------------------------


void Sonic_CodeGenContext::indent ( ostream &o, const char *s )
{
    for ( int i=0; i < indentLevel; ++i )
        o << ' ';

    o << s;
}


//-------------------------------------------------------------------------


void SonicParse_Statement_Compound::generateCode ( ostream &o, Sonic_CodeGenContext &x )
{
    if ( compound )
    {
        if ( compound->next )
        {
            x.indent ( o, "{\n" );
            x.pushIndent();

            for ( SonicParse_Statement *stmt = compound; stmt; stmt = stmt->next )
                stmt->generateCode ( o, x );

            x.popIndent();
            x.indent ( o, "}\n" );
        }
        else
            compound->generateCode ( o, x );
    }
    else
        x.indent ( o, ";\n" );
}


void SonicParse_Statement_FunctionCall::generateCode ( ostream &o, Sonic_CodeGenContext &x )
{
    x.indent ( o );
    call->generateCode ( o, x );
    o << ";\n";

    if ( queryNext() )
        o << "\n";
}


void SonicParse_Statement_If::generateCode ( ostream &o, Sonic_CodeGenContext &x )
{
    x.indent ( o, "if ( " );
    condition->generateCode ( o, x );
    o << " )\n";

    if ( !ifPart->needsBraces() )
        x.pushIndent();

    ifPart->generateCode ( o, x );

    if( !ifPart->needsBraces() )
        x.popIndent();

    if ( elsePart )
    {
        x.indent ( o, "else\n" );

        if ( !elsePart->needsBraces() )
            x.pushIndent();

        elsePart->generateCode ( o, x );

        if ( !elsePart->needsBraces() )
            x.popIndent();
    }

    if ( queryNext() )
        o << "\n";
}


void SonicParse_Statement_Repeat::generateCode ( ostream &o, Sonic_CodeGenContext &x )
{
    const int tag = (x.nextTempTag)++;
    char t[64];
    sprintf ( t, "%s%d", TEMPORARY_PREFIX, tag );

    x.indent ( o, "for ( long " );
    o << t << " = long(";
    count->generateCode ( o, x );
    o << "); " << t << " > 0; --" << t << " )\n";
    
    if ( !loop->needsBraces() )
        x.pushIndent();

    loop->generateCode ( o, x );

    if ( !loop->needsBraces() )
        x.popIndent();

    if ( queryNext() )
        o << "\n";
}


void SonicParse_Statement_While::generateCode ( ostream &o, Sonic_CodeGenContext &x )
{
    x.indent ( o, "while ( " );
    condition->generateCode ( o, x );
    o << " )\n";

    if ( !loop->needsBraces() )
        x.pushIndent();

    loop->generateCode ( o, x );

    if ( !loop->needsBraces() )
        x.popIndent();

    if ( queryNext() )
        o << "\n";
}


void SonicParse_Statement_Return::generateCode ( ostream &o, Sonic_CodeGenContext &x )
{
    x.indent ( o, "return" );

    if ( returnValue )
    {
        o << " ";
        returnValue->generateCode ( o, x );
    }

    o << ";\n";
}


void SonicParse_Statement_Assignment::generateCode ( ostream &o, Sonic_CodeGenContext &x )
{
    if ( lvalue->queryIsWave() )
    {
        x.indent ( o, "{\n" );
        x.pushIndent();

        // put a comment to explain the complexity to follow
        x.generatingComment = true;
        x.indent ( o, "//  " );
        o << lvalue->queryVarName().queryToken() << "[c,i";
        SonicParse_Expression *limit = lvalue->querySampleLimit();
        if ( limit )
        {
            o << ":";
            limit->generateCode ( o, x );
        }
        
        o << "] " << op.queryToken() << " ";
        rvalue->generateCode ( o, x );
        o << ";\n\n";
        x.generatingComment = false;

        // Obtain list of all wave variables in rvalue
        const int maxWaveSymbols = 256;
        const SonicToken *waveSymbol [maxWaveSymbols];
        int numWaveSymbols = 0;
        int numOccurrences = 0;
        waveSymbol [numWaveSymbols++] = &lvalue->queryVarName();

        rvalue->getWaveSymbolList ( 
            waveSymbol, 
            maxWaveSymbols, 
            numWaveSymbols, 
            numOccurrences );

        bool modify = false;
        for ( int i=1; i < numWaveSymbols && !modify; ++i )
            if ( *waveSymbol[i] == "$" )
                modify = true;

        x.indent ( o, LOCAL_SYMBOL_PREFIX );
        const char *lname = lvalue->queryVarName().queryToken();
        o << lname;
        if ( op == "=" && !modify )
            o << ".openForWrite();\n";
        else if ( op == "<<" )
        {
            if ( modify )
                throw SonicParseException ( "Cannot use append operator when '$' appears on right side", op );

            o << ".openForAppend();\n";
        }
        else
        {
            o << ".openForModify();\n";
            modify = true;
        }

        for ( i=1; i < numWaveSymbols; i++ )
        {
            if ( *waveSymbol[i] != "$" )
            {
                x.indent ( o, LOCAL_SYMBOL_PREFIX );
                o << waveSymbol[i]->queryToken();
                o << ".openForRead();\n";
            }
        }

        bool implicitSelfNumSamples = false;

        x.indent ( o, "double sample [NumChannels];\n" );
        x.indent ( o, "double t = double(0);\n" );
        if ( limit )
        {
            x.indent ( o, "const long numSamples = long(" );
            x.bracketer = &lvalue->queryVarName();
            limit->generateCode ( o, x );
            x.bracketer = 0;
            o << ");\n";
        }
        else if ( numOccurrences == 0 && modify )
        {
            x.indent ( o, "const long numSamples = " );
            o << LOCAL_SYMBOL_PREFIX << lvalue->queryVarName().queryToken();
            o << ".queryNumSamples();\n";
            implicitSelfNumSamples = true;
        }

        const bool rvalueIsVector = (rvalue->queryExpressionType() == ETYPE_VECTOR);
        x.insideVector = rvalueIsVector;
        rvalue->generatePreSampleLoopCode ( o, x );
        x.insideVector = false;

        if ( limit || implicitSelfNumSamples )
            x.indent ( o, "for ( long i=0; i < numSamples; ++i, t += SampleTime )\n" );
        else
        {
            if ( numOccurrences == 0 )
            {
                throw SonicParseException ( 
                    "cannot determine number of samples to generate",
                    rvalue->getFirstToken() );
            }
            x.indent ( o, "for ( long i=0; ; ++i, t += SampleTime )\n" );
        }

        x.indent ( o, "{\n" );
        x.pushIndent();

        if ( numOccurrences > 0 )
        {
            if ( !limit )
            {
                x.indent ( o, "int countdown = NumChannels" );

                if ( numOccurrences > 1 )
                    o << " * " << numOccurrences;

                o << ";\n";
            }
            else 
                x.indent ( o, "int countdown;\n" );
        }

        if ( modify )
        {
            x.indent ( o, LOCAL_SYMBOL_PREFIX );
            o << lname << ".read ( sample );\n";
        }

        const char *assignOp = op.queryToken();
        if ( op == "<<" )
            assignOp = "=";

        x.insideVector = rvalueIsVector;
        rvalue->generatePreChannelLoopCode ( o, x );
        x.insideVector = false;

        if ( rvalueIsVector )
        {
            SonicParse_Expression_Vector *v = (SonicParse_Expression_Vector *) rvalue;
            const int numChannels = v->queryNumChannels();
            SonicParse_Expression *comp = v->getComponentList();
            x.iAllowed = x.cAllowed = true;
            x.insideVector = true;
            for ( x.channelValue=0; x.channelValue < numChannels; ++(x.channelValue) )
            {
                x.indent ( o, "sample[" );
                o << x.channelValue << "] " << assignOp << " ";
                comp->generateCode ( o, x );
                o << ";\n";
                comp = comp->queryNext();
            }
            x.iAllowed = x.cAllowed = false;
            x.insideVector = false;
            x.channelValue = -1;
        }
        else
        {
            const int numChannels = x.prog->queryNumChannels();
            x.iAllowed = x.cAllowed = true;
            for ( x.channelValue=0; x.channelValue < numChannels; ++(x.channelValue) )
            {
                x.indent ( o, "sample[" );
                o << x.channelValue << "] " << assignOp << " ";
                rvalue->generateCode ( o, x );
                o << ";\n";
            }
            x.iAllowed = x.cAllowed = false;
            x.channelValue = -1;
        }

        if ( !limit && !implicitSelfNumSamples && numOccurrences > 0 )
            x.indent ( o, "if ( countdown <= 0 ) break;\n" );

        x.indent ( o, LOCAL_SYMBOL_PREFIX );
        o << lname << ".write ( sample );\n";
        x.popIndent();
        x.indent ( o, "}\n" );

        for ( i=0; i < numWaveSymbols; i++ )
        {
            if ( *waveSymbol[i] != "$" )
            {
                x.indent ( o, LOCAL_SYMBOL_PREFIX );
                o << waveSymbol[i]->queryToken();
                o << ".close();\n";
            }
        }

        x.popIndent();
        x.indent ( o, "}\n" );

        if ( !x.func )
            throw SonicParseException ( "internal error: context lacks enclosing function", op );

        x.func->clearAllResetFlags();
    }
    else    // it must be a simple assignment which can be expressed directly in C++
    {
        if ( op == "<<" )
            throw SonicParseException ( "append operator '<<' is allowed only in wave assignments", op );

        x.indent ( o, LOCAL_SYMBOL_PREFIX );
        o << lvalue->queryVarName().queryToken();
        o << " " << op.queryToken() << " ";
        rvalue->generateCode ( o, x );
        o << ";\n";
    }

    if ( queryNext() )
        o << "\n";
}


void SonicParse_Function::generatePrototype ( ostream &o, Sonic_CodeGenContext &x )
{
    switch ( returnType.queryTypeClass() )
    {
        case STYPE_VOID:        o << "void ";       break;
        case STYPE_INTEGER:     o << "long ";       break;
        case STYPE_REAL:        o << "double ";     break;
        case STYPE_BOOLEAN:     o << "int ";        break;

        case STYPE_WAVE:        
            throw SonicParseException ( "function not allowed to return wave type", name );

        case STYPE_STRING:
            throw SonicParseException ( "internal error: function return type was 'string'", name );

        case STYPE_VECTOR:
            throw SonicParseException ( "internal error: function return type was 'vector'", name );

        default:
            throw SonicParseException ( "internal error: undefined function return type", name );
    }

    o << FUNCTION_PREFIX << name.queryToken() << " (";

    if ( parmList )
    {
        o << "\n";
        x.pushIndent();
        x.insideFunctionParms = true;

        for ( SonicParse_VarDecl *pp = parmList; pp; pp = pp->next )
        {
            x.indent ( o );
            pp->generateCode ( o, x );
            if ( pp->next )
                o << ",\n";
        }

        x.insideFunctionParms = false;
        x.popIndent();
    }

    o << " )";
}


void SonicParse_Function::generateCode ( ostream &o, Sonic_CodeGenContext &x )
{
    SonicParse_Function *fsave = x.func;        // in case we ever have nested functions!
    x.func = this;

    o << "\n";
    generatePrototype ( o, x );
    o << "\n{\n";

    x.pushIndent();

    if ( varList )
    {
        for ( SonicParse_VarDecl *vp = varList; vp; vp = vp->next )
        {
            x.indent ( o );
            vp->generateCode ( o, x );
            o << ";\n";
        }

        o << "\n";
    }

    for ( SonicParse_Statement *sp = statementList; sp; sp = sp->next )
        sp->generateCode ( o, x );

    x.popIndent();
    o << "}\n\n";

    x.func = fsave;
}


void SonicParse_VarDecl::generateCode ( ostream &o, Sonic_CodeGenContext &x )
{
    switch ( type.queryTypeClass() )
    {
        case STYPE_VOID:
            throw SonicParseException ( "internal error: symbol with type 'void'", name );

        case STYPE_INTEGER:     o << "long ";       break;
        case STYPE_REAL:        o << "double ";     break;
        case STYPE_BOOLEAN:     o << "int ";        break;

        case STYPE_WAVE:
        {
            o << "SonicWave ";
            if ( x.insideFunctionParms )
                o << "&";
        }
        break;

        case STYPE_IMPORT:
        {
            const SonicToken *iname = type.queryImportName();
            if ( !iname )
                throw SonicParseException ( "internal error: cannot resolve import type!", name );

            o << IMPORT_PREFIX << iname->queryToken() << " ";
            if ( x.insideFunctionParms )
                o << "&";
        }
        break;

        case STYPE_STRING:
            throw SonicParseException ( "internal error: symbol with type 'string'", name );

        case STYPE_VECTOR:
            throw SonicParseException ( "internal error: symbol with type 'vector'", name );

        default:
            throw SonicParseException ( "internal error: symbol with undefined type", name );
    }

    if ( type.isReference() )
    {
        if ( !x.insideFunctionParms )
            throw SonicParseException ( "internal error: found reference type outside of function parms", name );

        o << "&";
    }

    o << LOCAL_SYMBOL_PREFIX << name.queryToken();

    if ( init )
    {
        if ( x.insideFunctionParms )
            throw SonicParseException ( "internal error: function parameter has initializer", name );

        if ( type == STYPE_WAVE )
        {
            throw SonicParseException ( "wave variable cannot have initializer", name );
        }
        else if ( type == STYPE_IMPORT )
        {
            o << " ( ";

            for ( SonicParse_Expression *ip = init; ip; ip = ip->queryNext() )
            {
                ip->generateCode ( o, x );
                if ( ip->queryNext() )
                    o << ", ";
            }

            o << " )";
        }
        else
        {
            if ( init->next )
                throw SonicParseException ( "this variable must not have multiple initializer expressions", name );

            o << " = ";
            bool typeCast = (type == STYPE_INTEGER && init->determineType() == STYPE_REAL);
            if ( typeCast )
                o << "long(";

            init->generateCode ( o, x );

            if ( typeCast )
                o << ")";
        }
    }
    else if ( !x.insideFunctionParms )
    {
        if ( type == STYPE_INTEGER || type == STYPE_REAL || type == STYPE_BOOLEAN )
        {
            // supply default initializer...

            o << " = 0";
        }
        else if ( type == STYPE_WAVE )
        {
            // supply constructor parameters

            o << " ( \"\", \"" << name.queryToken() << "\", SamplingRate, NumChannels )";
        }
    }
}


void SonicParse_Expression_Vector::generateCode ( ostream &o, Sonic_CodeGenContext &x )
{
    o << "{ ";

    for ( SonicParse_Expression *ep = exprList; ep; ep = ep->queryNext() )
    {
        ep->generateCode ( o, x );
        if ( ep->queryNext() )
            o << ", ";
    }

    o << " }";
}


void SonicParse_Expression_Vector::generatePreSampleLoopCode ( 
    ostream &o, 
    Sonic_CodeGenContext &x )
{
    x.channelValue = 0;
    for ( SonicParse_Expression *ep = exprList; ep; ep = ep->queryNext() )
    {
        ep->generatePreSampleLoopCode ( o, x );
        ++x.channelValue;
    }
}


void SonicParse_Expression_Vector::generatePreChannelLoopCode ( 
    ostream &o, 
    Sonic_CodeGenContext &x )
{
    x.channelValue = 0;
    for ( SonicParse_Expression *ep = exprList; ep; ep = ep->queryNext() )
    {
        ep->generatePreChannelLoopCode ( o, x );
        ++x.channelValue;
    }
}


void SonicParse_Expression_BinaryBoolOp::generateCode ( ostream &o, Sonic_CodeGenContext &x )
{
    bool paren = lchild->operatorPrecedence() < operatorPrecedence();

    if ( paren )
        o << "(";

    lchild->generateCode ( o, x );

    if ( paren )
        o << ")";

    if ( op == "|" )
        o << " || ";
    else if ( op == "&" )
        o << " && ";
    else
        o << " " << op.queryToken() << " ";

    if ( rchild->operatorPrecedence() == operatorPrecedence() )
        paren = groupsToRight();
    else
        paren = rchild->operatorPrecedence() < operatorPrecedence();

    if ( paren )
        o << "(";

    rchild->generateCode ( o, x );

    if ( paren )
        o << ")";
}


void SonicParse_Expression_BinaryOp::generatePreSampleLoopCode ( 
    ostream &o, 
    Sonic_CodeGenContext &x )
{
    lchild->generatePreSampleLoopCode ( o, x );
    rchild->generatePreSampleLoopCode ( o, x );
}


void SonicParse_Expression_BinaryOp::generatePreChannelLoopCode ( 
    ostream &o, 
    Sonic_CodeGenContext &x )
{
    lchild->generatePreChannelLoopCode ( o, x );
    rchild->generatePreChannelLoopCode ( o, x );
}


void SonicParse_Expression_UnaryOp::generatePreSampleLoopCode ( 
    ostream &o, 
    Sonic_CodeGenContext &x )
{
    child->generatePreSampleLoopCode ( o, x );
}


void SonicParse_Expression_UnaryOp::generatePreChannelLoopCode ( 
    ostream &o, 
    Sonic_CodeGenContext &x )
{
    child->generatePreChannelLoopCode ( o, x );
}


void SonicParse_Expression_BinaryMathOp::generateCode ( ostream &o, Sonic_CodeGenContext &x )
{
    bool paren = lchild->operatorPrecedence() < operatorPrecedence();

    if ( paren )
        o << "(";

    lchild->generateCode ( o, x );

    if ( paren )
        o << ")";

    bool space = (op == "+" || op == "-");
    if ( space )
        o << " ";

    o << op.queryToken();

    if ( space )
        o << " ";

    if ( rchild->operatorPrecedence() == operatorPrecedence() )
        paren = groupsToRight();
    else
        paren = rchild->operatorPrecedence() < operatorPrecedence();

    if ( paren )
        o << "(";

    rchild->generateCode ( o, x );

    if ( paren )
        o << ")";
}


void SonicParse_Expression_Mod::generateCode ( ostream &o, Sonic_CodeGenContext &x )
{
    SonicType ltype = lchild->determineType();
    SonicType rtype = rchild->determineType();

    if ( ltype != STYPE_INTEGER || rtype != STYPE_INTEGER )
    {
        o << "fmod(double(";
        lchild->generateCode ( o, x );
        o << "),double(";
        rchild->generateCode ( o, x );
        o << "))";
    }
    else
        SonicParse_Expression_BinaryMathOp::generateCode ( o, x );
}


void SonicParse_Expression_Power::generateCode ( ostream &o, Sonic_CodeGenContext &x )
{
    o << "pow(double(";
    lchild->generateCode ( o, x );
    o << "),double(";
    rchild->generateCode ( o, x );
    o << "))";
}


void SonicParse_Expression_OldData::generateCode ( ostream &o, Sonic_CodeGenContext &x )
{
    if ( x.generatingComment )
    {
        o << "$";
    }
    else
    {
        if ( !x.iAllowed )
            throw SonicParseException ( "Old-data symbol cannot appear here", dollarSign );

        o << "sample[" << x.channelValue << "]";
    }
}


void SonicParse_Expression_WaveExpr::generateCode ( ostream &o, Sonic_CodeGenContext &x )
{
    if ( x.generatingComment )
    {
        o << waveName.queryToken() << "[";
        cterm->generateCode ( o, x );
        o << ",";
        iterm->generateCode ( o, x );
        o << "]";
    }
    else
    {
        if ( !x.iAllowed )
            throw SonicParseException ( "wave expression not allowed here", waveName );

        const SonicToken *saveBracketer = x.bracketer;
        x.bracketer = &waveName;

        SonicType indexType = iterm->determineType();

        if ( x.prog->queryInterpolateFlag() && indexType != STYPE_INTEGER )
        {
            o << LOCAL_SYMBOL_PREFIX << waveName.queryToken() << ".interp(int(";
            cterm->generateCode ( o, x );
            o << "), double(";
            iterm->generateCode ( o, x );
            o << "), countdown)";
        }
        else
        {
            o << LOCAL_SYMBOL_PREFIX << waveName.queryToken() << ".fetch(int(";
            cterm->generateCode ( o, x );
            o << "), long(";
            iterm->generateCode ( o, x );
            o << "), countdown)";
        }

        x.bracketer = saveBracketer;
    }
}


void SonicParse_Expression_WaveExpr::generatePreSampleLoopCode ( 
    ostream &o, 
    Sonic_CodeGenContext &x )
{
    bool isave = x.iAllowed;
    bool csave = x.cAllowed;
    x.iAllowed = x.cAllowed = true;
    cterm->generatePreSampleLoopCode ( o, x );
    iterm->generatePreSampleLoopCode ( o, x );
    x.iAllowed = isave;
    x.cAllowed = csave;
}


void SonicParse_Expression_WaveExpr::generatePreChannelLoopCode ( 
    ostream &o, 
    Sonic_CodeGenContext &x )
{
    bool isave = x.iAllowed;
    bool csave = x.cAllowed;
    x.iAllowed = x.cAllowed = true;
    cterm->generatePreChannelLoopCode ( o, x );
    iterm->generatePreChannelLoopCode ( o, x );
    x.iAllowed = isave;
    x.cAllowed = csave;
}


void SonicParse_Expression_Constant::generateCode ( ostream &o, Sonic_CodeGenContext &x )
{
    if ( type == STYPE_STRING )
        o << '"' << value.queryToken() << '"';
    else
        o << value.queryToken();
}


void SonicParse_Expression_Constant::generatePreSampleLoopCode ( 
    ostream &o, 
    Sonic_CodeGenContext &x )
{
}


void SonicParse_Expression_Constant::generatePreChannelLoopCode ( 
    ostream &o, 
    Sonic_CodeGenContext &x )
{
}


void SonicParse_Expression_FunctionCall::generateCode ( ostream &o, Sonic_CodeGenContext &x )
{
    if ( !x.generatingComment )
    {
        if ( ftype == SFT_USER )
            o << FUNCTION_PREFIX;
        else if ( ftype == SFT_IMPORT )
            o << LOCAL_SYMBOL_PREFIX;
    }

    const bool needDoubleCast = isIntrinsic() && !x.generatingComment;

    o << name.queryToken() << "(";
    for ( SonicParse_Expression *pp = parmList; pp; pp = pp->queryNext() )
    {
        if ( needDoubleCast )
            o << "double(";

        pp->generateCode ( o, x );

        if ( needDoubleCast )
            o << ")";

        if ( pp->queryNext() )
            o << ", ";
    }
    o << ")";
}


void SonicParse_Expression_FunctionCall::generatePreSampleLoopCode ( 
    ostream &o, 
    Sonic_CodeGenContext &x )
{
    if ( ftype == SFT_IMPORT )
    {
        if ( !x.func )
            throw SonicParseException ( "internal error: context lacks enclosing function!", name );

        // Make sure we generate '.reset' call for an import function object 
        // exactly once per assignment statement, no matter how many times the
        // function is used in that statement.

        SonicParse_VarDecl *var = x.prog->findSymbol ( name, x.func, true );
        if ( !var->queryResetFlag() )
        {
            var->modifyResetFlag (true);
            x.indent ( o, LOCAL_SYMBOL_PREFIX );
            o << name.queryToken() << ".reset ( NumChannels, SamplingRate );\n";
        }
    }

    for ( SonicParse_Expression *pp = parmList; pp; pp = pp->queryNext() )
        pp->generatePreSampleLoopCode(o,x);
}


void SonicParse_Expression_FunctionCall::generatePreChannelLoopCode ( 
    ostream &o, 
    Sonic_CodeGenContext &x )
{
    for ( SonicParse_Expression *pp = parmList; pp; pp = pp->queryNext() )
        pp->generatePreChannelLoopCode(o,x);
}


void SonicParse_Expression_Builtin::generateCode ( ostream &o, Sonic_CodeGenContext &x )
{
    if ( x.generatingComment )
    {
        o << name.queryToken();
    }
    else
    {
        if ( name == "r" )
            o << "SamplingRate";
        else if ( name == "m" )
            o << "NumChannels";
        else if ( name == "true" )
            o << "1";
        else if ( name == "false" )
            o << "0";
        else if ( name == "interpolate" )
            o << "InterpolateFlag";
        else if ( name == "n" )
        {
            if ( x.bracketer )
                o << LOCAL_SYMBOL_PREFIX << x.bracketer->queryToken() << ".queryNumSamples()";
            else
                throw SonicParseException ( "expected '<wavename>.' before 'n'", name );
        }
        else
        {
            if ( !x.iAllowed )
            {
                if ( name == "i" || name == "t" )
                    throw SonicParseException ( "time-based placeholder not allowed here", name );
            }

            if ( !x.cAllowed )
            {
                if ( name == "c" )
                    throw SonicParseException ( "channel placeholder not allowed here", name );
            }

            if ( name == "c" && x.channelValue >= 0 )
                o << x.channelValue;
            else
                o << name.queryToken();
        }
    }
}


void SonicParse_Expression_Variable::generateCode ( ostream &o, Sonic_CodeGenContext &x )
{
    if ( !x.generatingComment )
        o << LOCAL_SYMBOL_PREFIX;

    o << name.queryToken();
}


SonicType SonicParse_Expression_WaveField::determineType() const
{
    if ( field == "max" )
        return STYPE_REAL;
    else
        return STYPE_INTEGER;
}



void SonicParse_Expression_WaveField::generateCode ( ostream &o, Sonic_CodeGenContext &x )
{
    if ( x.generatingComment )
    {
        o << varName.queryToken() << "." << field.queryToken();
    }
    else
    {
        if ( field == "r" )
            o << "SamplingRate";
        else if ( field == "m" )
            o << "NumChannels";
        else if ( field == "interpolate" )
            o << "InterpolateFlag";
        else
        {
            o << LOCAL_SYMBOL_PREFIX << varName.queryToken();
            if ( field == "n" )
                o << ".queryNumSamples()";
            else if ( field == "max" )
                o << ".queryMaxValue()";
            else
                throw SonicParseException ( "unknown wave field", field );
        }
    }
}


void SonicParse_Expression_UnaryOp::generateCode ( ostream &o, Sonic_CodeGenContext &x )
{
    o << op.queryToken();
    if ( child->operatorPrecedence() <= operatorPrecedence() )
    {
        o << "(";
        child->generateCode ( o, x );
        o << ")";
    }
    else
    {
        child->generateCode ( o, x );
    }
}


//----------------------------------------------------------------------------


void SonicParse_Expression_FFT::generatePreSampleLoopCode (
    ostream &o,
    Sonic_CodeGenContext &x )
{
    input->generatePreSampleLoopCode ( o, x );
    tempTag = (x.nextTempTag)++;
    x.indent ( o, "Sonic_FFT_Filter " );
    o << TEMPORARY_PREFIX << tempTag << " ( NumChannels, SamplingRate, int(";
    fftSize->generateCode ( o, x );
    o << "), " << FUNCTION_PREFIX << funcName.queryToken() << ", double(";
    freqShift->generateCode ( o, x );
    o << ") );\n";    
}


void SonicParse_Expression_FFT::generatePreChannelLoopCode ( 
    ostream &o, 
    Sonic_CodeGenContext &x )
{
    input->generatePreChannelLoopCode ( o, x );
}


void SonicParse_Expression_FFT::generateCode ( 
    ostream &o, 
    Sonic_CodeGenContext &x )
{
    if ( x.generatingComment )
    {
        o << "fft(";
        input->generateCode ( o, x );
        o << ",";
        fftSize->generateCode ( o, x );
        o << "," << funcName.queryToken() << ",";
        freqShift->generateCode ( o, x );
        o << ")";
    }
    else
    {
        if ( !x.iAllowed || !x.cAllowed )
            throw SonicParseException ( "pseudo-function 'fft' not allowed here", fftToken );

        o << TEMPORARY_PREFIX << tempTag << ".filter(" << x.channelValue << ", ";
        input->generateCode ( o, x );
        o << ")";
    }
}


//----------------------------------------------------------------------------


void SonicParse_Expression_IIR::generatePreSampleLoopCode  ( 
    ostream &o, 
    Sonic_CodeGenContext &x )
{
    filterInput->generatePreSampleLoopCode ( o, x );

    x.indent ( o, "const double " );
    o << TEMPORARY_PREFIX << (t_xCoeff = (x.nextTempTag)++) << "[] = {    // iir x-coefficients\n";
    x.pushIndent();

    int xCoeffCount = 0;
    for ( SonicParse_Expression *ep = xCoeffList; ep; ep = ep->queryNext() )
    {
        ++xCoeffCount;
        x.indent ( o );
        ep->generateCode ( o, x );
        if ( ep->queryNext() )
            o << ",\n";
    }

    o << " };\n";
    x.popIndent();

    int yCoeffCount = 0;
    for ( ep = yCoeffList; ep; ep = ep->queryNext() )
        ++yCoeffCount;

    if ( yCoeffCount > 0 )
    {
        x.indent ( o, "const double " );
        o << TEMPORARY_PREFIX << (t_yCoeff = (x.nextTempTag)++) << "[] = {    // iir y-coefficients\n";
        x.pushIndent();

        for ( ep = yCoeffList; ep; ep = ep->queryNext() )
        {
            x.indent ( o );
            ep->generateCode ( o, x );
            if ( ep->queryNext() )
                o << ",\n";
        }

        o << " };\n";
        x.popIndent();
    }

    const int numChannels = x.prog->queryNumChannels();
    for ( int c=0; c < numChannels; ++c )
    {
        x.indent ( o, "double " );
        o << TEMPORARY_PREFIX << (t_xBuffer[c] = (x.nextTempTag)++) << "[] = { ";
        for ( int k=0; k < xCoeffCount; ++k )
        {
            if ( k > 0 )
                o << ", ";

            o << "0";
        }
        o << " };     // iir x-buffer [c=" << c << "]\n";

        if ( yCoeffCount > 0 )
        {
            x.indent ( o, "double " );
            o << TEMPORARY_PREFIX << (t_yBuffer[c] = (x.nextTempTag)++) << "[] = { ";
            for ( k=0; k < yCoeffCount; ++k )
            {
                if ( k > 0 )
                    o << ", ";

                o << "0";
            }
            o << " };     // iir y-buffer [c=" << c << "]\n";
        }
    }

    x.indent ( o, "int " );
    o << TEMPORARY_PREFIX << (t_xIndex = (x.nextTempTag)++) << " = 0;   // iir x-index\n";

    if ( yCoeffCount > 0 )
    {
        x.indent ( o, "int " );
        o << TEMPORARY_PREFIX << (t_yIndex = (x.nextTempTag)++) << " = 0;   // iir y-index\n";
    }
    else
        t_yIndex = 0;
}


void SonicParse_Expression_IIR::generatePreChannelLoopCode ( 
    ostream &o, 
    Sonic_CodeGenContext &x )
{
    filterInput->generatePreChannelLoopCode ( o, x );

    int xCoeffCount = 0;
    for ( SonicParse_Expression *ep = xCoeffList; ep; ep = ep->queryNext() )
        ++xCoeffCount;

    int yCoeffCount = 0;
    for ( ep = yCoeffList; ep; ep = ep->queryNext() )
        ++yCoeffCount;

    char xIndex [16];
    sprintf ( xIndex, "%s%d", TEMPORARY_PREFIX, t_xIndex );

    char yIndex [16];
    sprintf ( yIndex, "%s%d", TEMPORARY_PREFIX, t_yIndex );

    if ( xCoeffCount > 1 )
    {
        if ( xCoeffCount == 2 )
        {
            x.indent ( o, xIndex );
            o << " ^= 1;\n";
        }
        else
        {
            x.indent ( o, "if ( --" );
            o << xIndex << " < 0 )  " << xIndex << " = " << (xCoeffCount-1) << ";\n";
        }
    }

    if ( yCoeffCount > 1 )
    {
        if ( yCoeffCount == 2 )
        {
            x.indent ( o, yIndex );
            o << " ^= 1;\n";
        }
        else
        {
            x.indent ( o, "if ( --" );
            o << yIndex << " < 0 )  " << yIndex << " = " << (yCoeffCount-1) << ";\n";
        }
    }

    bool isave = x.iAllowed;
    bool csave = x.cAllowed;
    x.iAllowed = x.cAllowed = true;
    const int numChannels = x.prog->queryNumChannels();
    for ( x.channelValue = 0; x.channelValue < numChannels; ++(x.channelValue) )
    {
        x.indent ( o, TEMPORARY_PREFIX );
        o << t_xBuffer[x.channelValue] << "[" << xIndex << "] = ";
        filterInput->generateCode ( o, x );
        o << ";\n";
    }
    x.iAllowed = isave;
    x.cAllowed = csave;

    t_accum = (x.nextTempTag)++;
    char accum [16];
    sprintf ( accum, "%s%d", TEMPORARY_PREFIX, t_accum );

    x.indent ( o, "double " );
    o << accum << "[] = { ";

    for ( int c=0; c < numChannels; ++c )
    {
        if ( c > 0 )
            o << ", ";

        o << "0";
    }

    o << " };   // iir accumulator\n";

    int t_wrap = (x.nextTempTag)++;
    char wrap [16];
    sprintf ( wrap, "%s%d", TEMPORARY_PREFIX, t_wrap );

    if ( yCoeffCount > 1 || xCoeffCount > 1 )
    {
        x.indent ( o, "int " );
        o << wrap << " = " << xIndex << ";    // iir wraparound index\n";
    }

    int t_counter = (x.nextTempTag)++;
    char counter [16];
    sprintf ( counter, "%s%d", TEMPORARY_PREFIX, t_counter );
    if ( yCoeffCount > 1 || xCoeffCount > 1 )
    {
        x.indent ( o, "int " );
        o << counter << ";\n";
    }

    if ( xCoeffCount == 1 )
    {
        for ( c=0; c < numChannels; ++c )
        {
            x.indent ( o, accum );
            o << "[" << c << "] += " << TEMPORARY_PREFIX << t_xBuffer[c] << "[0]";
            o << " * " << TEMPORARY_PREFIX << t_xCoeff << "[0];";

            if ( c == 0 )
                o << "    // iir x dot product";

            o << "\n";
        }
    }
    else
    {
        x.indent ( o, "for ( " );
        o << counter << "=0; " << counter << "<" << xCoeffCount << "; ++" << counter << " )";
        o << "    // iir x dot product\n";

        x.indent ( o, "{\n" );
        x.pushIndent();

        for ( c=0; c < numChannels; ++c )
        {
            x.indent ( o, accum );
            o << "[" << c << "] += " << TEMPORARY_PREFIX << t_xBuffer[c] << "[" << wrap << "]";
            o << " * " << TEMPORARY_PREFIX << t_xCoeff << "[" << counter << "];\n";
        }

        if ( xCoeffCount == 2 )
        {
            x.indent ( o, wrap );
            o << " ^= 1;\n";
        }
        else
        {
            x.indent ( o, "if ( ++" );
            o << wrap << " == " << xCoeffCount << " )  " << wrap << " = 0;\n";
        }
        x.popIndent();
        x.indent ( o, "}\n" );
    }

    if ( yCoeffCount > 0 )
    {
        if ( yCoeffCount == 1 )
        {
            for ( c=0; c < numChannels; ++c )
            {
                x.indent ( o, accum );
                o << "[" << c << "] += " << TEMPORARY_PREFIX << t_yBuffer[c] << "[0]";
                o << " * " << TEMPORARY_PREFIX << t_yCoeff << "[0];";

                if ( c == 0 )
                    o << "    // iir y dot product";

                o << "\n";
            }
        }
        else
        {
            x.indent ( o, "for ( " );
            o << wrap << "=" << yIndex << ", " << counter << "=0; ";
            o << counter << " < " << yCoeffCount << "; ++" << counter << " )";
            o << "    // iir y dot product\n";

            x.indent ( o, "{\n" );
            x.pushIndent();

            if ( yCoeffCount == 2 )
            {
                x.indent ( o, wrap );
                o << " ^= 1;\n";
            }
            else
            {
                x.indent ( o, "if ( ++" );
                o << wrap << " == " << yCoeffCount << " )  " << wrap << " = 0;\n";
            }

            for ( c=0; c < numChannels; ++c )
            {
                x.indent ( o, accum );
                o << "[" << c << "] += " << TEMPORARY_PREFIX << t_yBuffer[c] << "[" << wrap << "]";
                o << " * " << TEMPORARY_PREFIX << t_yCoeff << "[" << counter << "];\n";
            }

            x.popIndent();
            x.indent ( o, "}\n" );
        }

        for ( c=0; c < numChannels; ++c )
        {
            x.indent ( o, TEMPORARY_PREFIX );
            o << t_yBuffer[c] << "[" << yIndex << "] = " << accum << "[" << c << "];\n";
        }
    }
}


void SonicParse_Expression_IIR::generateCode ( 
    ostream &o, 
    Sonic_CodeGenContext &x )
{
    SonicParse_Expression *ep = 0;

    if ( x.generatingComment )
    {
        o << "iir({";

        for ( ep = xCoeffList; ep; ep = ep->queryNext() )
        {
            ep->generateCode ( o, x );
            if ( ep->queryNext() )
                o << ",";
        }

        o << "},{";             // hey, this looks like a VanB command prompt!

        for ( ep = yCoeffList; ep; ep = ep->queryNext() )
        {
            ep->generateCode ( o, x );
            if ( ep->queryNext() )
                o << ",";
        }

        o << "},";      
        filterInput->generateCode ( o, x );
        o << ")";
    }
    else
    {
        if ( !x.iAllowed )
            throw SonicParseException ( "iir construct not allowed here", iirToken );

        o << TEMPORARY_PREFIX << t_accum << "[" << x.channelValue << "]";
    }
}


//----------------------------------------------------------------------------


void SonicParse_Expression_Sawtooth::generatePreSampleLoopCode ( 
    ostream &o, 
    Sonic_CodeGenContext &x )
{
    channelDependent = isChannelDependent();

    int channelSave = x.channelValue;
    bool csave = x.cAllowed;
    x.cAllowed = true;

    int cLimit = 0;
    int cStart = 0;
    if ( x.insideVector )
    {
        cStart = x.channelValue;
        cLimit = 1 + cStart;
    }
    else
    {
        cStart = 0;
        cLimit = channelDependent ? x.prog->queryNumChannels() : 1;
    }

    for ( x.channelValue = cStart; x.channelValue < cLimit; ++x.channelValue )
    {
        tempTag[x.channelValue] = (x.nextTempTag)++;
        char t [16];
        sprintf ( t, "%s%d", TEMPORARY_PREFIX, tempTag[x.channelValue] );

        x.indent ( o, "double " );
        o << t << "[] = { 0, 4*SampleTime*(";
        frequencyHz->generateCode ( o, x );
        o << ") };   // sawtooth init";

        if ( channelDependent || x.insideVector )
            o << " [c=" << x.channelValue << "]";

        o << "\n";
        x.indent ( o, t );
        o << "[0] -= " << t << "[1];\n";
    }

    x.cAllowed = csave;
    x.channelValue = channelSave;

    if ( !channelDependent && !x.insideVector )
    {
        // replicate the tempTag to all channels...
        const int numChannels = x.prog->queryNumChannels();
        for ( int c=1; c < numChannels; ++c )
            tempTag[c] = tempTag[0];
    }
}


void SonicParse_Expression_Sawtooth::generatePreChannelLoopCode ( 
    ostream &o, 
    Sonic_CodeGenContext &x )
{
    int channelSave = x.channelValue;
    bool csave = x.cAllowed;
    x.cAllowed = true;
    
    int cLimit = 0;
    int cStart = 0;
    if ( x.insideVector )
    {
        cStart = x.channelValue;
        cLimit = 1 + cStart;
    }
    else
    {
        cStart = 0;
        cLimit = channelDependent ? x.prog->queryNumChannels() : 1;
    }

    for ( x.channelValue = cStart; x.channelValue < cLimit; ++x.channelValue )
    {
        char t [16];
        sprintf ( t, "%s%d", TEMPORARY_PREFIX, tempTag[x.channelValue] );

        x.indent ( o, t );
        o << "[0] += " << t << "[1];   // sawtooth update";

        if ( channelDependent || x.insideVector )
            o << " [c=" << x.channelValue << "]";

        o << "\n";
        x.indent ( o, "if ( " );
        o << t << "[0] > 1.0 )\n";
        x.indent ( o, "{\n" );
        x.pushIndent();
        x.indent ( o, t );
        o << "[1] = -" << t << "[1];\n";
        x.indent ( o, t );
        o << "[0] = 2.0 - " << t << "[0];\n";
        x.popIndent();
        x.indent ( o, "}\n" );
        x.indent ( o, "else if ( " );
        o << t << "[0] < -1.0 )\n";
        x.indent ( o, "{\n" );
        x.pushIndent();
        x.indent ( o, t );
        o << "[1] = -" << t << "[1];\n";
        x.indent ( o, t );
        o << "[0] = -2.0 - " << t << "[0];\n";
        x.popIndent();
        x.indent ( o, "}\n" );
    }

    x.cAllowed = csave;
    x.channelValue = channelSave;
}


void SonicParse_Expression_Sawtooth::generateCode ( 
    ostream &o, 
    Sonic_CodeGenContext &x )
{
    if ( x.generatingComment )
    {
        o << "sawtooth(";
        frequencyHz->generateCode ( o, x );
        o << ")";
    }
    else
    {
        if ( !x.cAllowed )
            throw SonicParseException ( "sawtooth construct not allowed here", sawtoothToken );

        o << TEMPORARY_PREFIX << tempTag[x.channelValue] << "[0]";
    }
}


//----------------------------------------------------------------------------


void SonicParse_Expression_Sinewave::generatePreSampleLoopCode  ( 
    ostream &o, 
    Sonic_CodeGenContext &x )
{
    channelDependent = isChannelDependent();
    bool csave = x.cAllowed;
    x.cAllowed = true;

    int cLimit = 0;
    int cStart = 0;
    if ( x.insideVector )
    {
        cStart = x.channelValue;
        cLimit = 1 + cStart;
    }
    else
    {
        cStart = 0;
        cLimit = channelDependent ? x.prog->queryNumChannels() : 1;
    }

    int channelSave = x.channelValue;
    for ( x.channelValue = cStart; x.channelValue < cLimit; ++x.channelValue )
    {
        tempTag[x.channelValue] = (x.nextTempTag)++;
        char t [16];
        sprintf ( t, "%s%d", TEMPORARY_PREFIX, tempTag[x.channelValue] );

        x.indent ( o, "double " );
        o << t << "[4];     // sinewave init";

        if ( channelDependent || x.insideVector )
            o << " [c=" << x.channelValue << "]";

        o << "\n";

        x.indent ( o, t );
        o << "[2] = -2 * pi * (";
        frequencyHz->generateCode ( o, x );
        o << ") * SampleTime;\n";

        x.indent ( o, t );
        o << "[1] = (";
        phaseDeg->generateCode ( o, x );
        o << ") * pi / 180.0;\n";

        x.indent ( o, t );
        o << "[3] = ";
        amplitude->generateCode ( o, x );
        o << ";\n";

        x.indent ( o, t );
        o << "[0] = " << t << "[3] * sin ( 2*" << t << "[2] + " << t << "[1] );\n";

        x.indent ( o, t );
        o << "[1] = " << t << "[3] * sin ( " << t << "[2] + " << t << "[1] );\n";

        x.indent ( o, t );
        o << "[3] = 2 * cos ( " << t << "[2] );\n";
    }

    x.channelValue = channelSave;
    x.cAllowed = csave;

    if ( !channelDependent && !x.insideVector )
    {
        // replicate the tempTag to all channels...
        const int numChannels = x.prog->queryNumChannels();
        for ( int c=1; c < numChannels; ++c )
            tempTag[c] = tempTag[0];
    }
}


void SonicParse_Expression_Sinewave::generatePreChannelLoopCode ( 
    ostream &o, 
    Sonic_CodeGenContext &x )
{
    bool csave = x.cAllowed;
    x.cAllowed = true;

    int cLimit = 0;
    int cStart = 0;
    if ( x.insideVector )
    {
        cStart = x.channelValue;
        cLimit = 1 + cStart;
    }
    else
    {
        cStart = 0;
        cLimit = channelDependent ? x.prog->queryNumChannels() : 1;
    }

    int channelSave = x.channelValue;
    for ( x.channelValue = cStart; x.channelValue < cLimit; ++x.channelValue )
    {
        char t [64];
        sprintf ( t, "%s%d", TEMPORARY_PREFIX, tempTag[x.channelValue] );

        x.indent ( o, t );
        o << "[2] = " << t << "[3]*" << t << "[1] - " << t << "[0];   // sinewave update";

        if ( channelDependent || x.insideVector )
            o << " [c=" << x.channelValue << "]";

        o << "\n";

        x.indent ( o, t );
        o << "[0] = " << t << "[1];\n";

        x.indent ( o, t );
        o << "[1] = " << t << "[2];\n"; 
    }

    x.channelValue = channelSave;
    x.cAllowed = csave;
}


void SonicParse_Expression_Sinewave::generateCode ( 
    ostream &o, 
    Sonic_CodeGenContext &x )
{
    if ( x.generatingComment )
    {
        o << "sinewave(";
        amplitude->generateCode ( o, x );
        o << ",";
        frequencyHz->generateCode ( o, x );
        o << ",";
        phaseDeg->generateCode ( o, x );
        o << ")";
    }
    else
    {
        if ( !x.iAllowed )
            throw SonicParseException ( "sinewave construct not allowed here", sinewaveToken );

        o << TEMPORARY_PREFIX << tempTag[x.channelValue] << "[2]";
    }
}


/*--- end of file codegen.cpp ---*/