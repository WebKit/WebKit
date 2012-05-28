/*
 * CSS Media Query
 *
 * Copyright (C) 2005, 2006 Kimmo Kinnunen <kimmo.t.kinnunen@nokia.com>.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "MediaQuery.h"

#include "MediaQueryExp.h"
#include <wtf/NonCopyingSort.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

String MediaQuery::serialize() const
{
    StringBuilder result;

    switch (m_restrictor) {
    case MediaQuery::Only:
        result.append("only ");
        break;
    case MediaQuery::Not:
        result.append("not ");
        break;
    case MediaQuery::None:
        break;
    }

    if (m_expressions->isEmpty()) {
        result.append(m_mediaType);
        return result.toString();
    }

    if (m_mediaType != "all" || m_restrictor != None) {
        result.append(m_mediaType);
        result.append(" and ");
    }

    result.append(m_expressions->at(0)->serialize());
    for (size_t i = 1; i < m_expressions->size(); ++i) {
        result.append(" and ");
        result.append(m_expressions->at(i)->serialize());
    }

    return result.toString();
}

static bool expressionCompare(const OwnPtr<MediaQueryExp>& a, const OwnPtr<MediaQueryExp>& b)
{
    return codePointCompare(a->serialize(), b->serialize()) < 0;
}

MediaQuery::MediaQuery(Restrictor restrictor, const String& mediaType, PassOwnPtr<ExpressionVector> expressions)
    : m_restrictor(restrictor)
    , m_mediaType(mediaType.lower())
    , m_expressions(expressions)
    , m_ignored(false)
{
    if (!m_expressions) {
        m_expressions = adoptPtr(new ExpressionVector);
        return;
    }

    ExpressionVector& vector = *m_expressions;

    nonCopyingSort(vector.begin(), vector.end(), expressionCompare);

    String previousSerializedExpression;
    for (size_t i = vector.size(); i; ) {
        --i;

        // If any expression is invalid, the entire media query must be ignored.
        m_ignored = m_ignored || !vector[i]->isValid();

        // Remove duplicate expressions.
        String serializedExpression = vector[i]->serialize();
        if (serializedExpression == previousSerializedExpression)
            vector.remove(i);
        previousSerializedExpression = serializedExpression;
    }
}

MediaQuery::MediaQuery(const MediaQuery& o)
    : m_restrictor(o.m_restrictor)
    , m_mediaType(o.m_mediaType)
    , m_expressions(adoptPtr(new Vector<OwnPtr<MediaQueryExp> >(o.m_expressions->size())))
    , m_ignored(o.m_ignored)
    , m_serializedQuery(o.m_serializedQuery)
{
    for (unsigned i = 0; i < m_expressions->size(); ++i)
        (*m_expressions)[i] = o.m_expressions->at(i)->copy();
}

PassOwnPtr<MediaQuery> MediaQuery::copy() const
{
    return adoptPtr(new MediaQuery(*this));
}

MediaQuery::~MediaQuery()
{
}

bool MediaQuery::operator==(const MediaQuery& other) const
{
    return cssText() == other.cssText();
}

const String& MediaQuery::cssText() const
{
    if (m_serializedQuery.isNull())
        const_cast<MediaQuery*>(this)->m_serializedQuery = serialize();
    return m_serializedQuery;
}

} // namespace WebCore
