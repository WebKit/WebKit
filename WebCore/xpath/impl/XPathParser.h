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
#ifndef XPathParser_H
#define XPathParser_H

#if XPATH_SUPPORT

#include <wtf/Noncopyable.h>

#include "StringHash.h"
#include "PlatformString.h"

#include "XPathStep.h"
#include "XPathPath.h"
#include "XPathPredicate.h"
#include "XPathExpressionNode.h"
#include "XPathUtil.h"

namespace WebCore {
namespace XPath {

struct Token
{
    int     type;
    String value;
    int     intValue; //0 if not set
    
    Token(int t): type(t), intValue(0) {}
    Token(int t, String v): type(t), value(v) {}
    Token(int t, int v): type(t), intValue(v) {}
};

class Parser : Noncopyable
{
private:
    static Parser* currentParser;
    
    unsigned m_nextPos;
    String m_data;
    int m_lastTokenType;
    
    static HashMap<String, Step::AxisType>* s_axisNamesDict;
    static HashSet<String>* s_nodeTypeNamesDict;
    
    enum XMLCat {
        NameStart,
        NameCont,
        NotPartOfName
    };
    
    XMLCat charCat(QChar aChar);
    
    bool isAxisName(String name, Step::AxisType &type);
    bool isNodeTypeName(String name);
    bool isOperatorContext();
    
    void  skipWS();
    Token makeTokenAndAdvance(int code, int advance = 1);
    Token makeIntTokenAndAdvance(int code, int val, int advance = 1);
    char  peekAheadHelper();
    char  peekCurHelper();
    
    Token lexString();
    Token lexNumber();
    Token lexNCName();
    Token lexQName();
    
    Token nextToken();
    Token nextTokenInternal();
    
    void reset(const String& data);
    
    HashSet<ParseNode*> m_parseNodes;
    HashSet<Vector<Predicate*>*> m_predicateVectors;
    HashSet<Vector<Expression*>*> m_expressionVectors;
    HashSet<String*> m_strings;
public:
    Parser();
    
    Expression* parseStatement(const String& statement, ExceptionCode&);

    static Parser* current() { return currentParser; }
          
    int lex(void* yylval);

    Expression* m_topExpr;
    bool m_gotNamespaceError;
    
    void registerParseNode(ParseNode*);
    void unregisterParseNode(ParseNode*);
    
    void registerPredicateVector(Vector<Predicate*>*);
    void unregisterPredicateVector(Vector<Predicate*>*);

    void registerExpressionVector(Vector<Expression*>*);
    void unregisterExpressionVector(Vector<Expression*>*);
    
    void registerString(String*);
    void unregisterString(String*);
};

}
}

#endif // XPATH_SUPPORT

#endif
