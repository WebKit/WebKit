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

#include "RenderStyle.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {
namespace Layout {

WTF_MAKE_ISO_ALLOCATED_IMPL(Box);

Box::Box(RenderStyle&& style, BaseTypeFlags baseTypeFlags)
    : m_style(WTFMove(style))
    , m_baseTypeFlags(baseTypeFlags)
    , m_isAnonymous(false)
{
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

bool Box::isRelativelyPositioned() const
{
    return m_style.position() == RelativePosition;
}

bool Box::isStickyPositioned() const
{
    return m_style.position() == StickyPosition;
}

bool Box::isAbsolutelyPositioned() const
{
    return m_style.position() == AbsolutePosition;
}

bool Box::isFixedPositioned() const
{
    return m_style.position() == FixedPosition;
}

bool Box::isFloatingPositioned() const
{
    return m_style.floating() != NoFloat;
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
        auto* initialContainingBlock = parent();
        for (; initialContainingBlock->parent(); initialContainingBlock = initialContainingBlock->parent()) { }
        return initialContainingBlock;
    }

    if (isOutOfFlowPositioned()) {
        auto* positionedAncestor = parent();
        for (; positionedAncestor->parent() && !positionedAncestor->isPositioned(); positionedAncestor = positionedAncestor->parent()) { }
        return positionedAncestor;
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

bool Box::isDescendantOf(Container& container) const
{
    auto* ancestor = parent();
    for (; ancestor && ancestor != &container; ancestor = ancestor->parent()) { }
    return ancestor;
}

bool Box::isInlineBlockBox() const
{
    return m_style.display() == INLINE_BLOCK;
}

bool Box::isBlockLevelBox() const
{
    // Block level elements generate block level boxes.
    auto display = m_style.display();
    return display == BLOCK || display == LIST_ITEM || display == TABLE;
}

bool Box::isInlineLevelBox() const
{
    // Inline level elements generate inline level boxes.
    auto display = m_style.display();
    return display == INLINE || display == INLINE_BLOCK || display == INLINE_TABLE;
}

bool Box::isBlockContainerBox() const
{
    // Inline level elements generate inline level boxes.
    auto display = m_style.display();
    return display == BLOCK || display == LIST_ITEM || display == INLINE_BLOCK || display == TABLE_CELL || display == TABLE_CAPTION; // TODO && !replaced element
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
    return m_style.overflowX() == OVISIBLE || m_style.overflowY() == OVISIBLE;
}

}
}

#endif
