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

#include "LayoutBoxGeometry.h"
#include "LayoutContainingBlockChainIterator.h"
#include "LayoutElementBox.h"
#include "LayoutInitialContainingBlock.h"
#include "LayoutPhase.h"
#include "LayoutState.h"
#include "RenderStyle.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {
namespace Layout {

WTF_MAKE_ISO_ALLOCATED_IMPL(Box);

Box::Box(ElementAttributes&& elementAttributes, RenderStyle&& style, std::unique_ptr<RenderStyle>&& firstLineStyle, OptionSet<BaseTypeFlag> baseTypeFlags)
    : m_style(WTFMove(style))
    , m_nodeType(elementAttributes.nodeType)
    , m_isAnonymous(static_cast<bool>(elementAttributes.isAnonymous))
    , m_baseTypeFlags(baseTypeFlags.toRaw())
{
    if (firstLineStyle)
        ensureRareData().firstLineStyle = WTFMove(firstLineStyle);
}

Box::~Box()
{
    if (UNLIKELY(m_hasRareData))
        removeRareData();
}

UniqueRef<Box> Box::removeFromParent()
{
    auto& nextOrFirst = m_previousSibling ? m_previousSibling->m_nextSibling : m_parent->m_firstChild;
    auto& previousOrLast = m_nextSibling ? m_nextSibling->m_previousSibling : m_parent->m_lastChild;

    ASSERT(nextOrFirst.get() == this);
    ASSERT(previousOrLast.get() == this);

    auto ownedSelf = std::exchange(nextOrFirst, std::exchange(m_nextSibling, nullptr));

    previousOrLast = std::exchange(m_previousSibling, nullptr);
    m_parent = nullptr;

    return makeUniqueRefFromNonNullUniquePtr(WTFMove(ownedSelf));
}

void Box::updateStyle(RenderStyle&& newStyle, std::unique_ptr<RenderStyle>&& newFirstLineStyle)
{
    m_style = WTFMove(newStyle);
    if (newFirstLineStyle)
        ensureRareData().firstLineStyle = WTFMove(newFirstLineStyle);
}

bool Box::establishesFormattingContext() const
{
    // We need the final tree structure to tell whether a box establishes a certain formatting context. 
    ASSERT(!Phase::isInTreeBuilding());
    return establishesInlineFormattingContext()
        || establishesBlockFormattingContext()
        || establishesTableFormattingContext()
        || establishesFlexFormattingContext()
        || establishesIndependentFormattingContext();
}

bool Box::establishesBlockFormattingContext() const
{
    // ICB always creates a new (inital) block formatting context.
    if (is<InitialContainingBlock>(*this))
        return true;

    if (isTableWrapperBox())
        return true;

    // A block box that establishes an independent formatting context establishes a new block formatting context for its contents.
    if (isBlockBox() && establishesIndependentFormattingContext())
        return true;

    // 9.4.1 Block formatting contexts
    // Floats, absolutely positioned elements, block containers (such as inline-blocks, table-cells, and table-captions)
    // that are not block boxes, and block boxes with 'overflow' other than 'visible' (except when that value has been propagated to the viewport)
    // establish new block formatting contexts for their contents.
    if (isFloatingPositioned()) {
        // Not all floating or out-of-positioned block level boxes establish BFC.
        // See [9.7 Relationships between 'display', 'position', and 'float'] for details.
        return style().display() == DisplayType::Block;
    }

    if (isBlockContainer() && !isBlockBox())
        return true;

    if (isBlockBox() && !isOverflowVisible())
        return true;

    return false;
}

bool Box::establishesInlineFormattingContext() const
{
    if (isInlineIntegrationRoot())
        return true;

    // 9.4.2 Inline formatting contexts
    // An inline formatting context is established by a block container box that contains no block-level boxes.
    if (!isBlockContainer())
        return false;

    if (!isElementBox())
        return false;

    // FIXME ???
    if (!downcast<ElementBox>(*this).firstInFlowChild())
        return false;

    // It's enough to check the first in-flow child since we can't have both block and inline level sibling boxes.
    return downcast<ElementBox>(*this).firstInFlowChild()->isInlineLevelBox();
}

bool Box::establishesTableFormattingContext() const
{
    return isTableBox();
}

bool Box::establishesFlexFormattingContext() const
{
    return isFlexBox();
}

bool Box::establishesIndependentFormattingContext() const
{
    return isLayoutContainmentBox() || isAbsolutelyPositioned() || isFlexItem();
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
    return m_style.floating() != Float::None;
}

bool Box::hasFloatClear() const
{
    return m_style.clear() != Clear::None && (isBlockLevelBox() || isLineBreakBox());
}

bool Box::isFloatAvoider() const
{
    if (isFloatingPositioned() || hasFloatClear())
        return true;

    return establishesTableFormattingContext() || establishesIndependentFormattingContext() || establishesBlockFormattingContext();
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
    return display == DisplayType::Block
        || display == DisplayType::ListItem
        || display == DisplayType::Table
        || display == DisplayType::Flex
        || display == DisplayType::Grid
        || display == DisplayType::FlowRoot;
}

bool Box::isBlockBox() const
{
    // A block-level box that is also a block container.
    return isBlockLevelBox() && isBlockContainer();
}

bool Box::isInlineLevelBox() const
{
    // Inline level elements generate inline level boxes.
    auto display = m_style.display();
    return display == DisplayType::Inline
        || display == DisplayType::InlineBox
        || display == DisplayType::InlineFlex
        || display == DisplayType::InlineGrid
        || isInlineBlockBox()
        || isInlineTableBox();
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

bool Box::isFlexItem() const
{
    // Each in-flow child of a flex container becomes a flex item (https://www.w3.org/TR/css-flexbox-1/#flex-items).
    return isInFlow() && parent().isFlexBox();
}

bool Box::isBlockContainer() const
{
    auto display = m_style.display();
    return display == DisplayType::Block
        || display == DisplayType::FlowRoot
        || display == DisplayType::ListItem
        || isInlineBlockBox()
        || isTableCell()
        || isTableCaption(); // TODO && !replaced element
}

bool Box::isLayoutContainmentBox() const
{
    auto supportsLayoutContainment = [&] {
        // If the element does not generate a principal box (as is the case with display values of contents or none),
        // or its principal box is an internal table box other than table-cell, or an internal ruby box, or a non-atomic inline-level box,
        // layout containment has no effect.
        if (isInternalTableBox())
            return isTableCell();
        if (isInternalRubyBox())
            return false;
        if (isInlineLevelBox())
            return isAtomicInlineLevelBox();
        return true;
    };
    return m_style.effectiveContainment().contains(Containment::Layout) && supportsLayoutContainment();
}

bool Box::isSizeContainmentBox() const
{
    auto supportsSizeContainment = [&] {
        // If the element does not generate a principal box (as is the case with display: contents or display: none),
        // or its inner display type is table, or its principal box is an internal table box, or an internal ruby box,
        // or a non-atomic inline-level box, size containment has no effect.
        if (isInternalTableBox() || isTableBox())
            return false;
        if (isInternalRubyBox())
            return false;
        if (isInlineLevelBox())
            return isAtomicInlineLevelBox();
        return true;
    };
    return m_style.effectiveContainment().contains(Containment::Size) && supportsSizeContainment();
}

bool Box::isInternalTableBox() const
{
    // table-row-group, table-header-group, table-footer-group, table-row, table-cell, table-column-group, table-column
    // generates the appropriate internal table box which participates in a table formatting context.
    return isTableBody() || isTableHeader() || isTableFooter() || isTableRow() || isTableCell() || isTableColumnGroup() || isTableColumn();
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

bool Box::isDescendantOf(const ElementBox& ancestor) const
{
    if (ancestor.isInitialContainingBlock())
        return true;
    for (auto& containingBlock : containingBlockChain(*this)) {
        if (&containingBlock == &ancestor)
            return true;
    }
    return false;
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
        auto& documentBox = parent();
        if (!documentBox.isDocumentBox())
            return isOverflowVisible;
        if (!documentBox.isOverflowVisible())
            return isOverflowVisible;
        return true;
    }
    if (is<InitialContainingBlock>(*this)) {
        auto* documentBox = downcast<ElementBox>(*this).firstChild();
        if (!documentBox || !documentBox->isDocumentBox() || !is<ElementBox>(documentBox))
            return isOverflowVisible;
        auto* bodyBox = downcast<ElementBox>(documentBox)->firstChild();
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

std::optional<LayoutUnit> Box::columnWidth() const
{
    if (!hasRareData())
        return { };
    return rareData().columnWidth;
}

void Box::setCachedGeometryForLayoutState(LayoutState& layoutState, std::unique_ptr<BoxGeometry> geometry) const
{
    ASSERT(!m_cachedLayoutState);
    m_cachedLayoutState = layoutState;
    m_cachedGeometryForLayoutState = WTFMove(geometry);
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

