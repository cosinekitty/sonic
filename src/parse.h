/*============================================================================

    parse.h  -  Copyright (C) 1998 by Don Cross <cosinekitty@gmail.com>

    Main header file for Sonic parser/translator.

    Revision history:

1998 September 23 [Don Cross]
    Added support for passing function arguments by reference.
    Adding 'fft' pseudo-function.

1998 September 21 [Don Cross]
    Added support for global variables.

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

1998 September 10 [Don Cross]
    Modified code generator so that '.reset()' call for import function
    objects is called only once per assignment, and only called at all
    when the assignment is a wave assignment.

1998 September 9 [Don Cross]
    Added crude support for "import functions".  This means user is
    allowed to specify imported C++ classes which are used as functions
    by overloading 'operator()'.
    All import functions are now assumed to return type 'real', and
    are allowed to have any number of parameters of any type.
    Therefore, errors will not be detected until generated C++ code
    is compiled.

1998 September 3 [Don Cross]
    Finally getting stable!

============================================================================*/
#ifndef __ddc_sonic_parse_h
#define __ddc_sonic_parse_h

const char * const SONIC_VERSION = "0.903 (beta)";
const char * const SONIC_RELEASE_DATE = "26 September 1998";

class ostream;

const int MAX_SONIC_CHANNELS = 64;   // should be big enough for a while!

class SonicToken;
class SonicParse_Expression;
class SonicParse_Function;
class SonicParse_Lvalue;
class SonicParse_VarDecl;
struct Sonic_CodeGenContext;

class SonicParse_Program
{
public:
    SonicParse_Program();
    ~SonicParse_Program();

    static const SonicToken & GetCurrentProgramName();

    void parse ( SonicScanner & );
    void generateCode();

    SonicParse_Function *queryProgramBody() const { return programBody; }
    SonicParse_Function *queryFunctionList() const { return functionBodyList; }
    SonicParse_Function *queryImportList() const { return importList; }

    SonicParse_VarDecl  *findSymbol ( 
        const SonicToken &name, 
        SonicParse_Function *encloser,
        bool forceFind ) const;

    SonicParse_VarDecl  *findGlobalVar ( const SonicToken &name ) const;
    SonicParse_Function *findFunction ( const SonicToken &funcName ) const;
    SonicParse_Function *findImportType ( const SonicToken &importName ) const;

    SonicParse_Function *findImportVar ( 
        const SonicToken &varname, 
        SonicParse_Function *enclosingFunc ) const;

    int countInstances ( const SonicToken &globalVarName ) const;
    long querySamplingRate() const { return samplingRate; }
    int  queryNumChannels() const { return numChannels; }
    bool queryInterpolateFlag() const { return interpolateFlag; }

    void clearAllResetFlags();

private:
    void genImportIncludes ( ostream & );
    void genFunctionPrototypes ( ostream &, Sonic_CodeGenContext & );
    void genGlobalVariables ( ostream &, Sonic_CodeGenContext & );
    void genMain ( ostream &, Sonic_CodeGenContext & );
    void genProgramFunction ( ostream &, Sonic_CodeGenContext & );
    void genFunctions ( ostream &, Sonic_CodeGenContext & );
    void validate();
    static void SaveCurrentProgramName ( const SonicToken &name );

private:
    long samplingRate;
    bool samplingRate_explicit;

    int  numChannels;
    bool numChannels_explicit;

    bool interpolateFlag;
    bool interpolateFlag_explicit;

    SonicParse_Function *programBody;
    SonicParse_Function *functionBodyList;
    SonicParse_Function *importList;
    SonicParse_VarDecl  *globalVars;

    static SonicToken CurrentProgramName;
};


enum SonicTypeClass
{
    STYPE_UNDEFINED,
    STYPE_VOID,     // used only for functions which do not return a value
    STYPE_INTEGER,
    STYPE_REAL,
    STYPE_BOOLEAN,
    STYPE_WAVE,
    STYPE_STRING,
    STYPE_VECTOR,
    STYPE_IMPORT    // class written in C++ and imported into Sonic program
};


class SonicType
{
public:
    SonicType ( SonicTypeClass _tclass ):
        tclass ( _tclass ),
        name ( 0 ),
        referenceFlag ( false )
        {}

    SonicType ( const SonicToken *importName ):
        tclass ( STYPE_IMPORT ),
        name ( importName ),
        referenceFlag ( false )
        {}

    bool operator== ( const SonicType & ) const;
    bool operator!= ( const SonicType &other ) const { return !(*this == other); }
    bool operator== ( SonicTypeClass x ) const { return tclass == x; }
    bool operator!= ( SonicTypeClass x ) const { return tclass != x; }

    void setReferenceFlag ( bool _newValue )  { referenceFlag = _newValue; }
    bool isReference() const { return referenceFlag; }
    SonicTypeClass queryTypeClass() const { return tclass; }
    const SonicToken *queryImportName() const { return name; }

private:
    SonicTypeClass  tclass;
    const SonicToken *name;     // used for STYPE_IMPORT
    bool referenceFlag;
};


enum SonicExpressionType
{
    ETYPE_UNDEFINED,
    ETYPE_CONSTANT,         // a numeric constant; either integer or real
    ETYPE_VARIABLE,         // simple variable name
    ETYPE_BUILTIN,
    ETYPE_VECTOR,           // { expr, ... , expr }
    ETYPE_WAVE_EXPR,        // name [ cexpr, iexpr ]
    ETYPE_WAVE_FIELD,       // name.field
    ETYPE_FUNCTION_CALL,
    ETYPE_BINARY_OP,
    ETYPE_UNARY_OP,
    ETYPE_SINEWAVE,
    ETYPE_SAWTOOTH,
    ETYPE_FFT,
    ETYPE_IIR,
    ETYPE_OLD_DATA          // '$' - represents previous value of lvalue[c,i]
};


enum SonicStatementType
{
    STMT_UNDEFINED,
    STMT_ASSIGNMENT,
    STMT_FUNCCALL,
    STMT_IF,
    STMT_WHILE,
    STMT_REPEAT,
    STMT_COMPOUND,
    STMT_RETURN
};


SonicType ParseType ( SonicScanner &, const SonicParse_Program & );
bool CanConvertTo ( SonicType source, SonicType target );


class Sonic_ExpressionVisitor
{
public:
    virtual void visitHook ( const SonicParse_Expression * ) = 0;
};


class SonicParse_Expression
{
public:
    SonicParse_Expression ( SonicExpressionType _exprType ):
        next ( 0 ),
        exprType ( _exprType )
        {}

    virtual ~SonicParse_Expression();

    static SonicParse_Expression *Parse ( SonicScanner & );
    static SonicParse_Expression *Parse_b0 ( SonicScanner & );
    static SonicParse_Expression *Parse_term ( SonicScanner & );
    static SonicParse_Expression *Parse_t3 ( SonicScanner & );

    virtual void visit ( Sonic_ExpressionVisitor &v ) const  { v.visitHook(this); }
    static void VisitList ( Sonic_ExpressionVisitor &v, SonicParse_Expression *list );

    bool isChannelDependent() const;

    bool canConvertTo ( SonicType ) const;
    virtual SonicType determineType() const = 0;
    virtual int operatorPrecedence() const = 0;
    virtual void validate ( SonicParse_Program &, SonicParse_Function * ) = 0;

    SonicExpressionType queryExpressionType() const { return exprType; }

    static void ValidateList ( 
        SonicParse_Expression *exprList,
        SonicParse_Program &program, 
        SonicParse_Function *func );

    virtual const SonicToken & getFirstToken() const = 0;

    SonicParse_Expression *queryNext() const { return next; }

    virtual void generateCode ( ostream &, Sonic_CodeGenContext & ) = 0;
    virtual void generatePreSampleLoopCode  ( ostream &, Sonic_CodeGenContext & ) = 0;
    virtual void generatePreChannelLoopCode ( ostream &, Sonic_CodeGenContext & ) = 0;

    virtual void getWaveSymbolList ( 
        const SonicToken *waveSymbol[], 
        int maxWaveSymbols,
        int &numSoFar,
        int &numOccurrences )
        {}

protected:
    static SonicParse_Expression *Parse_b1 ( SonicScanner & );
    static SonicParse_Expression *Parse_b2 ( SonicScanner & );
    static SonicParse_Expression *Parse_t1 ( SonicScanner & );
    static SonicParse_Expression *Parse_t2 ( SonicScanner & );

protected:
    friend class SonicParse_VarDecl;
    SonicParse_Expression *next;
    SonicExpressionType exprType;
};


class SonicParse_Expression_Vector: public SonicParse_Expression    // { expr, ... , expr }
{
public:
    SonicParse_Expression_Vector ( 
        const SonicToken &_lbrace, 
        SonicParse_Expression *_exprList ):
            SonicParse_Expression ( ETYPE_VECTOR ),
            lbrace ( _lbrace ),
            exprList ( _exprList )
            {}

    virtual ~SonicParse_Expression_Vector();

    virtual SonicType determineType() const { return STYPE_VECTOR; }
    int queryNumChannels() const;
    virtual int operatorPrecedence() const { return 100; }
    virtual void validate ( SonicParse_Program &, SonicParse_Function * );
    virtual const SonicToken & getFirstToken() const { return lbrace; }
    virtual void generateCode ( ostream &, Sonic_CodeGenContext & );
    virtual void generatePreSampleLoopCode  ( ostream &, Sonic_CodeGenContext & );
    virtual void generatePreChannelLoopCode ( ostream &, Sonic_CodeGenContext & );
    SonicParse_Expression *getComponentList() const { return exprList; }

    virtual void getWaveSymbolList ( 
        const SonicToken *waveSymbol[], 
        int maxWaveSymbols,
        int &numSoFar,
        int &numOccurrences );

    virtual void visit ( Sonic_ExpressionVisitor &v ) const
    {
        v.visitHook (this);
        VisitList ( v, exprList );
    }

private:
    SonicToken lbrace;
    SonicParse_Expression *exprList;
};


class SonicParse_Expression_WaveExpr: public SonicParse_Expression
{
public:
    SonicParse_Expression_WaveExpr ( 
        const SonicToken &_waveName,
        SonicParse_Expression *_cterm,
        SonicParse_Expression *_iterm ):
            SonicParse_Expression ( ETYPE_WAVE_EXPR ),
            waveName ( _waveName ),
            cterm ( _cterm ),
            iterm ( _iterm )
            {}

    virtual ~SonicParse_Expression_WaveExpr();

    virtual int operatorPrecedence() const { return 100; }
    virtual SonicType determineType() const { return STYPE_REAL; }
    virtual void validate ( SonicParse_Program &, SonicParse_Function * );
    virtual const SonicToken & getFirstToken() const { return waveName; }
    virtual void generateCode ( ostream &, Sonic_CodeGenContext & );
    virtual void generatePreSampleLoopCode  ( ostream &, Sonic_CodeGenContext & );
    virtual void generatePreChannelLoopCode ( ostream &, Sonic_CodeGenContext & );

    virtual void getWaveSymbolList ( 
        const SonicToken *waveSymbol[], 
        int maxWaveSymbols,
        int &numSoFar,
        int &numOccurrences );

    virtual void visit ( Sonic_ExpressionVisitor &v ) const
    {
        v.visitHook (this);
        cterm->visit (v);
        iterm->visit (v);
    }

private:
    SonicToken waveName;
    SonicParse_Expression *cterm;
    SonicParse_Expression *iterm;
};


class SonicParse_Expression_OldData: public SonicParse_Expression
{
public:
    SonicParse_Expression_OldData ( const SonicToken &_dollarSign ):
        SonicParse_Expression ( ETYPE_OLD_DATA ),
        dollarSign ( _dollarSign )
        {}
    
    virtual int operatorPrecedence() const { return 100; }
    virtual SonicType determineType() const { return STYPE_REAL; }
    virtual void validate ( SonicParse_Program &, SonicParse_Function * ) {}
    virtual const SonicToken & getFirstToken() const { return dollarSign; }
    virtual void generateCode ( ostream &, Sonic_CodeGenContext & );
    virtual void generatePreSampleLoopCode  ( ostream &, Sonic_CodeGenContext & ) {}
    virtual void generatePreChannelLoopCode ( ostream &, Sonic_CodeGenContext & ) {}

    virtual void getWaveSymbolList (
        const SonicToken *waveSymbol[],
        int maxWaveSymbols,
        int &numSoFar,
        int &numOccurrences );

private:
    SonicToken dollarSign;
};


class SonicParse_Expression_Constant: public SonicParse_Expression
{
public:
    SonicParse_Expression_Constant (
        const SonicToken &_value,
        SonicType _type ):
            SonicParse_Expression ( ETYPE_CONSTANT ),
            value ( _value ),
            type ( _type )
            {}
        
    virtual SonicType determineType() const { return type; }
    virtual int operatorPrecedence() const { return 100; }
    const SonicToken & queryValue() const { return value; }
    virtual void validate ( SonicParse_Program &, SonicParse_Function * ) {}
    virtual const SonicToken & getFirstToken() const { return value; }
    virtual void generateCode ( ostream &, Sonic_CodeGenContext & );
    virtual void generatePreSampleLoopCode  ( ostream &, Sonic_CodeGenContext & );
    virtual void generatePreChannelLoopCode ( ostream &, Sonic_CodeGenContext & );

private:
    SonicToken value;
    SonicType type;
};


class SonicParse_Expression_Sawtooth: public SonicParse_Expression
{
public:
    SonicParse_Expression_Sawtooth (
        const SonicToken &_sawtoothToken,
        SonicParse_Expression *_frequencyHz ):
            SonicParse_Expression ( ETYPE_SAWTOOTH ),
            channelDependent ( false ),
            sawtoothToken ( _sawtoothToken ),
            frequencyHz ( _frequencyHz )
            {}

    virtual ~SonicParse_Expression_Sawtooth()
    {
        if ( frequencyHz )
        {
            delete frequencyHz;
            frequencyHz = 0;
        }
    }

    virtual void generatePreSampleLoopCode  ( ostream &, Sonic_CodeGenContext & );
    virtual void generatePreChannelLoopCode ( ostream &, Sonic_CodeGenContext & );
    virtual void generateCode ( ostream &, Sonic_CodeGenContext & );
    virtual SonicType determineType() const { return STYPE_REAL; }
    virtual int operatorPrecedence() const { return 100; }
    virtual void validate ( SonicParse_Program &, SonicParse_Function * );
    virtual const SonicToken &getFirstToken() const { return sawtoothToken; }

    virtual void visit ( Sonic_ExpressionVisitor &v ) const
    {
        v.visitHook (this);
        frequencyHz->visit (v);
    }

private:
    bool channelDependent;
    int tempTag [MAX_SONIC_CHANNELS];
    SonicToken sawtoothToken;
    SonicParse_Expression *frequencyHz;
};


class SonicParse_Expression_Sinewave: public SonicParse_Expression
{
public:
    SonicParse_Expression_Sinewave (
        const SonicToken &_sinewaveToken,
        SonicParse_Expression *_amplitude,
        SonicParse_Expression *_frequencyHz,
        SonicParse_Expression *_phaseDeg ):
            SonicParse_Expression ( ETYPE_SINEWAVE ),
            channelDependent ( false ),
            sinewaveToken ( _sinewaveToken ),
            amplitude ( _amplitude ),
            frequencyHz ( _frequencyHz ),
            phaseDeg ( _phaseDeg )
            {}

    virtual ~SonicParse_Expression_Sinewave()
    {
        if ( amplitude )
        {
            delete amplitude;
            amplitude = 0;
        }

        if ( frequencyHz )
        {
            delete frequencyHz;
            frequencyHz = 0;
        }

        if ( phaseDeg )
        {
            delete phaseDeg;
            phaseDeg = 0;
        }
    }

    virtual void generatePreSampleLoopCode  ( ostream &, Sonic_CodeGenContext & );
    virtual void generatePreChannelLoopCode ( ostream &, Sonic_CodeGenContext & );
    virtual void generateCode ( ostream &, Sonic_CodeGenContext & );
    virtual SonicType determineType() const { return STYPE_REAL; }
    virtual int operatorPrecedence() const { return 100; }
    virtual void validate ( SonicParse_Program &, SonicParse_Function * );
    virtual const SonicToken &getFirstToken() const { return sinewaveToken; }

    virtual void visit ( Sonic_ExpressionVisitor &v ) const
    {
        v.visitHook (this);
        amplitude->visit (v);
        frequencyHz->visit (v);
        phaseDeg->visit (v);
    }

private:
    bool channelDependent;
    SonicToken sinewaveToken;
    SonicParse_Expression *amplitude;
    SonicParse_Expression *frequencyHz;
    SonicParse_Expression *phaseDeg;
    int tempTag [MAX_SONIC_CHANNELS];
};


class SonicParse_Expression_FFT: public SonicParse_Expression
{
public:
    SonicParse_Expression_FFT (
        const SonicToken &_fftToken,
        SonicParse_Expression *_input,
        SonicParse_Expression *_fftSize,
        SonicParse_Expression *_freqShift,
        const SonicToken &_funcName ):
            SonicParse_Expression ( ETYPE_FFT ),
            fftToken ( _fftToken ),
            input ( _input ),
            fftSize ( _fftSize ),
            freqShift ( _freqShift ),
            funcName ( _funcName ),
            tempTag(0)
            {}

    virtual ~SonicParse_Expression_FFT()
    {
        if ( input )
        {
            delete input;
            input = 0;
        }

        if ( fftSize )
        {
            delete fftSize;
            fftSize = 0;
        }

        if ( freqShift )
        {
            delete freqShift;
            freqShift = 0;
        }
    }

    virtual void generatePreSampleLoopCode  ( ostream &, Sonic_CodeGenContext & );
    virtual void generatePreChannelLoopCode ( ostream &, Sonic_CodeGenContext & );
    virtual void generateCode ( ostream &, Sonic_CodeGenContext & );
    virtual SonicType determineType() const { return STYPE_REAL; }
    virtual int operatorPrecedence() const { return 100; }
    virtual void validate ( SonicParse_Program &, SonicParse_Function * );
    virtual const SonicToken &getFirstToken() const { return fftToken; }

    virtual void getWaveSymbolList ( 
        const SonicToken *waveSymbol[], 
        int maxWaveSymbols,
        int &numSoFar,
        int &numOccurrences );

    virtual void visit ( Sonic_ExpressionVisitor &v ) const
    {
        v.visitHook (this);
        input->visit (v);
        fftSize->visit (v);
    }

private:
    SonicToken  fftToken;
    SonicParse_Expression *input;
    SonicParse_Expression *fftSize;
    SonicParse_Expression *freqShift;
    SonicToken  funcName;
    int tempTag;
};


class SonicParse_Expression_IIR: public SonicParse_Expression
{
public:
    SonicParse_Expression_IIR (
        const SonicToken &_iirToken,
        SonicParse_Expression *_xCoeffList,
        SonicParse_Expression *_yCoeffList,
        SonicParse_Expression *_filterInput ):
            SonicParse_Expression ( ETYPE_IIR ),
            iirToken ( _iirToken ),
            xCoeffList ( _xCoeffList ),
            yCoeffList ( _yCoeffList ),
            filterInput ( _filterInput )
            {}

    virtual ~SonicParse_Expression_IIR()
    {
        if ( xCoeffList )
        {
            delete xCoeffList;
            xCoeffList = 0;
        }

        if ( yCoeffList )
        {
            delete yCoeffList;
            yCoeffList = 0;
        }

        if ( filterInput )
        {
            delete filterInput;
            filterInput = 0;
        }
    }

    virtual void generatePreSampleLoopCode  ( ostream &, Sonic_CodeGenContext & );
    virtual void generatePreChannelLoopCode ( ostream &, Sonic_CodeGenContext & );
    virtual void generateCode ( ostream &, Sonic_CodeGenContext & );
    virtual SonicType determineType() const { return STYPE_REAL; }
    virtual int operatorPrecedence() const { return 100; }
    virtual void validate ( SonicParse_Program &, SonicParse_Function * );
    virtual const SonicToken &getFirstToken() const { return iirToken; }

    virtual void getWaveSymbolList ( 
        const SonicToken *waveSymbol[], 
        int maxWaveSymbols,
        int &numSoFar,
        int &numOccurrences );

    virtual void visit ( Sonic_ExpressionVisitor &v ) const
    {
        v.visitHook (this);
        VisitList ( v, xCoeffList );
        VisitList ( v, yCoeffList );
        filterInput->visit (v);
    }

private:
    SonicToken iirToken;
    SonicParse_Expression *xCoeffList;
    SonicParse_Expression *yCoeffList;
    SonicParse_Expression *filterInput;

    int t_xCoeff;                           // x-coefficient array variable tag
    int t_yCoeff;                           // y-coefficient array variable tag
    int t_xIndex;                           // x-index variable tag
    int t_yIndex;                           // y-index variable tag
    int t_xBuffer [MAX_SONIC_CHANNELS];     // tags for x-buffer variables
    int t_yBuffer [MAX_SONIC_CHANNELS];     // tags for y-buffer variables
    int t_accum;                            // accumulator array tag
};



struct Sonic_IntrinsicTableEntry
{
    const char *sname;  // name in Sonic
    const char *cname;  // name in C++
    int numParms;       // number of parameters required by function
};


const Sonic_IntrinsicTableEntry *FindIntrinsic ( const char *sname );
bool IsPseudoFunction ( const SonicToken &name );


enum SonicFunctionType
{
    SFT_UNDEFINED,
    SFT_USER,
    SFT_INTRINSIC,
    SFT_IMPORT
};


class SonicParse_Expression_FunctionCall: public SonicParse_Expression
{
public:
    SonicParse_Expression_FunctionCall ( 
        const SonicToken &_name,
        SonicParse_Expression *_parmList,
        SonicFunctionType _ftype ):
            SonicParse_Expression ( ETYPE_FUNCTION_CALL ),
            name ( _name ),
            type ( STYPE_UNDEFINED ),
            parmList ( _parmList ),
            ftype ( _ftype )
            {}

    virtual ~SonicParse_Expression_FunctionCall()
    {
        if ( parmList )
        {
            delete parmList;
            parmList = 0;
        }
    }

    virtual SonicType determineType() const { return type; }
    virtual int operatorPrecedence() const { return 100; }
    virtual void validate ( SonicParse_Program &, SonicParse_Function * );
    virtual const SonicToken & getFirstToken() const { return name; }
    virtual void generateCode ( ostream &, Sonic_CodeGenContext & );
    virtual void generatePreSampleLoopCode  ( ostream &, Sonic_CodeGenContext & );
    virtual void generatePreChannelLoopCode ( ostream &, Sonic_CodeGenContext & );

    virtual void getWaveSymbolList ( 
        const SonicToken *waveSymbol[], 
        int maxWaveSymbols,
        int &numSoFar,
        int &numOccurrences );

    SonicFunctionType queryFunctionType() const { return ftype; }
    bool isIntrinsic() const { return ftype == SFT_INTRINSIC; }
    virtual void visit ( Sonic_ExpressionVisitor &v ) const
    {
        v.visitHook (this);
        VisitList ( v, parmList );
    }

private:
    SonicToken name;
    SonicType type;     // type returned by function
    SonicParse_Expression *parmList;
    SonicFunctionType ftype;
};


class SonicParse_Expression_Builtin: public SonicParse_Expression
{
public:
    SonicParse_Expression_Builtin ( const SonicToken &_name ):
        SonicParse_Expression ( ETYPE_BUILTIN ),
        name ( _name )
        {}

    virtual SonicType determineType() const;
    virtual int operatorPrecedence() const { return 100; }
    virtual void validate ( SonicParse_Program &, SonicParse_Function * );
    virtual const SonicToken & getFirstToken() const { return name; }
    virtual void generateCode ( ostream &, Sonic_CodeGenContext & );
    virtual void generatePreSampleLoopCode  ( ostream &, Sonic_CodeGenContext & ) {}
    virtual void generatePreChannelLoopCode ( ostream &, Sonic_CodeGenContext & ) {}

private:
    SonicToken name;
};


class SonicParse_Expression_Variable: public SonicParse_Expression
{
public:
    SonicParse_Expression_Variable ( const SonicToken &_name ):
        SonicParse_Expression ( ETYPE_VARIABLE ),
        name ( _name ),
        type ( STYPE_UNDEFINED )
        {}

    virtual SonicType determineType() const { return type; }
    virtual int operatorPrecedence() const { return 100; }
    virtual void validate ( SonicParse_Program &, SonicParse_Function * );
    virtual const SonicToken & getFirstToken() const { return name; }
    virtual void generateCode ( ostream &, Sonic_CodeGenContext & );
    virtual void generatePreSampleLoopCode  ( ostream &, Sonic_CodeGenContext & ) {}
    virtual void generatePreChannelLoopCode ( ostream &, Sonic_CodeGenContext & ) {}

private:
    SonicToken name;
    SonicType type;
};


class SonicParse_Expression_WaveField: public SonicParse_Expression
{
public:
    SonicParse_Expression_WaveField ( 
        const SonicToken &_varName,
        const SonicToken &_field ):
            SonicParse_Expression ( ETYPE_WAVE_FIELD ),
            varName ( _varName ),
            field ( _field )
            {}

    virtual SonicType determineType() const;
    virtual int operatorPrecedence() const { return 100; }
    virtual void validate ( SonicParse_Program &, SonicParse_Function * ) {}
    virtual const SonicToken & getFirstToken() const { return varName; }
    virtual void generateCode ( ostream &, Sonic_CodeGenContext & );
    virtual void generatePreSampleLoopCode  ( ostream &, Sonic_CodeGenContext & ) {}
    virtual void generatePreChannelLoopCode ( ostream &, Sonic_CodeGenContext & ) {}

    virtual void getWaveSymbolList ( 
        const SonicToken *waveSymbol[], 
        int maxWaveSymbols,
        int &numSoFar,
        int &numOccurrences );

private:
    SonicToken varName;
    SonicToken field;
};


class SonicParse_Expression_BinaryOp: public SonicParse_Expression
{
public:
    SonicParse_Expression_BinaryOp (
        const SonicToken &_op,
        SonicParse_Expression *_lchild,
        SonicParse_Expression *_rchild ):
            SonicParse_Expression ( ETYPE_BINARY_OP ),
            op ( _op ),
            lchild ( _lchild ),
            rchild ( _rchild )
            {}

    virtual ~SonicParse_Expression_BinaryOp();

    const SonicToken &queryOp() const { return op; }
    virtual bool groupsToRight() const = 0;
    virtual const SonicToken & getFirstToken() const { return lchild->getFirstToken(); }

    virtual void getWaveSymbolList ( 
        const SonicToken *waveSymbol[], 
        int maxWaveSymbols,
        int &numSoFar,
        int &numOccurrences );

    virtual void generatePreSampleLoopCode  ( ostream &, Sonic_CodeGenContext & );
    virtual void generatePreChannelLoopCode ( ostream &, Sonic_CodeGenContext & );
    virtual void visit ( Sonic_ExpressionVisitor &v ) const
    { 
        v.visitHook (this);
        lchild->visit (v);
        rchild->visit (v);
    }

protected:
    SonicToken op;
    SonicParse_Expression *lchild;
    SonicParse_Expression *rchild;
};


class SonicParse_Expression_BinaryMathOp: public SonicParse_Expression_BinaryOp
{
public:
    SonicParse_Expression_BinaryMathOp (
        const SonicToken &_op,
        SonicParse_Expression *_lchild,
        SonicParse_Expression *_rchild ):
            SonicParse_Expression_BinaryOp ( _op, _lchild, _rchild )
            {}

    virtual SonicType determineType() const;
    virtual void validate ( SonicParse_Program &, SonicParse_Function * );
    virtual void generateCode ( ostream &, Sonic_CodeGenContext & );
};


class SonicParse_Expression_BinaryBoolOp: public SonicParse_Expression_BinaryOp
{
public:
    SonicParse_Expression_BinaryBoolOp (
        const SonicToken &_op,
        SonicParse_Expression *_lchild,
        SonicParse_Expression *_rchild,
        bool _requiresBooleanOperands ):
            SonicParse_Expression_BinaryOp ( _op, _lchild, _rchild ),
            requiresBooleanOperands ( _requiresBooleanOperands )
            {}

    virtual SonicType determineType() const { return STYPE_BOOLEAN; }
    virtual void validate ( SonicParse_Program &, SonicParse_Function * );
    virtual void generateCode ( ostream &, Sonic_CodeGenContext & );

private:
    bool requiresBooleanOperands;
};


class SonicParse_Expression_Add: public SonicParse_Expression_BinaryMathOp
{
public:
    SonicParse_Expression_Add (
        const SonicToken &_op,
        SonicParse_Expression *_lchild,
        SonicParse_Expression *_rchild ):
            SonicParse_Expression_BinaryMathOp ( _op, _lchild, _rchild )
            {}

    virtual bool groupsToRight() const { return false; }
    virtual int operatorPrecedence() const { return 10; }
};


class SonicParse_Expression_Subtract: public SonicParse_Expression_BinaryMathOp
{
public:
    SonicParse_Expression_Subtract (
        const SonicToken &_op,
        SonicParse_Expression *_lchild,
        SonicParse_Expression *_rchild ):
            SonicParse_Expression_BinaryMathOp ( _op, _lchild, _rchild )
            {}

    virtual bool groupsToRight() const { return true; }
    virtual int operatorPrecedence() const { return 10; }
};


class SonicParse_Expression_Multiply: public SonicParse_Expression_BinaryMathOp
{
public:
    SonicParse_Expression_Multiply (
        const SonicToken &_op,
        SonicParse_Expression *_lchild,
        SonicParse_Expression *_rchild ):
            SonicParse_Expression_BinaryMathOp ( _op, _lchild, _rchild )
            {}

    virtual bool groupsToRight() const { return false; }
    virtual int operatorPrecedence() const { return 11; }
};


class SonicParse_Expression_Divide: public SonicParse_Expression_BinaryMathOp
{
public:
    SonicParse_Expression_Divide (
        const SonicToken &_op,
        SonicParse_Expression *_lchild,
        SonicParse_Expression *_rchild ):
            SonicParse_Expression_BinaryMathOp ( _op, _lchild, _rchild )
            {}

    virtual bool groupsToRight() const { return true; }
    virtual int operatorPrecedence() const { return 11; }
};


class SonicParse_Expression_Mod: public SonicParse_Expression_BinaryMathOp
{
public:
    SonicParse_Expression_Mod (
        const SonicToken &_op,
        SonicParse_Expression *_lchild,
        SonicParse_Expression *_rchild ):
            SonicParse_Expression_BinaryMathOp ( _op, _lchild, _rchild )
            {}

    virtual bool groupsToRight() const { return true; }
    virtual int operatorPrecedence() const { return 11; }
    virtual void generateCode ( ostream &, Sonic_CodeGenContext & );
};


class SonicParse_Expression_Power: public SonicParse_Expression_BinaryMathOp
{
public:
    SonicParse_Expression_Power (
        const SonicToken &_op,
        SonicParse_Expression *_lchild,
        SonicParse_Expression *_rchild ):
            SonicParse_Expression_BinaryMathOp ( _op, _lchild, _rchild )
            {}

    virtual bool groupsToRight() const { return false; }
    virtual int operatorPrecedence() const { return 12; }
    virtual void generateCode ( ostream &, Sonic_CodeGenContext & );
};


class SonicParse_Expression_Or: public SonicParse_Expression_BinaryBoolOp
{
public:
    SonicParse_Expression_Or (
        const SonicToken &_op,
        SonicParse_Expression *_lchild,
        SonicParse_Expression *_rchild ):
            SonicParse_Expression_BinaryBoolOp ( _op, _lchild, _rchild, true )
            {}

    virtual bool groupsToRight() const { return false; }
    virtual int operatorPrecedence() const { return 1; }
};


class SonicParse_Expression_And: public SonicParse_Expression_BinaryBoolOp
{
public:
    SonicParse_Expression_And (
        const SonicToken &_op,
        SonicParse_Expression *_lchild,
        SonicParse_Expression *_rchild ):
            SonicParse_Expression_BinaryBoolOp ( _op, _lchild, _rchild, true )
            {}

    virtual bool groupsToRight() const { return false; }
    virtual int operatorPrecedence() const { return 2; }
};


class SonicParse_Expression_Equal: public SonicParse_Expression_BinaryBoolOp
{
public:
    SonicParse_Expression_Equal (
        const SonicToken &_op,
        SonicParse_Expression *_lchild,
        SonicParse_Expression *_rchild ):
            SonicParse_Expression_BinaryBoolOp ( _op, _lchild, _rchild, false )
            {}

    virtual bool groupsToRight() const { return false; }
    virtual int operatorPrecedence() const { return 3; }
};


class SonicParse_Expression_Less: public SonicParse_Expression_BinaryBoolOp
{
public:
    SonicParse_Expression_Less (
        const SonicToken &_op,
        SonicParse_Expression *_lchild,
        SonicParse_Expression *_rchild ):
            SonicParse_Expression_BinaryBoolOp ( _op, _lchild, _rchild, false )
            {}

    virtual bool groupsToRight() const { return false; }
    virtual int operatorPrecedence() const { return 3; }
};


class SonicParse_Expression_LessEqual: public SonicParse_Expression_BinaryBoolOp
{
public:
    SonicParse_Expression_LessEqual (
        const SonicToken &_op,
        SonicParse_Expression *_lchild,
        SonicParse_Expression *_rchild ):
            SonicParse_Expression_BinaryBoolOp ( _op, _lchild, _rchild, false )
            {}

    virtual bool groupsToRight() const { return false; }
    virtual int operatorPrecedence() const { return 3; }
};


class SonicParse_Expression_Greater: public SonicParse_Expression_BinaryBoolOp
{
public:
    SonicParse_Expression_Greater (
        const SonicToken &_op,
        SonicParse_Expression *_lchild,
        SonicParse_Expression *_rchild ):
            SonicParse_Expression_BinaryBoolOp ( _op, _lchild, _rchild, false )
            {}

    virtual bool groupsToRight() const { return false; }
    virtual int operatorPrecedence() const { return 3; }
};


class SonicParse_Expression_GreaterEqual: public SonicParse_Expression_BinaryBoolOp
{
public:
    SonicParse_Expression_GreaterEqual (
        const SonicToken &_op,
        SonicParse_Expression *_lchild,
        SonicParse_Expression *_rchild ):
            SonicParse_Expression_BinaryBoolOp ( _op, _lchild, _rchild, false )
            {}

    virtual bool groupsToRight() const { return false; }
    virtual int operatorPrecedence() const { return 3; }
};


class SonicParse_Expression_NotEqual: public SonicParse_Expression_BinaryBoolOp
{
public:
    SonicParse_Expression_NotEqual (
        const SonicToken &_op,
        SonicParse_Expression *_lchild,
        SonicParse_Expression *_rchild ):
            SonicParse_Expression_BinaryBoolOp ( _op, _lchild, _rchild, false )
            {}

    virtual bool groupsToRight() const { return false; }
    virtual int operatorPrecedence() const { return 3; }
};


class SonicParse_Expression_UnaryOp: public SonicParse_Expression
{
public:
    SonicParse_Expression_UnaryOp ( 
        const SonicToken &_op,
        SonicParse_Expression *_child ):
            SonicParse_Expression ( ETYPE_UNARY_OP ),
            op ( _op ),
            child ( _child )
            {}

    virtual ~SonicParse_Expression_UnaryOp();
    const SonicToken &queryOp() const { return op; }

    virtual int operatorPrecedence() const { return 50; }
    virtual const SonicToken & getFirstToken() const { return op; }
    virtual void generateCode ( ostream &, Sonic_CodeGenContext & );
    virtual void generatePreSampleLoopCode  ( ostream &, Sonic_CodeGenContext & );
    virtual void generatePreChannelLoopCode ( ostream &, Sonic_CodeGenContext & );

    virtual void getWaveSymbolList ( 
        const SonicToken *waveSymbol[], 
        int maxWaveSymbols,
        int &numSoFar,
        int &numOccurrences );

    virtual void visit ( Sonic_ExpressionVisitor &v ) const
    { 
        v.visitHook (this);
        child->visit (v);
    }

protected:
    SonicToken op;
    SonicParse_Expression *child;
};


class SonicParse_Expression_Negate: public SonicParse_Expression_UnaryOp
{
public:
    SonicParse_Expression_Negate (
        const SonicToken &_op,
        SonicParse_Expression *_child ):
            SonicParse_Expression_UnaryOp ( _op, _child )
            {}

    virtual SonicType determineType() const { return child->determineType(); }
    virtual void validate ( SonicParse_Program &, SonicParse_Function * );
};


class SonicParse_Expression_Not: public SonicParse_Expression_UnaryOp
{
public:
    SonicParse_Expression_Not (
        const SonicToken &_op,
        SonicParse_Expression *_child ):
            SonicParse_Expression_UnaryOp ( _op, _child )
            {}

    virtual SonicType determineType() const { return STYPE_BOOLEAN; }
    virtual void validate ( SonicParse_Program &, SonicParse_Function * );
};


class SonicParse_VarDecl
{
public:
    SonicParse_VarDecl ( 
        const SonicToken &_name,
        SonicType _type,
        SonicParse_Expression *_init,       // NULL if no explicit initializer
        bool _isGlobal );                   // NULL if global variable

    ~SonicParse_VarDecl();

    void validate ( SonicParse_Program &, SonicParse_Function * );
    const SonicToken &queryName() const { return name; }
    const SonicType &queryType() const { return type; }
    SonicParse_Expression *queryInit() const { return init; }
    SonicParse_VarDecl *queryNext() const { return next; }
    virtual void generateCode ( ostream &, Sonic_CodeGenContext & );

    void modifyResetFlag ( bool _resetFlag )  { resetFlag = _resetFlag; }
    bool queryResetFlag() const { return resetFlag; }

    static void ParseVarList ( 
        SonicScanner &,
        SonicParse_VarDecl * &varList,
        SonicParse_Program &prog,
        bool isGlobal );

    bool queryIsGlobal() const { return isGlobal; }

private:
    friend class SonicParse_Function;

    SonicParse_VarDecl  *next;  // for implementing linked list
    SonicToken name;
    SonicType  type;
    SonicParse_Expression  *init;
    bool resetFlag;     // used only for import function objects (so reset() generated only once)
    bool isGlobal;
};


class SonicParse_Statement
{
public:
    SonicParse_Statement(): next(0) {}
    virtual ~SonicParse_Statement()
    {
        if ( next )
        {
            delete next;
            next = 0;
        }
    }

    virtual SonicStatementType queryType() const = 0;
    virtual void validate ( SonicParse_Program &, SonicParse_Function * ) = 0;

    static SonicParse_Statement *Parse ( SonicScanner & );
    virtual void generateCode ( ostream &, Sonic_CodeGenContext & ) = 0;
    virtual bool needsBraces() const { return false; }

protected:
    SonicParse_Statement *queryNext() const { return next; }

private:
    friend class SonicParse_Function;
    friend class SonicParse_Statement_Compound;
    SonicParse_Statement *next;     // for implementing linked list
};


class SonicParse_Statement_Compound: public SonicParse_Statement
{
public:
    SonicParse_Statement_Compound ( SonicParse_Statement *_compound ):
        compound ( _compound )
        {}

    virtual ~SonicParse_Statement_Compound()
    {
        if ( compound )
        {
            delete compound;
            compound = 0;
        }
    }

    virtual SonicStatementType queryType() const { return STMT_COMPOUND; }
    virtual void validate ( SonicParse_Program &, SonicParse_Function * );
    virtual void generateCode ( ostream &, Sonic_CodeGenContext & );
    virtual bool needsBraces() const 
    {
        return compound && (compound->next || compound->needsBraces());
    }

private:
    SonicParse_Statement *compound;
};


class SonicParse_Statement_FunctionCall: public SonicParse_Statement
{
public:
    SonicParse_Statement_FunctionCall (
        SonicParse_Expression_FunctionCall *_call ):
            call ( _call )
            {}

    virtual ~SonicParse_Statement_FunctionCall()
    {
        if ( call )
        {
            delete call;
            call = 0;
        }
    }

    virtual SonicStatementType queryType() const { return STMT_FUNCCALL; }
    SonicParse_Expression_FunctionCall *queryCall() const { return call; }
    virtual void validate ( SonicParse_Program &, SonicParse_Function * );
    virtual void generateCode ( ostream &, Sonic_CodeGenContext & );

private:
    SonicParse_Expression_FunctionCall *call;
};


class SonicParse_Statement_If: public SonicParse_Statement
{
public:
    SonicParse_Statement_If (
        SonicParse_Expression *_condition,
        SonicParse_Statement *_ifPart,
        SonicParse_Statement *_elsePart ):
            condition ( _condition ),
            ifPart ( _ifPart ),
            elsePart ( _elsePart )
            {}

    virtual ~SonicParse_Statement_If()
    {
        if ( condition )
        {
            delete condition;
            condition = 0;
        }

        if ( ifPart )
        {
            delete ifPart;
            ifPart = 0;
        }

        if ( elsePart )
        {
            delete elsePart;
            elsePart = 0;
        }
    }

    virtual SonicStatementType queryType() const { return STMT_IF; }
    virtual void validate ( SonicParse_Program &, SonicParse_Function * );
    virtual void generateCode ( ostream &, Sonic_CodeGenContext & );

private:
    SonicParse_Expression *condition;
    SonicParse_Statement *ifPart;
    SonicParse_Statement *elsePart;
};


class SonicParse_Statement_Repeat: public SonicParse_Statement
{
public:
    SonicParse_Statement_Repeat (
        SonicParse_Expression *_count,
        SonicParse_Statement *_loop ):
            count ( _count ),
            loop ( _loop )
            {}

    virtual ~SonicParse_Statement_Repeat()
    {
        if ( count )
        {
            delete count;
            count = 0;
        }

        if ( loop )
        {
            delete loop;
            loop = 0;
        }
    }

    virtual SonicStatementType queryType() const { return STMT_REPEAT; }
    virtual void validate ( SonicParse_Program &, SonicParse_Function * );
    virtual void generateCode ( ostream &, Sonic_CodeGenContext & );

private:
    SonicParse_Expression *count;
    SonicParse_Statement *loop;
};


class SonicParse_Statement_While: public SonicParse_Statement
{
public:
    SonicParse_Statement_While (
        SonicParse_Expression *_condition,
        SonicParse_Statement *_loop ):
            condition ( _condition ),
            loop ( _loop )
            {}

    virtual ~SonicParse_Statement_While()
    {
        if ( condition )
        {
            delete condition;
            condition = 0;
        }

        if ( loop )
        {
            delete loop;
            loop = 0;
        }
    }

    virtual SonicStatementType queryType() const { return STMT_WHILE; }
    virtual void validate ( SonicParse_Program &, SonicParse_Function * );
    virtual void generateCode ( ostream &, Sonic_CodeGenContext & );

private:
    SonicParse_Expression *condition;
    SonicParse_Statement *loop;
};


class SonicParse_Statement_Return: public SonicParse_Statement
{
public:
    SonicParse_Statement_Return (
        const SonicToken &_returnToken,
        SonicParse_Expression *_returnValue ):
            returnToken ( _returnToken ),
            returnValue ( _returnValue )
            {}

    virtual ~SonicParse_Statement_Return()
    {
        if ( returnValue )
        {
            delete returnValue;
            returnValue = 0;
        }
    }

    virtual SonicStatementType queryType() const { return STMT_RETURN; }
    SonicParse_Expression *queryReturnValue() const { return returnValue; }
    virtual void validate ( SonicParse_Program &, SonicParse_Function * );
    virtual void generateCode ( ostream &, Sonic_CodeGenContext & );

private:
    SonicToken returnToken;
    SonicParse_Expression *returnValue;
};


class SonicParse_Lvalue
{
public:
    SonicParse_Lvalue (
        const SonicToken &_varName,
        bool _isWave,
        SonicParse_Expression *_sampleLimit ):
            varName ( _varName ),
            isWave ( _isWave ),
            sampleLimit ( _sampleLimit )
            {}

    virtual ~SonicParse_Lvalue()
    {
        if ( sampleLimit )
        {
            delete sampleLimit;
            sampleLimit = 0;
        }
    }

    const SonicToken &queryVarName() const { return varName; }
    bool queryIsWave() const { return isWave; }
    SonicParse_Expression *querySampleLimit() const { return sampleLimit; }
    void validate ( SonicParse_Program &, SonicParse_Function * );
    SonicType determineType ( SonicParse_Program &, SonicParse_Function * ) const;

private:
    SonicToken varName;
    bool isWave;
    SonicParse_Expression *sampleLimit;
};


class SonicParse_Statement_Assignment: public SonicParse_Statement
{
public:
    SonicParse_Statement_Assignment (
        const SonicToken &_op,
        SonicParse_Lvalue *_lvalue,
        SonicParse_Expression *_rvalue ):
            op ( _op ),
            lvalue ( _lvalue ),
            rvalue ( _rvalue )
            {}

    virtual ~SonicParse_Statement_Assignment()
    {
        if ( lvalue )
        {
            delete lvalue;
            lvalue = 0;
        }

        if ( rvalue )
        {
            delete rvalue;
            rvalue = 0;
        }
    }

    virtual SonicStatementType queryType() const { return STMT_ASSIGNMENT; }
    virtual void validate ( SonicParse_Program &, SonicParse_Function * );
    virtual void generateCode ( ostream &, Sonic_CodeGenContext & );
    virtual bool needsBraces() const { return lvalue->queryIsWave(); }

private:
    SonicToken op;
    SonicParse_Lvalue *lvalue;
    SonicParse_Expression *rvalue;
};


class SonicParse_Function   // includes both 'program' and 'function' bodies
{
public:
    SonicParse_Function ( 
        const SonicToken &_name,
        bool _isProgramBody,
        SonicType _returnType,
        SonicParse_VarDecl *_parmList,
        SonicParse_VarDecl *_varList,
        SonicParse_Statement *_statementList,
        SonicParse_Program & );

    ~SonicParse_Function();

    static SonicParse_Function *Parse ( SonicScanner &, SonicParse_Program & );
    const SonicToken &queryName() const { return name; }
    bool queryIsProgramBody() const { return isProgramBody; }
    void validate ( SonicParse_Program & );
    SonicType queryReturnType() const { return returnType; }
    SonicParse_VarDecl *findSymbol ( const SonicToken &symbol, bool forceFind ) const;
    int countInstances ( const SonicToken &name ) const;
    void validateUniqueSymbol ( SonicParse_Program &, const SonicToken & );
    SonicParse_VarDecl *queryParmList() const { return parmList; }
    virtual void generateCode ( ostream &, Sonic_CodeGenContext & );
    void generatePrototype ( ostream &, Sonic_CodeGenContext & );
    int numParameters() const;
    bool isImport() const { return importHeader.queryToken() != 0; }
    const SonicToken &queryImportHeader() const { return importHeader; }
    void clearAllResetFlags();   // operates on all owned local symbols

private:
    friend class SonicParse_Program;

    bool isProgramBody;
    SonicToken name;
    SonicParse_Function *next;   // for linked list of functions
    SonicType  returnType;
    SonicParse_VarDecl  *parmList;
    SonicParse_VarDecl  *varList;
    SonicParse_Statement *statementList;
    SonicToken importHeader;
    SonicParse_Program &prog;
};


const int           SPACES_PER_INDENT       = 4;
const char * const  LOCAL_SYMBOL_PREFIX     = "v_";
const char * const  FUNCTION_PREFIX         = "f_";
const char * const  TEMPORARY_PREFIX        = "t_";
const char * const  IMPORT_PREFIX           = "i_";

struct Sonic_CodeGenContext
{
    Sonic_CodeGenContext ( SonicParse_Program *_prog ):
        indentLevel (0),
        iAllowed (false),
        cAllowed (false),
        nextTempTag (0),
        insideFunctionParms (false),
        generatingComment (false),
        bracketer(0),
        channelValue (-1),
        prog (_prog),
        func (0),
        insideVector (false)
        {}

    void indent ( ostream &, const char *s = "" );
    void pushIndent() { indentLevel += SPACES_PER_INDENT; }
    void popIndent()  { indentLevel -= SPACES_PER_INDENT; }

public:
    int     indentLevel;                // number of spaces to indent output C++ code
    bool    iAllowed;                   // 'i' allowed to occur in expression
    bool    cAllowed;                   // 'c' allowed to occur in expression
    int     nextTempTag;                // t_0, t_1, t_2, etc.
    bool    insideFunctionParms;
    bool    generatingComment;          // suppress picky errors; we're just documenting!
    const SonicToken *bracketer;        // set to non-NULL when inside wave expr []
    int     channelValue;               // if not -1, the value of the current channel
    SonicParse_Program *prog;
    SonicParse_Function *func;
    bool    insideVector;
};


#endif // __ddc_sonic_parse_h
/*--- end of file parse.h ---*/
