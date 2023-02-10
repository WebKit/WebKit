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

#include "InlineDisplayLine.h"
#include "InlineFormattingContext.h"
#include "InlineLineBuilder.h"

namespace WebCore {
namespace Layout {

class LineBox;

class InlineDisplayLineBuilder {
public:
    InlineDisplayLineBuilder(const InlineFormattingContext&);

    InlineDisplay::Line build(const LineBuilder::LineContent&, const LineBox&, const ConstraintsForInlineContent&) const;

    static std::optional<FloatRect> trailingEllipsisVisualRectAfterTruncation(LineEndingEllipsisPolicy, const InlineDisplay::Line&, DisplayBoxes&, bool isLastLineWithInlineContent);

private:
    struct EnclosingLineGeometry {
        InlineDisplay::Line::EnclosingTopAndBottom enclosingTopAndBottom;
        InlineRect contentOverflowRect;
    };
    EnclosingLineGeometry collectEnclosingLineGeometry(const LineBuilder::LineContent&, const LineBox&, const InlineRect& lineBoxRect) const;

    const InlineFormattingContext& formattingContext() const { return m_inlineFormattingContext; }
    const Box& root() const { return formattingContext().root(); }
    LayoutState& layoutState() const { return formattingContext().layoutState(); }

    const InlineFormattingContext& m_inlineFormattingContext;
};

}
}

