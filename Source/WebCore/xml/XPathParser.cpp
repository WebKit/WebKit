/*
 * Copyright 2005 Maksim Orlovich <maksim@kde.org>
 * Copyright (C) 2006, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "XPathParser.h"

#include "ExceptionCode.h"
#include "XPathEvaluator.h"
#include "XPathException.h"
#include "XPathNSResolver.h"
#include "XPathPath.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/StringHash.h>

using namespace WebCore;
using namespace XPath;

extern int xpathyyparse(Parser&);

#include "XPathGrammar.h"

namespace WebCore {
namespace XPath {

struct Parser::Token {
    int type;
    String string;
    Step::Axis axis;
    NumericOp::Opcode numericOpcode;
    EqTestOp::Opcode equalityTestOpcode;

    Token(int type) : type(type) { }
    Token(int type, const String& string) : type(type), string(string) { }
    Token(int type, Step::Axis axis) : type(type), axis(axis) { }
    Token(int type, NumericOp::Opcode opcode) : type(type), numericOpcode(opcode) { }
    Token(int type, EqTestOp::Opcode opcode) : type(type), equalityTestOpcode(opcode) { }
};

enum XMLCat { NameStart, NameCont, NotPartOfName };

static XMLCat charCat(UChar character)
{
    if (character == '_')
        return NameStart;

    if (character == '.' || character == '-')
        return NameCont;
    unsigned characterTypeMask = U_GET_GC_MASK(character);
    if (characterTypeMask & (U_GC_LU_MASK | U_GC_LL_MASK | U_GC_LO_MASK | U_GC_LT_MASK | U_GC_NL_MASK))
        return NameStart;
    if (characterTypeMask & (U_GC_M_MASK | U_GC_LM_MASK | U_GC_ND_MASK))
        return NameCont;
    return NotPartOfName;
}

static void populateAxisNamesMap(HashMap<String, Step::Axis>& axisNames)
{
    struct AxisName {
        const char* name;
        Step::Axis axis;
    };
    const AxisName axisNameList[] = {
        { "ancestor", Step::AncestorAxis },
        { "ancestor-or-self", Step::AncestorOrSelfAxis },
        { "attribute", Step::AttributeAxis },
        { "child", Step::ChildAxis },
        { "descendant", Step::DescendantAxis },
        { "descendant-or-self", Step::DescendantOrSelfAxis },
        { "following", Step::FollowingAxis },
        { "following-sibling", Step::FollowingSiblingAxis },
        { "namespace", Step::NamespaceAxis },
        { "parent", Step::ParentAxis },
        { "preceding", Step::PrecedingAxis },
        { "preceding-sibling", Step::PrecedingSiblingAxis },
        { "self", Step::SelfAxis }
    };
    for (unsigned i = 0; i < WTF_ARRAY_LENGTH(axisNameList); ++i)
        axisNames.add(axisNameList[i].name, axisNameList[i].axis);
}

static bool parseAxisName(const String& name, Step::Axis& type)
{
    static NeverDestroyed<HashMap<String, Step::Axis>> axisNames;
    if (axisNames.get().isEmpty())
        populateAxisNamesMap(axisNames);

    auto it = axisNames.get().find(name);
    if (it == axisNames.get().end())
        return false;
    type = it->value;
    return true;
}

// Returns whether the current token can possibly be a binary operator, given
// the previous token. Necessary to disambiguate some of the operators
// (* (multiply), div, and, or, mod) in the [32] Operator rule
// (check http://www.w3.org/TR/xpath#exprlex).
bool Parser::isBinaryOperatorContext() const
{
    switch (m_lastTokenType) {
    case 0:
    case '@': case AXISNAME: case '(': case '[': case ',':
    case AND: case OR: case MULOP:
    case '/': case SLASHSLASH: case '|': case PLUS: case MINUS:
    case EQOP: case RELOP:
        return false;
    default:
        return true;
    }
}

void Parser::skipWS()
{
    while (m_nextPos < m_data.length() && isSpaceOrNewline(m_data[m_nextPos]))
        ++m_nextPos;
}

Parser::Token Parser::makeTokenAndAdvance(int code, int advance)
{
    m_nextPos += advance;
    return Token(code);
}

Parser::Token Parser::makeTokenAndAdvance(int code, NumericOp::Opcode val, int advance)
{
    m_nextPos += advance;
    return Token(code, val);
}

Parser::Token Parser::makeTokenAndAdvance(int code, EqTestOp::Opcode val, int advance)
{
    m_nextPos += advance;
    return Token(code, val);
}

// Returns next char if it's there and interesting, 0 otherwise.
char Parser::peekAheadHelper()
{
    if (m_nextPos + 1 >= m_data.length())
        return 0;
    UChar next = m_data[m_nextPos + 1];
    if (next >= 0xff)
        return 0;
    return next;
}

char Parser::peekCurHelper()
{
    if (m_nextPos >= m_data.length())
        return 0;
    UChar next = m_data[m_nextPos];
    if (next >= 0xff)
        return 0;
    return next;
}

Parser::Token Parser::lexString()
{
    UChar delimiter = m_data[m_nextPos];
    int startPos = m_nextPos + 1;

    for (m_nextPos = startPos; m_nextPos < m_data.length(); ++m_nextPos) {
        if (m_data[m_nextPos] == delimiter) {
            String value = m_data.substring(startPos, m_nextPos - startPos);
            if (value.isNull())
                value = "";
            ++m_nextPos; // Consume the char.
            return Token(LITERAL, value);
        }
    }

    // Ouch, went off the end -- report error.
    return Token(XPATH_ERROR);
}

Parser::Token Parser::lexNumber()
{
    int startPos = m_nextPos;
    bool seenDot = false;

    // Go until end or a non-digits character.
    for (; m_nextPos < m_data.length(); ++m_nextPos) {
        UChar aChar = m_data[m_nextPos];
        if (aChar >= 0xff) break;

        if (aChar < '0' || aChar > '9') {
            if (aChar == '.' && !seenDot)
                seenDot = true;
            else
                break;
        }
    }

    return Token(NUMBER, m_data.substring(startPos, m_nextPos - startPos));
}

bool Parser::lexNCName(String& name)
{
    int startPos = m_nextPos;
    if (m_nextPos >= m_data.length())
        return false;

    if (charCat(m_data[m_nextPos]) != NameStart)
        return false;

    // Keep going until we get a character that's not good for names.
    for (; m_nextPos < m_data.length(); ++m_nextPos)
        if (charCat(m_data[m_nextPos]) == NotPartOfName)
            break;

    name = m_data.substring(startPos, m_nextPos - startPos);
    return true;
}

bool Parser::lexQName(String& name)
{
    String n1;
    if (!lexNCName(n1))
        return false;

    skipWS();

    // If the next character is :, what we just got it the prefix, if not,
    // it's the whole thing.
    if (peekAheadHelper() != ':') {
        name = n1;
        return true;
    }

    String n2;
    if (!lexNCName(n2))
        return false;

    name = n1 + ":" + n2;
    return true;
}

inline Parser::Token Parser::nextTokenInternal()
{
    skipWS();

    if (m_nextPos >= m_data.length())
        return Token(0);

    char code = peekCurHelper();
    switch (code) {
    case '(': case ')': case '[': case ']':
    case '@': case ',': case '|':
        return makeTokenAndAdvance(code);
    case '\'':
    case '\"':
        return lexString();
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
        return lexNumber();
    case '.': {
        char next = peekAheadHelper();
        if (next == '.')
            return makeTokenAndAdvance(DOTDOT, 2);
        if (next >= '0' && next <= '9')
            return lexNumber();
        return makeTokenAndAdvance('.');
    }
    case '/':
        if (peekAheadHelper() == '/')
            return makeTokenAndAdvance(SLASHSLASH, 2);
        return makeTokenAndAdvance('/');
    case '+':
        return makeTokenAndAdvance(PLUS);
    case '-':
        return makeTokenAndAdvance(MINUS);
    case '=':
        return makeTokenAndAdvance(EQOP, EqTestOp::OP_EQ);
    case '!':
        if (peekAheadHelper() == '=')
            return makeTokenAndAdvance(EQOP, EqTestOp::OP_NE, 2);
        return Token(XPATH_ERROR);
    case '<':
        if (peekAheadHelper() == '=')
            return makeTokenAndAdvance(RELOP, EqTestOp::OP_LE, 2);
        return makeTokenAndAdvance(RELOP, EqTestOp::OP_LT);
    case '>':
        if (peekAheadHelper() == '=')
            return makeTokenAndAdvance(RELOP, EqTestOp::OP_GE, 2);
        return makeTokenAndAdvance(RELOP, EqTestOp::OP_GT);
    case '*':
        if (isBinaryOperatorContext())
            return makeTokenAndAdvance(MULOP, NumericOp::OP_Mul);
        ++m_nextPos;
        return Token(NAMETEST, "*");
    case '$': { // $ QName
        m_nextPos++;
        String name;
        if (!lexQName(name))
            return Token(XPATH_ERROR);
        return Token(VARIABLEREFERENCE, name);
    }
    }

    String name;
    if (!lexNCName(name))
        return Token(XPATH_ERROR);

    skipWS();
    // If we're in an operator context, check for any operator names
    if (isBinaryOperatorContext()) {
        if (name == "and") //### hash?
            return Token(AND);
        if (name == "or")
            return Token(OR);
        if (name == "mod")
            return Token(MULOP, NumericOp::OP_Mod);
        if (name == "div")
            return Token(MULOP, NumericOp::OP_Div);
    }

    // See whether we are at a :
    if (peekCurHelper() == ':') {
        m_nextPos++;
        // Any chance it's an axis name?
        if (peekCurHelper() == ':') {
            m_nextPos++;
            
            //It might be an axis name.
            Step::Axis axis;
            if (parseAxisName(name, axis))
                return Token(AXISNAME, axis);
            // Ugh, :: is only valid in axis names -> error
            return Token(XPATH_ERROR);
        }

        // Seems like this is a fully qualified qname, or perhaps the * modified one from NameTest
        skipWS();
        if (peekCurHelper() == '*') {
            m_nextPos++;
            return Token(NAMETEST, name + ":*");
        }
        
        // Make a full qname.
        String n2;
        if (!lexNCName(n2))
            return Token(XPATH_ERROR);
        
        name = name + ":" + n2;
    }

    skipWS();

    if (peekCurHelper() == '(') {
        // note: we don't swallow the '(' here!

        // Either node type oor function name.

        if (name == "processing-instruction")
            return Token(PI);
        if (name == "node")
            return Token(NODE);
        if (name == "text")
            return Token(TEXT);
        if (name == "comment")
            return Token(COMMENT);

        return Token(FUNCTIONNAME, name);
    }

    // At this point, it must be NAMETEST.
    return Token(NAMETEST, name);
}

inline Parser::Token Parser::nextToken()
{
    Token token = nextTokenInternal();
    m_lastTokenType = token.type;
    return token;
}

Parser::Parser(const String& statement, XPathNSResolver* resolver)
    : m_data(statement)
    , m_resolver(resolver)
    , m_nextPos(0)
    , m_lastTokenType(0)
    , m_sawNamespaceError(false)
{
}

int Parser::lex(YYSTYPE& yylval)
{
    Token token = nextToken();

    switch (token.type) {
    case AXISNAME:
        yylval.axis = token.axis;
        break;
    case MULOP:
        yylval.numericOpcode = token.numericOpcode;
        break;
    case RELOP:
    case EQOP:
        yylval.equalityTestOpcode = token.equalityTestOpcode;
        break;
    case NODETYPE:
    case FUNCTIONNAME:
    case LITERAL:
    case VARIABLEREFERENCE:
    case NUMBER:
    case NAMETEST:
        yylval.string = token.string.releaseImpl().leakRef();
        break;
    }

    return token.type;
}

bool Parser::expandQualifiedName(const String& qualifiedName, String& localName, String& namespaceURI)
{
    size_t colon = qualifiedName.find(':');
    if (colon != notFound) {
        if (!m_resolver) {
            m_sawNamespaceError = true;
            return false;
        }
        namespaceURI = m_resolver->lookupNamespaceURI(qualifiedName.left(colon));
        if (namespaceURI.isNull()) {
            m_sawNamespaceError = true;
            return false;
        }
        localName = qualifiedName.substring(colon + 1);
    } else
        localName = qualifiedName;

    return true;
}

std::unique_ptr<Expression> Parser::parseStatement(const String& statement, XPathNSResolver* resolver, ExceptionCode& ec)
{
    Parser parser(statement, resolver);

    int parseError = xpathyyparse(parser);

    if (parser.m_sawNamespaceError) {
        ec = NAMESPACE_ERR;
        return nullptr;
    }

    if (parseError) {
        ec = XPathException::INVALID_EXPRESSION_ERR;
        return nullptr;
    }

    return std::move(parser.m_result);
}

} }
