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

#include "InlineFormattingContext.h"
#include "InlineLayoutState.h"
#include "InlineLineBuilder.h"
#include "TextUtil.h"

namespace WebCore {
namespace Layout {

class Box;
class ElementBox;
class LayoutState;

class LineBoxBuilder {
public:
    LineBoxBuilder(const InlineFormattingContext&, const InlineLayoutState&, const LineBuilder::LayoutResult&);

    LineBox build(size_t lineIndex);

private:
    void setVerticalPropertiesForInlineLevelBox(const LineBox&, InlineLevelBox&) const;
    void setLayoutBoundsForInlineBox(InlineLevelBox&, FontBaseline) const;
    void adjustInlineBoxHeightsForLineBoxContainIfApplicable(LineBox&);
    void computeLineBoxGeometry(LineBox&) const;
    InlineLevelBox::AscentAndDescent enclosingAscentDescentWithFallbackFonts(const InlineLevelBox&, const TextUtil::FallbackFontList& fallbackFontsForContent, FontBaseline) const;
    TextUtil::FallbackFontList collectFallbackFonts(const InlineLevelBox& parentInlineBox, const Line::Run&, const RenderStyle&);
    void adjustMarginStartForListMarker(const ElementBox& listMarkerBox, LayoutUnit nestedListMarkerMarginStart, InlineLayoutUnit rootInlineBoxOffset) const;

    void constructInlineLevelBoxes(LineBox&);
    void adjustIdeographicBaselineIfApplicable(LineBox&);
    void adjustOutsideListMarkersPosition(LineBox&);

    bool isFirstLine() const { return lineLayoutResult().isFirstLast.isFirstFormattedLine != LineBuilder::LayoutResult::IsFirstLast::FirstFormattedLine::No; }
    bool isLastLine() const { return lineLayoutResult().isFirstLast.isLastLineWithInlineContent; }
    const InlineFormattingContext& formattingContext() const { return m_inlineFormattingContext; }
    const LineBuilder::LayoutResult& lineLayoutResult() const { return m_lineLayoutResult; }
    const Box& rootBox() const { return formattingContext().root(); }
    const RenderStyle& rootStyle() const { return isFirstLine() ? rootBox().firstLineStyle() : rootBox().style(); }

    const InlineLayoutState& inlineLayoutState() const { return m_inlineLayoutState; }
    const BlockLayoutState& blockLayoutState() const { return inlineLayoutState().parentBlockLayoutState(); }
    LayoutState& layoutState() const { return formattingContext().layoutState(); }

private:
    const InlineFormattingContext& m_inlineFormattingContext;
    const InlineLayoutState& m_inlineLayoutState;
    const LineBuilder::LayoutResult& m_lineLayoutResult;
    bool m_fallbackFontRequiresIdeographicBaseline { false };
    HashMap<const InlineLevelBox*, TextUtil::FallbackFontList> m_fallbackFontsForInlineBoxes;
    Vector<size_t> m_outsideListMarkers;
};

}
}

