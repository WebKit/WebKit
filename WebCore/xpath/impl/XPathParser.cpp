/*
 * Copyright 2005 Maksim Orlovich <maksim@kde.org>
 * Copyright (C) 2006 Apple Computer, Inc.
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

#if XPATH_SUPPORT

#include "XPathParser.h"

#include "DeprecatedString.h"
#include "XPathEvaluator.h"

namespace WebCore {
namespace XPath {

#include "XPathGrammar.h"    

struct AxisNameMapping {
    const char* name;
    Step::AxisType type;
};

static AxisNameMapping axisNames[] = {
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

static unsigned axisNamesCount = sizeof(axisNames) / sizeof(axisNames[0]);
static const char* const nodeTypeNames[] = {
    "comment",
    "text",
    "processing-instruction",
    "node",
    0
};

HashMap<String, Step::AxisType>* Parser::s_axisNamesDict = 0;
HashSet<String>* Parser::s_nodeTypeNamesDict = 0;

Parser* Parser::currentParser = 0;

Parser::XMLCat Parser::charCat(UChar aChar)
{
    //### might need to add some special cases from the XML spec.

    if (aChar == '_')
        return NameStart;

    if (aChar == '.' || aChar == '-')
        return NameCont;
    switch (u_charType(aChar)) {
        case U_LOWERCASE_LETTER: //Ll
        case U_UPPERCASE_LETTER: //Lu
        case U_OTHER_LETTER:     //Lo
        case U_TITLECASE_LETTER: //Lt
        case U_LETTER_NUMBER:    //Nl
            return NameStart;
        case U_COMBINING_SPACING_MARK: //Mc
        case U_ENCLOSING_MARK:         //Me
        case U_NON_SPACING_MARK:       //Mn
        case U_MODIFIER_LETTER:        //Lm
        case U_DECIMAL_DIGIT_NUMBER:   //Nd
            return NameCont;
        default:
            return NotPartOfName;
    }
}

bool Parser::isAxisName(const String& name, Step::AxisType& type)
{
    if (!s_axisNamesDict) {
        s_axisNamesDict = new HashMap<String, Step::AxisType>;
        for (unsigned p = 0; p < axisNamesCount; ++p)
            s_axisNamesDict->set(axisNames[p].name, axisNames[p].type);
    }

    HashMap<String, Step::AxisType>::iterator it = s_axisNamesDict->find(name);

    if (it == s_axisNamesDict->end())
        return false;
    type = it->second;
    return true;
}

bool Parser::isNodeTypeName(const String& name)
{
    if (!s_nodeTypeNamesDict) {
        s_nodeTypeNamesDict = new HashSet<String>;
        for (int p = 0; nodeTypeNames[p]; ++p)
            s_nodeTypeNamesDict->add(nodeTypeNames[p]);
    }
    return s_nodeTypeNamesDict->contains(String(name));
}

/* Returns whether the last parsed token matches the [32] Operator rule
 * (check http://www.w3.org/TR/xpath#exprlex). Necessary to disambiguate
 * the tokens.
 */
bool Parser::isOperatorContext()
{
    if (m_nextPos == 0)
        return false;

    switch (m_lastTokenType) {
        case AND: case OR: case MULOP:
        case '/': case SLASHSLASH: case '|': case PLUS: case MINUS:
        case EQOP: case RELOP:
        case '@': case AXISNAME:   case '(': case '[':
            return false;
        default:
            return true;
    }
}

void Parser::skipWS()
{
    while (m_nextPos < m_data.length() && QChar(m_data[m_nextPos]).isSpace())
        ++m_nextPos;
}

Token Parser::makeTokenAndAdvance(int code, int advance)
{
    m_nextPos += advance;
    return Token(code);
}

Token Parser::makeIntTokenAndAdvance(int code, int val, int advance)
{
    m_nextPos += advance;
    return Token(code, val);
}

// Returns next char if it's there and interesting, 0 otherwise
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

Token Parser::lexString()
{
    UChar delimiter = m_data[m_nextPos];
    int   startPos  = m_nextPos + 1;

    for (m_nextPos = startPos; m_nextPos < m_data.length(); ++m_nextPos) {
        if (m_data[m_nextPos] == delimiter) {
            String value = m_data.deprecatedString().mid(startPos, m_nextPos - startPos);
            if (value.isNull())
                value = "";
                
            ++m_nextPos; //Consume the char;
            return Token(LITERAL, value);
        }
    }

    //Ouch, went off the end -- report error
    return Token(ERROR);
}

Token Parser::lexNumber()
{
    int startPos = m_nextPos;
    bool seenDot = false;

    //Go until end or a non-digits character
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

    String value = m_data.deprecatedString().mid(startPos, m_nextPos - startPos);
    return Token(NUMBER, value);
}

Token Parser::lexNCName()
{
    int startPos = m_nextPos;
    if (m_nextPos < m_data.length() && charCat(m_data[m_nextPos]) == NameStart) {
        //Keep going until we get a character that's not good for names.
        for (; m_nextPos < m_data.length(); ++m_nextPos) {
            if (charCat(m_data[m_nextPos]) == NotPartOfName)
                break;
        }
        
        String value = m_data.deprecatedString().mid(startPos, m_nextPos - startPos);
        return Token(ERROR + 1, value);
    }

    return makeTokenAndAdvance(ERROR);
}

Token Parser::lexQName()
{
    Token t1 = lexNCName();
    if (t1.type == ERROR) return t1;
    skipWS();
    //If the next character is :, what we just got it the prefix, if not,
    //it's the whole thing
    if (peekAheadHelper() != ':')
        return t1;

    Token t2 = lexNCName();
    if (t2.type == ERROR) return t2;

    return Token(ERROR + 1, t1.value + ":" + t2.value);
}

Token Parser::nextTokenInternal()
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
            else if (next >= '0' && next <= '9')
                return lexNumber();
            else
                return makeTokenAndAdvance('.');
        }
        case '/':
            if (peekAheadHelper() == '/')
                return makeTokenAndAdvance(SLASHSLASH, 2);
            else
                return makeTokenAndAdvance('/');
        case '+':
            return makeTokenAndAdvance(PLUS);
        case '-':
            return makeTokenAndAdvance(MINUS);
        case '=':
            return makeIntTokenAndAdvance(EQOP, EqTestOp::OP_EQ);
        case '!':
            if (peekAheadHelper() == '=')
                return makeIntTokenAndAdvance(EQOP, EqTestOp::OP_NE, 2);

            return Token(ERROR);
        case '<':
            if (peekAheadHelper() == '=')
                return makeIntTokenAndAdvance(RELOP, NumericOp::OP_LE, 2);

            return makeIntTokenAndAdvance(RELOP, NumericOp::OP_LT);
        case '>':
            if (peekAheadHelper() == '=')
                return makeIntTokenAndAdvance(RELOP, NumericOp::OP_GE, 2);

            return makeIntTokenAndAdvance(RELOP, NumericOp::OP_GT);
        case '*':
            if (isOperatorContext())
                return makeIntTokenAndAdvance(MULOP, NumericOp::OP_Mul);
            else {
                ++m_nextPos;
                return Token(NAMETEST, "*");
            }
        case '$': {//$ QName
            m_nextPos++;
            Token par = lexQName();
            if (par.type == ERROR)
                return par;

            return Token(VARIABLEREFERENCE, par.value);
        }
    }

    Token t1 = lexNCName();
    if (t1.type == ERROR) return t1;

    skipWS();
    // If we're in an operator context, check for any operator names
    if (isOperatorContext()) {
        if (t1.value == "and") //### hash?
            return Token(AND);
        if (t1.value == "or")
            return Token(OR);
        if (t1.value == "mod")
            return Token(MULOP, NumericOp::OP_Mod);
        if (t1.value == "div")
            return Token(MULOP, NumericOp::OP_Div);
    }

    // See whether we are at a :
    if (peekCurHelper() == ':') {
        m_nextPos++;
        // Any chance it's an axis name?
        if (peekCurHelper() == ':') {
            m_nextPos++;
            
            //It might be an axis name.
            Step::AxisType axisType;
            if (isAxisName(t1.value, axisType))
                return Token(AXISNAME, axisType);
            // Ugh, :: is only valid in axis names -> error
            return Token(ERROR);
        }

        // Seems like this is a fully qualified qname, or perhaps the * modified one from NameTest
        skipWS();
        if (peekCurHelper() == '*') {
            m_nextPos++;
            return Token(NAMETEST, t1.value + ":*");
        }
        
        // Make a full qname..
        Token t2 = lexNCName();
        if (t2.type == ERROR)
            return t2;
        
        t1.value = t1.value + ":" + t2.value;
    }

    skipWS();
    if (peekCurHelper() == '(') {
        //note: we don't swallow the (here!
        
        //either node type of function name
        if (isNodeTypeName(t1.value)) {
            if (t1.value == "processing-instruction")
                return Token(PI, t1.value);

            return Token(NODETYPE, t1.value);
        }
        //must be a function name.
        return Token(FUNCTIONNAME, t1.value);
    }

    //At this point, it must be NAMETEST
    return Token(NAMETEST, t1.value);
}

Token Parser::nextToken()
{
    Token toRet = nextTokenInternal();
    m_lastTokenType = toRet.type;
    return toRet;
}

Parser::Parser()
{
    reset(String());
}

void Parser::reset(const String& data)
{
    m_nextPos = 0;
    m_data = data;
    m_lastTokenType = 0;
    
    m_topExpr = 0;
    m_gotNamespaceError = false;
}

int Parser::lex(void* data)
{
    YYSTYPE* yylval = static_cast<YYSTYPE*>(data);
    Token tok = nextToken();
 
    if (!tok.value.isNull()) {
        yylval->str = new String(tok.value);
        registerString(yylval->str);
    } else if (tok.intValue)
        yylval->num = tok.intValue;
    
    return tok.type;
}

Expression* Parser::parseStatement(const String& statement, ExceptionCode& ec)
{
    reset(statement);
    
    Parser* oldParser = currentParser;
    currentParser = this;
    int parseError = xpathyyparse(this);
    currentParser = oldParser;
        
    if (parseError) {
        deleteAllValues(m_parseNodes);
        deleteAllValues(m_expressionVectors);
        deleteAllValues(m_predicateVectors);
        deleteAllValues(m_strings);
        
        if (m_gotNamespaceError)
            ec = NAMESPACE_ERR;
        else
            ec = INVALID_EXPRESSION_ERR;
        return 0;
    } else {
        ASSERT(m_parseNodes.size() == 1); // Top expression
        ASSERT(m_expressionVectors.size() == 0);
        ASSERT(m_predicateVectors.size() == 0);
        ASSERT(m_strings.size() == 0);
        
        m_parseNodes.clear();
    }
    
    return m_topExpr;
}

void Parser::registerParseNode(ParseNode* node)
{
    if (node == 0)
        return;
    
    ASSERT(!m_parseNodes.contains(node));
    
    m_parseNodes.add(node);
}

void Parser::unregisterParseNode(ParseNode* node)
{
    if (node == 0)
        return;
    
    ASSERT(m_parseNodes.contains(node));

    m_parseNodes.remove(node);
}

void Parser::registerPredicateVector(Vector<Predicate*>* vector)
{
    if (vector == 0)
        return;

    ASSERT(!m_predicateVectors.contains(vector));
    
    m_predicateVectors.add(vector);
}

void Parser::unregisterPredicateVector(Vector<Predicate*>* vector)
{
    if (vector == 0)
        return;

    ASSERT(m_predicateVectors.contains(vector));
    
    m_predicateVectors.remove(vector);
}


void Parser::registerExpressionVector(Vector<Expression*>* vector)
{
    if (vector == 0)
        return;

    ASSERT(!m_expressionVectors.contains(vector));
    
    m_expressionVectors.add(vector);    
}

void Parser::unregisterExpressionVector(Vector<Expression*>* vector)
{
    if (vector == 0)
        return;

    ASSERT(m_expressionVectors.contains(vector));
    
    m_expressionVectors.remove(vector);        
}

void Parser::registerString(String* s)
{
    if (s == 0)
        return;
    
    ASSERT(!m_strings.contains(s));
    
    m_strings.add(s);        
}

void Parser::unregisterString(String* s)
{
    if (s == 0)
        return;
    
    ASSERT(m_strings.contains(s));
    
    m_strings.remove(s);
}

}
}

#endif // XPATH_SUPPORT
