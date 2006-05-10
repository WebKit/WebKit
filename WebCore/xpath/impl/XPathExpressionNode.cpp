/*
 * Copyright 2005 Frerich Raabe <raabe@kde.org>
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

#include "XPathExpressionNode.h"

#include "Logging.h"
#include "Node.h"
#include "XPathValue.h"
#include <math.h>

using namespace std;

namespace WebCore {
namespace XPath {
    
EvaluationContext &Expression::evaluationContext()
{
    static EvaluationContext evaluationContext;
    return evaluationContext;
}

Expression::Expression()
    : m_constantValue(0)
{
}

Expression::~Expression()
{
    deleteAllValues(m_subExpressions);
    delete m_constantValue;
}

Value Expression::evaluate() const
{
    if (m_constantValue)
        return *m_constantValue;
    return doEvaluate();
}

void Expression::addSubExpression(Expression* expr)
{
    m_subExpressions.append(expr);
}

void Expression::optimize()
{
    bool allSubExpressionsConstant = true;
    
    for (unsigned i = 0; i < m_subExpressions.size(); i++) {
        if (m_subExpressions[i]->isConstant())
            m_subExpressions[i]->optimize();
        else
            allSubExpressionsConstant = false;
    }

    if (allSubExpressionsConstant) {
        ASSERT (!m_constantValue);
        m_constantValue = new Value(doEvaluate());
        deleteAllValues(m_subExpressions);
        m_subExpressions.clear();
    }
}

unsigned Expression::subExprCount() const
{
    return m_subExpressions.size();
}

Expression* Expression::subExpr(unsigned i)
{
    ASSERT(i < subExprCount());
    return m_subExpressions[i];
}

const Expression* Expression::subExpr(unsigned i) const
{
    ASSERT(i < subExprCount());
    return m_subExpressions[i];
}

bool Expression::isConstant() const
{
    for (unsigned i = 0; i < m_subExpressions.size(); i++)
        if (!m_subExpressions[i]->isConstant())
            return false;
    return true;
}

}
}

#endif // XPATH_SUPPORT

