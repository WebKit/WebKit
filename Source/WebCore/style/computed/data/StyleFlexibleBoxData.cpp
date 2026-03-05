/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2025-2026 Samuel Weinig <sam@webkit.org>
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
#include "StyleFlexibleBoxData.h"

#include "StyleComputedStyle+DifferenceLogging.h"
#include "StyleComputedStyle+InitialInlines.h"
#include "StylePrimitiveKeyword+Logging.h"
#include "StylePrimitiveNumericTypes+Logging.h"

namespace WebCore {
namespace Style {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(FlexibleBoxData);

FlexibleBoxData::FlexibleBoxData()
    : flexGrow(ComputedStyle::initialFlexGrow())
    , flexShrink(ComputedStyle::initialFlexShrink())
    , flexBasis(ComputedStyle::initialFlexBasis())
    , flexDirection(static_cast<unsigned>(ComputedStyle::initialFlexDirection()))
    , flexWrap(static_cast<unsigned>(ComputedStyle::initialFlexWrap()))
{
}

inline FlexibleBoxData::FlexibleBoxData(const FlexibleBoxData& other)
    : RefCounted<FlexibleBoxData>()
    , flexGrow(other.flexGrow)
    , flexShrink(other.flexShrink)
    , flexBasis(other.flexBasis)
    , flexDirection(other.flexDirection)
    , flexWrap(other.flexWrap)
{
}

Ref<FlexibleBoxData> FlexibleBoxData::copy() const
{
    return adoptRef(*new FlexibleBoxData(*this));
}

bool FlexibleBoxData::operator==(const FlexibleBoxData& other) const
{
    return flexGrow == other.flexGrow
        && flexShrink == other.flexShrink
        && flexBasis == other.flexBasis
        && flexDirection == other.flexDirection
        && flexWrap == other.flexWrap;
}

#if !LOG_DISABLED
void FlexibleBoxData::dumpDifferences(TextStream& ts, const FlexibleBoxData& other) const
{
    LOG_IF_DIFFERENT(flexGrow);
    LOG_IF_DIFFERENT(flexShrink);
    LOG_IF_DIFFERENT(flexBasis);

    LOG_IF_DIFFERENT_WITH_CAST(FlexDirection, flexDirection);
    LOG_IF_DIFFERENT_WITH_CAST(FlexWrap, flexWrap);
}
#endif // !LOG_DISABLED

} // namespace Style
} // namespace WebCore
