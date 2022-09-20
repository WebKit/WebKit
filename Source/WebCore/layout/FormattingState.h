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

#include "FormattingContext.h"
#include "LayoutState.h"
#include <wtf/IsoMalloc.h>

namespace WebCore {
namespace Layout {

class Box;
class FloatingState;
enum class StyleDiff;

class FormattingState {
    WTF_MAKE_NONCOPYABLE(FormattingState);
    WTF_MAKE_ISO_ALLOCATED(FormattingState);
public:
    FloatingState& floatingState() const { return m_floatingState; }

    void markNeedsLayout(const Box&, StyleDiff);
    bool needsLayout(const Box&);

    void setIntrinsicWidthConstraintsForBox(const Box&, IntrinsicWidthConstraints);
    std::optional<IntrinsicWidthConstraints> intrinsicWidthConstraintsForBox(const Box&) const;
    void clearIntrinsicWidthConstraints(const Box&);

    void setIntrinsicWidthConstraints(IntrinsicWidthConstraints intrinsicWidthConstraints) { m_intrinsicWidthConstraints = intrinsicWidthConstraints; }
    std::optional<IntrinsicWidthConstraints> intrinsicWidthConstraints() const { return m_intrinsicWidthConstraints; }

    bool isBlockFormattingState() const { return m_type == Type::Block; }
    bool isInlineFormattingState() const { return m_type == Type::Inline; }
    bool isTableFormattingState() const { return m_type == Type::Table; }
    bool isFlexFormattingState() const { return m_type == Type::Flex; }

    LayoutState& layoutState() const { return m_layoutState; }

    // FIXME: We need to find a way to limit access to mutatable geometry.
    BoxGeometry& boxGeometry(const Box& layoutBox);
    // Since we layout the out-of-flow boxes at the end of the formatting context layout, it's okay to store them in the formatting state -as opposed to the containing block level.
    using OutOfFlowBoxList = Vector<CheckedRef<const Box>>;
    void addOutOfFlowBox(const Box& outOfFlowBox) { m_outOfFlowBoxes.append(outOfFlowBox); }
    void removeOutOfFlowBox(const Box&);
    const OutOfFlowBoxList& outOfFlowBoxes() const { return m_outOfFlowBoxes; }

protected:
    enum class Type { Block, Inline, Table, Flex };
    FormattingState(Ref<FloatingState>&&, Type, LayoutState&);
    ~FormattingState();

private:
    LayoutState& m_layoutState;
    Ref<FloatingState> m_floatingState;
    HashMap<const Box*, IntrinsicWidthConstraints> m_intrinsicWidthConstraintsForBoxes;
    std::optional<IntrinsicWidthConstraints> m_intrinsicWidthConstraints;
    // FIXME: This needs WeakListHashSet
    OutOfFlowBoxList m_outOfFlowBoxes;
    Type m_type;
};

inline void FormattingState::setIntrinsicWidthConstraintsForBox(const Box& layoutBox, IntrinsicWidthConstraints intrinsicWidthConstraints)
{
    ASSERT(!m_intrinsicWidthConstraintsForBoxes.contains(&layoutBox));
    ASSERT(&m_layoutState.formattingStateForFormattingContext(FormattingContext::formattingContextRoot(layoutBox)) == this);
    m_intrinsicWidthConstraintsForBoxes.set(&layoutBox, intrinsicWidthConstraints);
}

inline void FormattingState::clearIntrinsicWidthConstraints(const Box& layoutBox)
{
    m_intrinsicWidthConstraints = { };
    m_intrinsicWidthConstraintsForBoxes.remove(&layoutBox);
}

inline std::optional<IntrinsicWidthConstraints> FormattingState::intrinsicWidthConstraintsForBox(const Box& layoutBox) const
{
    ASSERT(&m_layoutState.formattingStateForFormattingContext(FormattingContext::formattingContextRoot(layoutBox)) == this);
    auto iterator = m_intrinsicWidthConstraintsForBoxes.find(&layoutBox);
    if (iterator == m_intrinsicWidthConstraintsForBoxes.end())
        return { };
    return iterator->value;
}

}
}

#define SPECIALIZE_TYPE_TRAITS_LAYOUT_FORMATTING_STATE(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::Layout::ToValueTypeName) \
    static bool isType(const WebCore::Layout::FormattingState& formattingState) { return formattingState.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()

