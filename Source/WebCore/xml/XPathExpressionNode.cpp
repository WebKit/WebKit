/*
 * Copyright 2005 Frerich Raabe <raabe@kde.org>
 * Copyright (C) 2006, 2013 Apple Inc.
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
#include "XPathExpressionNode.h"

#include <wtf/NeverDestroyed.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {
namespace XPath {

EvaluationContext& Expression::evaluationContext()
{
    static NeverDestroyed<EvaluationContext> context;
    return context;
}

Expression::Expression()
    : m_isContextNodeSensitive(false)
    , m_isContextPositionSensitive(false)
    , m_isContextSizeSensitive(false)
{
}

void Expression::setSubexpressions(Vector<std::unique_ptr<Expression>> subexpressions)
{
    ASSERT(m_subexpressions.isEmpty());
    m_subexpressions = WTF::move(subexpressions);
    for (unsigned i = 0; i < m_subexpressions.size(); ++i) {
        m_isContextNodeSensitive |= m_subexpressions[i]->m_isContextNodeSensitive;
        m_isContextPositionSensitive |= m_subexpressions[i]->m_isContextPositionSensitive;
        m_isContextSizeSensitive |= m_subexpressions[i]->m_isContextSizeSensitive;
    }
}

} // namespace XPath
} // namespace WebCore
