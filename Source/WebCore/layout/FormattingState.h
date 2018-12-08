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

#include "FloatingState.h"
#include "FormattingContext.h"
#include "LayoutBox.h"
#include "LayoutState.h"
#include "LayoutUnit.h"
#include <wtf/IsoMalloc.h>

namespace WebCore {

namespace Layout {

class Box;
enum class StyleDiff;

class FormattingState {
    WTF_MAKE_ISO_ALLOCATED(FormattingState);
public:
    virtual ~FormattingState();

    virtual std::unique_ptr<FormattingContext> createFormattingContext(const Box& formattingContextRoot) = 0;

    FloatingState& floatingState() const { return m_floatingState; }

    void markNeedsLayout(const Box&, StyleDiff);
    bool needsLayout(const Box&);

    void setInstrinsicWidthConstraints(const Box&,  FormattingContext::InstrinsicWidthConstraints);
    void clearInstrinsicWidthConstraints(const Box&);
    std::optional<FormattingContext::InstrinsicWidthConstraints> instrinsicWidthConstraints(const Box&) const;

    bool isBlockFormattingState() const { return m_type == Type::Block; }
    bool isInlineFormattingState() const { return m_type == Type::Inline; }

    LayoutState& layoutState() const { return m_layoutState; }

protected:
    enum class Type { Block, Inline };
    FormattingState(Ref<FloatingState>&&, Type, LayoutState&);

private:
    LayoutState& m_layoutState;
    Ref<FloatingState> m_floatingState;
    HashMap<const Box*, FormattingContext::InstrinsicWidthConstraints> m_instrinsicWidthConstraints;
    Type m_type;
};

inline void FormattingState::setInstrinsicWidthConstraints(const Box& layoutBox, FormattingContext::InstrinsicWidthConstraints instrinsicWidthConstraints)
{
    ASSERT(!m_instrinsicWidthConstraints.contains(&layoutBox));
    ASSERT(&m_layoutState.formattingStateForBox(layoutBox) == this);
    m_instrinsicWidthConstraints.set(&layoutBox, instrinsicWidthConstraints);
}

inline void FormattingState::clearInstrinsicWidthConstraints(const Box& layoutBox)
{
    m_instrinsicWidthConstraints.remove(&layoutBox);
}

inline std::optional<FormattingContext::InstrinsicWidthConstraints> FormattingState::instrinsicWidthConstraints(const Box& layoutBox) const
{
    ASSERT(&m_layoutState.formattingStateForBox(layoutBox) == this);
    auto iterator = m_instrinsicWidthConstraints.find(&layoutBox);
    if (iterator == m_instrinsicWidthConstraints.end())
        return { };
    return iterator->value;
}

}
}

#define SPECIALIZE_TYPE_TRAITS_LAYOUT_FORMATTING_STATE(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::Layout::ToValueTypeName) \
    static bool isType(const WebCore::Layout::FormattingState& formattingState) { return formattingState.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()

#endif
