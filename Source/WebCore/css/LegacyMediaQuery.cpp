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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "LegacyMediaQuery.h"

#include <wtf/text/StringBuilder.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

// http://dev.w3.org/csswg/cssom/#serialize-a-media-query
String LegacyMediaQuery::serialize() const
{
    if (m_ignored) {
        // If query is invalid, serialized text should turn into "not all".
        return "not all"_s;
    }

    bool shouldOmitMediaType = false;
    StringBuilder result;
    switch (m_restrictor) {
    case LegacyMediaQuery::Only:
        result.append("only ");
        break;
    case LegacyMediaQuery::Not:
        result.append("not ");
        break;
    case LegacyMediaQuery::None:
        shouldOmitMediaType = !m_expressions.isEmpty() && m_mediaType == "all"_s;
        break;
    }
    bool needsAnd = false;
    if (!shouldOmitMediaType) {
        result.append(m_mediaType);
        needsAnd = true;
    }
    for (auto& expression : m_expressions) {
        if (needsAnd)
            result.append(" and ");
        result.append(expression.serialize());
        needsAnd = true;
    }
    return result.toString();
}

LegacyMediaQuery::LegacyMediaQuery(Restrictor restrictor, const String& mediaType, Vector<MediaQueryExpression>&& expressions)
    : m_mediaType(mediaType.convertToASCIILowercase())
    , m_expressions(WTFMove(expressions))
    , m_restrictor(restrictor)
{
}

// http://dev.w3.org/csswg/cssom/#compare-media-queries
bool LegacyMediaQuery::operator==(const LegacyMediaQuery& other) const
{
    return cssText() == other.cssText();
}

// http://dev.w3.org/csswg/cssom/#serialize-a-list-of-media-queries
const String& LegacyMediaQuery::cssText() const
{
    if (m_serializationCache.isNull())
        m_serializationCache = serialize();
    return m_serializationCache;
}

TextStream& operator<<(TextStream& ts, const LegacyMediaQuery& query)
{
    ts << query.cssText();
    return ts;
}

} // namespace
