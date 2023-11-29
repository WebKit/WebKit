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

#include "LineLayoutResult.h"

namespace WebCore {
namespace Layout {

class AbstractLineBuilder;
class InlineFormattingContext;
class InlineContentCache;
struct IntrinsicWidthConstraints;

class IntrinsicWidthHandler {
public:
    IntrinsicWidthHandler(InlineFormattingContext&, const InlineItemList&, bool mayUseSimplifiedTextOnlyInlineLayout);

    LayoutUnit minimumContentSize();
    LayoutUnit maximumContentSize();

    struct LineBreakingResult {
        InlineLayoutUnit constraint;
        LineLayoutResult result;
    };
    std::optional<LineBreakingResult>& maximumIntrinsicWidthResult() { return m_maximumIntrinsicWidthResultForSingleLine; }

private:
    enum class MayCacheLayoutResult : bool { No, Yes };
    InlineLayoutUnit computedIntrinsicWidthForConstraint(IntrinsicWidthMode, AbstractLineBuilder&, MayCacheLayoutResult = MayCacheLayoutResult::No);
    InlineLayoutUnit simplifiedMinimumWidth(const ElementBox& root) const;
    InlineLayoutUnit simplifiedMaximumWidth(MayCacheLayoutResult = MayCacheLayoutResult::No);

    InlineFormattingContext& formattingContext();
    const InlineFormattingContext& formattingContext() const;
    const InlineContentCache& formattingState() const;
    const ElementBox& root() const;

private:
    InlineFormattingContext& m_inlineFormattingContext;
    const InlineItemList& m_inlineItemList;
    const bool m_mayUseSimplifiedTextOnlyInlineLayout { false };

    std::optional<LineBreakingResult> m_maximumIntrinsicWidthResultForSingleLine { };
};

}
}

