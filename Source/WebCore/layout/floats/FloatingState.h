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

#include "DisplayBox.h"
#include <wtf/IsoMalloc.h>
#include <wtf/Ref.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

namespace Layout {

class Box;
class Container;
class FormattingState;
class LayoutContext;

// FloatingState holds the floating boxes per formatting context.
class FloatingState : public RefCounted<FloatingState> {
    WTF_MAKE_ISO_ALLOCATED(FloatingState);
public:
    static Ref<FloatingState> create(LayoutContext& layoutContext, const Box& formattingContextRoot) { return adoptRef(*new FloatingState(layoutContext, formattingContextRoot)); }

    void append(const Box& layoutBox);
    void remove(const Box& layoutBox);

    bool isEmpty() const { return m_floats.isEmpty(); }

    std::optional<LayoutUnit> leftBottom(const Box& formattingContextRoot) const;
    std::optional<LayoutUnit> rightBottom(const Box& formattingContextRoot) const;
    std::optional<LayoutUnit> bottom(const Box& formattingContextRoot) const;

    class FloatItem {
    public:
        FloatItem(const Box&, const FloatingState&);

        const Box& layoutBox() const { return *m_layoutBox; }
        const Container& containingBlock() const { return *m_containingBlock; }

        const Display::Box& displayBox() const { return m_absoluteDisplayBox; }
        const Display::Box& containingBlockDisplayBox() const { return m_containingBlockAbsoluteDisplayBox; }

    private:
        WeakPtr<Box> m_layoutBox;
        WeakPtr<Container> m_containingBlock;

        Display::Box m_absoluteDisplayBox;
        Display::Box m_containingBlockAbsoluteDisplayBox;
    };
    using FloatList = Vector<FloatItem>;
    const FloatList& floats() const { return m_floats; }
    const FloatItem* last() const { return isEmpty() ? nullptr : &m_floats.last(); }

private:
    friend class FloatingContext;
    FloatingState(LayoutContext&, const Box& formattingContextRoot);

    LayoutContext& layoutContext() const { return m_layoutContext; }
    const Box& root() const { return *m_formattingContextRoot; }

    std::optional<LayoutUnit> bottom(const Box& formattingContextRoot, Clear) const;

    LayoutContext& m_layoutContext;
    WeakPtr<Box> m_formattingContextRoot;
    FloatList m_floats;
};

inline std::optional<LayoutUnit> FloatingState::leftBottom(const Box& formattingContextRoot) const
{ 
    return bottom(formattingContextRoot, Clear::Left);
}

inline std::optional<LayoutUnit> FloatingState::rightBottom(const Box& formattingContextRoot) const
{
    return bottom(formattingContextRoot, Clear::Right);
}

inline std::optional<LayoutUnit> FloatingState::bottom(const Box& formattingContextRoot) const
{
    return bottom(formattingContextRoot, Clear::Both);
}

}
}
#endif
