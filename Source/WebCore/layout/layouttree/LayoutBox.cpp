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

#include "config.h"
#include "LayoutBox.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "LayoutContainer.h"
#include "RenderStyle.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {
namespace Layout {

WTF_MAKE_ISO_ALLOCATED_IMPL(Box);

Box::Box(Optional<ElementAttributes> attributes, RenderStyle&& style, BaseTypeFlags baseTypeFlags)
    : m_style(WTFMove(style))
    , m_elementAttributes(attributes)
    , m_baseTypeFlags(baseTypeFlags)
{
    if (m_elementAttributes && m_elementAttributes.value().elementType == ElementType::Replaced)
        m_replaced = std::make_unique<Replaced>(*this);
}

Box::~Box()
{
}

bool Box::establishesFormattingContext() const
{
    return establishesBlockFormattingContext() || establishesInlineFormattingContext();
}

bool Box::establishesBlockFormattingContext() const
{
    // Initial Containing Block always creates a new (inital) block formatting context.
    if (!parent())
        return true;

    // 9.4.1 Block formatting contexts
    // Floats, absolutely positioned elements, block containers (such as inline-blocks, table-cells, and table-captions)
    // that are not block boxes, and block boxes with 'overflow' other than 'visible' (except when that value has been propagated to the viewport)
    // establish new block formatting contexts for their contents.
    if (isFloatingPositioned() || isAbsolutelyPositioned())
        return true;

    if (isBlockContainerBox() && !isBlockLevelBox())
        return true;

    if (isBlockLevelBox() && !isOverflowVisible())
        return true;

    return false;
}

bool Box::establishesBlockFormattingContextOnly() const
{
    return establishesBlockFormattingContext() && !establishesInlineFormattingContext();
}

bool Box::isRelativelyPositioned() const
{
    return m_style.position() == PositionType::Relative;
}

bool Box::isStickyPositioned() const
{
    return m_style.position() == PositionType::Sticky;
}

bool Box::isAbsolutelyPositioned() const
{
    return m_style.position() == PositionType::Absolute || isFixedPositioned(); 
}

bool Box::isFixedPositioned() const
{
    return m_style.position() == PositionType::Fixed;
}

bool Box::isFloatingPositioned() const
{
    return m_style.floating() != Float::No;
}

bool Box::isLeftFloatingPositioned() const
{
    return m_style.floating() == Float::Left;
}

bool Box::isRightFloatingPositioned() const
{
    return m_style.floating() == Float::Right;
}

bool Box::hasFloatClear() const
{
    return m_style.clear() != Clear::None;
}

const Container* Box::containingBlock() const
{
    // The containing block in which the root element lives is a rectangle called the initial containing block.
    // For other elements, if the element's position is 'relative' or 'static', the containing block is formed by the
    // content edge of the nearest block container ancestor box.
    // If the element has 'position: fixed', the containing block is established by the viewport
    // If the element has 'position: absolute', the containing block is established by the nearest ancestor with a
    // 'position' of 'absolute', 'relative' or 'fixed'.
    if (!parent())
        return nullptr;

    if (!isPositioned() || isInFlowPositioned()) {
        auto* nearestBlockContainer = parent();
        for (; nearestBlockContainer->parent() && !nearestBlockContainer->isBlockContainerBox(); nearestBlockContainer = nearestBlockContainer->parent()) { }
        return nearestBlockContainer;
    }

    if (isFixedPositioned()) {
        auto* ancestor = parent();
        for (; ancestor->parent() && !ancestor->style().hasTransform(); ancestor = ancestor->parent()) { }
        return ancestor;
    }

    if (isOutOfFlowPositioned()) {
        auto* ancestor = parent();
        for (; ancestor->parent() && !ancestor->isPositioned() && !ancestor->style().hasTransform(); ancestor = ancestor->parent()) { }
        return ancestor;
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}

const Container& Box::formattingContextRoot() const
{
    for (auto* ancestor = containingBlock(); ancestor; ancestor = ancestor->containingBlock()) {
        if (ancestor->establishesFormattingContext())
            return *ancestor;
    }

    // Initial containing block always establishes a formatting context.
    if (isInitialContainingBlock())
        return downcast<Container>(*this);

    RELEASE_ASSERT_NOT_REACHED();
}

const Container& Box::initialContainingBlock() const
{
    if (isInitialContainingBlock())
        return downcast<Container>(*this);

    auto* parent = this->parent();
    for (; parent->parent(); parent = parent->parent()) { }

    return *parent;
}

bool Box::isDescendantOf(const Container& container) const
{ 
    for (auto* ancestor = containingBlock(); ancestor; ancestor = ancestor->containingBlock()) {
        if (ancestor == &container)
            return true;
    }
    return false;
}

bool Box::isInlineBlockBox() const
{
    return m_style.display() == DisplayType::InlineBlock;
}

bool Box::isBlockLevelBox() const
{
    // Block level elements generate block level boxes.
    auto display = m_style.display();
    return display == DisplayType::Block || display == DisplayType::ListItem || display == DisplayType::Table;
}

bool Box::isInlineLevelBox() const
{
    // Inline level elements generate inline level boxes.
    auto display = m_style.display();
    return display == DisplayType::Inline || display == DisplayType::InlineBlock || display == DisplayType::InlineTable;
}

bool Box::isBlockContainerBox() const
{
    // Inline level elements generate inline level boxes.
    auto display = m_style.display();
    return display == DisplayType::Block || display == DisplayType::ListItem || display == DisplayType::InlineBlock || display == DisplayType::TableCell || display == DisplayType::TableCaption; // TODO && !replaced element
}

bool Box::isInitialContainingBlock() const
{
    return !parent();
}

const Box* Box::nextInFlowSibling() const
{
    if (auto* nextSibling = this->nextSibling()) {
        if (nextSibling->isInFlow())
            return nextSibling;
        return nextSibling->nextInFlowSibling();
    }
    return nullptr;
}

const Box* Box::nextInFlowOrFloatingSibling() const
{
    if (auto* nextSibling = this->nextSibling()) {
        if (nextSibling->isInFlow() || nextSibling->isFloatingPositioned())
            return nextSibling;
        return nextSibling->nextInFlowSibling();
    }
    return nullptr;
}

const Box* Box::previousInFlowSibling() const
{
    if (auto* previousSibling = this->previousSibling()) {
        if (previousSibling->isInFlow())
            return previousSibling;
        return previousSibling->previousInFlowSibling();
    }
    return nullptr;
}

const Box* Box::previousInFlowOrFloatingSibling() const
{
    if (auto* previousSibling = this->previousSibling()) {
        if (previousSibling->isInFlow() || previousSibling->isFloatingPositioned())
            return previousSibling;
        return previousSibling->previousInFlowOrFloatingSibling();
    }
    return nullptr;
}

bool Box::isOverflowVisible() const
{
    auto isOverflowVisible = m_style.overflowX() == Overflow::Visible || m_style.overflowY() == Overflow::Visible;
    // UAs must apply the 'overflow' property set on the root element to the viewport. When the root element is an HTML "HTML" element
    // or an XHTML "html" element, and that element has an HTML "BODY" element or an XHTML "body" element as a child,
    // user agents must instead apply the 'overflow' property from the first such child element to the viewport,
    // if the value on the root element is 'visible'. The 'visible' value when used for the viewport must be interpreted as 'auto'.
    // The element from which the value is propagated must have a used value for 'overflow' of 'visible'.
    if (isBodyBox()) {
        auto* documentBox = parent();
        ASSERT(documentBox);
        if (!documentBox->isDocumentBox())
            return isOverflowVisible;
        if (!documentBox->isOverflowVisible())
            return isOverflowVisible;
        return true;
    }
    if (isInitialContainingBlock()) {
        auto* documentBox = downcast<Container>(*this).firstChild();
        if (!documentBox || !documentBox->isDocumentBox() || !is<Container>(documentBox))
            return isOverflowVisible;
        auto* bodyBox = downcast<Container>(documentBox)->firstChild();
        if (!bodyBox || !bodyBox->isBodyBox())
            return isOverflowVisible;
        auto& bodyBoxStyle = bodyBox->style();
        return bodyBoxStyle.overflowX() == Overflow::Visible || bodyBoxStyle.overflowY() == Overflow::Visible;
    }
    return isOverflowVisible;
}

bool Box::isPaddingApplicable() const
{
    // 8.4 Padding properties:
    // Applies to: all elements except table-row-group, table-header-group, table-footer-group, table-row, table-column-group and table-column
    if (isAnonymous())
        return false;

    auto elementType = m_elementAttributes.value().elementType;
    return elementType != ElementType::TableRowGroup
        && elementType != ElementType::TableHeaderGroup
        && elementType != ElementType::TableFooterGroup
        && elementType != ElementType::TableRow
        && elementType != ElementType::TableColumnGroup
        && elementType != ElementType::TableColumn;
}

}
}

#endif
