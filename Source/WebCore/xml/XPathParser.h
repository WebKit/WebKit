/*
 * Copyright 2005 Maksim Orlovich <maksim@kde.org>
 * Copyright (C) 2006, 2013 Apple Inc. All rights reserved.
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

#pragma once

#include "ExceptionOr.h"
#include "XPathPredicate.h"

union YYSTYPE;

namespace WebCore {

class XPathNSResolver;

namespace XPath {

class Parser {
    WTF_MAKE_NONCOPYABLE(Parser);
public:
    static ExceptionOr<std::unique_ptr<Expression>> parseStatement(const String& statement, RefPtr<XPathNSResolver>&&);

    int lex(YYSTYPE&);
    bool expandQualifiedName(const String& qualifiedName, String& localName, String& namespaceURI);
    void setParseResult(std::unique_ptr<Expression>&& expression) { m_result = WTFMove(expression); }

private:
    Parser(const String&, RefPtr<XPathNSResolver>&&);

    struct Token;

    bool isBinaryOperatorContext() const;

    void skipWS();
    Token makeTokenAndAdvance(int type, int advance = 1);
    Token makeTokenAndAdvance(int type, NumericOp::Opcode, int advance = 1);
    Token makeTokenAndAdvance(int type, EqTestOp::Opcode, int advance = 1);
    char peekAheadHelper();
    char peekCurHelper();

    Token lexString();
    Token lexNumber();
    bool lexNCName(String&);
    bool lexQName(String&);

    Token nextToken();
    Token nextTokenInternal();

    const String& m_data;
    RefPtr<XPathNSResolver> m_resolver;

    unsigned m_nextPos { 0 };
    int m_lastTokenType { 0 };

    std::unique_ptr<Expression> m_result;
    bool m_sawNamespaceError { false };
};

} }
