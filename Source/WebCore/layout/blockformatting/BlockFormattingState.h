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

#include "FormattingState.h"
#include "MarginTypes.h"
#include <wtf/IsoMalloc.h>

namespace WebCore {

namespace Layout {

// BlockFormattingState holds the state for a particular block formatting context tree.
class BlockFormattingState : public FormattingState {
    WTF_MAKE_ISO_ALLOCATED(BlockFormattingState);
public:
    BlockFormattingState(Ref<FloatingState>&&, LayoutState&);
    virtual ~BlockFormattingState();

    std::unique_ptr<FormattingContext> createFormattingContext(const Box& formattingContextRoot) override;

    void setPositiveAndNegativeVerticalMargin(const Box& layoutBox, PositiveAndNegativeVerticalMargin verticalMargin) { m_positiveAndNegativeVerticalMargin.set(&layoutBox, verticalMargin); }
    bool hasPositiveAndNegativeVerticalMargin(const Box& layoutBox) const { return m_positiveAndNegativeVerticalMargin.contains(&layoutBox); }
    PositiveAndNegativeVerticalMargin positiveAndNegativeVerticalMargin(const Box& layoutBox) const { return m_positiveAndNegativeVerticalMargin.get(&layoutBox); }

    void setHasEstimatedMarginBefore(const Box& layoutBox) { m_estimatedMarginBeforeList.add(&layoutBox); }
    void clearHasEstimatedMarginBefore(const Box& layoutBox) { m_estimatedMarginBeforeList.remove(&layoutBox); }
    bool hasEstimatedMarginBefore(const Box& layoutBox) const { return m_estimatedMarginBeforeList.contains(&layoutBox); }

private:
    HashMap<const Box*, PositiveAndNegativeVerticalMargin> m_positiveAndNegativeVerticalMargin;
    HashSet<const Box*> m_estimatedMarginBeforeList;
};

}
}

SPECIALIZE_TYPE_TRAITS_LAYOUT_FORMATTING_STATE(BlockFormattingState, isBlockFormattingState())

#endif
