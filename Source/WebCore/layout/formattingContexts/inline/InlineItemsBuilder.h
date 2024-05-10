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

#include "InlineContentCache.h"
#include "InlineItem.h"
#include "InlineLineTypes.h"
#include "LayoutElementBox.h"
#include "SecurityOrigin.h"
#include <wtf/HashMap.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
namespace Layout {
class InlineTextBox;

class InlineItemsBuilder {
public:
    InlineItemsBuilder(InlineContentCache&, const ElementBox& root, const SecurityOrigin&);
    void build(InlineItemPosition startPosition);

    static void populateBreakingPositionCache(const InlineItemList&, const Document&);

private:
    void collectInlineItems(InlineItemList&, InlineItemPosition startPosition);
    using LayoutQueue = Vector<CheckedRef<const Box>, 8>;
    LayoutQueue initializeLayoutQueue(InlineItemPosition startPosition);
    LayoutQueue traverseUntilDamaged(const Box& firstDamagedLayoutBox);
    void breakAndComputeBidiLevels(InlineItemList&);
    void computeInlineTextItemWidths(InlineItemList&);

    void handleTextContent(const InlineTextBox&, InlineItemList&, std::optional<size_t> partialContentOffset);
    bool buildInlineItemListForTextFromBreakingPositionsCache(const InlineTextBox&, InlineItemList&);
    void handleInlineBoxStart(const Box&, InlineItemList&);
    void handleInlineBoxEnd(const Box&, InlineItemList&);
    void handleInlineLevelBox(const Box&, InlineItemList&);
    
    bool contentRequiresVisualReordering() const { return m_contentRequiresVisualReordering; }

    const ElementBox& root() const { return m_root; }
    InlineContentCache& inlineContentCache() { return m_inlineContentCache; }

private:
    InlineContentCache& m_inlineContentCache;
    const ElementBox& m_root;
    const SecurityOrigin& m_securityOrigin;

    bool m_contentRequiresVisualReordering { false };
    bool m_isTextAndForcedLineBreakOnlyContent { true };
    size_t m_inlineBoxCount { 0 };
};

}
}

