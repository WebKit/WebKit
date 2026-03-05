/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "StyleGridItemData.h"

#include "StyleComputedStyle+DifferenceLogging.h"
#include "StyleComputedStyle+InitialInlines.h"
#include "StylePrimitiveKeyword+Logging.h"
#include "StylePrimitiveNumericTypes+Logging.h"

namespace WebCore {
namespace Style {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(GridItemData);

GridItemData::GridItemData()
    : gridItemColumnStart(ComputedStyle::initialGridItemColumnStart())
    , gridItemColumnEnd(ComputedStyle::initialGridItemColumnEnd())
    , gridItemRowStart(ComputedStyle::initialGridItemRowStart())
    , gridItemRowEnd(ComputedStyle::initialGridItemRowEnd())
{
}

inline GridItemData::GridItemData(const GridItemData& o)
    : RefCounted<GridItemData>()
    , gridItemColumnStart(o.gridItemColumnStart)
    , gridItemColumnEnd(o.gridItemColumnEnd)
    , gridItemRowStart(o.gridItemRowStart)
    , gridItemRowEnd(o.gridItemRowEnd)
{
}

Ref<GridItemData> GridItemData::copy() const
{
    return adoptRef(*new GridItemData(*this));
}

bool GridItemData::operator==(const GridItemData& o) const
{
    return gridItemColumnStart == o.gridItemColumnStart
        && gridItemColumnEnd == o.gridItemColumnEnd
        && gridItemRowStart == o.gridItemRowStart
        && gridItemRowEnd == o.gridItemRowEnd;
}

#if !LOG_DISABLED
void GridItemData::dumpDifferences(TextStream& ts, const GridItemData& other) const
{
    LOG_IF_DIFFERENT(gridItemColumnStart);
    LOG_IF_DIFFERENT(gridItemColumnEnd);
    LOG_IF_DIFFERENT(gridItemRowStart);
    LOG_IF_DIFFERENT(gridItemRowEnd);
}
#endif

} // namespace Style
} // namespace WebCore
