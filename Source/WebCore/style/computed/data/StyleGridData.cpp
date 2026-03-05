/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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

#include "StyleComputedStyle+DifferenceLogging.h"
#include "StyleComputedStyle+InitialInlines.h"
#include "StylePrimitiveKeyword+Logging.h"
#include "StylePrimitiveNumericTypes+Logging.h"

namespace WebCore {
namespace Style {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(GridData);

GridData::GridData()
    : gridAutoFlow(ComputedStyle::initialGridAutoFlow())
    , gridAutoColumns(ComputedStyle::initialGridAutoColumns())
    , gridAutoRows(ComputedStyle::initialGridAutoRows())
    , gridTemplateAreas(ComputedStyle::initialGridTemplateAreas())
    , gridTemplateColumns(ComputedStyle::initialGridTemplateColumns())
    , gridTemplateRows(ComputedStyle::initialGridTemplateRows())
    , flowTolerance(ComputedStyle::initialFlowTolerance())
{
}

inline GridData::GridData(const GridData& o)
    : RefCounted<GridData>()
    , gridAutoFlow(o.gridAutoFlow)
    , gridAutoColumns(o.gridAutoColumns)
    , gridAutoRows(o.gridAutoRows)
    , gridTemplateAreas(o.gridTemplateAreas)
    , gridTemplateColumns(o.gridTemplateColumns)
    , gridTemplateRows(o.gridTemplateRows)
    , flowTolerance(o.flowTolerance)
{
}

bool GridData::operator==(const GridData& o) const
{
    return gridAutoFlow == o.gridAutoFlow
        && gridAutoColumns == o.gridAutoColumns
        && gridAutoRows == o.gridAutoRows
        && gridTemplateAreas == o.gridTemplateAreas
        && gridTemplateColumns == o.gridTemplateColumns
        && gridTemplateRows == o.gridTemplateRows
        && flowTolerance == o.flowTolerance;
}

Ref<GridData> GridData::copy() const
{
    return adoptRef(*new GridData(*this));
}

#if !LOG_DISABLED
void GridData::dumpDifferences(TextStream& ts, const GridData& other) const
{
    LOG_IF_DIFFERENT(gridAutoFlow);
    LOG_IF_DIFFERENT(gridAutoColumns);
    LOG_IF_DIFFERENT(gridAutoRows);
    LOG_IF_DIFFERENT(gridTemplateAreas);
    LOG_IF_DIFFERENT(gridTemplateColumns);
    LOG_IF_DIFFERENT(gridTemplateRows);
    LOG_IF_DIFFERENT(flowTolerance);
}
#endif // !LOG_DISABLED

} // namespace Style
} // namespace WebCore
