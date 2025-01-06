/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "BlockStepSizing.h"

#include "RenderBox.h"
#include "RenderStyleInlines.h"

namespace WebCore {

namespace BlockStepSizing {

bool childHasSupportedStyle(const RenderStyle& childStyle)
{
    return childStyle.blockStepInsert() == BlockStepInsert::MarginBox
        && childStyle.blockStepAlign() == BlockStepAlign::Auto
        && childStyle.blockStepRound() == BlockStepRound::Up;
}

LayoutUnit computeExtraSpace(LayoutUnit stepSize, LayoutUnit boxOuterSize)
{
    if (!stepSize)
        return { };

    if (!boxOuterSize)
        return stepSize;

    if (auto remainder = intMod(boxOuterSize, stepSize))
        return stepSize - remainder;
    return { };
}

void distributeExtraSpaceToChildMargins(RenderBox& child, LayoutUnit extraSpace, WritingMode containingBlockWritingMode)
{
    auto halfExtraSpace = extraSpace / 2;
    child.setMarginBefore(child.marginBefore(containingBlockWritingMode) + halfExtraSpace);
    child.setMarginAfter(child.marginAfter(containingBlockWritingMode) + halfExtraSpace);
}

NO_RETURN_DUE_TO_ASSERT void distributeExtraSpaceToChildPadding(RenderBox& /* child */, LayoutUnit /* extraSpace */, WritingMode /* containingBlockWritingMode */)
{
    ASSERT_NOT_IMPLEMENTED_YET();
}

NO_RETURN_DUE_TO_ASSERT void distributeExtraSpaceToChildContentArea(RenderBox& /* child */, LayoutUnit /* extraSpace */, WritingMode /* containingBlockWritingMode */)
{
    ASSERT_NOT_IMPLEMENTED_YET();
}

} // namespace BlockStepSizing

} // namespace WebCore
