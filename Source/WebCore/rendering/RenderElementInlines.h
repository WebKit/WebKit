/**
 * Copyright (C) 2003-2023 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#include <WebCore/PseudoElement.h>
#include <WebCore/RenderBox.h>
#include <WebCore/RenderObjectDocument.h>
#include <WebCore/RenderObjectNode.h>
#include <WebCore/RenderStyleInlines.h>
#include <WebCore/StyleOpacity.h>
#include <WebCore/StyleShapeOutside.h>

namespace WebCore {

inline Overflow RenderElement::effectiveOverflowBlockDirection() const { return writingMode().isHorizontal() ? effectiveOverflowY() : effectiveOverflowX(); }
inline Overflow RenderElement::effectiveOverflowInlineDirection() const { return writingMode().isHorizontal() ? effectiveOverflowX() : effectiveOverflowY(); }
inline Element* RenderElement::element() const { return downcast<Element>(RenderObject::node()); }
inline RefPtr<Element> RenderElement::protectedElement() const { return element(); }
inline Element* RenderElement::nonPseudoElement() const { return downcast<Element>(RenderObject::nonPseudoNode()); }
inline RefPtr<Element> RenderElement::protectedNonPseudoElement() const { return nonPseudoElement(); }

inline bool RenderElement::isFixedPositioned() const
{
    return isOutOfFlowPositioned() && style().position() == PositionType::Fixed;
}

inline bool RenderElement::isAbsolutelyPositioned() const
{
    return isOutOfFlowPositioned() && style().position() == PositionType::Absolute;
}

inline bool RenderElement::isBlockLevelBox() const
{
    // block-level boxes are boxes that participate in a block formatting context.
    auto* renderBox = dynamicDowncast<RenderBox>(*this);
    if (!renderBox)
        return false;

    if (renderBox->isFlexItem() || renderBox->isGridItem() || renderBox->isRenderTableCell())
        return false;
    return style().isDisplayBlockLevel();
}

inline bool RenderElement::isAnonymousBlock() const
{
    return isAnonymous()
        && (style().display() == DisplayType::Block || style().display() == DisplayType::Box)
        && !style().pseudoElementType()
        && isRenderBlock()
#if ENABLE(MATHML)
        && !isRenderMathMLBlock()
#endif
        && !isRenderListMarker()
        && !isRenderFragmentedFlow()
        && !isRenderMultiColumnSet()
        && !isRenderView()
        && !isViewTransitionContainingBlock();
}

inline bool RenderElement::isBlockContainer() const
{
    auto display = style().display();
    return (display == DisplayType::Block
        || display == DisplayType::InlineBlock
        || display == DisplayType::FlowRoot
        || display == DisplayType::ListItem
        || display == DisplayType::TableCell
        || display == DisplayType::TableCaption) && !isRenderReplaced();
}

inline bool RenderElement::isBlockBox() const
{
    // A block-level box that is also a block container.
    return isBlockLevelBox() && isBlockContainer();
}

inline bool RenderElement::hasPotentiallyScrollableOverflow() const
{
    // We only need to test one overflow dimension since 'visible' and 'clip' always get accompanied
    // with 'clip' or 'visible' in the other dimension (see Style::Adjuster::adjust).
    return hasNonVisibleOverflow() && style().overflowX() != Overflow::Clip && style().overflowX() != Overflow::Visible;
}

inline bool RenderElement::isBeforeContent() const
{
    // Text nodes don't have their own styles, so ignore the style on a text node.
    // if (isRenderText())
    //     return false;
    if (style().pseudoElementType() != PseudoElementType::Before)
        return false;
    return true;
}

inline bool RenderElement::isAfterContent() const
{
    // Text nodes don't have their own styles, so ignore the style on a text node.
    // if (isRenderText())
    //     return false;
    if (style().pseudoElementType() != PseudoElementType::After)
        return false;
    return true;
}

inline bool RenderElement::isBeforeOrAfterContent() const
{
    return isBeforeContent() || isAfterContent();
}

inline Element* RenderElement::generatingElement() const
{
    return isPseudoElement() ? downcast<PseudoElement>(*element()).hostElement() : element();
}

} // namespace WebCore
