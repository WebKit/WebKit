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

#include "LayoutBoxGeometry.h"
#include "LayoutElementBox.h"
#include "Shape.h"
#include <wtf/IsoMalloc.h>
#include <wtf/OptionSet.h>

namespace WebCore {
namespace Layout {

class Box;
class BoxGeometry;
class Rect;

// PlacedFloats holds the floating boxes for BFC/IFC using the BFC's writing mode.
// PlacedFloats may be inherited by IFCs with mismataching writing mode. In such cases floats
// are added to PlacledFloats as if they had matching inline direction (i.e. all boxes within PlacedFloats share the same writing mode)
class PlacedFloats {
    WTF_MAKE_ISO_ALLOCATED(PlacedFloats);
public:
    PlacedFloats(const ElementBox& blockFormattingContextRoot);

    const ElementBox& formattingContextRoot() const { return m_blockFormattingContextRoot; }

    class Item {
    public:
        // FIXME: This c'tor is only used by the render tree integation codepath.
        enum class Position { Left, Right };
        Item(Position, const BoxGeometry& absoluteBoxGeometry, LayoutPoint localTopLeft, const Shape*);
        Item(const Box&, Position, const BoxGeometry& absoluteBoxGeometry, LayoutPoint localTopLeft, std::optional<size_t> line);

        ~Item();

        bool isLeftPositioned() const { return m_position == Position::Left; }
        bool isRightPositioned() const { return m_position == Position::Right; }
        bool isInFormattingContextOf(const ElementBox& formattingContextRoot) const;

        BoxGeometry boxGeometry() const;

        Rect absoluteRectWithMargin() const { return BoxGeometry::marginBoxRect(m_absoluteBoxGeometry); }
        Rect absoluteBorderBoxRect() const { return BoxGeometry::borderBoxRect(m_absoluteBoxGeometry); }
        BoxGeometry::HorizontalEdges horizontalMargin() const { return m_absoluteBoxGeometry.horizontalMargin(); }
        PositionInContextRoot absoluteBottom() const { return { absoluteRectWithMargin().bottom() }; }

        const Shape* shape() const { return m_shape.get(); }
        std::optional<size_t> placedByLine() const { return m_placedByLine; }

        const Box* layoutBox() const { return m_layoutBox.get(); }

    private:
        CheckedPtr<const Box> m_layoutBox;
        Position m_position;
        BoxGeometry m_absoluteBoxGeometry;
        LayoutPoint m_localTopLeft;
        RefPtr<const Shape> m_shape;
        std::optional<size_t> m_placedByLine;
    };
    using List = Vector<Item>;
    const List& list() const { return m_list; }
    const Item* last() const { return list().isEmpty() ? nullptr : &m_list.last(); }

    void append(Item);
    void clear();

    bool isEmpty() const { return list().isEmpty(); }
    bool hasLeftPositioned() const;
    bool hasRightPositioned() const;

    bool isLeftToRightDirection() const { return m_isLeftToRightDirection; }
    // FIXME: This should always be placedFloats's root().style().isLeftToRightDirection() if we used the actual containing block of the intrusive
    // floats to initiate the floating state in the integration codepath (i.e. when the float comes from the parent BFC).
    void setIsLeftToRightDirection(bool isLeftToRightDirection) { m_isLeftToRightDirection = isLeftToRightDirection; }

    void shrinkToFit();

private:
    CheckedRef<const ElementBox> m_blockFormattingContextRoot;
    List m_list;
    enum class PositionType {
        Left = 1 << 0,
        Right  = 1 << 1
    };
    OptionSet<PositionType> m_positionTypes;
    bool m_isLeftToRightDirection { true };
};

inline bool PlacedFloats::hasLeftPositioned() const
{
    return m_positionTypes.contains(PositionType::Left);
}

inline bool PlacedFloats::hasRightPositioned() const
{
    return m_positionTypes.contains(PositionType::Right);
}

}
}
