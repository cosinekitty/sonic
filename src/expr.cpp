/*========================================================================

    expr.cpp  -  Copyright (C) 1998 by Don Cross <cosinekitty@gmail.com>

    This module is responsible for parsing Sonic expressions.
    These expressions appear in many parts of the Sonic grammar,
    including assignment statements and while/if conditions.

Revision history:

1998 September 28 [Don Cross]
    Adding support for arrays.

1998 September 23 [Don Cross]
    Added parse code for new 'fft' pseudo-function.

1998 September 16 [Don Cross]
    Made SonicType::operator== be a const function.

========================================================================*/
#include <iostream.h>
#include <stdio.h>
#include <string.h>

#include "scan.h"
#include "parse.h"


SonicParse_Expression::~SonicParse_Expression()
{
    if ( next )
    {
        delete next;
        next = 0;
    }
}


void SonicType::initDimArray()
{
    for ( int i=0; i < MAX_SONIC_ARRAY_DIMENSIONS; ++i )
        arrayDim[i] = 0;
}


void SonicType::copyDimArray ( const int _dimArray[] )
{
    if ( numDimensions < 0 || numDimensions > MAX_SONIC_ARRAY_DIMENSIONS )
        throw SonicParseException ( "Invalid number of array dimensions" );

    for ( int i=0; i < numDimensions; ++i )
        arrayDim[i] = _dimArray[i];

    for ( ; i < MAX_SONIC_ARRAY_DIMENSIONS; ++i )
        arrayDim[i] = 0;
}


bool SonicType::operator== ( const SonicType &other ) const
{
    if ( tclass != other.tclass )
        return false;

    if ( tclass == STYPE_IMPORT )
        return *name == *other.name;

    if ( tclass == STYPE_ARRAY )
    {
        if ( arrayElementClass != other.arrayElementClass )
            return false;

        if ( numDimensions != other.numDimensions )
            return false;

        for ( int i=0; i < numDimensions; ++i )
        {
            if ( arrayDim[i] != other.arrayDim[i] )
                return false;
        }
    }

    return true;
}


bool CanConvertTo ( SonicType source, SonicType target )
{
    if ( target == STYPE_VOID || target == STYPE_UNDEFINED )
        return false;

    if ( source == STYPE_VOID || source == STYPE_UNDEFINED )
        return false;

    if ( target == STYPE_REAL || target == STYPE_INTEGER )
        return source == STYPE_REAL || source == STYPE_INTEGER;

    if ( target == STYPE_WAVE )
        return source == STYPE_WAVE || source == STYPE_STRING;

    if ( target == STYPE_VECTOR )
        return source == STYPE_VECTOR || source == STYPE_REAL || source == STYPE_INTEGER;

    if ( target == STYPE_ARRAY )
    {
        if ( source != STYPE_ARRAY )
            return false;

        const int numDimensions = target.queryNumDimensions();
        if ( numDimensions != source.queryNumDimensions() )
            return false;

        if ( source.queryElementType() != target.queryElementType() )
            return false;

        const int *sdim = source.queryDimensionArray();
        const int *tdim = target.queryDimensionArray();

        // In conversion checking, the first array dimensions do not need to match,
        // but all remaining dimensions do.  This is the only requirement for
        // generating valid C++ for passing parameters to functions.  Calling code
        // needs to be aware of this special case.

        for ( int i=1; i < numDimensions; ++i )
        {
            if ( sdim[i] != tdim[i] )
                return false;
        }

        return true;
    }

    if ( target == source )
        return true;

    return false;
}


//------------------------------------------------------------------------------------


class Sonic_ExpressionVisitor_ChannelDependent: public Sonic_ExpressionVisitor
{
public:
    Sonic_ExpressionVisitor_ChannelDependent():
        numChannelDependencies ( 0 )
        {}

    virtual void visitHook ( const SonicParse_Expression *ep )
    {
        SonicExpressionType etype = ep->queryExpressionType();

        if ( etype == ETYPE_BUILTIN )
        {
            if ( ep->getFirstToken() == "c" )
                ++numChannelDependencies;
        }
        else if ( etype == ETYPE_OLD_DATA || etype == ETYPE_IIR )
        {
            ++numChannelDependencies;
        }
    }

    int queryNumChannelDependencies() const { return numChannelDependencies; }

private:
    int numChannelDependencies;
};


bool SonicParse_Expression::isChannelDependent() const
{
    Sonic_ExpressionVisitor_ChannelDependent  visitor;
    visit ( visitor );
    return visitor.queryNumChannelDependencies() > 0;
}


//------------------------------------------------------------------------------------


bool SonicParse_Expression::canConvertTo ( SonicType target ) const
{
    return CanConvertTo ( determineType(), target );
}


void SonicParse_Expression::VisitList ( 
    Sonic_ExpressionVisitor &v, 
    SonicParse_Expression *list )
{
    for ( SonicParse_Expression *ep = list; ep; ep = ep->next )
        ep->visit(v);
}


SonicParse_Expression *SonicParse_Expression::Parse ( 
    SonicScanner &scanner, 
    SonicParseContext &px )
{
    SonicParse_Expression *expr = 0;

    SonicToken t;
    scanner.getToken ( t );
    if ( t == "{" )
    {
        SonicToken lbrace = t;
        SonicParse_Expression *vectorExprList=0, *vectorExprTail=0;
        for(;;)
        {
            SonicParse_Expression *newVectorExpr = Parse_b0 (scanner, px);
            if ( vectorExprTail )
                vectorExprTail = vectorExprTail->next = newVectorExpr;
            else
                vectorExprTail = vectorExprList = newVectorExpr;

            scanner.getToken ( t );
            if ( t == "}" )
                break;
            else if ( t != "," )
                throw SonicParseException ( "expected '}' or ',' after expression", t );
        }

        expr = new SonicParse_Expression_Vector ( lbrace, vectorExprList );
    }
    else
    {
        scanner.pushToken (t);
        expr = Parse_b0 (scanner, px);
    }

    if ( !expr )
        throw SonicParseException ( "internal error: failed to parse expression", t );

    return expr;
}


SonicParse_Expression *SonicParse_Expression::Parse_b0 ( 
    SonicScanner &scanner, 
    SonicParseContext &px )
{
    SonicParse_Expression *expr = Parse_b1 (scanner, px);
    SonicToken t;
    scanner.getToken ( t );
    while ( t == "|" )
    {
        SonicParse_Expression *lchild = expr;
        SonicParse_Expression *rchild = Parse_b1 (scanner, px);
        expr = new SonicParse_Expression_Or ( t, lchild, rchild );
        scanner.getToken ( t );
    }

    scanner.pushToken ( t );
    return expr;
}


SonicParse_Expression *SonicParse_Expression::Parse_b1 ( 
    SonicScanner &scanner, 
    SonicParseContext &px )
{
    SonicParse_Expression *expr = Parse_b2 (scanner, px);
    SonicToken t;
    scanner.getToken ( t );
    while ( t == "&" )
    {
        SonicParse_Expression *lchild = expr;
        SonicParse_Expression *rchild = Parse_b2 (scanner, px);
        expr = new SonicParse_Expression_And ( t, lchild, rchild );
        scanner.getToken ( t );
    }

    scanner.pushToken ( t );
    return expr;
}


SonicParse_Expression *SonicParse_Expression::Parse_b2 ( 
    SonicScanner &scanner, 
    SonicParseContext &px )
{
    SonicParse_Expression *expr = Parse_term (scanner, px);
    SonicToken t;
    scanner.getToken ( t );
    if ( t=="==" || t=="<" || t=="<=" || t==">" || t==">=" || t=="!=" || t=="<>" )
    {
        SonicParse_Expression *lchild = expr;
        SonicParse_Expression *rchild = Parse_term (scanner, px);

        if ( t == "==" )
            expr = new SonicParse_Expression_Equal ( t, lchild, rchild );
        else if ( t == "<" )
            expr = new SonicParse_Expression_Less ( t, lchild, rchild );
        else if ( t == "<=" )
            expr = new SonicParse_Expression_LessEqual ( t, lchild, rchild );
        else if ( t == ">" )
            expr = new SonicParse_Expression_Greater ( t, lchild, rchild );
        else if ( t == ">=" )
            expr = new SonicParse_Expression_GreaterEqual ( t, lchild, rchild );
        else if ( t == "!=" || t == "<>" )
            expr = new SonicParse_Expression_NotEqual ( t, lchild, rchild );
        else
            throw SonicParseException ( "internal error:  unhandled relational operator", t );
    }
    else
        scanner.pushToken ( t );

    return expr;
}


SonicParse_Expression *SonicParse_Expression::Parse_term ( 
    SonicScanner &scanner, 
    SonicParseContext &px )
{
    SonicParse_Expression *expr = Parse_t1 (scanner, px);
    SonicToken t;
    for(;;)
    {
        scanner.getToken ( t );
        if ( t=="+" || t=="-" )
        {
            SonicParse_Expression *lchild = expr;
            SonicParse_Expression *rchild = Parse_t1 (scanner, px);
            if ( t == "+" )
                expr = new SonicParse_Expression_Add ( t, lchild, rchild );
            else
                expr = new SonicParse_Expression_Subtract ( t, lchild, rchild );
        }
        else
        {
            scanner.pushToken ( t );
            break;
        }
    }

    return expr;
}


SonicParse_Expression *SonicParse_Expression::Parse_t1 ( 
    SonicScanner &scanner, 
    SonicParseContext &px )
{
    SonicParse_Expression *expr = Parse_t2 (scanner, px);
    SonicToken t;
    for(;;)
    {
        scanner.getToken ( t );
        if ( t=="*" || t=="/" || t=="%" )
        {
            SonicParse_Expression *lchild = expr;
            SonicParse_Expression *rchild = Parse_t2 (scanner, px);
            if ( t == "*" )
                expr = new SonicParse_Expression_Multiply ( t, lchild, rchild );
            else if ( t == "/" )
                expr = new SonicParse_Expression_Divide ( t, lchild, rchild );
            else
                expr = new SonicParse_Expression_Mod ( t, lchild, rchild );
        }
        else
        {
            scanner.pushToken ( t );
            break;
        }
    }

    return expr;
}


SonicParse_Expression *SonicParse_Expression::Parse_t2 ( 
    SonicScanner &scanner, 
    SonicParseContext &px )
{
    SonicParse_Expression *expr = Parse_t3 (scanner, px);
    SonicToken t;
    scanner.getToken ( t );
    if ( t == "^" )
    {
        SonicParse_Expression *lchild = expr;
        SonicParse_Expression *rchild = Parse_t2 (scanner, px);
        expr = new SonicParse_Expression_Power ( t, lchild, rchild );
    }
    else
        scanner.pushToken ( t );

    return expr;
}


static Sonic_IntrinsicTableEntry IntrinsicTable[] =
{
    // trig-related...
    {"sin",     "sin",      1},
    {"sinh",    "sinh",     1},
    {"cos",     "cos",      1},
    {"cosh",    "cosh",     1},
    {"tan",     "tan",      1},
    {"tanh",    "tanh",     1},
    {"acos",    "acos",     1},
    {"asin",    "asin",     1},
    {"atan",    "atan",     1},
    {"atan2",   "atan2",    2},

    // misc...
    {"abs",     "fabs",     1},
    {"ceil",    "ceil",     1},
    {"floor",   "floor",    1},
    {"sqrt",    "sqrt",     1},
    {"hypot",   "_hypot",   2},
    {"square",  "Sonic_Square",     1},
    {"cube",    "Sonic_Cube",       1},
    {"quart",   "Sonic_Quart",      1},
    {"recip",   "Sonic_Recip",      1},
    {"noise",   "Sonic_Noise",      1},

    // logarithmic/exponential...
    {"ln",      "log",      1},
    {"log",     "log10",    1},
    {"exp",     "exp",      1},
    {"dB",      "Sonic_dB", 1},

    {0, 0, 0}       // marks end of table
};


const Sonic_IntrinsicTableEntry *FindIntrinsic ( const char *sname )
{
    for ( int i=0; IntrinsicTable[i].sname; ++i )
        if ( strcmp(sname,IntrinsicTable[i].sname) == 0 )
            return &IntrinsicTable[i];

    return 0;
}



static bool SearchIntrinsicTable ( 
    SonicToken &sonicName, 
    SonicToken &funcName, 
    int numParms )
{
    const Sonic_IntrinsicTableEntry *te = FindIntrinsic ( sonicName.queryToken() );
    if ( te )
    {
        if ( numParms != te->numParms )
        {
            throw SonicParseException ( 
                "wrong number of parameters to intrinsic function", 
                sonicName );
        }

        // Make the 'funcName' token be identical to 'sonicName', except
        // change the string itself to be the actual C++ function name.

        funcName.define ( 
            te->cname, 
            sonicName.queryLine(), 
            sonicName.queryColumn(),
            sonicName.queryTokenType() );

        return true;
    }

    funcName = sonicName;
    return false;
}



SonicParse_Expression *SonicParse_Expression::Parse_t3 ( 
    SonicScanner &scanner, 
    SonicParseContext &px )
{
    SonicParse_Expression *expr = 0;

    SonicToken t;
    scanner.getToken ( t );
    if ( t.queryTokenType() == STT_CONSTANT )
    {
        SonicType type = STYPE_INTEGER;
        if ( strchr(t.queryToken(),'.') || 
             strchr(t.queryToken(),'e') || 
             strchr(t.queryToken(),'E') )
            type = STYPE_REAL;

        expr = new SonicParse_Expression_Constant ( t, type );
    }
    else if ( t.queryTokenType() == STT_STRING )
    {
        expr = new SonicParse_Expression_Constant ( t, STYPE_STRING );
    }
    else if ( t.queryTokenType() == STT_BUILTIN )
    {
        expr = new SonicParse_Expression_Builtin ( t );
    }
    else if ( t.queryTokenType() == STT_IDENTIFIER )
    {
        SonicToken t2;
        scanner.getToken ( t2 );
        if ( t2 == "[" )
        {
            // Figure out whether this is an array expression or a wave expression.
            const SonicParse_VarDecl *decl = px.findVar(t);
            const SonicType &varType = decl->queryType();
            if ( varType == STYPE_ARRAY )
            {
                SonicParse_Expression *indexList=0, *indexTail=0;
                SonicToken punct;
                for(;;)
                {
                    SonicParse_Expression *newIndex = Parse_term (scanner, px);
                    if ( indexList )
                        indexTail = indexTail->next = newIndex;
                    else
                        indexList = indexTail = newIndex;
                    scanner.getToken ( punct );
                    if ( punct == "]" )
                        break;
                    else if ( punct != "," )
                        throw SonicParseException ( "expected ',' or ']'" );
                }

                expr = new SonicParse_Expression_ArraySubscript ( t, indexList );
            }
            else if ( varType == STYPE_WAVE )
            {
                SonicParse_Expression *cterm = Parse_term (scanner, px);
                scanner.scanExpected ( "," );
                SonicParse_Expression *iterm = Parse_term (scanner, px);
                scanner.scanExpected ( "]" );
                expr = new SonicParse_Expression_WaveExpr ( t, cterm, iterm );
            }
            else
                throw SonicParseException ( "'[' may appear only after array or wave variable", t2 );
        }
        else if ( t2 == "." )
        {
            scanner.getToken ( t2 );
            if ( t2 == "n" || t2 == "m" || t2 == "r" || t2 == "max" )
                expr = new SonicParse_Expression_WaveField ( t, t2 );
            else
                throw SonicParseException ( "expected wave field after '.'", t2 );
        }
        else if ( t2 == "(" )
        {
            if ( t == "sinewave" )
            {
                // scan 3 terms: amplitude, frequency, phase
                SonicParse_Expression *amplitude = Parse_term (scanner, px);
                scanner.scanExpected ( "," );
                SonicParse_Expression *frequency = Parse_term (scanner, px);
                scanner.scanExpected ( "," );
                SonicParse_Expression *phase = Parse_term (scanner, px);
                scanner.scanExpected ( ")" );
                expr = new SonicParse_Expression_Sinewave ( t, amplitude, frequency, phase );
            }
            else if ( t == "sawtooth" )
            {
                SonicParse_Expression *frequency = Parse_term (scanner, px);
                scanner.scanExpected ( ")" );
                expr = new SonicParse_Expression_Sawtooth ( t, frequency );
            }
            else if ( t == "fft" )
            {
                SonicParse_Expression *input = Parse_term (scanner, px);
                scanner.scanExpected ( "," );
                SonicParse_Expression *fftSize = Parse_term (scanner, px);
                scanner.scanExpected ( "," );
                SonicToken funcName;

                scanner.getToken ( funcName );
                if ( funcName.queryTokenType() != STT_IDENTIFIER )
                    throw SonicParseException ( "third parameter to 'fft' must be transfer function name", funcName );

                scanner.scanExpected ( "," );
                SonicParse_Expression *freqShift = Parse_term (scanner, px);
                scanner.scanExpected ( ")" );

                expr = new SonicParse_Expression_FFT (
                    t,
                    input,
                    fftSize,
                    freqShift,
                    funcName );
            }
            else if ( t == "iir" )
            {
                SonicToken tp;

                scanner.scanExpected ( "{" );
                SonicParse_Expression *xCoeffList=0, *xCoeffTail=0;
                for(;;)
                {
                    SonicParse_Expression *xCoeff = SonicParse_Expression::Parse_term (scanner, px);
                    if ( xCoeffTail )
                        xCoeffTail = xCoeffTail->next = xCoeff;
                    else
                        xCoeffTail = xCoeffList = xCoeff;

                    scanner.getToken ( tp );
                    if ( tp == "}" )
                        break;
                    else if ( tp != "," )
                        throw SonicParseException ( "expected ',' or '}' after x-coeff expression", tp );
                }

                scanner.scanExpected ( "," );
                scanner.scanExpected ( "{" );
    
                SonicParse_Expression *yCoeffList=0, *yCoeffTail=0;
                scanner.getToken ( tp );
                if ( tp != "}" )
                {
                    scanner.pushToken ( tp );
                    for(;;)
                    {
                        SonicParse_Expression *yCoeff = SonicParse_Expression::Parse_term (scanner, px);
                        if ( yCoeffTail )
                            yCoeffTail = yCoeffTail->next = yCoeff;
                        else
                            yCoeffTail = yCoeffList = yCoeff;

                        scanner.getToken ( tp );
                        if ( tp == "}" )
                            break;
                        else if ( tp != "," )
                            throw SonicParseException ( "expected ',' or '}' after y-coeff expression", tp );                           
                    }
                }

                scanner.scanExpected ( "," );
                SonicParse_Expression *iirInput = SonicParse_Expression::Parse_term (scanner, px);
                scanner.scanExpected ( ")" );

                expr = new SonicParse_Expression_IIR (
                    t,
                    xCoeffList,
                    yCoeffList,
                    iirInput );
            }
            else    // it must be a normal function call
            {
                int numParms = 0;
                SonicParse_Expression *parmList=0, *parmTail=0;
                scanner.getToken ( t2 );
                while ( t2 != ")" )
                {
                    scanner.pushToken ( t2 );
                    SonicParse_Expression *newParm = Parse (scanner, px);
                    if ( parmTail )
                        parmTail = parmTail->next = newParm;
                    else
                        parmTail = parmList = newParm;

                    ++numParms;

                    scanner.getToken ( t2 );
                    if ( t2 != ")" )
                    {
                        if ( t2 != "," )
                            throw SonicParseException ( "expected ',' or ')'", t2 );

                        scanner.getToken ( t2 );
                    }
                }

                SonicToken funcName;
                bool intrinsic = SearchIntrinsicTable ( t, funcName, numParms );
                SonicFunctionType ftype = intrinsic ? SFT_INTRINSIC : SFT_USER;
                expr = new SonicParse_Expression_FunctionCall ( funcName, parmList, ftype );
            }
        }
        else
        {
            scanner.pushToken ( t2 );
            expr = new SonicParse_Expression_Variable ( t );
        }
    }
    else if ( t == "(" )
    {
        expr = Parse ( scanner, px );
        scanner.scanExpected ( ")" );
    }
    else if ( t == "!" )
    {
        SonicParse_Expression *child = Parse_t3 (scanner, px);
        expr = new SonicParse_Expression_Not ( t, child );
    }
    else if ( t == "-" )
    {
        SonicParse_Expression *child = Parse_t3 (scanner, px);
        expr = new SonicParse_Expression_Negate ( t, child );
    }
    else if ( t == "$" )
    {
        expr = new SonicParse_Expression_OldData (t);
    }
    else
        throw SonicParseException ( "error in expression", t );

    if ( !expr )
        throw SonicParseException ( "internal error <t3>: cannot parse expression", t );

    return expr;
}


//----------------------------------------------------------------------------


void SonicParse_Expression_FunctionCall::getWaveSymbolList ( 
    const SonicToken *waveSymbol[], 
    int maxWaveSymbols,
    int &numSoFar,
    int &numOccurrences )
{
    for ( SonicParse_Expression *pp = parmList; pp; pp = pp->queryNext() )
        pp->getWaveSymbolList ( waveSymbol, maxWaveSymbols, numSoFar, numOccurrences );
}


//-----------------------------------------------------------------------------


SonicParse_Expression_Vector::~SonicParse_Expression_Vector()
{
    if ( exprList )
    {
        delete exprList;
        exprList = 0;
    }
}


void SonicParse_Expression_Vector::getWaveSymbolList ( 
    const SonicToken *waveSymbol[], 
    int maxWaveSymbols,
    int &numSoFar,
    int &numOccurrences )
{
    for ( SonicParse_Expression *ep = exprList; ep; ep = ep->queryNext() )
        ep->getWaveSymbolList ( waveSymbol, maxWaveSymbols, numSoFar, numOccurrences );
}


//----------------------------------------------------------------------------


SonicParse_Expression_BinaryOp::~SonicParse_Expression_BinaryOp()
{
    if ( lchild )
    {
        delete lchild;
        lchild = 0;
    }

    if ( rchild )
    {
        delete rchild;
        rchild = 0;
    }
}


void SonicParse_Expression_BinaryOp::getWaveSymbolList ( 
    const SonicToken *waveSymbol[], 
    int maxWaveSymbols,
    int &numSoFar,
    int &numOccurrences )
{
    lchild->getWaveSymbolList ( waveSymbol, maxWaveSymbols, numSoFar, numOccurrences );
    rchild->getWaveSymbolList ( waveSymbol, maxWaveSymbols, numSoFar, numOccurrences );
}

//----------------------------------------------------------------------------


SonicType SonicParse_Expression_BinaryMathOp::determineType() const
{
    // The type of a binary math operator is determined by the type of its operands.

    SonicType ltype = lchild->determineType();
    SonicType rtype = rchild->determineType();

    if ( ltype != STYPE_REAL && ltype != STYPE_INTEGER )
        throw SonicParseException ( "left operand has invalid type", op );

    if ( rtype != STYPE_REAL && rtype != STYPE_INTEGER )
        throw SonicParseException ( "right operand has invalid type", op );

    if ( ltype == STYPE_REAL || rtype == STYPE_REAL )
        return STYPE_REAL;

    return STYPE_INTEGER;
}


//----------------------------------------------------------------------------


SonicParse_Expression_WaveExpr::~SonicParse_Expression_WaveExpr()
{
    if ( cterm )
    {
        delete cterm;
        cterm = 0;
    }

    if ( iterm )
    {
        delete iterm;
        iterm = 0;
    }
}


void Append ( 
    const SonicToken *waveSymbol[],
    int maxWaveSymbols,
    int &numSoFar,
    const SonicToken &token )
{
    for ( int i=0; i < numSoFar; ++i )
    {
        if ( token == *waveSymbol[i] )
            return;   // already in the list
    }

    if ( numSoFar >= maxWaveSymbols )
        throw SonicParseException ( "internal error:  wave symbol table overflow!", token );

    waveSymbol [numSoFar++] = &token;
}


void SonicParse_Expression_WaveExpr::getWaveSymbolList ( 
    const SonicToken *waveSymbol[], 
    int maxWaveSymbols,
    int &numSoFar,
    int &numOccurrences )
{
    Append ( waveSymbol, maxWaveSymbols, numSoFar, waveName );
    ++numOccurrences;
}


void SonicParse_Expression_OldData::getWaveSymbolList (
    const SonicToken *waveSymbol[],
    int maxWaveSymbols,
    int &numSoFar,
    int &numOccurrences )
{
    Append ( waveSymbol, maxWaveSymbols, numSoFar, dollarSign );
}


void SonicParse_Expression_WaveField::getWaveSymbolList ( 
    const SonicToken *waveSymbol[], 
    int maxWaveSymbols,
    int &numSoFar,
    int &numOccurrences )
{
    Append ( waveSymbol, maxWaveSymbols, numSoFar, varName );
}

//----------------------------------------------------------------------------


SonicType SonicParse_Expression_Builtin::determineType() const
{
    if ( name=="true" || name=="false" || name=="interpolate" )
        return STYPE_BOOLEAN;
    else if ( name=="pi" || name=="e" || name=="t" )
        return STYPE_REAL;
    else if ( name=="i" || name=="c" || name=="r" || name=="n" || name=="m" )
        return STYPE_INTEGER;

    throw SonicParseException ( "internal error: cannot determine built-in type", name );

    return STYPE_UNDEFINED;
}


//----------------------------------------------------------------------------


SonicParse_Expression_UnaryOp::~SonicParse_Expression_UnaryOp()
{
}


void SonicParse_Expression_UnaryOp::getWaveSymbolList ( 
    const SonicToken *waveSymbol[], 
    int maxWaveSymbols,
    int &numSoFar,
    int &numOccurrences )
{
    child->getWaveSymbolList ( waveSymbol, maxWaveSymbols, numSoFar, numOccurrences );
}


//------------------------------------------------------------------------------


SonicParse_Expression_ArraySubscript::~SonicParse_Expression_ArraySubscript()
{
    if ( indexList )
    {
        delete indexList;
        indexList = 0;
    }
}


void SonicParse_Expression_ArraySubscript::getWaveSymbolList (
    const SonicToken *waveSymbol[], 
    int maxWaveSymbols,
    int &numSoFar,
    int &numOccurrences )
{
    for ( SonicParse_Expression *ip = indexList; ip; ip = ip->queryNext() )
        ip->getWaveSymbolList ( waveSymbol, maxWaveSymbols, numSoFar, numOccurrences );
}
    

//------------------------------------------------------------------------------


void SonicParse_Expression_IIR::getWaveSymbolList ( 
    const SonicToken *waveSymbol[], 
    int maxWaveSymbols,
    int &numSoFar,
    int &numOccurrences )
{
    filterInput->getWaveSymbolList ( waveSymbol, maxWaveSymbols, numSoFar, numOccurrences );
}


//------------------------------------------------------------------------------


void SonicParse_Expression_FFT::getWaveSymbolList (
    const SonicToken *waveSymbol[],
    int maxWaveSymbols,
    int &numSoFar,
    int &numOccurrences )
{
    input->getWaveSymbolList ( waveSymbol, maxWaveSymbols, numSoFar, numOccurrences );
    fftSize->getWaveSymbolList ( waveSymbol, maxWaveSymbols, numSoFar, numOccurrences );
}


/*--- end of file expr.cpp ---*/
