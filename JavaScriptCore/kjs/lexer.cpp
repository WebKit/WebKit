// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
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
 *  the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "value.h"
#include "object.h"
#include "types.h"
#include "interpreter.h"
#include "nodes.h"
#include "lexer.h"
#include "identifier.h"
#include "lookup.h"
#include "internal.h"

// we can't specify the namespace in yacc's C output, so do it here
using namespace KJS;

static Lexer *currLexer = 0;

#ifndef KDE_USE_FINAL
#include "grammar.h"
#endif

#include "lexer.lut.h"

extern YYLTYPE kjsyylloc; // global bison variable holding token info

// a bridge for yacc from the C world to C++
int kjsyylex()
{
  return Lexer::curr()->lex();
}

Lexer::Lexer()
  : yylineno(1),
    size8(128), size16(128), restrKeyword(false),
    eatNextIdentifier(false), stackToken(-1), lastToken(-1), pos(0),
    code(0), length(0),
#ifndef KJS_PURE_ECMA
    bol(true),
#endif
    current(0), next1(0), next2(0), next3(0),
    strings(0), numStrings(0), stringsCapacity(0),
    identifiers(0), numIdentifiers(0), identifiersCapacity(0)
{
  // allocate space for read buffers
  buffer8 = new char[size8];
  buffer16 = new UChar[size16];
  currLexer = this;
}

Lexer::~Lexer()
{
  doneParsing();
  delete [] buffer8;
  delete [] buffer16;
}

Lexer *Lexer::curr()
{
  if (!currLexer) {
    // create singleton instance
    currLexer = new Lexer();
  }
  return currLexer;
}

#ifdef KJS_DEBUG_MEM
void Lexer::globalClear()
{
  delete currLexer;
  currLexer = 0L;
}
#endif

void Lexer::setCode(const UString &sourceURL, int startingLineNumber, const UChar *c, unsigned int len)
{
  yylineno = 1 + startingLineNumber;
  m_sourceURL = sourceURL;
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
#ifndef KJS_PURE_ECMA
  bol = true;
#endif

  // read first characters
  current = (length > 0) ? code[0].uc : 0;
  next1 = (length > 1) ? code[1].uc : 0;
  next2 = (length > 2) ? code[2].uc : 0;
  next3 = (length > 3) ? code[3].uc : 0;
}

void Lexer::shift(unsigned int p)
{
  while (p--) {
    pos++;
    current = next1;
    next1 = next2;
    next2 = next3;
    next3 = (pos + 3 < length) ? code[pos+3].uc : 0;
  }
}

// called on each new line
void Lexer::nextLine()
{
  yylineno++;
#ifndef KJS_PURE_ECMA
  bol = true;
#endif
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
  pos8 = pos16 = 0;
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
      } else if (current == 0) {
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
        stringType = current;
      } else if (isIdentLetter(current)) {
        record16(current);
        state = InIdentifier;
      } else if (current == '0') {
        record8(current);
        state = InNum0;
      } else if (isDecimalDigit(current)) {
        record8(current);
        state = InNum;
      } else if (current == '.' && isDecimalDigit(next1)) {
        record8(current);
        state = InDecimal;
#ifndef KJS_PURE_ECMA
        // <!-- marks the beginning of a line comment (for www usage)
      } else if (current == '<' && next1 == '!' &&
                 next2 == '-' && next3 == '-') {
        shift(3);
        state = InSingleLineComment;
        // same for -->
      } else if (bol && current == '-' && next1 == '-' &&  next2 == '>') {
        shift(2);
        state = InSingleLineComment;
#endif
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
      } else if (current == 0 || isLineTerminator()) {
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
        record16(singleEscape(current));
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
      if (isHexDigit(current) && isHexDigit(next1) &&
          isHexDigit(next2) && isHexDigit(next3)) {
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
      } else if (current == 0) {
        setDone(Eof);
      }
      break;
    case InMultiLineComment:
      if (current == 0) {
        setDone(Bad);
      } else if (isLineTerminator()) {
        nextLine();
      } else if (current == '*' && next1 == '/') {
        state = Start;
        shift(1);
      }
      break;
    case InIdentifier:
      if (isIdentLetter(current) || isDecimalDigit(current)) {
        record16(current);
        break;
      }
      setDone(Identifier);
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
    default:
      assert(!"Unhandled state in switch statement");
    }

    // move on to the next character
    if (!done)
      shift(1);
#ifndef KJS_PURE_ECMA
    if (state != Start && state != InSingleLineComment)
      bol = false;
#endif
  }

  // no identifiers allowed directly after numeric literal, e.g. "3in" is bad
  if ((state == Number || state == Octal || state == Hex)
      && isIdentLetter(current))
    state = Bad;

  // terminate string
  buffer8[pos8] = '\0';

#ifdef KJS_DEBUG_LEX
  fprintf(stderr, "line: %d ", lineNo());
  fprintf(stderr, "yytext (%x): ", buffer8[0]);
  fprintf(stderr, "%s ", buffer8);
#endif

  double dval = 0;
  if (state == Number) {
    dval = strtod(buffer8, 0L);
  } else if (state == Hex) { // scan hex numbers
    const char *p = buffer8 + 2;
    while (char c = *p++) {
      dval *= 16;
      dval += convertHex(c);
    }
    state = Number;
  } else if (state == Octal) {   // scan octal number
    const char *p = buffer8 + 1;
    while (char c = *p++) {
      dval *= 8;
      dval += c - '0';
    }
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
  case Identifier:
    if ((token = Lookup::find(&mainTable, buffer16, pos16)) < 0) {
      // Lookup for keyword failed, means this is an identifier
      // Apply anonymous-function hack below (eat the identifier)
      if (eatNextIdentifier) {
        eatNextIdentifier = false;
        token = lex();
        break;
      }
      kjsyylval.ident = makeIdentifier(buffer16, pos16);
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
    kjsyylval.ustr = makeUString(buffer16, pos16);
    token = STRING;
    break;
  case Number:
    kjsyylval.dval = dval;
    token = NUMBER;
    break;
  case Bad:
    fprintf(stderr, "yylex: ERROR.\n");
    error = true;
    return -1;
  default:
    assert(!"unhandled numeration value in switch");
    error = true;
    return -1;
  }
  lastToken = token;
  return token;
}

bool Lexer::isWhiteSpace() const
{
  return (current == ' ' || current == '\t' ||
          current == 0x0b || current == 0x0c || current == 0xa0);
}

bool Lexer::isLineTerminator()
{
  bool cr = (current == '\r');
  bool lf = (current == '\n');
  if (cr)
      skipLF = true;
  else if (lf)
      skipCR = true;
  return cr || lf;
}

bool Lexer::isIdentLetter(unsigned short c)
{
  /* TODO: allow other legitimate unicode chars */
  return (c >= 'a' && c <= 'z' ||
          c >= 'A' && c <= 'Z' ||
          c == '$' || c == '_');
}

bool Lexer::isDecimalDigit(unsigned short c)
{
  return (c >= '0' && c <= '9');
}

bool Lexer::isHexDigit(unsigned short c)
{
  return (c >= '0' && c <= '9' ||
          c >= 'a' && c <= 'f' ||
          c >= 'A' && c <= 'F');
}

bool Lexer::isOctalDigit(unsigned short c) const
{
  return (c >= '0' && c <= '7');
}

int Lexer::matchPunctuator(unsigned short c1, unsigned short c2,
                              unsigned short c3, unsigned short c4)
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

unsigned short Lexer::singleEscape(unsigned short c) const
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

unsigned short Lexer::convertOctal(unsigned short c1, unsigned short c2,
                                      unsigned short c3) const
{
  return ((c1 - '0') * 64 + (c2 - '0') * 8 + c3 - '0');
}

unsigned char Lexer::convertHex(unsigned short c)
{
  if (c >= '0' && c <= '9')
    return (c - '0');
  else if (c >= 'a' && c <= 'f')
    return (c - 'a' + 10);
  else
    return (c - 'A' + 10);
}

unsigned char Lexer::convertHex(unsigned short c1, unsigned short c2)
{
  return ((convertHex(c1) << 4) + convertHex(c2));
}

UChar Lexer::convertUnicode(unsigned short c1, unsigned short c2,
                                     unsigned short c3, unsigned short c4)
{
  return UChar((convertHex(c1) << 4) + convertHex(c2),
               (convertHex(c3) << 4) + convertHex(c4));
}

void Lexer::record8(unsigned short c)
{
  assert(c <= 0xff);

  // enlarge buffer if full
  if (pos8 >= size8 - 1) {
    char *tmp = new char[2 * size8];
    memcpy(tmp, buffer8, size8 * sizeof(char));
    delete [] buffer8;
    buffer8 = tmp;
    size8 *= 2;
  }

  buffer8[pos8++] = (char) c;
}

void Lexer::record16(UChar c)
{
  // enlarge buffer if full
  if (pos16 >= size16 - 1) {
    UChar *tmp = new UChar[2 * size16];
    memcpy(tmp, buffer16, size16 * sizeof(UChar));
    delete [] buffer16;
    buffer16 = tmp;
    size16 *= 2;
  }

  buffer16[pos16++] = c;
}

bool Lexer::scanRegExp()
{
  pos16 = 0;
  bool lastWasEscape = false;
  bool inBrackets = false;

  while (1) {
    if (isLineTerminator() || current == 0)
      return false;
    else if (current != '/' || lastWasEscape == true || inBrackets == true)
    {
        // keep track of '[' and ']'
        if ( !lastWasEscape ) {
          if ( current == '[' && !inBrackets )
            inBrackets = true;
          if ( current == ']' && inBrackets )
            inBrackets = false;
        }
        record16(current);
        lastWasEscape =
            !lastWasEscape && (current == '\\');
    }
    else { // end of regexp
      pattern = UString(buffer16, pos16);
      pos16 = 0;
      shift(1);
      break;
    }
    shift(1);
  }

  while (isIdentLetter(current)) {
    record16(current);
    shift(1);
  }
  flags = UString(buffer16, pos16);

  return true;
}


void Lexer::doneParsing()
{
  for (unsigned i = 0; i < numIdentifiers; i++) {
    delete identifiers[i];
  }
  fastFree(identifiers);
  identifiers = 0;
  numIdentifiers = 0;
  identifiersCapacity = 0;

  for (unsigned i = 0; i < numStrings; i++) {
    delete strings[i];
  }
  fastFree(strings);
  strings = 0;
  numStrings = 0;
  stringsCapacity = 0;
}

const int initialCapacity = 64;
const int growthFactor = 2;

Identifier *Lexer::makeIdentifier(UChar *buffer, unsigned int pos)
{
  if (numIdentifiers == identifiersCapacity) {
    identifiersCapacity = (identifiersCapacity == 0) ? initialCapacity : identifiersCapacity *growthFactor;
    identifiers = (KJS::Identifier **)fastRealloc(identifiers, sizeof(KJS::Identifier *) * identifiersCapacity);
  }

  KJS::Identifier *identifier = new KJS::Identifier(buffer16, pos16);
  identifiers[numIdentifiers++] = identifier;
  return identifier;
}
 
UString *Lexer::makeUString(UChar *buffer, unsigned int pos)
{
  if (numStrings == stringsCapacity) {
    stringsCapacity = (stringsCapacity == 0) ? initialCapacity : stringsCapacity *growthFactor;
    strings = (UString **)fastRealloc(strings, sizeof(UString *) * stringsCapacity);
  }

  UString *string = new UString(buffer16, pos16);
  strings[numStrings++] = string;
  return string;
}
