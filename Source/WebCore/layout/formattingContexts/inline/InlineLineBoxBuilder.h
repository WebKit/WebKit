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
#include "InlineLineBuilder.h"
#include "TextUtil.h"

namespace WebCore {
namespace Layout {

class Box;
class ElementBox;
class LayoutState;

class LineBoxBuilder {
public:
    LineBoxBuilder(const InlineFormattingContext&);

    LineBox build(const LineBuilder::LineContent&, size_t lineIndex);

private:
    void setVerticalPropertiesForInlineLevelBox(const LineBox&, InlineLevelBox&) const;
    void adjustLayoutBoundsWithFallbackFonts(InlineLevelBox&, const TextUtil::FallbackFontList& fallbackFontsForContent, FontBaseline) const;
    TextUtil::FallbackFontList collectFallbackFonts(const InlineLevelBox& parentInlineBox, const Line::Run&, const RenderStyle&);

    void constructInlineLevelBoxes(LineBox&, const LineBuilder::LineContent&, size_t lineIndex);
    void adjustIdeographicBaselineIfApplicable(LineBox&, size_t lineIndex);

    const InlineFormattingContext& formattingContext() const { return m_inlineFormattingContext; }
    const Box& rootBox() const { return formattingContext().root(); }
    LayoutState& layoutState() const { return formattingContext().layoutState(); }

private:
    const InlineFormattingContext& m_inlineFormattingContext;
    bool m_fallbackFontRequiresIdeographicBaseline { false };
    HashMap<const InlineLevelBox*, TextUtil::FallbackFontList> m_fallbackFontsForInlineBoxes;
};

}
}

