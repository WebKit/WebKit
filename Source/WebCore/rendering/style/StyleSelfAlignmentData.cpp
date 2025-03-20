/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "StyleSelfAlignmentData.h"

#include "BoxSides.h"
#include "LayoutUnit.h"
#include "WritingMode.h"

#include <wtf/text/TextStream.h>

namespace WebCore {

LayoutUnit StyleSelfAlignmentData::adjustmentFromStartEdge(LayoutUnit extraSpace, ItemPosition alignmentPosition, LogicalBoxAxis containerAxis, WritingMode containerWritingMode, WritingMode selfWritingMode)
{
    ASSERT(ItemPosition::Auto != alignmentPosition);

    switch (alignmentPosition) {
    case ItemPosition::Normal:
    case ItemPosition::Stretch:
    case ItemPosition::Start:
    case ItemPosition::FlexStart:
        return 0_lu;

    case ItemPosition::Center:
    case ItemPosition::AnchorCenter:
        return extraSpace / 2;

    case ItemPosition::End:
    case ItemPosition::FlexEnd:
        return extraSpace;

    case ItemPosition::SelfStart:
        if (LogicalBoxAxis::Inline == containerAxis)
            return containerWritingMode.isInlineMatchingAny(selfWritingMode) ? 0_lu : extraSpace;
        return containerWritingMode.isBlockMatchingAny(selfWritingMode) ? 0_lu : extraSpace;
    case ItemPosition::SelfEnd:
        if (LogicalBoxAxis::Inline == containerAxis)
            return containerWritingMode.isInlineMatchingAny(selfWritingMode) ? extraSpace : 0_lu;
        return containerWritingMode.isBlockMatchingAny(selfWritingMode) ? extraSpace : 0_lu;

    case ItemPosition::Left:
        if (LogicalBoxAxis::Inline == containerAxis)
            return containerWritingMode.isBidiLTR() ? 0_lu : extraSpace;
        return containerWritingMode.isBlockLeftToRight() ? 0_lu : extraSpace;

    case ItemPosition::Right:
        if (LogicalBoxAxis::Inline == containerAxis)
            return containerWritingMode.isBidiLTR() ? extraSpace : 0_lu;
        return containerWritingMode.isBlockLeftToRight() ? extraSpace : 0_lu;

    case ItemPosition::Baseline:
        // Self-start if self block axis; else fall back to start.
        if (!selfWritingMode.isOrthogonal(containerWritingMode)) {
            if (LogicalBoxAxis::Inline == containerAxis)
                return 0_lu;
            return containerWritingMode.isBlockOpposing(selfWritingMode) ? extraSpace : 0_lu;
        }
        if (LogicalBoxAxis::Inline == containerAxis) // Self block axis.
            return containerWritingMode.isInlineMatchingAny(selfWritingMode) ? 0_lu : extraSpace;
        return 0_lu;

    case ItemPosition::LastBaseline:
        // Self-end if self block axis; else fall back to end.
        if (!selfWritingMode.isOrthogonal(containerWritingMode)) {
            if (LogicalBoxAxis::Inline == containerAxis)
                return extraSpace;
            return containerWritingMode.isBlockOpposing(selfWritingMode) ? 0_lu : extraSpace;
        }
        if (LogicalBoxAxis::Inline == containerAxis) // Self block axis.
            return containerWritingMode.isInlineMatchingAny(selfWritingMode) ? extraSpace : 0_lu;
        return extraSpace;

    default:
        ASSERT_NOT_REACHED();
        return 0_lu;
    }
}

TextStream& operator<<(TextStream& ts, const StyleSelfAlignmentData& o)
{
    return ts << o.position() << ' ' << o.positionType() << ' ' << o.overflow();
}

}
