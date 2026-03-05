/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
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
#include "StyleVisitedLinkColorData.h"

#include "StyleComputedStyle+DifferenceLogging.h"
#include "StyleComputedStyle+InitialInlines.h"
#include "StylePrimitiveKeyword+Logging.h"
#include "StylePrimitiveNumericTypes+Logging.h"

namespace WebCore {
namespace Style {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(VisitedLinkColorData);

VisitedLinkColorData::VisitedLinkColorData()
    : visitedLinkBackgroundColor(ComputedStyle::initialBackgroundColor())
    , visitedLinkBorderColors({ ComputedStyle::initialBorderTopColor(), ComputedStyle::initialBorderRightColor(), ComputedStyle::initialBorderBottomColor(), ComputedStyle::initialBorderLeftColor() })
    , visitedLinkTextDecorationColor(ComputedStyle::initialTextDecorationColor())
    , visitedLinkOutlineColor(ComputedStyle::initialOutlineColor())
{
}

VisitedLinkColorData::VisitedLinkColorData(const VisitedLinkColorData& o)
    : RefCounted<VisitedLinkColorData>()
    , visitedLinkBackgroundColor(o.visitedLinkBackgroundColor)
    , visitedLinkBorderColors(o.visitedLinkBorderColors)
    , visitedLinkTextDecorationColor(o.visitedLinkTextDecorationColor)
    , visitedLinkOutlineColor(o.visitedLinkOutlineColor)
{
}

VisitedLinkColorData::~VisitedLinkColorData() = default;

Ref<VisitedLinkColorData> VisitedLinkColorData::copy() const
{
    return adoptRef(*new VisitedLinkColorData(*this));
}

bool VisitedLinkColorData::operator==(const VisitedLinkColorData& o) const
{
    return visitedLinkBackgroundColor == o.visitedLinkBackgroundColor
        && visitedLinkBorderColors == o.visitedLinkBorderColors
        && visitedLinkTextDecorationColor == o.visitedLinkTextDecorationColor
        && visitedLinkOutlineColor == o.visitedLinkOutlineColor;
}

#if !LOG_DISABLED
void VisitedLinkColorData::dumpDifferences(TextStream& ts, const VisitedLinkColorData& other) const
{
    LOG_IF_DIFFERENT(visitedLinkBackgroundColor);
    LOG_IF_DIFFERENT(visitedLinkBorderColors);
    LOG_IF_DIFFERENT(visitedLinkTextDecorationColor);
    LOG_IF_DIFFERENT(visitedLinkOutlineColor);
}
#endif

} // namespace Style
} // namespace WebCore
