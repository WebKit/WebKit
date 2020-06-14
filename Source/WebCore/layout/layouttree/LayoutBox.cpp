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

#include "DisplayBox.h"
#include "LayoutContainerBox.h"
#include "LayoutInitialContainingBlock.h"
#include "LayoutPhase.h"
#include "LayoutState.h"
#include "RenderStyle.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {
namespace Layout {

WTF_MAKE_ISO_ALLOCATED_IMPL(Box);

Box::Box(Optional<ElementAttributes> attributes, RenderStyle&& style, BaseTypeFlags baseTypeFlags)
    : m_style(WTFMove(style))
    , m_elementAttributes(attributes)
    , m_baseTypeFlags(baseTypeFlags)
    , m_hasRareData(false)
    , m_isAnonymous(false)
{
}

Box::~Box()
{
    if (UNLIKELY(m_hasRareData))
        removeRareData();
}

void Box::updateStyle(const RenderStyle& newStyle)
{
    m_style = RenderStyle::clone(newStyle);
}

bool Box::establishesFormattingContext() const
{
    // We need the final tree structure to tell whether a box establishes a certain formatting context. 
    ASSERT(!Phase::isInTreeBuilding());
    return establishesBlockFormattingContext() || establishesInlineFormattingContext() || establishesTableFormattingContext() || establishesIndependentFormattingContext();
}

bool Box::establishesBlockFormattingContext() const
{
    // ICB always creates a new (inital) block formatting context.
    if (is<InitialContainingBlock>(*this))
        return true;

    if (isTableWrapperBox())
        return true;

    // 9.4.1 Block formatting contexts
    // Floats, absolutely positioned elements, block containers (such as inline-blocks, table-cells, and table-captions)
    // that are not block boxes, and block boxes with 'overflow' other than 'visible' (except when that value has been propagated to the viewport)
    // establish new block formatting contexts for their contents.
    if (isFloatingPositioned() || isAbsolutelyPositioned()) {
        // Not all floating or out-of-positioned block level boxes establish BFC.
        // See [9.7 Relationships between 'display', 'position', and 'float'] for details.
        return style().display() == DisplayType::Block;
    }

    if (isBlockContainerBox() && !isBlockLevelBox())
        return true;

    if (isBlockLevelBox() && !isOverflowVisible())
        return true;

    return false;
}

bool Box::establishesInlineFormattingContext() const
{
    // 9.4.2 Inline formatting contexts
    // An inline formatting context is established by a block container box that contains no block-level boxes.
    if (!isBlockContainerBox())
        return false;

    if (!isContainerBox())
        return false;

    // FIXME ???
    if (!downcast<ContainerBox>(*this).firstInFlowChild())
        return false;

    // It's enough to check the first in-flow child since we can't have both block and inline level sibling boxes.
    return downcast<ContainerBox>(*this).firstInFlowChild()->isInlineLevelBox();
}

bool Box::establishesTableFormattingContext() const
{
    return isTableBox();
}

bool Box::establishesIndependentFormattingContext() const
{
    // FIXME: This is where we would check for 'contain' property.
    return isAbsolutelyPositioned();
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
    // FIXME: Rendering code caches values like this. (style="position: absolute; float: left")
    if (isOutOfFlowPositioned())
        return false;
    return m_style.floating() != Float::No;
}

bool Box::isLeftFloatingPositioned() const
{
    if (!isFloatingPositioned())
        return false;
    return m_style.floating() == Float::Left;
}

bool Box::isRightFloatingPositioned() const
{
    if (!isFloatingPositioned())
        return false;
    return m_style.floating() == Float::Right;
}

bool Box::hasFloatClear() const
{
    return m_style.clear() != Clear::None;
}

bool Box::isFloatAvoider() const
{
    if (isFloatingPositioned() || hasFloatClear())
        return true;

    return establishesTableFormattingContext() || establishesIndependentFormattingContext() || (establishesBlockFormattingContext() && !establishesInlineFormattingContext());
}

const ContainerBox& Box::containingBlock() const
{
    // Finding the containing block by traversing the tree during tree construction could provide incorrect result.
    ASSERT(!Phase::isInTreeBuilding());
    // If we ever end up here with the ICB, we must be doing something not-so-great.
    RELEASE_ASSERT(!is<InitialContainingBlock>(*this));
    // The containing block in which the root element lives is a rectangle called the initial containing block.
    // For other elements, if the element's position is 'relative' or 'static', the containing block is formed by the
    // content edge of the nearest block container ancestor box or which establishes a formatting context.
    // If the element has 'position: fixed', the containing block is established by the viewport
    // If the element has 'position: absolute', the containing block is established by the nearest ancestor with a
    // 'position' of 'absolute', 'relative' or 'fixed'.
    if (!isPositioned() || isInFlowPositioned()) {
        auto* ancestor = &parent();
        for (; !is<InitialContainingBlock>(*ancestor); ancestor = &ancestor->parent()) {
            if (ancestor->isBlockContainerBox() || ancestor->establishesFormattingContext())
                return *ancestor;
        }
        return *ancestor;
    }

    if (isFixedPositioned()) {
        auto* ancestor = &parent();
        for (; !is<InitialContainingBlock>(*ancestor); ancestor = &ancestor->parent()) {
            if (ancestor->style().hasTransform())
                return *ancestor;
        }
        return *ancestor;
    }

    if (isOutOfFlowPositioned()) {
        auto* ancestor = &parent();
        for (; !is<InitialContainingBlock>(*ancestor); ancestor = &ancestor->parent()) {
            if (ancestor->isPositioned() || ancestor->style().hasTransform())
                return *ancestor;
        }
        return *ancestor;
    }

    ASSERT_NOT_REACHED();
    return initialContainingBlock();
}

const ContainerBox& Box::formattingContextRoot() const
{
    // Finding the context root by traversing the tree during tree construction could provide incorrect result.
    ASSERT(!Phase::isInTreeBuilding());
    // We should never need to ask this question on the ICB.
    ASSERT(!is<InitialContainingBlock>(*this));
    // A box lives in the same formatting context as its containing block unless the containing block establishes a formatting context.
    // However relatively positioned (inflow) inline container lives in the formatting context where its parent lives unless
    // the parent establishes a formatting context.
    //
    // <div id=outer style="position: absolute"><div id=inner><span style="position: relative">content</span></div></div>
    // While the relatively positioned inline container (span) is placed relative to its containing block "outer", it lives in the inline
    // formatting context established by "inner".
    auto& ancestor = isInlineLevelBox() && isInFlowPositioned() ? parent() : containingBlock();
    if (ancestor.establishesFormattingContext())
        return ancestor;
    return ancestor.formattingContextRoot();
}

const InitialContainingBlock& Box::initialContainingBlock() const
{
    if (is<InitialContainingBlock>(*this))
        return downcast<InitialContainingBlock>(*this);

    auto* ancestor = &parent();
    for (; !is<InitialContainingBlock>(*ancestor); ancestor = &ancestor->parent()) { }
    return downcast<InitialContainingBlock>(*ancestor);
}

bool Box::isInFormattingContextOf(const ContainerBox& formattingContextRoot) const
{ 
    ASSERT(formattingContextRoot.establishesFormattingContext());
    ASSERT(!is<InitialContainingBlock>(*this));
    auto* ancestor = &containingBlock();
    while (ancestor) {
        if (ancestor == &formattingContextRoot)
            return true;
        if (is<InitialContainingBlock>(*ancestor))
            return false;
        ancestor = &ancestor->containingBlock();
    }
    ASSERT_NOT_REACHED();
    return false;
}

bool Box::isInlineBlockBox() const
{
    return m_style.display() == DisplayType::InlineBlock;
}

bool Box::isInlineTableBox() const
{
    return m_style.display() == DisplayType::InlineTable;
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
    return display == DisplayType::Inline || isInlineBlockBox() || isInlineTableBox();
}

bool Box::isInlineBox() const
{
    // An inline box is one that is both inline-level and whose contents participate in its containing inline formatting context.
    // A non-replaced element with a 'display' value of 'inline' generates an inline box.
    return m_style.display() == DisplayType::Inline && !isReplacedBox();
}

bool Box::isAtomicInlineLevelBox() const
{
    // Inline-level boxes that are not inline boxes (such as replaced inline-level elements, inline-block elements, and inline-table elements)
    // are called atomic inline-level boxes because they participate in their inline formatting context as a single opaque box.
    return isInlineLevelBox() && !isInlineBox();
}

bool Box::isBlockContainerBox() const
{
    auto display = m_style.display();
    return display == DisplayType::Block || display == DisplayType::ListItem || isInlineBlockBox() || isTableCell() || isTableCaption(); // TODO && !replaced element
}

const Box* Box::nextInFlowSibling() const
{
    auto* nextSibling = this->nextSibling();
    while (nextSibling && !nextSibling->isInFlow())
        nextSibling = nextSibling->nextSibling();
    return nextSibling;
}

const Box* Box::nextInFlowOrFloatingSibling() const
{
    auto* nextSibling = this->nextSibling();
    while (nextSibling && !(nextSibling->isInFlow() || nextSibling->isFloatingPositioned()))
        nextSibling = nextSibling->nextSibling();
    return nextSibling;
}

const Box* Box::previousInFlowSibling() const
{
    auto* previousSibling = this->previousSibling();
    while (previousSibling && !previousSibling->isInFlow())
        previousSibling = previousSibling->previousSibling();
    return previousSibling;
}

const Box* Box::previousInFlowOrFloatingSibling() const
{
    auto* previousSibling = this->previousSibling();
    while (previousSibling && !(previousSibling->isInFlow() || previousSibling->isFloatingPositioned()))
        previousSibling = previousSibling->previousSibling();
    return previousSibling;
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
        auto& documentBox = containingBlock();
        if (!documentBox.isDocumentBox())
            return isOverflowVisible;
        if (!documentBox.isOverflowVisible())
            return isOverflowVisible;
        return true;
    }
    if (is<InitialContainingBlock>(*this)) {
        auto* documentBox = downcast<ContainerBox>(*this).firstChild();
        if (!documentBox || !documentBox->isDocumentBox() || !is<ContainerBox>(documentBox))
            return isOverflowVisible;
        auto* bodyBox = downcast<ContainerBox>(documentBox)->firstChild();
        if (!bodyBox || !bodyBox->isBodyBox())
            return isOverflowVisible;
        auto& bodyBoxStyle = bodyBox->style();
        return bodyBoxStyle.overflowX() == Overflow::Visible || bodyBoxStyle.overflowY() == Overflow::Visible;
    }
    return isOverflowVisible;
}

bool Box::isPaddingApplicable() const
{
    if (isAnonymous())
        return false;

    if (isTableBox() && style().borderCollapse() == BorderCollapse::Collapse) {
        // When the table collapses its borders with inner table elements, there's no room for padding.
        return false;
    }

    // 8.4 Padding properties:
    // Applies to: all elements except table-row-group, table-header-group, table-footer-group, table-row, table-column-group and table-column
    return !isTableHeader()
        && !isTableBody()
        && !isTableFooter()
        && !isTableRow()
        && !isTableColumnGroup()
        && !isTableColumn();
}

void Box::setRowSpan(size_t rowSpan)
{
    ensureRareData().tableCellSpan.row = rowSpan;
}

void Box::setColumnSpan(size_t columnSpan)
{
    ensureRareData().tableCellSpan.column = columnSpan;
}

size_t Box::rowSpan() const
{
    if (!hasRareData())
        return 1;
    return rareData().tableCellSpan.row;
}

size_t Box::columnSpan() const
{
    if (!hasRareData())
        return 1;
    return rareData().tableCellSpan.column;
}

void Box::setColumnWidth(LayoutUnit columnWidth)
{
    ensureRareData().columnWidth = columnWidth;
}

Optional<LayoutUnit> Box::columnWidth() const
{
    if (!hasRareData())
        return { };
    return rareData().columnWidth;
}

void Box::setCachedDisplayBoxForLayoutState(LayoutState& layoutState, std::unique_ptr<Display::Box> box) const
{
    ASSERT(!m_cachedLayoutState);
    m_cachedLayoutState = makeWeakPtr(layoutState);
    m_cachedDisplayBoxForLayoutState = WTFMove(box);
}

Box::RareDataMap& Box::rareDataMap()
{
    static NeverDestroyed<RareDataMap> map;
    return map;
}

const Box::BoxRareData& Box::rareData() const
{
    ASSERT(hasRareData());
    return *rareDataMap().get(this);
}

Box::BoxRareData& Box::ensureRareData()
{
    setHasRareData(true);
    return *rareDataMap().ensure(this, [] { return makeUnique<BoxRareData>(); }).iterator->value;
}

void Box::removeRareData()
{
    rareDataMap().remove(this);
    setHasRareData(false);
}

}
}

#endif
