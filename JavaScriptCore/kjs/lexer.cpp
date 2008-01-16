// -*- c-basic-offset: 2 -*-
/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2006, 2007 Apple Inc. All Rights Reserved.
 *  Copyright (C) 2007 Cameron Zwarich (cwzwarich@uwaterloo.ca)
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "lexer.h"

#include "dtoa.h"
#include "function.h"
#include "nodes.h"
#include "NodeInfo.h"
#include <ctype.h>
#include <limits.h>
#include <string.h>
#include <wtf/Assertions.h>
#include <wtf/unicode/Unicode.h>

using namespace WTF;
using namespace Unicode;

// we can't specify the namespace in yacc's C output, so do it here
using namespace KJS;

#ifndef KDE_USE_FINAL
#include "grammar.h"
#endif

#include "lookup.h"
#include "lexer.lut.h"

extern YYLTYPE kjsyylloc; // global bison variable holding token info

// a bridge for yacc from the C world to C++
int kjsyylex()
{
  return lexer().lex();
}

namespace KJS {

static bool isDecimalDigit(int);

static const size_t initialReadBufferCapacity = 32;
static const size_t initialStringTableCapacity = 64;

Lexer& lexer()
{
    ASSERT(JSLock::currentThreadIsHoldingLock());

    // FIXME: We'd like to avoid calling new here, but we don't currently 
    // support tearing down the Lexer at app quit time, since that would involve
    // tearing down its UString data members without holding the JSLock.
    static Lexer* staticLexer = new Lexer;
    return *staticLexer;
}

Lexer::Lexer()
    : yylineno(1)
    , restrKeyword(false)
    , eatNextIdentifier(false)
    , stackToken(-1)
    , lastToken(-1)
    , pos(0)
    , code(0)
    , length(0)
    , atLineStart(true)
    , current(0)
    , next1(0)
    , next2(0)
    , next3(0)
{
    m_buffer8.reserveCapacity(initialReadBufferCapacity);
    m_buffer16.reserveCapacity(initialReadBufferCapacity);
    m_strings.reserveCapacity(initialStringTableCapacity);
    m_identifiers.reserveCapacity(initialStringTableCapacity);
}

void Lexer::setCode(int startingLineNumber, const KJS::UChar *c, unsigned int len)
{
  yylineno = 1 + startingLineNumber;
  restrKeyword = false;
  delimited = false;
  eatNextIdentifier = false;
  stackToken = -1;
  lastToken = -1;
  pos = 0;
  code = c;
  length = len;
  skipLF = false;
  skipCR = false;
  error = false;
  atLineStart = true;

  // read first characters
  current = (length > 0) ? code[0].uc : -1;
  next1 = (length > 1) ? code[1].uc : -1;
  next2 = (length > 2) ? code[2].uc : -1;
  next3 = (length > 3) ? code[3].uc : -1;
}

void Lexer::shift(unsigned int p)
{
  // Here would be a good place to strip Cf characters, but that has caused compatibility problems:
  // <http://bugs.webkit.org/show_bug.cgi?id=10183>.
  while (p--) {
    pos++;
    current = next1;
    next1 = next2;
    next2 = next3;
    next3 = (pos + 3 < length) ? code[pos + 3].uc : -1;
  }
}

// called on each new line
void Lexer::nextLine()
{
  yylineno++;
  atLineStart = true;
}

void Lexer::setDone(State s)
{
  state = s;
  done = true;
}

int Lexer::lex()
{
  int token = 0;
  state = Start;
  unsigned short stringType = 0; // either single or double quotes
  m_buffer8.clear();
  m_buffer16.clear();
  done = false;
  terminator = false;
  skipLF = false;
  skipCR = false;

  // did we push a token on the stack previously ?
  // (after an automatic semicolon insertion)
  if (stackToken >= 0) {
    setDone(Other);
    token = stackToken;
    stackToken = 0;
  }

  while (!done) {
    if (skipLF && current != '\n') // found \r but not \n afterwards
        skipLF = false;
    if (skipCR && current != '\r') // found \n but not \r afterwards
        skipCR = false;
    if (skipLF || skipCR) // found \r\n or \n\r -> eat the second one
    {
        skipLF = false;
        skipCR = false;
        shift(1);
    }
    switch (state) {
    case Start:
      if (isWhiteSpace()) {
        // do nothing
      } else if (current == '/' && next1 == '/') {
        shift(1);
        state = InSingleLineComment;
      } else if (current == '/' && next1 == '*') {
        shift(1);
        state = InMultiLineComment;
      } else if (current == -1) {
        if (!terminator && !delimited) {
          // automatic semicolon insertion if program incomplete
          token = ';';
          stackToken = 0;
          setDone(Other);
        } else
          setDone(Eof);
      } else if (isLineTerminator()) {
        nextLine();
        terminator = true;
        if (restrKeyword) {
          token = ';';
          setDone(Other);
        }
      } else if (current == '"' || current == '\'') {
        state = InString;
        stringType = static_cast<unsigned short>(current);
      } else if (isIdentStart(current)) {
        record16(current);
        state = InIdentifierOrKeyword;
      } else if (current == '\\') {
        state = InIdentifierStartUnicodeEscapeStart;
      } else if (current == '0') {
        record8(current);
        state = InNum0;
      } else if (isDecimalDigit(current)) {
        record8(current);
        state = InNum;
      } else if (current == '.' && isDecimalDigit(next1)) {
        record8(current);
        state = InDecimal;
        // <!-- marks the beginning of a line comment (for www usage)
      } else if (current == '<' && next1 == '!' &&
                 next2 == '-' && next3 == '-') {
        shift(3);
        state = InSingleLineComment;
        // same for -->
      } else if (atLineStart && current == '-' && next1 == '-' &&  next2 == '>') {
        shift(2);
        state = InSingleLineComment;
      } else {
        token = matchPunctuator(current, next1, next2, next3);
        if (token != -1) {
          setDone(Other);
        } else {
          //      cerr << "encountered unknown character" << endl;
          setDone(Bad);
        }
      }
      break;
    case InString:
      if (current == stringType) {
        shift(1);
        setDone(String);
      } else if (isLineTerminator() || current == -1) {
        setDone(Bad);
      } else if (current == '\\') {
        state = InEscapeSequence;
      } else {
        record16(current);
      }
      break;
    // Escape Sequences inside of strings
    case InEscapeSequence:
      if (isOctalDigit(current)) {
        if (current >= '0' && current <= '3' &&
            isOctalDigit(next1) && isOctalDigit(next2)) {
          record16(convertOctal(current, next1, next2));
          shift(2);
          state = InString;
        } else if (isOctalDigit(current) && isOctalDigit(next1)) {
          record16(convertOctal('0', current, next1));
          shift(1);
          state = InString;
        } else if (isOctalDigit(current)) {
          record16(convertOctal('0', '0', current));
          state = InString;
        } else {
          setDone(Bad);
        }
      } else if (current == 'x')
        state = InHexEscape;
      else if (current == 'u')
        state = InUnicodeEscape;
      else if (isLineTerminator()) {
        nextLine();
        state = InString;
      } else {
        record16(singleEscape(static_cast<unsigned short>(current)));
        state = InString;
      }
      break;
    case InHexEscape:
      if (isHexDigit(current) && isHexDigit(next1)) {
        state = InString;
        record16(convertHex(current, next1));
        shift(1);
      } else if (current == stringType) {
        record16('x');
        shift(1);
        setDone(String);
      } else {
        record16('x');
        record16(current);
        state = InString;
      }
      break;
    case InUnicodeEscape:
      if (isHexDigit(current) && isHexDigit(next1) && isHexDigit(next2) && isHexDigit(next3)) {
        record16(convertUnicode(current, next1, next2, next3));
        shift(3);
        state = InString;
      } else if (current == stringType) {
        record16('u');
        shift(1);
        setDone(String);
      } else {
        setDone(Bad);
      }
      break;
    case InSingleLineComment:
      if (isLineTerminator()) {
        nextLine();
        terminator = true;
        if (restrKeyword) {
          token = ';';
          setDone(Other);
        } else
          state = Start;
      } else if (current == -1) {
        setDone(Eof);
      }
      break;
    case InMultiLineComment:
      if (current == -1) {
        setDone(Bad);
      } else if (isLineTerminator()) {
        nextLine();
      } else if (current == '*' && next1 == '/') {
        state = Start;
        shift(1);
      }
      break;
    case InIdentifierOrKeyword:
    case InIdentifier:
      if (isIdentPart(current))
        record16(current);
      else if (current == '\\')
        state = InIdentifierPartUnicodeEscapeStart;
      else
        setDone(state == InIdentifierOrKeyword ? IdentifierOrKeyword : Identifier);
      break;
    case InNum0:
      if (current == 'x' || current == 'X') {
        record8(current);
        state = InHex;
      } else if (current == '.') {
        record8(current);
        state = InDecimal;
      } else if (current == 'e' || current == 'E') {
        record8(current);
        state = InExponentIndicator;
      } else if (isOctalDigit(current)) {
        record8(current);
        state = InOctal;
      } else if (isDecimalDigit(current)) {
        record8(current);
        state = InDecimal;
      } else {
        setDone(Number);
      }
      break;
    case InHex:
      if (isHexDigit(current)) {
        record8(current);
      } else {
        setDone(Hex);
      }
      break;
    case InOctal:
      if (isOctalDigit(current)) {
        record8(current);
      }
      else if (isDecimalDigit(current)) {
        record8(current);
        state = InDecimal;
      } else
        setDone(Octal);
      break;
    case InNum:
      if (isDecimalDigit(current)) {
        record8(current);
      } else if (current == '.') {
        record8(current);
        state = InDecimal;
      } else if (current == 'e' || current == 'E') {
        record8(current);
        state = InExponentIndicator;
      } else
        setDone(Number);
      break;
    case InDecimal:
      if (isDecimalDigit(current)) {
        record8(current);
      } else if (current == 'e' || current == 'E') {
        record8(current);
        state = InExponentIndicator;
      } else
        setDone(Number);
      break;
    case InExponentIndicator:
      if (current == '+' || current == '-') {
        record8(current);
      } else if (isDecimalDigit(current)) {
        record8(current);
        state = InExponent;
      } else
        setDone(Bad);
      break;
    case InExponent:
      if (isDecimalDigit(current)) {
        record8(current);
      } else
        setDone(Number);
      break;
    case InIdentifierStartUnicodeEscapeStart:
      if (current == 'u')
        state = InIdentifierStartUnicodeEscape;
      else
        setDone(Bad);
      break;
    case InIdentifierPartUnicodeEscapeStart:
      if (current == 'u')
        state = InIdentifierPartUnicodeEscape;
      else
        setDone(Bad);
      break;
    case InIdentifierStartUnicodeEscape:
      if (!isHexDigit(current) || !isHexDigit(next1) || !isHexDigit(next2) || !isHexDigit(next3)) {
        setDone(Bad);
        break;
      }
      token = convertUnicode(current, next1, next2, next3).uc;
      shift(3);
      if (!isIdentStart(token)) {
        setDone(Bad);
        break;
      }
      record16(token);
      state = InIdentifier;
      break;
    case InIdentifierPartUnicodeEscape:
      if (!isHexDigit(current) || !isHexDigit(next1) || !isHexDigit(next2) || !isHexDigit(next3)) {
        setDone(Bad);
        break;
      }
      token = convertUnicode(current, next1, next2, next3).uc;
      shift(3);
      if (!isIdentPart(token)) {
        setDone(Bad);
        break;
      }
      record16(token);
      state = InIdentifier;
      break;
    default:
      ASSERT(!"Unhandled state in switch statement");
    }

    // move on to the next character
    if (!done)
      shift(1);
    if (state != Start && state != InSingleLineComment)
      atLineStart = false;
  }

  // no identifiers allowed directly after numeric literal, e.g. "3in" is bad
  if ((state == Number || state == Octal || state == Hex) && isIdentStart(current))
    state = Bad;

  // terminate string
  m_buffer8.append('\0');

#ifdef KJS_DEBUG_LEX
  fprintf(stderr, "line: %d ", lineNo());
  fprintf(stderr, "yytext (%x): ", m_buffer8[0]);
  fprintf(stderr, "%s ", buffer8.data());
#endif

  double dval = 0;
  if (state == Number) {
    dval = kjs_strtod(m_buffer8.data(), 0L);
  } else if (state == Hex) { // scan hex numbers
    const char* p = m_buffer8.data() + 2;
    while (char c = *p++) {
      dval *= 16;
      dval += convertHex(c);
    }

    if (dval >= mantissaOverflowLowerBound)
      dval = parseIntOverflow(m_buffer8.data() + 2, p - (m_buffer8.data() + 3), 16);

    state = Number;
  } else if (state == Octal) {   // scan octal number
    const char* p = m_buffer8.data() + 1;
    while (char c = *p++) {
      dval *= 8;
      dval += c - '0';
    }

    if (dval >= mantissaOverflowLowerBound)
      dval = parseIntOverflow(m_buffer8.data() + 1, p - (m_buffer8.data() + 2), 8);

    state = Number;
  }

#ifdef KJS_DEBUG_LEX
  switch (state) {
  case Eof:
    printf("(EOF)\n");
    break;
  case Other:
    printf("(Other)\n");
    break;
  case Identifier:
    printf("(Identifier)/(Keyword)\n");
    break;
  case String:
    printf("(String)\n");
    break;
  case Number:
    printf("(Number)\n");
    break;
  default:
    printf("(unknown)");
  }
#endif

  if (state != Identifier && eatNextIdentifier)
    eatNextIdentifier = false;

  restrKeyword = false;
  delimited = false;
  kjsyylloc.first_line = yylineno; // ???
  kjsyylloc.last_line = yylineno;

  switch (state) {
  case Eof:
    token = 0;
    break;
  case Other:
    if(token == '}' || token == ';') {
      delimited = true;
    }
    break;
  case IdentifierOrKeyword:
    if ((token = Lookup::find(&mainTable, m_buffer16.data(), m_buffer16.size())) < 0) {
  case Identifier:
      // Lookup for keyword failed, means this is an identifier
      // Apply anonymous-function hack below (eat the identifier)
      if (eatNextIdentifier) {
        eatNextIdentifier = false;
        token = lex();
        break;
      }
      kjsyylval.ident = makeIdentifier(m_buffer16);
      token = IDENT;
      break;
    }

    eatNextIdentifier = false;
    // Hack for "f = function somename() { ... }", too hard to get into the grammar
    if (token == FUNCTION && lastToken == '=' )
      eatNextIdentifier = true;

    if (token == CONTINUE || token == BREAK ||
        token == RETURN || token == THROW)
      restrKeyword = true;
    break;
  case String:
    kjsyylval.string = makeUString(m_buffer16);
    token = STRING;
    break;
  case Number:
    kjsyylval.doubleValue = dval;
    token = NUMBER;
    break;
  case Bad:
#ifdef KJS_DEBUG_LEX
    fprintf(stderr, "yylex: ERROR.\n");
#endif
    error = true;
    return -1;
  default:
    ASSERT(!"unhandled numeration value in switch");
    error = true;
    return -1;
  }
  lastToken = token;
  return token;
}

bool Lexer::isWhiteSpace() const
{
  return current == '\t' || current == 0x0b || current == 0x0c || isSeparatorSpace(current);
}

bool Lexer::isLineTerminator()
{
  bool cr = (current == '\r');
  bool lf = (current == '\n');
  if (cr)
      skipLF = true;
  else if (lf)
      skipCR = true;
  return cr || lf || current == 0x2028 || current == 0x2029;
}

bool Lexer::isIdentStart(int c)
{
  return (category(c) & (Letter_Uppercase | Letter_Lowercase | Letter_Titlecase | Letter_Modifier | Letter_Other))
    || c == '$' || c == '_';
}

bool Lexer::isIdentPart(int c)
{
  return (category(c) & (Letter_Uppercase | Letter_Lowercase | Letter_Titlecase | Letter_Modifier | Letter_Other
        | Mark_NonSpacing | Mark_SpacingCombining | Number_DecimalDigit | Punctuation_Connector))
    || c == '$' || c == '_';
}

static bool isDecimalDigit(int c)
{
  return (c >= '0' && c <= '9');
}

bool Lexer::isHexDigit(int c)
{
  return (c >= '0' && c <= '9' ||
          c >= 'a' && c <= 'f' ||
          c >= 'A' && c <= 'F');
}

bool Lexer::isOctalDigit(int c)
{
  return (c >= '0' && c <= '7');
}

int Lexer::matchPunctuator(int c1, int c2, int c3, int c4)
{
  if (c1 == '>' && c2 == '>' && c3 == '>' && c4 == '=') {
    shift(4);
    return URSHIFTEQUAL;
  } else if (c1 == '=' && c2 == '=' && c3 == '=') {
    shift(3);
    return STREQ;
  } else if (c1 == '!' && c2 == '=' && c3 == '=') {
    shift(3);
    return STRNEQ;
   } else if (c1 == '>' && c2 == '>' && c3 == '>') {
    shift(3);
    return URSHIFT;
  } else if (c1 == '<' && c2 == '<' && c3 == '=') {
    shift(3);
    return LSHIFTEQUAL;
  } else if (c1 == '>' && c2 == '>' && c3 == '=') {
    shift(3);
    return RSHIFTEQUAL;
  } else if (c1 == '<' && c2 == '=') {
    shift(2);
    return LE;
  } else if (c1 == '>' && c2 == '=') {
    shift(2);
    return GE;
  } else if (c1 == '!' && c2 == '=') {
    shift(2);
    return NE;
  } else if (c1 == '+' && c2 == '+') {
    shift(2);
    if (terminator)
      return AUTOPLUSPLUS;
    else
      return PLUSPLUS;
  } else if (c1 == '-' && c2 == '-') {
    shift(2);
    if (terminator)
      return AUTOMINUSMINUS;
    else
      return MINUSMINUS;
  } else if (c1 == '=' && c2 == '=') {
    shift(2);
    return EQEQ;
  } else if (c1 == '+' && c2 == '=') {
    shift(2);
    return PLUSEQUAL;
  } else if (c1 == '-' && c2 == '=') {
    shift(2);
    return MINUSEQUAL;
  } else if (c1 == '*' && c2 == '=') {
    shift(2);
    return MULTEQUAL;
  } else if (c1 == '/' && c2 == '=') {
    shift(2);
    return DIVEQUAL;
  } else if (c1 == '&' && c2 == '=') {
    shift(2);
    return ANDEQUAL;
  } else if (c1 == '^' && c2 == '=') {
    shift(2);
    return XOREQUAL;
  } else if (c1 == '%' && c2 == '=') {
    shift(2);
    return MODEQUAL;
  } else if (c1 == '|' && c2 == '=') {
    shift(2);
    return OREQUAL;
  } else if (c1 == '<' && c2 == '<') {
    shift(2);
    return LSHIFT;
  } else if (c1 == '>' && c2 == '>') {
    shift(2);
    return RSHIFT;
  } else if (c1 == '&' && c2 == '&') {
    shift(2);
    return AND;
  } else if (c1 == '|' && c2 == '|') {
    shift(2);
    return OR;
  }

  switch(c1) {
    case '=':
    case '>':
    case '<':
    case ',':
    case '!':
    case '~':
    case '?':
    case ':':
    case '.':
    case '+':
    case '-':
    case '*':
    case '/':
    case '&':
    case '|':
    case '^':
    case '%':
    case '(':
    case ')':
    case '{':
    case '}':
    case '[':
    case ']':
    case ';':
      shift(1);
      return static_cast<int>(c1);
    default:
      return -1;
  }
}

unsigned short Lexer::singleEscape(unsigned short c)
{
  switch(c) {
  case 'b':
    return 0x08;
  case 't':
    return 0x09;
  case 'n':
    return 0x0A;
  case 'v':
    return 0x0B;
  case 'f':
    return 0x0C;
  case 'r':
    return 0x0D;
  case '"':
    return 0x22;
  case '\'':
    return 0x27;
  case '\\':
    return 0x5C;
  default:
    return c;
  }
}

unsigned short Lexer::convertOctal(int c1, int c2, int c3)
{
  return static_cast<unsigned short>((c1 - '0') * 64 + (c2 - '0') * 8 + c3 - '0');
}

unsigned char Lexer::convertHex(int c)
{
  if (c >= '0' && c <= '9')
    return static_cast<unsigned char>(c - '0');
  if (c >= 'a' && c <= 'f')
    return static_cast<unsigned char>(c - 'a' + 10);
  return static_cast<unsigned char>(c - 'A' + 10);
}

unsigned char Lexer::convertHex(int c1, int c2)
{
  return ((convertHex(c1) << 4) + convertHex(c2));
}

KJS::UChar Lexer::convertUnicode(int c1, int c2, int c3, int c4)
{
  return KJS::UChar((convertHex(c1) << 4) + convertHex(c2),
               (convertHex(c3) << 4) + convertHex(c4));
}

void Lexer::record8(int c)
{
    ASSERT(c >= 0);
    ASSERT(c <= 0xff);
    m_buffer8.append(static_cast<char>(c));
}

void Lexer::record16(int c)
{
    ASSERT(c >= 0);
    ASSERT(c <= USHRT_MAX);
    record16(UChar(static_cast<unsigned short>(c)));
}

void Lexer::record16(KJS::UChar c)
{
    m_buffer16.append(c);
}

bool Lexer::scanRegExp()
{
  m_buffer16.clear();
  bool lastWasEscape = false;
  bool inBrackets = false;

  while (1) {
    if (isLineTerminator() || current == -1)
      return false;
    else if (current != '/' || lastWasEscape == true || inBrackets == true)
    {
        // keep track of '[' and ']'
        if (!lastWasEscape) {
          if ( current == '[' && !inBrackets )
            inBrackets = true;
          if ( current == ']' && inBrackets )
            inBrackets = false;
        }
        record16(current);
        lastWasEscape =
            !lastWasEscape && (current == '\\');
    } else { // end of regexp
        m_pattern = UString(m_buffer16);
        m_buffer16.clear();
        shift(1);
        break;
    }
    shift(1);
  }

  while (isIdentPart(current)) {
    record16(current);
    shift(1);
  }
  m_flags = UString(m_buffer16);

  return true;
}

void Lexer::clear()
{
    deleteAllValues(m_strings);
    Vector<UString*> newStrings;
    newStrings.reserveCapacity(initialStringTableCapacity);
    m_strings.swap(newStrings);

    deleteAllValues(m_identifiers);
    Vector<KJS::Identifier*> newIdentifiers;
    newIdentifiers.reserveCapacity(initialStringTableCapacity);
    m_identifiers.swap(newIdentifiers);

    Vector<char> newBuffer8;
    newBuffer8.reserveCapacity(initialReadBufferCapacity);
    m_buffer8.swap(newBuffer8);

    Vector<UChar> newBuffer16;
    newBuffer16.reserveCapacity(initialReadBufferCapacity);
    m_buffer16.swap(newBuffer16);

    m_pattern = 0;
    m_flags = 0;
}

Identifier* Lexer::makeIdentifier(const Vector<KJS::UChar>& buffer)
{
    KJS::Identifier* identifier = new KJS::Identifier(buffer.data(), buffer.size());
    m_identifiers.append(identifier);
    return identifier;
}
 
UString* Lexer::makeUString(const Vector<KJS::UChar>& buffer)
{
    UString* string = new UString(buffer);
    m_strings.append(string);
    return string;
}

} // namespace KJS
