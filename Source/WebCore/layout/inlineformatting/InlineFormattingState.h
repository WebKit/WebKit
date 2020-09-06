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

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "DisplayInlineContent.h"
#include "FormattingState.h"
#include "InlineItem.h"
#include <wtf/IsoMalloc.h>

namespace WebCore {
namespace Layout {

using InlineItems = Vector<InlineItem>;

// InlineFormattingState holds the state for a particular inline formatting context tree.
class InlineFormattingState : public FormattingState {
    WTF_MAKE_ISO_ALLOCATED(InlineFormattingState);
public:
    InlineFormattingState(Ref<FloatingState>&&, LayoutState&);
    ~InlineFormattingState();

    InlineItems& inlineItems() { return m_inlineItems; }
    const InlineItems& inlineItems() const { return m_inlineItems; }
    void addInlineItem(InlineItem&& inlineItem) { m_inlineItems.append(WTFMove(inlineItem)); }

    const Display::InlineContent* displayInlineContent() const { return m_displayInlineContent.get(); }
    Display::InlineContent& ensureDisplayInlineContent();

    void clearDisplayInlineContent() { m_displayInlineContent = nullptr; }
    void shrinkDisplayInlineContent();

private:
    // Cacheable input to line layout.
    InlineItems m_inlineItems;

    RefPtr<Display::InlineContent> m_displayInlineContent;
};

inline Display::InlineContent& InlineFormattingState::ensureDisplayInlineContent()
{
    if (!m_displayInlineContent)
        m_displayInlineContent = adoptRef(*new Display::InlineContent);
    return *m_displayInlineContent;
}

inline void InlineFormattingState::shrinkDisplayInlineContent()
{
    if (!m_displayInlineContent)
        return;
    m_displayInlineContent->runs.shrinkToFit();
    m_displayInlineContent->lines.shrinkToFit();
}

}
}

SPECIALIZE_TYPE_TRAITS_LAYOUT_FORMATTING_STATE(InlineFormattingState, isInlineFormattingState())

#endif
