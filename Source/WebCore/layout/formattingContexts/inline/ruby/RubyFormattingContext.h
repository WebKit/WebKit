/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#include "InlineContentBreaker.h"
#include "InlineDisplayContent.h"
#include <wtf/Range.h>

namespace WebCore {
namespace Layout {

class InlineFormattingContext;
class InlineLevelBox;
class Line;
struct InlineItemRange;

class RubyFormattingContext {
public:
    RubyFormattingContext(const InlineFormattingContext& parentFormattingContext);

    void applyAnnotationContributionToLayoutBounds(InlineLevelBox& rubyBaseInlineBox) const;
    InlineLayoutPoint placeAnnotationBox(const Box& rubyBaseLayoutBox);
    InlineLayoutSize sizeAnnotationBox(const Box& rubyBaseLayoutBox);

    InlineLayoutUnit overhangForAnnotationBefore(const Box& rubyBaseLayoutBox, size_t rubyBaseStart, const InlineDisplay::Boxes&);
    InlineLayoutUnit overhangForAnnotationAfter(const Box& rubyBaseLayoutBox, WTF::Range<size_t> rubyBaseRange, const InlineDisplay::Boxes&);

    static std::optional<size_t> nextWrapOpportunity(size_t inlineItemIndex, std::optional<size_t> previousInlineItemIndex, const InlineItemRange&, const InlineItemList&);

    static bool isAtSoftWrapOpportunity(const InlineItem& previous, const InlineItem& current);
    static std::optional<InlineLayoutUnit> annotationBoxLogicalWidth(const Box& rubyBaseLayoutBox, const InlineFormattingContext&);
    static InlineLayoutUnit baseLogicalWidthFromRubyBaseEnd(const InlineItem& rubyBaseEnd, const Line::RunList&, const InlineContentBreaker::ContinuousContent::RunList&);
    static void applyRubyAlign(Line&, const InlineFormattingContext&);

private:
    bool annotationOverlapCheck(const InlineDisplay::Box&, const InlineLayoutRect& overhangingRect) const;
    InlineLayoutRect visualRectIncludingBlockDirection(const InlineLayoutRect& visualRectIgnoringBlockDirection) const;

    const InlineFormattingContext& parentFormattingContext() const { return m_parentFormattingContext; }

private:
    const InlineFormattingContext& m_parentFormattingContext;
};

} // namespace Layout
} // namespace WebCore
