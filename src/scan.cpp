/*=======================================================================

    scan.cpp  -  Copyright (C) 1998 by Don Cross <cosinekitty@gmail.com>

    This module is responsible for scanning and classifying
    lexical tokens for the Sonic programming language.
    Tokens are scanned from an input stream, and can be pushed
    and popped from a stack as necessary for backtracking by the
    recursive descent parser.  (When SonicScanner::getToken() is
    called, it automatically pops the top token off the stack if
    any are there and returns that token.)

Revision history:

1998 October 2 [Don Cross]
    Adding support for multiple source files.

1998 September 29 [Don Cross]
    Fixed bug:  double-slash comments on consecutive lines were getting
    messed up in SonicScanner::skipWhitespace().  I have for once
    found a good use for the 'continue' statement to hack this into
    correctness.

=======================================================================*/
#include <iostream>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "scan.h"


SonicToken::SonicToken():
    tokenType(STT_UNKNOWN),
    token(0),
    line(0),
    column(0),
    sourceFile(0)
{
}


SonicToken::SonicToken(const SonicToken &other):
    tokenType(other.tokenType),
    token(CopyString(other.token)),
    line(other.line),
    column(other.column),
    sourceFile(other.sourceFile)
{
}


SonicToken::~SonicToken()
{
    DeleteString(token);
}


const SonicToken & SonicToken::operator= (const SonicToken &other)
{
    if (&other != this)
    {
        DeleteString(token);
        token = CopyString(other.token);
        tokenType = other.tokenType;
        line = other.line;
        column = other.column;
        sourceFile = other.sourceFile;
    }

    return *this;
}


void SonicToken::define(
    const char *_token,
    int _line,
    int _column,
    SonicTokenType _tokenType)
{
    DeleteString(token);
    token = CopyString(_token);
    line = _line;
    column = _column;
    tokenType = _tokenType;
    sourceFile = SonicScanner::GetCurrentSourceFilename();
}


bool SonicToken::operator== (const char *s) const
{
    if (!token || !s)
        return false;

    return strcmp(token, s) == 0;
}


//-----------------------------------------------------------------------


char *SonicScanner::FilenameTable [MAX_SONIC_SOURCE_FILES];
int SonicScanner::NumFilenames = 0;


SonicScanner::SonicScanner(std::istream &_input, const char *_filename):
    input(_input),
    filename(CopyString(_filename)),
    line(1),
    column(1),
    charStackTop(-1),
    tokenStackTop(-1)
{
    if (SonicScanner::NumFilenames >= MAX_SONIC_SOURCE_FILES)
        throw "Too many Sonic source files!";

    SonicScanner::FilenameTable [SonicScanner::NumFilenames++] = CopyString(_filename);
}


SonicScanner::~SonicScanner()
{
    DeleteString(filename);
}


const char *SonicScanner::GetCurrentSourceFilename()
{
    int index = SonicScanner::NumFilenames - 1;
    if (index < 0)
        throw "Attempt to access source filename when there was none!";

    return SonicScanner::FilenameTable[index];
}


SonicTokenType SonicScanner::ClassifySymbol(const char *s)
{
    static const char *keyword[] =
    {
        "program",
        "function",
        "var",
        "return",
        "if",
        "while",
        "repeat",
        "real",
        "integer",
        "boolean",
        "wave",
        "import",
        "from",
        0
    };

    // search keyword table...

    for (int i=0; keyword[i]; ++i)
    {
        if (strcmp(s,keyword[i]) == 0)
            return STT_KEYWORD;
    }

    static const char *builtin[] =
    {
        "i",
        "c",
        "pi",
        "e",
        "r",
        "t",
        "true",
        "false",
        "m",
        "n",
        "interpolate",
        0
    };

    for (int i=0; builtin[i]; ++i)
    {
        if (strcmp(s,builtin[i]) == 0)
            return STT_BUILTIN;
    }

    return STT_IDENTIFIER;
}



static void accept(char *s, int len, int &index, int c)
{
    if (index >= len-1)
        throw SonicParseException("token buffer overflow");

    s[index++] = static_cast<char>(c);
    s[index] = '\0';
}


bool SonicScanner::getToken(SonicToken &t, bool forceGet)
{
    if (tokenStackTop >= 0)
    {
        // Getting here means that the token stack is not empty.
        // Pop the top token off the stack and return it, instead of
        // scanning a new token from the input stream.
        // This mechanism allows the recursive descent parser to
        // backtrack when necessary (which is very often!).

        t = tokenStack[tokenStackTop--];
        return true;
    }

    char s [1024];      // Allocate a big buffer to hold the next token
    int index = 0;
    s[index] = '\0';

    if (!skipWhitespace())
    {
        if (forceGet)
            throw SonicParseException("unexpected end of file");

        return false;
    }

    int tline = line;       // keep track of line number and column in the input
    int tcolumn = column;

    SonicTokenChar tc = get();
    accept(s, sizeof(s), index, tc.c);
    if (isalpha(tc.c) || tc.c=='_')
    {
        tc = peek();
        while (isalnum(tc.c) || tc.c=='_')
        {
            accept(s, sizeof(s), index, tc.c);
            get();
            tc = peek();
        }

        t.define(s, tline, tcolumn, ClassifySymbol(s));
    }
    else if (tc.c == '"')
    {
        for (;;)
        {
            tc = get();
            if (tc.c == '"')
                break;
            else if (tc.c == EOF || tc.c == '\n' || tc.c == '\r')
            {
                t.define(s, tline, tcolumn, STT_STRING);
                throw SonicParseException("unterminated string constant", t);
            }
            else
                accept(s, sizeof(s), index, tc.c);
        }

        for (int k=0; (s[k] = s[k+1]) != '\0'; ++k);    // delete '"' at beginning

        t.define(s, tline, tcolumn, STT_STRING);
    }
    else if (isdigit(tc.c))
    {
        int eCount = 0;
        bool eFollow = false;
        bool eAfter = false;
        int dotCount = 0;
        for (;;)
        {
            tc = peek();
            if (tc.c == EOF)
                break;

            if (!isdigit(tc.c) && tc.c!='e' && tc.c!='E' && tc.c!='.')
            {
                if (tc.c == '+' || tc.c == '-')
                {
                    if (!eFollow)
                        break;
                }
                else
                    break;
            }

            if (tc.c == '.')
            {
                if (++dotCount > 1)
                {
                    t.define(s, tline, column, STT_CONSTANT);
                    throw SonicParseException("extraneous '.' in numeric constant", t);
                }

                if (eAfter)
                {
                    t.define(s, tline, column, STT_CONSTANT);
                    throw SonicParseException("error in numeric constant: '.' not allowed after 'e'/'E'", t);
                }
            }

            eFollow = (tc.c=='e' || tc.c=='E');
            if (eFollow)
            {
                eAfter = true;
                if (++eCount > 1)
                {
                    t.define(s, tline, tcolumn, STT_CONSTANT);
                    throw SonicParseException("extraneous 'e'/'E' in numeric constant", t);
                }
            }

            accept(s, sizeof(s), index, tc.c);
            get();
        }

        t.define(s, tline, tcolumn, STT_CONSTANT);
    }
    else
    {
        SonicTokenChar tc2 = peek();
        if (tc.c == '<')
        {
            if (tc2.c == '<' || tc2.c == '>' || tc2.c == '=')
            {
                accept(s, sizeof(s), index, tc2.c);
                get();
            }
        }
        else if (strchr("+-*/%=>!", tc.c))
        {
            if (tc2.c == '=')
            {
                accept(s, sizeof(s), index, tc2.c);
                get();
            }
        }

        t.define(s, tline, tcolumn, STT_PUNCTUATION);
    }

    return true;
}


void SonicScanner::pushToken(const SonicToken &t)
{
    if (tokenStackTop >= SCANNER_STACK_SIZE-1)
        throw SonicParseException("scanner token stack overflow!");

    tokenStack[++tokenStackTop] = t;
}


// The following member function is useful when the parser knows
// exactly which token must appear next in the input for the
// syntax to be correct.

void SonicScanner::scanExpected(const char *expectedToken)
{
    SonicToken t;
    const bool gotToken = getToken(t, false);      // disable EOF exception...
    if (!gotToken || t != expectedToken)
    {
        // ... so that we can give a more specific error message to the user.

        char temp [256];
        sprintf(temp, "expected '%s'", expectedToken);
        throw SonicParseException(temp, t);
    }
}


// The following member function skips over whitespace and positions the
// input stream at the beginning of the next lexical token.
// Note that it also is responsible for ignoring C++ style comments
// in the input.

bool SonicScanner::skipWhitespace()
{
    for (;;)
    {
        SonicTokenChar tc = peek();
        if (tc.c == EOF)
            return false;

        if (tc.c == '/')
        {
            // might be beginning of comment...
            get();
            SonicTokenChar tc2 = get();
            if (tc2.c == '/')
            {
                do
                {
                    tc = get();
                }
                while (tc.c != '\n' && tc.c != EOF);
                continue;
            }
            else if (tc2.c == '*')
            {
                bool startOver = false;
                for (;;)
                {
                    const char *unterm = "Unterminated '/*' comment at EOF";
                    tc = get();
                    if (tc.c == EOF)
                        throw SonicParseException(unterm);
                    else if (tc.c == '*')
                    {
                        tc = get();
                        if (tc.c == EOF)
                            throw SonicParseException(unterm);
                        else if (tc.c == '/')
                        {
                            startOver = true;
                            break;
                        }
                    }
                }

                if (startOver)
                    continue;
            }
            else
            {
                pushChar(tc2);
                pushChar(tc);
                break;
            }
        }

        if (!isspace(tc.c))
            break;

        get();
    }

    return true;
}


// Note that there is a character stack as well as a token stack.
// This character stack allows the scanner to do its own internal
// backtracking at the lexical level, just as the token stack
// allows the parser to do backtracking at the syntactical level.

void SonicScanner::pushChar(const SonicTokenChar &c)
{
    if (charStackTop >= SCANNER_STACK_SIZE-1)
        throw SonicParseException("scanner character stack overflow!");

    charStack[++charStackTop] = c;
}


SonicTokenChar SonicScanner::peek()
{
    if (charStackTop >= 0)
        return charStack[charStackTop];

    SonicTokenChar tc;
    tc.c = input.peek();
    tc.line = line;
    tc.column = column;

    return tc;
}


SonicTokenChar SonicScanner::get()
{
    if (charStackTop >= 0)
        return charStack[charStackTop--];

    SonicTokenChar tc;
    tc.c = input.get();
    tc.line = line;
    tc.column = column;
    if (tc.c == '\n')
    {
        ++line;
        column = 1;
    }
    else
    {
        ++column;
    }

    return tc;
}


//---------------------------------------------------------------------------------


SonicParseException::SonicParseException(
    const char *_reason)
{
    strncpy(reason, _reason, sizeof(reason));
}


SonicParseException::SonicParseException(
    const char *_reason,
    const SonicToken &_nearToken):
    nearToken(_nearToken)
{
    strncpy(reason, _reason, sizeof(reason));
}


std::ostream & operator<< (std::ostream &output, const SonicParseException &e)
{
    output << "Error:  " << e.reason << std::endl;
    if (e.nearToken.queryToken())
    {
        const char *source = e.nearToken.querySourceFilename();
        if (source)
        {
            output << "Source file:  '" << source << "'  ";
        }
        output << "line " << e.nearToken.queryLine();
        output << "  column " << e.nearToken.queryColumn() << std::endl;
        output << "near token '" << e.nearToken.queryToken() << "'  ";
    }

    return output;
}


//------------------------------------------------------------------------


std::ostream & operator << (std::ostream &output, SonicTokenType t)
{
    switch (t)
    {
    case STT_UNKNOWN:       output << "unknown";        break;
    case STT_KEYWORD:       output << "keyword";        break;
    case STT_IDENTIFIER:    output << "identifier";     break;
    case STT_BUILTIN:       output << "builtin";        break;
    case STT_CONSTANT:      output << "constant";       break;
    case STT_PUNCTUATION:   output << "punctuation";    break;
    case STT_STRING:        output << "string";         break;
    default:                output << "invalid(" << int(t) << ")";  break;
    }

    return output;
}


//------------------------------------------------------------------------


char *CopyString(const char *s)
{
    if (!s)
        throw "Attempt to copy a NULL string";

    char *p = new char [ 1 + strlen(s) ];
    strcpy(p, s);
    return p;
}


void DeleteString(char * & s)
{
    if (s)
    {
        delete[] s;
        s = 0;
    }
}


/*--- end of file scan.cpp ---*/
