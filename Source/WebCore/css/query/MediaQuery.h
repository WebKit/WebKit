/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "GenericMediaQueryTypes.h"
#include <wtf/EnumTraits.h>
#include <wtf/PackedRefPtr.h>

namespace WebCore {
namespace MQ {

enum class Prefix : bool { Not, Only };

class MediaQuery {
public:
    MediaQuery(std::optional<Prefix> prefix, const AtomString& mediaType, std::optional<Condition>&& condition = std::nullopt)
        : m_mediaType(mediaType.impl())
    {
        m_hasPrefix = prefix.has_value();
        if (m_hasPrefix)
            m_prefix = WTF::enumToUnderlyingType(*prefix);

        m_hasCondition = condition.has_value();
        if (m_hasCondition) {
            m_logicalOperator = WTF::enumToUnderlyingType(condition->logicalOperator);
            m_conditionQueries = WTFMove(condition->queries);
        }
    }

    AtomString mediaType() const { return m_mediaType.get(); }
    bool hasCondition() const { return m_hasCondition; }

    std::optional<Prefix> prefix() const
    {
        if (!m_hasPrefix)
            return std::nullopt;
        return { static_cast<Prefix>(m_prefix) };
    }

    const Vector<QueryInParens>& conditionQueries() const
    {
        ASSERT(m_hasCondition);
        return m_conditionQueries;
    }

    LogicalOperator logicalOperator() const
    {
        ASSERT(m_hasCondition);
        return static_cast<LogicalOperator>(m_logicalOperator);
    }

private:
    Vector<QueryInParens> m_conditionQueries;
    PackedRefPtr<AtomStringImpl> m_mediaType;
    unsigned m_logicalOperator : 2;
    bool m_prefix : 1;
    bool m_hasPrefix : 1;
    bool m_hasCondition : 1;
};

using MediaQueryList = Vector<MediaQuery>;

struct MediaQueryResult {
    MediaQueryList mediaQueryList;
    bool result;
};

enum class MediaQueryDynamicDependency : uint8_t  {
    Viewport = 1 << 0,
    Appearance = 1 << 1,
    Accessibility = 1 << 2,
};

template<typename TraverseFunction>
void traverseFeatures(const MediaQuery& query, TraverseFunction&& function)
{
    if (query.hasCondition()) {
        for (auto& queryInParens : query.conditionQueries())
            traverseFeatures(queryInParens, function);
    }
}

template<typename TraverseFunction>
void traverseFeatures(const MediaQueryList& list, TraverseFunction&& function)
{
    for (auto& query : list)
        traverseFeatures(query, function);
}

}
}
