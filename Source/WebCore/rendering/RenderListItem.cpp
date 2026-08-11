/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003-2018 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Andrew Wellington (proton@wiretapped.net)
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
 *
 */

#include "config.h"
#include "RenderListItem.h"

#include "ContainerNodeInlines.h"
#include "CSSFontSelector.h"
#include "ElementInlines.h"
#include "ElementTraversal.h"
#include "HTMLNames.h"
#include "HTMLOListElement.h"
#include "HTMLUListElement.h"
#include "LocalFrameView.h"
#include "LocalFrameViewLayoutContext.h"
#include "PseudoElement.h"
#include "RenderBlockInlines.h"
#include "RenderBoxInlines.h"
#include "RenderBoxModelObjectInlines.h"
#include "RenderChildIterator.h"
#include "RenderElementStyleInlines.h"
#include "RenderInline.h"
#include "RenderMenuList.h"
#include "RenderMultiColumnFlow.h"
#include "RenderMultiColumnSet.h"
#include "RenderMultiColumnSpannerPlaceholder.h"
#include "RenderObjectInlines.h"
#include "RenderTable.h"
#include "RenderText.h"
#include "RenderTreeBuilder.h"
#include "RenderView.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StyleComputedStyle+SettersInlines.h"
#include "UnicodeBidi.h"
#include <wtf/StackStats.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

using namespace HTMLNames;

WTF_MAKE_TZONE_ALLOCATED_IMPL(RenderListItem);

enum class MarkerSearchBoxType : uint8_t {
    BlockContainer,
    SpannerPlaceholder,
    InlineContent,
    Opaque,
    NestedListInQuirksMode,
    TableRubyOrReplaced,
};
static MarkerSearchBoxType markerSearchBoxType(const RenderObject& box)
{
    if (box.isFloating() || box.isOutOfFlowPositioned())
        return MarkerSearchBoxType::Opaque;
    if (is<RenderMultiColumnSpannerPlaceholder>(box))
        return MarkerSearchBoxType::SpannerPlaceholder;
    // A nested list item's own marker is inline level but takes no part in any line, so it must not make its list item
    // the answer: that list item may well have nothing but block level children and never run inline layout.
    if (box.isExcludedMarker())
        return MarkerSearchBoxType::Opaque;
    // Before the form control check below: a control on a line is inline content like any other.
    if (box.isInline())
        return MarkerSearchBoxType::InlineContent;
    if (is<RenderMenuList>(box))
        return MarkerSearchBoxType::Opaque;
    if (auto* renderBox = dynamicDowncast<RenderBox>(box); renderBox && renderBox->isWritingModeRoot())
        return MarkerSearchBoxType::Opaque;
    if (is<RenderListItem>(box.parent()) && box.document().inQuirksMode() && box.node() && isHTMLListElement(*box.node()))
        return MarkerSearchBoxType::NestedListInQuirksMode;
    if (!is<RenderBlock>(box) || is<RenderTable>(box) || box.style().display() == Style::DisplayType::BlockRuby)
        return MarkerSearchBoxType::TableRubyOrReplaced;
    return MarkerSearchBoxType::BlockContainer;
}

static LayoutUnit excludedMarkerLogicalLeftOffsetFor(const RenderBlockFlow& firstFormattedLineRoot, const RenderListItem& listItem, LayoutUnit lineStartInset)
{
    // <ul><li id=o><ul><li id=i><div style="border-left: 5px solid">text</div></li></ul></li></ul>
    // UA sets 40px start padding on <ul>
    // x=0        40         80     85
    // |          |          |      |
    // |[o marker]|[i marker]|      text
    // |          |          |      |
    // +----------+----------+------+-------------
    //                       |<-----|  toEnclosingListItem = -5
    //            |<---------|         toAssociatedListItem = -40
    //            |<----------------|  return value = -45
    // Both markers on the same line and these values are for the outer marker. The second offset is zero unless the line is inside a nested list item.
    auto toEnclosingListItem = LayoutUnit { };
    auto hasAccountedForBorderAndPadding = false;
    CheckedPtr<const RenderBlock> ancestor = &firstFormattedLineRoot;
    for (; ancestor; ancestor = ancestor->containingBlock()) {
        if (!hasAccountedForBorderAndPadding)
            toEnclosingListItem -= ancestor->borderAndPaddingStart();
        if (is<RenderListItem>(*ancestor))
            break;

        toEnclosingListItem -= ancestor->marginStart();
        if (ancestor->isFlexItem()) {
            toEnclosingListItem -= ancestor->logicalLeft();
            hasAccountedForBorderAndPadding = true;
            continue;
        }
        hasAccountedForBorderAndPadding = false;
    }

    auto toAssociatedListItem = LayoutUnit { };
    if (ancestor && ancestor.get() != &listItem) {
        for (CheckedPtr<const RenderBlock> box = ancestor->containingBlock(); box; box = box->containingBlock()) {
            toAssociatedListItem -= (box->marginStart() + box->borderAndPaddingStart());
            if (box.get() == &listItem)
                break;
        }
        // A float pushing the line start inwards also constrains the marker, which sits to the logical left of it.
        // Nesting list items then all end up with their marker in the same place, just left of the line.
        if (lineStartInset)
            toAssociatedListItem = std::min(0_lu, std::max(lineStartInset, toAssociatedListItem));
    }

    // The offsets above are inline start relative (negative means further towards the inline start), while what we return is a logical left offset, so in a right to left inline direction it points the other way.
    if (!listItem.writingMode().isLogicalLeftInlineStart())
        return -(toEnclosingListItem + toAssociatedListItem);
    return toEnclosingListItem + toAssociatedListItem;
}

RenderListItem::RenderListItem(Element& element, Style::ComputedStyle&& style)
    : RenderBlockFlow(Type::ListItem, element, WTF::move(style))
{
    ASSERT(isRenderListItem());
    setInline(false);
}

RenderListItem::~RenderListItem()
{
    // Do not add any code here. Add it to willBeDestroyed() instead.
    ASSERT(!m_marker);
}

Style::ComputedStyle RenderListItem::computeMarkerStyle() const
{
    auto markerStyle = [&] {
        if (!is<PseudoElement>(element())) {
            if (auto markerStyle = style().pseudoElementStyle({ PseudoElementType::Marker }))
                return Style::ComputedStyle::clone(*markerStyle);
        }

        // The marker always inherits from the list item, regardless of where it might end
        // up (e.g., in some deeply nested line box). See CSS3 spec.
        auto markerStyle = Style::ComputedStyle::create();
        markerStyle.inheritFrom(style());

        // In the case of a ::before or ::after pseudo-element, we manually apply the properties
        // otherwise set in the user-agent stylesheet since we don't support ::before::marker or
        // ::after::marker. See bugs.webkit.org/b/218897.
        auto fontDescription = style().fontDescription();
        fontDescription.setVariantNumericSpacing(FontVariantNumericSpacing::TabularNumbers);
        markerStyle.setFontDescription(WTF::move(fontDescription));
        markerStyle.setUnicodeBidi(UnicodeBidi::Isolate);
        markerStyle.setWhiteSpaceCollapse(WhiteSpaceCollapse::Preserve);
        markerStyle.setTextWrapMode(TextWrapMode::NoWrap);
        markerStyle.setTextTransform({ });
        return markerStyle;
    }();

    // The marker box is a text-decoration boundary: the originating element's text-decoration must
    // not propagate into the marker's generated contents, matching list-style-type markers which are
    // never decorated by the list's text-decoration.
    markerStyle.setTextDecorationLineInEffect(markerStyle.textDecorationLine());

    // text-align neither applies to nor is inherited by ::marker (css-pseudo-4): reset the value
    // inherited from the originating element so it cannot leak into generated marker contents.
    // (list-style-type markers ignore text-align as well.)
    markerStyle.setTextAlign(Style::ComputedStyle::initialTextAlign());
    markerStyle.setTextAlignLast(Style::ComputedStyle::initialTextAlignLast());

    return markerStyle;
}

bool isHTMLListElement(const Node& node)
{
    return isAnyOf<HTMLUListElement, HTMLOListElement>(node);
}

// Returns the enclosing list with respect to the DOM order.
static Element* enclosingList(const RenderListItem& listItem)
{
    SUPPRESS_UNCOUNTED_LOCAL auto* element = listItem.element();
    SUPPRESS_UNCOUNTED_LOCAL auto* pseudoElement = dynamicDowncast<PseudoElement>(element);
    SUPPRESS_UNCOUNTED_LOCAL auto* parent = pseudoElement ? pseudoElement->hostElement() : element->parentElement();
    for (SUPPRESS_UNCOUNTED_LOCAL auto* ancestor = parent; ancestor; ancestor = ancestor->parentElement()) {
        if (isHTMLListElement(*ancestor) || (ancestor->renderer() && ancestor->renderer()->shouldApplyStyleContainment()))
            return ancestor;
    }

    // If there's no actual list element, then the parent element acts as our
    // list for purposes of determining what other list items should be numbered as
    // part of the same list.
    return parent;
}

static RenderListItem* nextListItemHelper(const Element& list, const Element& element)
{
    RefPtr current = &element;
    auto advance = [&] {
        if (!current->renderOrDisplayContentsStyle())
            current = ElementTraversal::nextIncludingPseudoSkippingChildren(*current, &list);
        else
            current = ElementTraversal::nextIncludingPseudo(*current, &list);
    };
    advance();
    while (current) {
        auto* item = dynamicDowncast<RenderListItem>(current->renderer());
        if (!item) {
            advance();
            continue;
        }
        RefPtr otherList = enclosingList(*item);
        if (!otherList) {
            advance();
            continue;
        }

        // This item is part of our current list, so it's what we're looking for.
        if (&list == otherList)
            return item;

        // We found ourself inside another list; skip the rest of its contents.
        current = ElementTraversal::nextIncludingPseudoSkippingChildren(*current, &list);
    }

    return nullptr;
}

static inline RenderListItem* nextListItem(const Element& list, const RenderListItem& item)
{
    return nextListItemHelper(list, protect(*item.element()));
}

static inline RenderListItem* firstListItem(const Element& list)
{
    return nextListItemHelper(list, list);
}

static RenderListItem* previousListItem(const Element& list, const RenderListItem& item)
{
    RefPtr current = item.element();
    auto advance = [&] {
        current = ElementTraversal::previousIncludingPseudo(*current, &list);
    };
    advance();
    while (current) {
        auto* item = dynamicDowncast<RenderListItem>(current->renderer());
        if (!item) {
            advance();
            continue;
        }
        RefPtr otherList = enclosingList(*item);
        if (!otherList) {
            advance();
            continue;
        }

        // This item is part of our current list, so we found what we're looking for.
        if (&list == otherList)
            return item;

        // We found ourself inside another list; skip the rest of its contents by
        // advancing to it. However, since the list itself might be a list item,
        // don't advance past it.
        current = otherList;
    }
    return nullptr;
}

void RenderListItem::updateItemValuesForOrderedList(const HTMLOListElement& list)
{
    for (auto* listItem = firstListItem(list); listItem; listItem = nextListItem(list, *listItem))
        listItem->updateValue();
}

unsigned RenderListItem::itemCountForOrderedList(const HTMLOListElement& list)
{
    unsigned itemCount = 0;
    for (auto* listItem = firstListItem(list); listItem; listItem = nextListItem(list, *listItem))
        ++itemCount;
    return itemCount;
}

int RenderListItem::startForReversedOrderedList(const HTMLOListElement& list)
{
    ASSERT(list.isReversed() && !list.hasExplicitStart());
    size_t itemsBefore = 0;
    for (CheckedPtr item = firstListItem(list); item; item = nextListItem(list, *item)) {
        auto directives = item->style().usedCounterDirectives().map.get("list-item"_s);
        if (directives.setValue)
            return itemsBefore + *directives.setValue;
        ++itemsBefore;
    }
    return list.start();
}

void RenderListItem::updateValueNow() const
{
    RefPtr list = enclosingList(*this);
    RefPtr orderedList = dynamicDowncast<HTMLOListElement>(list);

    // The start item is either the closest item before this one in the list that already has a value,
    // or the first item in the list if none have before this have values yet.
    // FIXME: This should skip over items with counter-reset.
    auto* startItem = this;
    if (list) {
        auto* item = this;
        while ((item = previousListItem(*list, *item))) {
            startItem = item;
            if (item->m_value)
                break;
        }
    }

    int defaultIncrement = orderedList && orderedList->isReversed() ? -1 : 1;
    auto valueForItem = [&](int previousValue, CounterDirectives& directives) {
        if (directives.setValue)
            return *directives.setValue;
        int increment = directives.incrementValue.value_or(defaultIncrement);
        if (directives.resetValue)
            return *directives.resetValue + increment;
        return previousValue + increment;
    };

    auto& startValue = startItem->m_value;
    if (!startValue) {
        // Take in account enclosing list counter-reset.
        // FIXME: This can be a lot more simple when lists use presentational hints.
        if (list && list->renderer()) {
            auto listDirectives = list->renderer()->style().usedCounterDirectives().map.get("list-item"_s);
            if (listDirectives.resetValue)
                startValue = *listDirectives.resetValue;
            else if (orderedList && orderedList->isReversed() && !orderedList->hasExplicitStart())
                startValue = startForReversedOrderedList(*orderedList) - defaultIncrement;
            else
                startValue = orderedList ? orderedList->start() - defaultIncrement : 0;
        }
        auto directives = startItem->style().usedCounterDirectives().map.get("list-item"_s);
        startValue = valueForItem(startValue.value_or(0), directives);
    }

    int value = *startValue;

    for (auto* item = startItem; item != this; ) {
        item = nextListItem(*list, *item);
        auto directives = item->style().usedCounterDirectives().map.get("list-item"_s);
        item->m_value = valueForItem(value, directives);
        // counter-reset creates a new nested counter, so it should not be counted towards the current counter.
        if (!directives.resetValue)
            value = *item->m_value;
    }
}

void RenderListItem::updateValue()
{
    m_value = std::nullopt;
    if (m_marker) {
        m_marker->setNeedsLayoutAndInvalidateContentLogicalWidths();
        if (m_marker->excludedPosition())
            m_marker->invalidateExcludedMarkerContainer();
    }
}

void RenderListItem::styleDidChange(Style::Difference diff, const Style::ComputedStyle* oldStyle)
{
    RenderBlockFlow::styleDidChange(diff, oldStyle);

    if (diff == Style::DifferenceResult::Layout) {
        if (oldStyle && oldStyle->usedCounterDirectives().map.get("list-item"_s) != style().usedCounterDirectives().map.get("list-item"_s))
            usedCounterDirectivesChanged();
        if (m_marker && m_marker->excludedPosition())
            m_marker->invalidateExcludedMarkerContainer();
    }
}

void RenderListItem::computeIntrinsicLogicalWidthContributions()
{
    // FIXME: RenderListMarker::updateInlineMargins() mutates margin style which affects preferred widths.
    if (m_marker && m_marker->hasInvalidContentLogicalWidths())
        m_marker->updateInlineMarginsAndContent();

    RenderBlockFlow::computeIntrinsicLogicalWidthContributions();
}

RenderListMarker* RenderListItem::excludedMarker() const
{
    if (m_marker && m_marker->parent() == this && m_marker->isExcludedMarker())
        return m_marker.get();
    return { };
}

void RenderListItem::layoutBlock(RelayoutChildren relayoutChildren, LayoutUnit pageLogicalHeight)
{
    CheckedPtr excludedMarker = this->excludedMarker();
    if (!excludedMarker)
        return RenderBlockFlow::layoutBlock(relayoutChildren, pageLogicalHeight);

    auto markerScope = ListItemExcludedMarkerScope { view().frameView().layoutContext(), *excludedMarker };
    RenderBlockFlow::layoutBlock(relayoutChildren, pageLogicalHeight);
    placeExcludedMarker(*excludedMarker);
    addOverflowFromContainedBox(*excludedMarker);
}

void RenderListItem::layoutExcludedChildren(RelayoutChildren relayoutChildren)
{
    CheckedPtr excludedMarker = this->excludedMarker();
    if (!excludedMarker)
        return RenderBlockFlow::layoutExcludedChildren(relayoutChildren);

    excludedMarker->setIsExcludedFromNormalLayout(true);
    if (relayoutChildren == RelayoutChildren::Yes)
        excludedMarker->setNeedsLayout(MarkingBehavior::MarkOnlyThis);
    auto oldLayoutBounds = excludedMarker->layoutBounds();
    excludedMarker->layoutIfNeeded();

    if (excludedMarker->excludedPosition() && excludedMarker->layoutBounds() != oldLayoutBounds)
        excludedMarker->invalidateExcludedMarkerContainer();

    RenderBlockFlow::layoutExcludedChildren(relayoutChildren);
}

void RenderListItem::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (!logicalHeight() && hasNonVisibleOverflow())
        return;

    RenderBlockFlow::paint(paintInfo, paintOffset);
}

void RenderListItem::paintObject(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    RenderBlockFlow::paintObject(paintInfo, paintOffset);

    if (CheckedPtr excludedMarker = this->excludedMarker(); excludedMarker && !excludedMarker->hasSelfPaintingLayer())
        excludedMarker->paintAsInlineBlock(paintInfo, flipForWritingModeForChild(*excludedMarker, paintOffset));
}

String RenderListItem::markerTextWithoutSuffix() const
{
    if (!m_marker)
        return { };
    return m_marker->textWithoutSuffix();
}

String RenderListItem::markerTextWithSuffix() const
{
    if (!m_marker)
        return { };
    return m_marker->textWithSuffix();
}

void RenderListItem::usedCounterDirectivesChanged()
{
    if (m_marker)
        m_marker->setNeedsLayoutAndInvalidateContentLogicalWidths();

    updateValue();
    RefPtr list = enclosingList(*this);
    if (!list)
        return;
    auto* item = this;
    while ((item = nextListItem(*list, *item)))
        item->updateValue();
}

void RenderListItem::updateListMarkerNumbers()
{
    RefPtr list = enclosingList(*this);
    if (!list)
        return;

    bool isInReversedOrderedList = false;
    if (auto* orderedList = dynamicDowncast<HTMLOListElement>(*list)) {
        orderedList->itemCountChanged();
        isInReversedOrderedList = orderedList->isReversed();
    }

    // If an item has been marked for update before, we know that all following items have, too.
    // This gives us the opportunity to stop and avoid marking the same nodes again.
    auto* item = this;
    auto subsequentListItem = isInReversedOrderedList ? previousListItem : nextListItem;
    while ((item = subsequentListItem(*list, *item)) && item->m_value)
        item->updateValue();
}

bool RenderListItem::isInReversedOrderedList() const
{
    RefPtr list = dynamicDowncast<HTMLOListElement>(enclosingList(*this));
    return list && list->isReversed();
}

void RenderListItem::placeExcludedMarker(RenderListMarker& marker)
{
    auto excludedPosition = marker.excludedPosition();
    CheckedPtr firstFormattedLineRoot = excludedPosition ? excludedPosition->firstFormattedLineRoot.get() : nullptr;
    if (!firstFormattedLineRoot) {
        // Reached only when no line took the marker: we have no formatted line to align it with, either because we have no content at all.
        // Let's top align it at our content edge; the marker contributes no height, which is what a marker on a collapsed line inside such content would have done.
        setLogicalTopForChild(marker, borderAndPaddingBefore());
        auto markerLogicalLeft = writingMode().isLogicalLeftInlineStart() ? marginStartForChild(marker) : logicalWidth() - logicalWidthForChild(marker) - marginStartForChild(marker);
        setLogicalLeftForChild(marker, markerLogicalLeft);
        return;
    }

    // Line layout placed the marker on the line as if it belonged to the list item establishing that formatting context.
    // Take it from there: out to our own border box start, then across the in-flow content up to us.
    auto logicalTop = LayoutUnit { excludedPosition->topLeft.y() };
    auto logicalLeft = LayoutUnit { excludedPosition->topLeft.x() } + excludedMarkerLogicalLeftOffsetFor(*firstFormattedLineRoot, *this, LayoutUnit { excludedPosition->lineStartInset });
    for (CheckedPtr<RenderBlock> ancestor = firstFormattedLineRoot; ancestor && ancestor != this; ancestor = ancestor->containingBlock()) {
        // Inside a fragmented flow the coordinates are in flow thread space, where the content is one continuous
        // strip. The column the line ended up in is a sibling of the flow thread rather than an ancestor, so leave
        // the flow thread for that column set and carry on from there, in visual space.
        if (CheckedPtr fragmentedFlow = dynamicDowncast<RenderMultiColumnFlow>(ancestor.get())) {
            CheckedPtr columnSet = dynamicDowncast<RenderMultiColumnSet>(static_cast<const RenderFragmentedFlow&>(*fragmentedFlow).fragmentAtBlockOffset(fragmentedFlow.get(), logicalTop, true));
            if (!columnSet)
                continue;
            auto translation = fragmentedFlow->physicalTranslationOffsetFromFlowToFragment(columnSet.get(), logicalTop);
            logicalTop += translation.height();
            logicalLeft += translation.width();
            ancestor = columnSet.get();
        }
        logicalTop += ancestor->logicalTop();
        logicalLeft += ancestor->logicalLeft();
    }
    setLogicalTopForChild(marker, logicalTop);
    setLogicalLeftForChild(marker, logicalLeft);
}

RenderListItem::FirstFormattedLineCandidate RenderListItem::firstFormattedLineRootFor(RenderBlock& blockContainer, const RenderListMarker& marker)
{
    RenderBlock* fallbackParent = { };
    bool stoppedAtTableRubyOrReplaced = false;

    for (auto& child : childrenOfType<RenderObject>(blockContainer)) {
        if (&child == &marker)
            continue;

        switch (markerSearchBoxType(child)) {
        case MarkerSearchBoxType::Opaque:
            break;
        case MarkerSearchBoxType::NestedListInQuirksMode:
            return { { }, fallbackParent, stoppedAtTableRubyOrReplaced };
        case MarkerSearchBoxType::TableRubyOrReplaced:
            return { { }, fallbackParent, true };
        case MarkerSearchBoxType::InlineContent:
            // Neither an empty inline (e.g. <a id="anchor"></a>) nor collapsible whitespace produces a line.
            if (CheckedPtr inlineBox = dynamicDowncast<RenderInline>(child); inlineBox && isEmptyInline(*inlineBox)) {
                fallbackParent = &blockContainer;
                break;
            }
            if (CheckedPtr text = dynamicDowncast<RenderText>(child); text && text->containsOnlyCollapsibleWhitespace())
                break;
            return { &blockContainer, { }, false };
        case MarkerSearchBoxType::BlockContainer:
        case MarkerSearchBoxType::SpannerPlaceholder: {
            auto blockToDescendInto = [&] {
                if (CheckedPtr placeholder = dynamicDowncast<RenderMultiColumnSpannerPlaceholder>(child))
                    return dynamicDowncast<RenderBlock>(placeholder->spanner());
                return dynamicDowncast<RenderBlock>(child);
            };
            if (CheckedPtr block = blockToDescendInto()) {
                auto nestedResult = firstFormattedLineRootFor(*block, marker);
                if (nestedResult.parent) {
                    // Finding a line box parent is mutually exclusive with having stopped at one of those.
                    ASSERT(!nestedResult.stoppedAtTableRubyOrReplaced);
                    return { nestedResult.parent, { }, false };
                }

                // Propagate it from nested searches, so we know whether the search failed on such a box or simply found no inline content.
                stoppedAtTableRubyOrReplaced |= nestedResult.stoppedAtTableRubyOrReplaced;

                if (!fallbackParent) {
                    if (nestedResult.fallbackParent)
                        fallbackParent = nestedResult.fallbackParent;
                    else if (auto* firstInFlowChild = block->firstInFlowChild(); !firstInFlowChild || firstInFlowChild == &marker)
                        fallbackParent = block.get();
                }
            }
            break;
        }
        }
    }
    return { { }, fallbackParent, stoppedAtTableRubyOrReplaced };
}

Vector<CheckedPtr<RenderListMarker>> RenderListItem::excludedMarkersForContainer(const RenderBlockFlow& inlineRoot, const Vector<SingleThreadWeakPtr<RenderListMarker>>& allExcludedMarkers)
{
    auto markersForContainer = Vector<CheckedPtr<RenderListMarker>> { };
    for (auto& marker : allExcludedMarkers) {
        ASSERT(marker->isExcludedMarker());
        CheckedPtr listItem = marker->listItem();
        if (!listItem) {
            ASSERT_NOT_REACHED();
            continue;
        }
        if (firstFormattedLineRootFor(*listItem, *marker).parent == &inlineRoot)
            markersForContainer.append(marker.get());
    }
    return markersForContainer;
}

} // namespace WebCore
