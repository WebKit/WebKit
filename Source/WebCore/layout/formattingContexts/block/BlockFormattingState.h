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

#include "FloatingState.h"
#include "FormattingState.h"
#include <wtf/HashSet.h>
#include <wtf/IsoMalloc.h>

namespace WebCore {
namespace Layout {

// BlockFormattingState holds the state for a particular block formatting context tree.
class BlockFormattingState : public FormattingState {
    WTF_MAKE_ISO_ALLOCATED(BlockFormattingState);
public:
    BlockFormattingState(LayoutState&, const ElementBox& blockFormattingContextRoot);
    ~BlockFormattingState();

    const FloatingState& floatingState() const { return m_floatingState; }
    FloatingState& floatingState() { return m_floatingState; }

    void setUsedVerticalMargin(const Box& layoutBox, const UsedVerticalMargin& usedVerticalMargin) { m_usedVerticalMargins.set(layoutBox, usedVerticalMargin); }
    UsedVerticalMargin usedVerticalMargin(const Box& layoutBox) const { return m_usedVerticalMargins.get(layoutBox); }
    bool hasUsedVerticalMargin(const Box& layoutBox) const { return m_usedVerticalMargins.contains(layoutBox); }

    void setHasClearance(const Box& layoutBox) { m_clearanceSet.add(layoutBox); }
    void clearHasClearance(const Box& layoutBox) { m_clearanceSet.remove(layoutBox); }
    bool hasClearance(const Box& layoutBox) const { return m_clearanceSet.contains(layoutBox); }

private:
    FloatingState m_floatingState;
    HashMap<CheckedRef<const Box>, UsedVerticalMargin> m_usedVerticalMargins;
    HashSet<CheckedRef<const Box>> m_clearanceSet;
};

}
}

SPECIALIZE_TYPE_TRAITS_LAYOUT_FORMATTING_STATE(BlockFormattingState, isBlockFormattingState())

