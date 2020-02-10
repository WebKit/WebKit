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
#include "LayoutContainer.h"
#include <wtf/IsoMalloc.h>
#include <wtf/Ref.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

namespace Display {
class Box;
}

namespace Layout {

class Box;
class FloatingContext;
class LayoutState;

// FloatingState holds the floating boxes per formatting context.
class FloatingState : public RefCounted<FloatingState> {
    WTF_MAKE_ISO_ALLOCATED(FloatingState);
public:
    static Ref<FloatingState> create(LayoutState& layoutState, const Container& formattingContextRoot) { return adoptRef(*new FloatingState(layoutState, formattingContextRoot)); }

    const Container& root() const { return *m_formattingContextRoot; }

    Optional<PositionInContextRoot> top(const Container& formattingContextRoot) const;
    Optional<PositionInContextRoot> leftBottom(const Container& formattingContextRoot) const;
    Optional<PositionInContextRoot> rightBottom(const Container& formattingContextRoot) const;
    Optional<PositionInContextRoot> bottom(const Container& formattingContextRoot) const;

    class FloatItem {
    public:
        FloatItem(const Box&, Display::Box absoluteDisplayBox);

        enum class Position { Left, Right };
        FloatItem(Position, Display::Box absoluteDisplayBox);

        bool isLeftPositioned() const { return m_position == Position::Left; }
        bool isDescendantOfFormattingRoot(const Container&) const;

        Display::Rect rectWithMargin() const { return m_absoluteDisplayBox.rectWithMargin(); }
        UsedHorizontalMargin horizontalMargin() const { return m_absoluteDisplayBox.horizontalMargin(); }
        PositionInContextRoot bottom() const { return { m_absoluteDisplayBox.bottom() }; }

    private:
        WeakPtr<const Box> m_layoutBox;
        Position m_position;
        Display::Box m_absoluteDisplayBox;
    };
    using FloatList = Vector<FloatItem>;
    const FloatList& floats() const { return m_floats; }
    const FloatItem* last() const { return floats().isEmpty() ? nullptr : &m_floats.last(); }

    void append(FloatItem);
    void clear() { m_floats.clear(); }

private:
    friend class FloatingContext;
    FloatingState(LayoutState&, const Container& formattingContextRoot);

    LayoutState& layoutState() const { return m_layoutState; }

    Optional<PositionInContextRoot> bottom(const Container& formattingContextRoot, Clear) const;

    LayoutState& m_layoutState;
    WeakPtr<const Container> m_formattingContextRoot;
    FloatList m_floats;
};

inline Optional<PositionInContextRoot> FloatingState::leftBottom(const Container& formattingContextRoot) const
{ 
    ASSERT(formattingContextRoot.establishesFormattingContext());
    return bottom(formattingContextRoot, Clear::Left);
}

inline Optional<PositionInContextRoot> FloatingState::rightBottom(const Container& formattingContextRoot) const
{
    ASSERT(formattingContextRoot.establishesFormattingContext());
    return bottom(formattingContextRoot, Clear::Right);
}

inline Optional<PositionInContextRoot> FloatingState::bottom(const Container& formattingContextRoot) const
{
    ASSERT(formattingContextRoot.establishesFormattingContext());
    return bottom(formattingContextRoot, Clear::Both);
}

inline bool FloatingState::FloatItem::isDescendantOfFormattingRoot(const Container& formattingContextRoot) const
{
    ASSERT(formattingContextRoot.establishesFormattingContext());
    return m_layoutBox->isContainingBlockDescendantOf(downcast<Container>(formattingContextRoot));
}

}
}
#endif
