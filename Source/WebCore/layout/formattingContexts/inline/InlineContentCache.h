/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "FormattingConstraints.h"
#include "InlineDisplayContent.h"
#include "InlineItem.h"
#include "IntrinsicWidthHandler.h"
#include <wtf/IsoMalloc.h>

namespace WebCore {
namespace Layout {

// InlineContentCache is used to cache content for subsequent layouts.
class InlineContentCache {
    WTF_MAKE_ISO_ALLOCATED_INLINE(InlineContentCache);
public:
    struct InlineItems {
        InlineItemList& content() { return m_inlineItemList; }
        const InlineItemList& content() const { return m_inlineItemList; }

        void set(InlineItemList&& inlineItemList) { m_inlineItemList = WTFMove(inlineItemList); }
        void append(InlineItemList&& inlineItemList) { m_inlineItemList.appendVector(WTFMove(inlineItemList)); }
        void clear() { m_inlineItemList.clear(); }
        void shrinkToFit() { m_inlineItemList.shrinkToFit(); }

        bool isEmpty() const { return content().isEmpty(); }
        size_t size() const { return content().size(); }

        void setRequiresVisualReordering(bool contentRequiresVisualReordering) { m_contentRequiresVisualReordering = contentRequiresVisualReordering; }
        bool requiresVisualReordering() const { return m_contentRequiresVisualReordering; }

        void setIsNonBidiTextAndForcedLineBreakOnlyContent(bool isNonBidiTextAndForcedLineBreakOnlyContent) { m_isNonBidiTextAndForcedLineBreakOnlyContent = isNonBidiTextAndForcedLineBreakOnlyContent; }
        bool isNonBidiTextAndForcedLineBreakOnlyContent() const { return m_isNonBidiTextAndForcedLineBreakOnlyContent; }

    private:
        InlineItemList m_inlineItemList;
        bool m_contentRequiresVisualReordering { false };
        bool m_isNonBidiTextAndForcedLineBreakOnlyContent { false };
    };
    const InlineItems& inlineItems() const { return m_inlineItems; }
    InlineItems& inlineItems() { return m_inlineItems; }

    void setMaximumIntrinsicWidthLayoutResult(IntrinsicWidthHandler::LineBreakingResult&& layoutResult) { m_maximumIntrinsicWidthLayoutResult = WTFMove(layoutResult); }
    void clearMaximumIntrinsicWidthLayoutResult() { m_maximumIntrinsicWidthLayoutResult = { }; }
    std::optional<IntrinsicWidthHandler::LineBreakingResult>& maximumIntrinsicWidthLayoutResult() { return m_maximumIntrinsicWidthLayoutResult; }

    void setIntrinsicWidthConstraints(IntrinsicWidthConstraints intrinsicWidthConstraints) { m_intrinsicWidthConstraints = intrinsicWidthConstraints; }
    void resetIntrinsicWidthConstraints() { m_intrinsicWidthConstraints = { }; }
    std::optional<IntrinsicWidthConstraints> intrinsicWidthConstraints() const { return m_intrinsicWidthConstraints; }

private:
    InlineItems m_inlineItems;
    std::optional<IntrinsicWidthHandler::LineBreakingResult> m_maximumIntrinsicWidthLayoutResult { };
    std::optional<IntrinsicWidthConstraints> m_intrinsicWidthConstraints { };
};

}
}

