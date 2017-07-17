/*====================================================================

    scan.h  -  Copyright (C) 1998 by Don Cross <cosinekitty@gmail.com>

    This module is responsible for scanning and classifying 
    lexical tokens for the Sonic programming language.
    Tokens are scanned from an input stream, and can be pushed
    and popped from a stack as necessary for backtracking by the
    recursive descent parser.  (When SonicScanner::getToken() is 
    called, it automatically pops the top token off the stack if 
    any are there and returns that token.)

====================================================================*/
#ifndef __scan_h
#define __scan_h

#ifndef _MSC_VER
    #define     bool    int
    #define     true    1
    #define     false   0
#elif _MSC_VER < 1100
    #define     bool    int
    #define     true    1
    #define     false   0
#endif


char *CopyString ( const char * );
void DeleteString ( char * & );


enum SonicTokenType
{
    STT_UNKNOWN,
    STT_KEYWORD,
    STT_IDENTIFIER,
    STT_BUILTIN,
    STT_CONSTANT,
    STT_PUNCTUATION,
    STT_STRING
};


ostream & operator << ( ostream &, SonicTokenType );


struct SonicTokenChar
{
    int     c;
    int     line;
    int     column;
};


class SonicToken
{
public:
    SonicToken();
    SonicToken ( const SonicToken & );
    ~SonicToken();

    const SonicToken & operator= ( const SonicToken & );
    void define ( const char *_token, int _line, int _column, SonicTokenType );
    bool operator== ( const char * ) const;
    bool operator!= ( const char *s ) const { return !(*this == s); }
    bool operator== ( const SonicToken &t ) const { return *this == t.queryToken(); }
    bool operator!= ( const SonicToken &t ) const { return *this != t.queryToken(); }
    SonicTokenType queryTokenType() const { return tokenType; }
    const char *queryToken() const { return token; }
    int queryLine() const { return line; }
    int queryColumn() const { return column; }

private:
    static SonicTokenType ClassifySymbol ( const char * );

private:
    SonicTokenType  tokenType;
    char            *token;
    int             line;
    int             column;
};


class istream;
class ostream;


const int SCANNER_STACK_SIZE = 16;


class SonicScanner
{
public:
    SonicScanner ( istream &_input, const char *_filename );
    ~SonicScanner();

    bool getToken ( SonicToken &, bool forceGet = true );
    void scanExpected ( const char *expectedToken );
    void pushToken ( const SonicToken & );

private:
    static SonicTokenType SonicScanner::ClassifySymbol ( const char *s );

    bool skipWhitespace();      // eats whitespace and comments
    void pushChar ( const SonicTokenChar & );
    SonicTokenChar peek();
    SonicTokenChar get();

private:
    istream &input;
    char *filename;
    int line;
    int column;

    SonicTokenChar charStack [SCANNER_STACK_SIZE];
    int charStackTop;   // index to top item; -1 means stack is empty

    SonicToken tokenStack [SCANNER_STACK_SIZE];
    int tokenStackTop;  // index to top item; -1 means stack is empty
};


class SonicParseException
{
public:
    SonicParseException ( const char *_reason );
    SonicParseException ( const char *_reason, const SonicToken &_nearToken );
    friend ostream & operator<< ( ostream &, const SonicParseException & );

private:
    char reason [128];
    SonicToken nearToken;
};


#endif // __scan_h
/*--- end of file scan.h ---*/
