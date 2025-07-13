/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
 *  THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 *  EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 *  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "config.h"
#include "StyleGridData.h"

#include "RenderStyleInlines.h"
#include "RenderStyleDifference.h"

namespace WebCore {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(StyleGridData);

StyleGridData::StyleGridData()
    : m_gridAutoFlow(RenderStyle::initialGridAutoFlow())
    , m_gridAutoColumns(RenderStyle::initialGridAutoColumns())
    , m_gridAutoRows(RenderStyle::initialGridAutoRows())
    , m_gridTemplateAreas(RenderStyle::initialGridTemplateAreas())
    , m_gridTemplateColumns(RenderStyle::initialGridTemplateColumns())
    , m_gridTemplateRows(RenderStyle::initialGridTemplateRows())
{
}

inline StyleGridData::StyleGridData(const StyleGridData& o)
    : RefCounted<StyleGridData>()
    , m_gridAutoFlow(o.m_gridAutoFlow)
    , m_gridAutoColumns(o.m_gridAutoColumns)
    , m_gridAutoRows(o.m_gridAutoRows)
    , m_gridTemplateAreas(o.m_gridTemplateAreas)
    , m_gridTemplateColumns(o.m_gridTemplateColumns)
    , m_gridTemplateRows(o.m_gridTemplateRows)
{
}

bool StyleGridData::operator==(const StyleGridData& o) const
{
    return m_gridAutoFlow == o.m_gridAutoFlow
        && m_gridAutoColumns == o.m_gridAutoColumns
        && m_gridAutoRows == o.m_gridAutoRows
        && m_gridTemplateAreas == o.m_gridTemplateAreas
        && m_gridTemplateColumns == o.m_gridTemplateColumns
        && m_gridTemplateRows == o.m_gridTemplateRows;
}

Ref<StyleGridData> StyleGridData::copy() const
{
    return adoptRef(*new StyleGridData(*this));
}

#if !LOG_DISABLED
void StyleGridData::dumpDifferences(TextStream& ts, const StyleGridData& other) const
{
    LOG_IF_DIFFERENT(m_gridAutoFlow);
    LOG_IF_DIFFERENT(m_gridAutoColumns);
    LOG_IF_DIFFERENT(m_gridAutoRows);
    LOG_IF_DIFFERENT(m_gridTemplateAreas);
    LOG_IF_DIFFERENT(m_gridTemplateColumns);
    LOG_IF_DIFFERENT(m_gridTemplateRows);
}
#endif // !LOG_DISABLED

} // namespace WebCore
