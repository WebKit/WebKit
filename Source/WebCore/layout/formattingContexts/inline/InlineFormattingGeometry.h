/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#pragma once

#include "FloatingContext.h"
#include "FormattingGeometry.h"
#include "InlineLineBuilder.h"

namespace WebCore {
namespace Layout {

class FloatingContext;
class InlineFormattingContext;

class InlineFormattingGeometry : public FormattingGeometry {
public:
    InlineFormattingGeometry(const InlineFormattingContext&);

    InlineLayoutUnit logicalTopForNextLine(const LineBuilder::LineContent&, const InlineRect& lineLogicalRect, const FloatingContext&) const;

    ContentHeightAndMargin inlineBlockContentHeightAndMargin(const Box&, const HorizontalConstraints&, const OverriddenVerticalValues&) const;
    ContentWidthAndMargin inlineBlockContentWidthAndMargin(const Box&, const HorizontalConstraints&, const OverriddenHorizontalValues&) const;

    enum class IsIntrinsicWidthMode : uint8_t { Yes, No };
    InlineLayoutUnit computedTextIndent(IsIntrinsicWidthMode, std::optional<bool> previousLineEndsWithLineBreak, InlineLayoutUnit availableWidth) const;

    bool inlineLevelBoxAffectsLineBox(const InlineLevelBox&, const LineBox&) const;

    InlineLayoutUnit initialLineHeight(bool isFirstLine) const;

    FloatingContext::Constraints floatConstraintsForLine(InlineLayoutUnit lineLogicalTop, InlineLayoutUnit contentLogicalHeight, const FloatingContext&) const;

    static InlineRect flipVisualRectToLogicalForWritingMode(const InlineRect& visualRect, WritingMode);

    LayoutPoint staticPositionForOutOfFlowInlineLevelBox(const Box&, LayoutPoint contentBoxTopLeft) const;
    LayoutPoint staticPositionForOutOfFlowBlockLevelBox(const Box&, LayoutPoint contentBoxTopLeft) const;

private:
    const InlineFormattingContext& formattingContext() const { return downcast<InlineFormattingContext>(FormattingGeometry::formattingContext()); }

};

}
}

SPECIALIZE_TYPE_TRAITS_LAYOUT_FORMATTING_GEOMETRY(InlineFormattingGeometry, isInlineFormattingGeometry())

