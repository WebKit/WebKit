/*
 * expression.h - Copyright 2005 Frerich Raabe <raabe@kde.org>
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
#ifndef XPathExpressionNode_H
#define XPathExpressionNode_H

#if XPATH_SUPPORT

#include "XPathValue.h"

#include "StringHash.h"
#include <wtf/HashMap.h>
#include <wtf/Vector.h>

namespace WebCore {
class XPathNSResolver;

namespace XPath {
        
struct EvaluationContext
{
    EvaluationContext() : node(0), size(0), position(0), resolver(0) { }

    RefPtr<Node> node;
    unsigned long size;
    unsigned long position;
    HashMap<String, String> variableBindings;

    /* The function library is globally accessible through
     * FunctionLibrary::self()
     */
    XPathNSResolver* resolver;
};

class ParseNode
{
};

class Expression : public ParseNode
{
public:
    static EvaluationContext& evaluationContext();

    Expression();
    virtual ~Expression();
    virtual Value evaluate() const;

    void addSubExpression(Expression* expr);
    void optimize();
    virtual bool isConstant() const;

protected:
    unsigned int subExprCount() const;
    Expression* subExpr(unsigned i);
    const Expression* subExpr(unsigned i) const;

private:
    virtual Value doEvaluate() const = 0;

    Vector<Expression*> m_subExpressions;
    Value* m_constantValue;
};

}
}

#endif // XPATH_SUPPORT

#endif // EXPRESSION_H

