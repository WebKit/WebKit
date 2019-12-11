/*
 * Copyright (C) 2005 Frerich Raabe <raabe@kde.org>
 * Copyright (C) 2006, 2009, 2013 Apple Inc. All rights reserved.
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

#include "XPathValue.h"

namespace WebCore {
namespace XPath {

struct EvaluationContext {
    RefPtr<Node> node;
    unsigned size;
    unsigned position;
    HashMap<String, String> variableBindings;

    bool hadTypeConversionError;
};

class Expression {
    WTF_MAKE_NONCOPYABLE(Expression); WTF_MAKE_FAST_ALLOCATED;
public:
    static EvaluationContext& evaluationContext();

    virtual ~Expression() = default;

    virtual Value evaluate() const = 0;
    virtual Value::Type resultType() const = 0;

    bool isContextNodeSensitive() const { return m_isContextNodeSensitive; }
    bool isContextPositionSensitive() const { return m_isContextPositionSensitive; }
    bool isContextSizeSensitive() const { return m_isContextSizeSensitive; }

protected:
    Expression();

    unsigned subexpressionCount() const { return m_subexpressions.size(); }
    const Expression& subexpression(unsigned i) const { return *m_subexpressions[i]; }

    void addSubexpression(std::unique_ptr<Expression> expression)
    {
        m_isContextNodeSensitive |= expression->m_isContextNodeSensitive;
        m_isContextPositionSensitive |= expression->m_isContextPositionSensitive;
        m_isContextSizeSensitive |= expression->m_isContextSizeSensitive;
        m_subexpressions.append(WTFMove(expression));
    }

    void setSubexpressions(Vector<std::unique_ptr<Expression>>);

    void setIsContextNodeSensitive(bool value) { m_isContextNodeSensitive = value; }
    void setIsContextPositionSensitive(bool value) { m_isContextPositionSensitive = value; }
    void setIsContextSizeSensitive(bool value) { m_isContextSizeSensitive = value; }

private:
    Vector<std::unique_ptr<Expression>> m_subexpressions;

    // Evaluation details that can be used for optimization.
    bool m_isContextNodeSensitive;
    bool m_isContextPositionSensitive;
    bool m_isContextSizeSensitive;
};

} // namespace XPath
} // namespace WebCore
