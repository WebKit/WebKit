/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2007 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2003-2025 Apple Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#include "config.h"
#include "RenderBlock.h"

#include "AXObjectCache.h"
#include "BorderShape.h"
#include "ContainerNodeInlines.h"
#include "Editor.h"
#include "Element.h"
#include "ElementInlines.h"
#include "EventRegion.h"
#include "EventTargetInlines.h"
#include "FloatQuad.h"
#include "FrameSelection.h"
#include "GraphicsContext.h"
#include "HTMLNames.h"
#include "HTMLTextFormControlElement.h"
#include "HitTestLocation.h"
#include "HitTestResult.h"
#include "ImageBuffer.h"
#include "InlineIteratorInlineBox.h"
#include "LayoutRepainter.h"
#include "LayoutScope.h"
#include "LocalFrame.h"
#include "LocalFrameView.h"
#include "Logging.h"
#include "LogicalSelectionOffsetCachesInlines.h"
#include "Page.h"
#include "PaintInfo.h"
#include "RenderBlockFlow.h"
#include "RenderBlockInlines.h"
#include "RenderBoxFragmentInfo.h"
#include "RenderBoxInlines.h"
#include "RenderButton.h"
#include "RenderChildIterator.h"
#include "RenderCombineText.h"
#include "RenderDeprecatedFlexibleBox.h"
#include "RenderElementStyleInlines.h"
#include "RenderFlexibleBox.h"
#include "RenderFragmentedFlow.h"
#include "RenderGrid.h"
#include "RenderInline.h"
#include "RenderIterator.h"
#include "RenderLayer.h"
#include "RenderLayerScrollableArea.h"
#include "RenderLayoutState.h"
#include "RenderListMarker.h"
#include "RenderMenuList.h"
#include "RenderObjectInlines.h"
#include "RenderTableCell.h"
#include "RenderTextControl.h"
#include "RenderTextFragment.h"
#include "RenderTheme.h"
#include "RenderTreeBuilder.h"
#include "RenderTreePosition.h"
#include "RenderView.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "ShapeOutsideInfo.h"
#include "TransformState.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/SetForScope.h>
#include <wtf/StackStats.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

using namespace HTMLNames;
using namespace WTF::Unicode;

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(RenderBlock);

struct SameSizeAsRenderBlock : public RenderBox {
};

static_assert(sizeof(RenderBlock) == sizeof(SameSizeAsRenderBlock), "RenderBlock should stay small");

using TrackedDescendantsMap = SingleThreadWeakHashMap<const RenderBlock, std::unique_ptr<TrackedRendererListHashSet>>;
using TrackedContainerMap = SingleThreadWeakHashMap<const RenderBox, SingleThreadWeakHashSet<const RenderBlock>>;

static TrackedDescendantsMap* percentHeightDescendantsMap;
static TrackedContainerMap* percentHeightContainerMap;

static void insertIntoTrackedRendererMaps(const RenderBlock& container, RenderBox& descendant)
{
    if (!percentHeightDescendantsMap) {
        percentHeightDescendantsMap = new TrackedDescendantsMap;
        percentHeightContainerMap = new TrackedContainerMap;
    }
    
    auto& descendantSet = percentHeightDescendantsMap->ensure(container, [] {
        return makeUnique<TrackedRendererListHashSet>();
    }).iterator->value;

    bool added = descendantSet->add(descendant).isNewEntry;
    if (!added) {
#if ASSERT_ENABLED
        auto it = percentHeightContainerMap->find(descendant);
        ASSERT(it != percentHeightContainerMap->end());
        ASSERT(it->value.contains(container));
#endif
        return;
    }
    
    auto& containerSet = percentHeightContainerMap->add(descendant, SingleThreadWeakHashSet<const RenderBlock>()).iterator->value;
    ASSERT(!containerSet.contains(container));
    containerSet.add(container);
}

static void removeFromTrackedRendererMaps(RenderBox& descendant)
{
    if (!percentHeightDescendantsMap)
        return;
    
    auto containerSet = percentHeightContainerMap->take(descendant);
    for (auto& container : containerSet) {
        // FIXME: Disabling this assert temporarily until we fix the layout
        // bugs associated with positioned objects not properly cleared from
        // their ancestor chain before being moved. See webkit bug 93766.
        // ASSERT(descendant->isDescendantOf(container));
        auto descendantsMapIterator = percentHeightDescendantsMap->find(container);
        ASSERT(descendantsMapIterator != percentHeightDescendantsMap->end());
        if (descendantsMapIterator == percentHeightDescendantsMap->end())
            continue;
        auto& descendantSet = descendantsMapIterator->value;
        ASSERT(descendantSet->contains(descendant));
        descendantSet->remove(descendant);
        if (descendantSet->isEmptyIgnoringNullReferences())
            percentHeightDescendantsMap->remove(descendantsMapIterator);
    }
}

class OutOfFlowDescendantsMap {
public:
    void addDescendant(const RenderBlock& containingBlock, RenderBox& outOfFlowDescendant)
    {
        // Protect against double insert where a descendant would end up with multiple containing blocks.
        auto previousContainingBlock = m_containerMap.get(outOfFlowDescendant);
        if (previousContainingBlock && previousContainingBlock != &containingBlock) {
            if (auto* descendants = m_descendantsMap.get(*previousContainingBlock))
                descendants->remove(outOfFlowDescendant);
        }

        auto& descendants = m_descendantsMap.ensure(containingBlock, [] {
            return makeUnique<TrackedRendererListHashSet>();
        }).iterator->value;

        auto isNewEntry = false;
        if (!is<RenderView>(containingBlock) || descendants->isEmptyIgnoringNullReferences())
            isNewEntry = descendants->add(outOfFlowDescendant).isNewEntry;
        else if (outOfFlowDescendant.isFixedPositioned() || isInTopLayerOrBackdrop(outOfFlowDescendant.style(), outOfFlowDescendant.element()))
            isNewEntry = descendants->appendOrMoveToLast(outOfFlowDescendant).isNewEntry;
        else {
            auto ensureLayoutDepentBoxPosition = [&] {
                // RenderView is a special containing block as it may hold both absolute and fixed positioned containing blocks.
                // When a fixed positioned box is also a descendant of an absolute positioned box anchored to the RenderView,
                // we have to make sure that the absolute positioned box is inserted before the fixed box to follow
                // block layout dependency.
                for (auto it = descendants->begin(); it != descendants->end(); ++it) {
                    if (it->isFixedPositioned()) {
                        isNewEntry = descendants->insertBefore(it, outOfFlowDescendant).isNewEntry;
                        return;
                    }
                }
                isNewEntry = descendants->appendOrMoveToLast(outOfFlowDescendant).isNewEntry;
            };
            ensureLayoutDepentBoxPosition();
        }

        if (!isNewEntry) {
            ASSERT(m_containerMap.contains(outOfFlowDescendant));
            return;
        }
        m_containerMap.set(outOfFlowDescendant, containingBlock);
    }

    void removeDescendant(const RenderBox& outOfFlowDescendant)
    {
        auto containingBlock = m_containerMap.take(outOfFlowDescendant);
        if (!containingBlock)
            return;

        auto descendantsIterator = m_descendantsMap.find(*containingBlock.get());
        ASSERT(descendantsIterator != m_descendantsMap.end());
        if (descendantsIterator == m_descendantsMap.end())
            return;

        auto& descendants = descendantsIterator->value;
        ASSERT(descendants->contains(const_cast<RenderBox&>(outOfFlowDescendant)));

        descendants->remove(const_cast<RenderBox&>(outOfFlowDescendant));
        if (descendants->isEmptyIgnoringNullReferences())
            m_descendantsMap.remove(descendantsIterator);
    }
    
    void removeContainingBlock(const RenderBlock& containingBlock)
    {
        auto descendants = m_descendantsMap.take(containingBlock);
        if (!descendants)
            return;

        for (auto& renderer : *descendants)
            m_containerMap.remove(renderer);
    }
    
    TrackedRendererListHashSet* positionedRenderers(const RenderBlock& containingBlock) const
    {
        return m_descendantsMap.get(containingBlock);
    }

private:
    using DescendantsMap = SingleThreadWeakHashMap<const RenderBlock, std::unique_ptr<TrackedRendererListHashSet>>;
    using ContainerMap = SingleThreadWeakHashMap<const RenderBox, SingleThreadWeakPtr<const RenderBlock>>;
    
    DescendantsMap m_descendantsMap;
    ContainerMap m_containerMap;
};

static OutOfFlowDescendantsMap& outOfFlowDescendantsMap()
{
    static NeverDestroyed<OutOfFlowDescendantsMap> mapForOutOfFlowDescendants;
    return mapForOutOfFlowDescendants;
}

using ContinuationOutlineTableMap = SingleThreadWeakHashMap<RenderBlock, std::unique_ptr<SingleThreadWeakListHashSet<RenderInline>>>;

// Allocated only when some of these fields have non-default values

struct RenderBlockRareData {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(RenderBlockRareData);
    WTF_MAKE_NONCOPYABLE(RenderBlockRareData);
public:
    RenderBlockRareData()
    {
    }

    LayoutUnit m_paginationStrut;
    LayoutUnit m_pageLogicalOffset;
    LayoutUnit m_intrinsicBorderForFieldset;
    
    std::optional<SingleThreadWeakPtr<RenderFragmentedFlow>> m_enclosingFragmentedFlow;
};

using RenderBlockRareDataMap = SingleThreadWeakHashMap<const RenderBlock, std::unique_ptr<RenderBlockRareData>>;
static RenderBlockRareDataMap* gRareDataMap;

RenderBlock::RenderBlock(Type type, Element& element, RenderStyle&& style, OptionSet<TypeFlag> baseTypeFlags, TypeSpecificFlags typeSpecificFlags)
    : RenderBox(type, element, WTFMove(style), baseTypeFlags | TypeFlag::IsRenderBlock, typeSpecificFlags)
{
    ASSERT(isRenderBlock());
}

RenderBlock::RenderBlock(Type type, Document& document, RenderStyle&& style, OptionSet<TypeFlag> baseTypeFlags, TypeSpecificFlags typeSpecificFlags)
    : RenderBox(type, document, WTFMove(style), baseTypeFlags | TypeFlag::IsRenderBlock, typeSpecificFlags)
{
    ASSERT(isRenderBlock());
}

RenderBlock::~RenderBlock()
{
    // Blocks can be added to gRareDataMap during willBeDestroyed(), so this code can't move there.
    if (renderBlockHasRareData())
        gRareDataMap->remove(*this);

    // Do not add any more code here. Add it to willBeDestroyed() instead.
}

void RenderBlock::styleWillChange(StyleDifference diff, const RenderStyle& newStyle)
{
    const RenderStyle* oldStyle = hasInitializedStyle() ? &style() : nullptr;
    setBlockLevelReplacedOrAtomicInline(newStyle.isDisplayInlineType());
    if (oldStyle) {
        removeOutOfFlowBoxesIfNeededOnStyleChange(*this, *oldStyle, newStyle);
        if (isLegend() && !oldStyle->isFloating() && newStyle.isFloating())
            setIsExcludedFromNormalLayout(false);
    }
    RenderBox::styleWillChange(diff, newStyle);
}

bool RenderBlock::scrollbarWidthDidChange(const RenderStyle& oldStyle, const RenderStyle& newStyle, ScrollbarOrientation orientation)
{
    return (orientation == ScrollbarOrientation::Vertical ? includeVerticalScrollbarSize() : includeHorizontalScrollbarSize()) && oldStyle.scrollbarWidth() != newStyle.scrollbarWidth();
}

bool RenderBlock::contentBoxLogicalWidthChanged(const RenderStyle& oldStyle, const RenderStyle& newStyle)
{
    if (newStyle.writingMode().isHorizontal()) {
        return oldStyle.borderLeftWidth() != newStyle.borderLeftWidth()
            || oldStyle.borderRightWidth() != newStyle.borderRightWidth()
            || oldStyle.paddingLeft() != newStyle.paddingLeft()
            || oldStyle.paddingRight() != newStyle.paddingRight()
            || scrollbarWidthDidChange(oldStyle, newStyle, ScrollbarOrientation::Vertical);
    }

    return oldStyle.borderTopWidth() != newStyle.borderTopWidth()
        || oldStyle.borderBottomWidth() != newStyle.borderBottomWidth()
        || oldStyle.paddingTop() != newStyle.paddingTop()
        || oldStyle.paddingBottom() != newStyle.paddingBottom()
        || scrollbarWidthDidChange(oldStyle, newStyle, ScrollbarOrientation::Horizontal);
}

bool RenderBlock::paddingBoxLogicaHeightChanged(const RenderStyle& oldStyle, const RenderStyle& newStyle)
{
    auto scrollbarHeightDidChange = [&] (auto orientation) {
        return (orientation == ScrollbarOrientation::Vertical ? includeVerticalScrollbarSize() : includeHorizontalScrollbarSize()) && oldStyle.scrollbarWidth() != newStyle.scrollbarWidth();
    };
    if (newStyle.writingMode().isHorizontal())
        return oldStyle.borderTopWidth() != newStyle.borderTopWidth() || oldStyle.borderBottomWidth() != newStyle.borderBottomWidth() || scrollbarHeightDidChange(ScrollbarOrientation::Horizontal);
    return oldStyle.borderLeftWidth() != newStyle.borderLeftWidth() || oldStyle.borderRightWidth() != newStyle.borderRightWidth() || scrollbarHeightDidChange(ScrollbarOrientation::Vertical);
}

void RenderBlock::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderBox::styleDidChange(diff, oldStyle);

    if (oldStyle)
        adjustFragmentedFlowStateOnContainingBlockChangeIfNeeded(*oldStyle, style());

    propagateStyleToAnonymousChildren(StylePropagationType::BlockAndRubyChildren);

    // It's possible for our border/padding to change, but for the overall logical width of the block to
    // end up being the same. We keep track of this change so in layoutBlock, we can know to set relayoutChildren=true.
    auto shouldForceRelayoutChildren = false;
    if (oldStyle && diff == StyleDifference::Layout && needsLayout()) {
        // Out-of-flow boxes anchored to the padding box.
        shouldForceRelayoutChildren = contentBoxLogicalWidthChanged(*oldStyle, style()) || (outOfFlowBoxes() && paddingBoxLogicaHeightChanged(*oldStyle, style()));
    }
    setShouldForceRelayoutChildren(shouldForceRelayoutChildren);
}

bool RenderBlock::childrenPreventSelfCollapsing() const
{
    // Whether or not we collapse is dependent on whether all our normal flow children
    // are also self-collapsing.
    for (RenderBox* child = firstChildBox(); child; child = child->nextSiblingBox()) {
        if (child->isFloatingOrOutOfFlowPositioned())
            continue;
        if (!child->isSelfCollapsingBlock())
            return true;
    }
    return false;
}

bool RenderBlock::isSelfCollapsingBlock() const
{
    // We are not self-collapsing if we
    // (a) have a non-zero height according to layout (an optimization to avoid wasting time)
    // (b) are a table,
    // (c) have border/padding,
    // (d) have a min-height
    // (e) have specified that one of our margins can't collapse using a CSS extension

    if (isOutOfFlowPositioned())
        return false;

    auto minHeightIsPositive = [&] {
        return WTF::switchOn(style().logicalMinHeight(),
            [](const Style::MinimumSize::Fixed& fixedValue) {
                return fixedValue.isPositive();
            },
            [&](const Style::MinimumSize::Percentage& percentageValue) {
                return percentageValue.isPositive();
            },
            [&](const Style::PreferredSize::Calc&) {
                return true;
            },
            [](const auto&) {
                return false;
            }
        );
    };

    auto heightIsZeroOrAuto = [&] {
        auto handleNonZeroPercentageOrCalc = [&] {
            // While in quirks mode there's always a fixed height ancestor to resolve percent value against (ICB),
            // in standards mode we can only use the containing block.
            if (document().inQuirksMode())
                return false;
            CheckedPtr containingBlock = this->containingBlock();
            if (!containingBlock) {
                ASSERT_NOT_REACHED();
                return false;
            }
            return is<RenderView>(*containingBlock) || !containingBlock->style().logicalHeight().isFixed();
        };

        return WTF::switchOn(style().logicalHeight(),
            [](const Style::PreferredSize::Fixed& fixedValue) {
                return fixedValue.isZero();
            },
            [&](const Style::PreferredSize::Percentage& percentageValue) {
                if (percentageValue.isZero())
                    return true;
                return handleNonZeroPercentageOrCalc();
            },
            [&](const Style::PreferredSize::Calc&) {
                return handleNonZeroPercentageOrCalc();
            },
            [](const CSS::Keyword::Auto&) {
                return true;
            },
// FIXME(GCC): Can be removed on Aug 9th, 2026 or later.
#if COMPILER(GCC) && (__GNUC__ < 13)
            [](auto const&) {
                return false;
            }
#else
            [](CSS::PrimitiveKeyword auto const&) {
                return false;
            }
#endif
        );
    };

    if (logicalHeight() > 0 || isRenderTable() || borderAndPaddingLogicalHeight() || minHeightIsPositive())
        return false;

    if (heightIsZeroOrAuto()) {
        // If the height is 0 or auto, then whether or not we are a self-collapsing block depends
        // on whether we have content that is all self-collapsing or not.
        return !createsNewFormattingContext() && !childrenPreventSelfCollapsing();
    }

    return false;
}

void RenderBlock::beginUpdateScrollInfoAfterLayoutTransaction()
{
    ++view().frameView().layoutContext().updateScrollInfoAfterLayoutTransaction().nestedCount;
}

void RenderBlock::endAndCommitUpdateScrollInfoAfterLayoutTransaction()
{
    auto* transaction = view().frameView().layoutContext().updateScrollInfoAfterLayoutTransactionIfExists();
    ASSERT(transaction);
    if (--transaction->nestedCount)
        return;

    // Calling RenderLayer::updateScrollInfoAfterLayout() may cause its associated block to layout again and
    // updates its scroll info (i.e. call RenderBlock::updateScrollInfoAfterLayout()). We decrement the nestedCount first
    // so that all subsequent calls to RenderBlock::updateScrollInfoAfterLayout() are dispatched immediately.
    // That is, to ensure that such subsequent calls aren't added to |transaction| while we are processing it.
    auto blocksToUpdate = copyToVector(transaction->blocks);
    transaction->blocks.clear();

    for (auto block : blocksToUpdate) {
        if (block->hasControlClip() && block->hasRenderOverflow())
            block->clearLayoutOverflow();
        if (block->hasNonVisibleOverflow())
            block->layer()->updateScrollInfoAfterLayout();
    }
}

static inline bool isDelayingUpdateScrollInfoAfterLayout(const RenderBlock& renderer)
{
    auto* transaction = renderer.view().frameView().layoutContext().updateScrollInfoAfterLayoutTransactionIfExists();
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=97937
    // Workaround for now. We cannot delay the scroll info for overflow
    // for items with opposite writing directions, as the contents needs
    // to overflow in that direction
    return transaction && transaction->nestedCount && !renderer.writingMode().isBlockFlipped();
};

void RenderBlock::updateScrollInfoAfterLayout()
{
    auto hasNonVisibleOverflow = this->hasNonVisibleOverflow();

    if (isDelayingUpdateScrollInfoAfterLayout(*this)) {
        auto shouldUpdate = hasNonVisibleOverflow || hasControlClip();
        if (shouldUpdate) {
            view().frameView().layoutContext().updateScrollInfoAfterLayoutTransactionIfExists()->blocks.add(*this);
            return;
        }
    }

    if (hasNonVisibleOverflow && layer())
        layer()->updateScrollInfoAfterLayout();
}

void RenderBlock::layout()
{
    StackStats::LayoutCheckPoint layoutCheckPoint;

    // Table cells call layoutBlock directly, so don't add any logic here. Put code into layoutBlock().
    {
        auto scope = LayoutScope { *this };
        layoutBlock(RelayoutChildren::No);
    }
    
    // It's safe to check for control clip here, since controls can never be table cells.
    // If we have a lightweight clip, there can never be any overflow from children.
    if (hasControlClip() && m_overflow && !isDelayingUpdateScrollInfoAfterLayout(*this))
        clearLayoutOverflow();

    invalidateBackgroundObscurationStatus();
}

RenderBlockRareData* RenderBlock::blockRareData() const
{
    if (!renderBlockHasRareData())
        return nullptr;
    ASSERT(gRareDataMap);
    return gRareDataMap->get(*this);
}

RenderBlockRareData& RenderBlock::ensureBlockRareData()
{
    if (!gRareDataMap)
        gRareDataMap = new RenderBlockRareDataMap;

    return *gRareDataMap->ensure(*this, [this] {
        setRenderBlockHasRareData(true);
        return makeUnique<RenderBlockRareData>();
    }).iterator->value;
}

void RenderBlock::preparePaginationBeforeBlockLayout(RelayoutChildren& relayoutChildren)
{
    // Fragments changing widths can force us to relayout our children.
    if (CheckedPtr fragmentedFlow = enclosingFragmentedFlow())
        fragmentedFlow->logicalWidthChangedInFragmentsForBlock(*this, relayoutChildren);
}

bool RenderBlock::recomputeLogicalWidth()
{
    LayoutUnit oldWidth = logicalWidth();

    updateLogicalWidth();
    
    bool hasBorderOrPaddingLogicalWidthChanged = this->hasBorderOrPaddingLogicalWidthChanged();
    setShouldForceRelayoutChildren(false);

    return oldWidth != logicalWidth() || hasBorderOrPaddingLogicalWidthChanged;
}

void RenderBlock::layoutBlock(RelayoutChildren, LayoutUnit)
{
    ASSERT_NOT_REACHED();
}

// Overflow is always relative to the border-box of the element in question.
// Therefore, if the element has a vertical scrollbar placed on the left, an overflow rect at x=2px would conceptually intersect the scrollbar.
void RenderBlock::computeOverflow(LayoutUnit oldClientAfterEdge, OptionSet<ComputeOverflowOptions> options)
{
    clearOverflow();
    addOverflowFromInFlowChildren(options);
    addOverflowFromOutOfFlowBoxes();

    if (hasNonVisibleOverflow()) {
        auto includePaddingAfter = [&] {
            // When we have overflow clip, propagate the original spillout since it will include collapsed bottom margins and bottom padding.
            auto clientRect = flippedClientBoxRect();
            auto rectToApply = clientRect;
            // Set the axis we don't care about to be 1, since we want this overflow to always be considered reachable.
            if (isHorizontalWritingMode()) {
                rectToApply.setWidth(1);
                rectToApply.setHeight(std::max(0_lu, oldClientAfterEdge - clientRect.y()));
            } else {
                rectToApply.setWidth(std::max(0_lu, oldClientAfterEdge - clientRect.x()));
                rectToApply.setHeight(1);
            }
            addLayoutOverflow(rectToApply);
        };
        includePaddingAfter();
        if (hasRenderOverflow())
            m_overflow->setLayoutClientAfterEdge(oldClientAfterEdge);
    }
        
    // Add visual overflow from box-shadow, border-image-outset and outline.
    addVisualEffectOverflow();

    // Add visual overflow from theme.
    addVisualOverflowFromTheme();
}

void RenderBlock::clearLayoutOverflow()
{
    if (!m_overflow)
        return;
    
    if (visualOverflowRect() == borderBoxRect()) {
        // FIXME: Implement complete solution for fragments overflow.
        clearOverflow();
        return;
    }
    
    m_overflow->setLayoutOverflow(borderBoxRect());
}

void RenderBlock::addOverflowFromOutOfFlowBoxes()
{
    TrackedRendererListHashSet* outOfFlowDescendants = outOfFlowBoxes();
    if (!outOfFlowDescendants)
        return;

    for (auto& outOfFlowBox : *outOfFlowDescendants) {
        // Fixed positioned elements don't contribute to layout overflow, since they don't scroll with the content.
        if (outOfFlowBox.isAbsolutelyPositioned())
            addOverflowFromContainedBox(outOfFlowBox);
    }
}

void RenderBlock::addVisualOverflowFromTheme()
{
    if (!style().hasUsedAppearance())
        return;

    FloatRect inflatedRect = borderBoxRect();
    theme().adjustRepaintRect(*this, inflatedRect);
    addVisualOverflow(snappedIntRect(LayoutRect(inflatedRect)));

    if (CheckedPtr fragmentedFlow = enclosingFragmentedFlow())
        fragmentedFlow->addFragmentsVisualOverflowFromTheme(*this);
}

void RenderBlock::setLogicalLeftForChild(RenderBox& child, LayoutUnit logicalLeft, ApplyLayoutDeltaMode applyDelta)
{
    if (isHorizontalWritingMode()) {
        if (applyDelta == ApplyLayoutDelta)
            view().frameView().layoutContext().addLayoutDelta(LayoutSize(child.x() - logicalLeft, 0_lu));
        child.setX(logicalLeft);
    } else {
        if (applyDelta == ApplyLayoutDelta)
            view().frameView().layoutContext().addLayoutDelta(LayoutSize(0_lu, child.y() - logicalLeft));
        child.setY(logicalLeft);
    }
}

void RenderBlock::setLogicalTopForChild(RenderBox& child, LayoutUnit logicalTop, ApplyLayoutDeltaMode applyDelta)
{
    if (isHorizontalWritingMode()) {
        if (applyDelta == ApplyLayoutDelta)
            view().frameView().layoutContext().addLayoutDelta(LayoutSize(0_lu, child.y() - logicalTop));
        child.setY(logicalTop);
    } else {
        if (applyDelta == ApplyLayoutDelta)
            view().frameView().layoutContext().addLayoutDelta(LayoutSize(child.x() - logicalTop, 0_lu));
        child.setX(logicalTop);
    }
}

void RenderBlock::updateBlockChildDirtyBitsBeforeLayout(RelayoutChildren relayoutChildren, RenderBox& child)
{
    if (child.isOutOfFlowPositioned())
        return;

    // FIXME: Technically percentage height objects only need a relayout if their percentage isn't going to be turned into
    // an auto value. Add a method to determine this, so that we can avoid the relayout.
    auto childHasRelativeHeight = [&] {
        auto& style = child.style();
        return style.height().isPercentOrCalculated() || style.minHeight().isPercentOrCalculated() || style.maxHeight().isPercentOrCalculated();
    };
    if (relayoutChildren == RelayoutChildren::Yes || (childHasRelativeHeight() && !isRenderView()))
        child.setChildNeedsLayout(MarkOnlyThis);

    // If relayoutChildren is set and the child has percentage padding or an embedded content box, we also need to invalidate the childs pref widths.
    if (relayoutChildren == RelayoutChildren::Yes && child.shouldInvalidatePreferredWidths())
        child.setNeedsPreferredWidthsUpdate(MarkOnlyThis);
}

void RenderBlock::simplifiedNormalFlowLayout()
{
    ASSERT(!childrenInline());

    for (auto& box : childrenOfType<RenderBox>(*this)) {
        if (!box.isOutOfFlowPositioned())
            box.layoutIfNeeded();
    }
}

bool RenderBlock::canPerformSimplifiedLayout() const
{
    if (selfNeedsLayout() || normalChildNeedsLayout() || outOfFlowChildNeedsStaticPositionLayout())
        return false;
    if (auto wasSkippedDuringLastLayout = wasSkippedDuringLastLayoutDueToContentVisibility(); wasSkippedDuringLastLayout && *wasSkippedDuringLastLayout)
        return false;
    if (layoutContext().isSkippedContentRootForLayout(*this) && (outOfFlowChildNeedsLayout() || canContainFixedPositionObjects()))
        return false;
    if (isSkippedContentRoot(*this) && firstChild() && firstChild()->wasSkippedDuringLastLayoutDueToContentVisibility())
        return false;
    return outOfFlowChildNeedsLayout() || needsSimplifiedNormalFlowLayout();
}

bool RenderBlock::simplifiedLayout()
{
    if (!canPerformSimplifiedLayout())
        return false;

    LayoutStateMaintainer statePusher(*this, locationOffset(), isTransformed() || hasReflection() || writingMode().isBlockFlipped());
    if (needsOutOfFlowMovementLayout() && !tryLayoutDoingOutOfFlowMovementOnly())
        return false;

    // Lay out positioned descendants or objects that just need to recompute overflow.
    if (needsSimplifiedNormalFlowLayout())
        simplifiedNormalFlowLayout();

    // Make sure a forced break is applied after the content if we are a flow thread in a simplified layout.
    // This ensures the size information is correctly computed for the last auto-height fragment receiving content.
    if (CheckedPtr fragmentedFlow = dynamicDowncast<RenderFragmentedFlow>(*this))
        fragmentedFlow->applyBreakAfterContent(clientLogicalBottom());

    // Lay out our positioned objects if our positioned child bit is set.
    // Also, if an absolute position element inside a relative positioned container moves, and the absolute element has a fixed position
    // child, neither the fixed element nor its container learn of the movement since outOfFlowChildNeedsLayout() is only marked as far as the
    // relative positioned container. So if we can have fixed pos objects in our positioned objects list check if any of them
    // are statically positioned and thus need to move with their absolute ancestors.
    bool canContainFixedPosObjects = canContainFixedPositionObjects();
    if (outOfFlowChildNeedsLayout() || canContainFixedPosObjects)
        layoutOutOfFlowBoxes(RelayoutChildren::No, !outOfFlowChildNeedsLayout() && canContainFixedPosObjects);

    // Recompute our overflow information.
    // FIXME: We could do better here by computing a temporary overflow object from layoutOutOfFlowBoxes and only
    // updating our overflow if we either used to have overflow or if the new temporary object has overflow.
    // For now just always recompute overflow.  This is no worse performance-wise than the old code that called rightmostPosition and
    // lowestPosition on every relayout so it's not a regression.
    // computeOverflow expects the bottom edge before we clamp our height. Since this information isn't available during
    // simplifiedLayout, we cache the value in m_overflow.
    LayoutUnit oldClientAfterEdge = hasRenderOverflow() ? m_overflow->layoutClientAfterEdge() : clientLogicalBottom();
    computeOverflow(oldClientAfterEdge, ComputeOverflowOptions::RecomputeFloats);

    updateLayerTransform();

    updateScrollInfoAfterLayout();

    clearNeedsLayout();
    return true;
}

void RenderBlock::markFixedPositionBoxForLayoutIfNeeded(RenderBox& positionedChild)
{
    if (positionedChild.style().position() != PositionType::Fixed)
        return;

    bool hasStaticBlockPosition = positionedChild.style().hasStaticBlockPosition(isHorizontalWritingMode());
    bool hasStaticInlinePosition = positionedChild.style().hasStaticInlinePosition(isHorizontalWritingMode());
    if (!hasStaticBlockPosition && !hasStaticInlinePosition)
        return;

    auto* parent = positionedChild.parent();
    while (parent && !is<RenderView>(*parent) && parent->style().position() != PositionType::Absolute)
        parent = parent->parent();
    if (!parent || parent->style().position() != PositionType::Absolute)
        return;

    if (hasStaticInlinePosition) {
        LogicalExtentComputedValues computedValues;
        positionedChild.computeLogicalWidth(computedValues);
        LayoutUnit newLeft = computedValues.m_position;
        if (newLeft != positionedChild.logicalLeft())
            positionedChild.setChildNeedsLayout(MarkOnlyThis);
    } else if (hasStaticBlockPosition) {
        auto logicalTop = positionedChild.logicalTop();
        if (logicalTop != positionedChild.computeLogicalHeight(positionedChild.logicalHeight(), logicalTop).m_position)
            positionedChild.setChildNeedsLayout(MarkOnlyThis);
    }
}

LayoutUnit RenderBlock::marginIntrinsicLogicalWidthForChild(RenderBox& child) const
{
    // A margin has three types: fixed, percentage, and auto (variable).
    // Auto and percentage margins become 0 when computing min/max width.
    // Fixed margins can be added in as is.
    auto& marginLeft = child.style().marginStart(writingMode());
    auto& marginRight = child.style().marginEnd(writingMode());
    const auto& zoomFactor = child.style().usedZoomForLength();
    LayoutUnit margin;
    if (auto fixedMarginLeft = marginLeft.tryFixed(); fixedMarginLeft && !shouldTrimChildMargin(Style::MarginTrimSide::InlineStart, child))
        margin += fixedMarginLeft->resolveZoom(zoomFactor);
    if (auto fixedMarginRight = marginRight.tryFixed(); fixedMarginRight && !shouldTrimChildMargin(Style::MarginTrimSide::InlineEnd, child))
        margin += fixedMarginRight->resolveZoom(zoomFactor);
    return margin;
}

void RenderBlock::layoutOutOfFlowBox(RenderBox& outOfFlowBox, RelayoutChildren relayoutChildren, bool fixedPositionObjectsOnly)
{
    ASSERT(outOfFlowBox.isOutOfFlowPositioned());

    if (layoutContext().isSkippedContentRootForLayout(*this)) {
        outOfFlowBox.clearNeedsLayoutForSkippedContent();
        return;
    }

    estimateFragmentRangeForBoxChild(outOfFlowBox);

    // A fixed position element with an absolute positioned ancestor has no way of knowing if the latter has changed position. So
    // if this is a fixed position element, mark it for layout if it has an abspos ancestor and needs to move with that ancestor, i.e. 
    // it has static position.
    markFixedPositionBoxForLayoutIfNeeded(outOfFlowBox);
    if (fixedPositionObjectsOnly) {
        outOfFlowBox.layoutIfNeeded();
        return;
    }

    // When a non-positioned block element moves, it may have positioned children that are implicitly positioned relative to the
    // non-positioned block.  Rather than trying to detect all of these movement cases, we just always lay out positioned
    // objects that are positioned implicitly like this.  Such objects are rare, and so in typical DHTML menu usage (where everything is
    // positioned explicitly) this should not incur a performance penalty.
    if (relayoutChildren == RelayoutChildren::Yes || (outOfFlowBox.style().hasStaticBlockPosition(isHorizontalWritingMode()) && outOfFlowBox.parent() != this))
        outOfFlowBox.setChildNeedsLayout(MarkOnlyThis);

    // If relayoutChildren is set and the child has percentage padding or an embedded content box, we also need to invalidate the childs pref widths.
    if (relayoutChildren == RelayoutChildren::Yes && outOfFlowBox.shouldInvalidatePreferredWidths())
        outOfFlowBox.setNeedsPreferredWidthsUpdate(MarkOnlyThis);
    
    outOfFlowBox.markForPaginationRelayoutIfNeeded();
    
    // We don't have to do a full layout.  We just have to update our position. Try that first. If we have shrink-to-fit width
    // and we hit the available width constraint, the layoutIfNeeded() will catch it and do a full layout.
    if (outOfFlowBox.needsOutOfFlowMovementLayoutOnly() && outOfFlowBox.tryLayoutDoingOutOfFlowMovementOnly()) {
        if (Style::AnchorPositionEvaluator::isAnchorPositioned(outOfFlowBox.style()))
            Style::AnchorPositionEvaluator::captureScrollSnapshots(outOfFlowBox);
        outOfFlowBox.clearNeedsLayout();
    }

    // If we are paginated or in a line grid, compute a vertical position for our object now.
    // If it's wrong we'll lay out again.
    LayoutUnit oldLogicalTop;
    auto* layoutState = view().frameView().layoutContext().layoutState();
    bool needsBlockDirectionLocationSetBeforeLayout = outOfFlowBox.needsLayout() && layoutState && layoutState->needsBlockDirectionLocationSetBeforeLayout();
    if (needsBlockDirectionLocationSetBeforeLayout) {
        if (isHorizontalWritingMode() == outOfFlowBox.isHorizontalWritingMode())
            outOfFlowBox.updateLogicalHeight();
        else
            outOfFlowBox.updateLogicalWidth();
        oldLogicalTop = logicalTopForChild(outOfFlowBox);
    }

    outOfFlowBox.layoutIfNeeded();
    
    auto* parent = outOfFlowBox.parent();
    bool layoutChanged = false;
    if (auto* flexibleBox = dynamicDowncast<RenderFlexibleBox>(*parent); flexibleBox && flexibleBox->setStaticPositionForPositionedLayout(outOfFlowBox)) {
        // The static position of an abspos child of a flexbox depends on its size
        // (for example, they can be centered). So we may have to reposition the
        // item after layout.
        // FIXME: We could probably avoid a layout here and just reposition?
        layoutChanged = true;
    }

    // Lay out again if our estimate was wrong.
    if (layoutChanged || (needsBlockDirectionLocationSetBeforeLayout && logicalTopForChild(outOfFlowBox) != oldLogicalTop)) {
        outOfFlowBox.setChildNeedsLayout(MarkOnlyThis);
        outOfFlowBox.layoutIfNeeded();
    }

    if (updateFragmentRangeForBoxChild(outOfFlowBox)) {
        outOfFlowBox.setNeedsLayout(MarkOnlyThis);
        outOfFlowBox.layoutIfNeeded();
    }
    
    if (layoutState && layoutState->isPaginated()) {
        if (CheckedPtr blockFlow = dynamicDowncast<RenderBlockFlow>(*this))
            blockFlow->adjustSizeContainmentChildForPagination(outOfFlowBox, outOfFlowBox.logicalTop());
    }
}

void RenderBlock::layoutOutOfFlowBoxes(RelayoutChildren relayoutChildren, bool fixedPositionObjectsOnly)
{
    auto* outOfFlowDescendants = outOfFlowBoxes();
    if (!outOfFlowDescendants)
        return;
    
    // Do not cache outOfFlowDescendants->end() in a local variable, since |outOfFlowDescendants| can be mutated
    // as it is walked. We always need to fetch the new end() value dynamically.
    for (auto& descendant : *outOfFlowDescendants)
        layoutOutOfFlowBox(descendant, relayoutChildren, fixedPositionObjectsOnly);
}

void RenderBlock::markOutOfFlowBoxesForLayout()
{
    auto* outOfFlowDescendants = outOfFlowBoxes();
    if (!outOfFlowDescendants)
        return;

    for (auto& descendant : *outOfFlowDescendants)
        descendant.setChildNeedsLayout();
}

void RenderBlock::markForPaginationRelayoutIfNeeded()
{
    auto* layoutState = view().frameView().layoutContext().layoutState();
    if (needsLayout() || !layoutState || !layoutState->isPaginated())
        return;

    if (layoutState->pageLogicalHeightChanged() || (layoutState->pageLogicalHeight() && layoutState->pageLogicalOffset(this, logicalTop()) != pageLogicalOffset()))
        setChildNeedsLayout(MarkOnlyThis);
}

void RenderBlock::paintCarets(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (paintInfo.phase == PaintPhase::Foreground) {
        paintCaret(paintInfo, paintOffset, CursorCaret);
        paintCaret(paintInfo, paintOffset, DragCaret);
    }
}

void RenderBlock::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    auto adjustedPaintOffset = paintOffset + location();
    PaintPhase phase = paintInfo.phase;

    // FIXME: Could eliminate the isDocumentElementRenderer() check if we fix background painting so that the RenderView paints the root's background.
    auto visualContentIsClippedOut = [&](LayoutRect paintingRect) {
        if (isDocumentElementRenderer())
            return false;

        if (paintInfo.paintBehavior.contains(PaintBehavior::CompositedOverflowScrollContent) && hasLayer() && layer()->usesCompositedScrolling())
            return false;

        auto overflowBox = visualOverflowRect();
        flipForWritingMode(overflowBox);
        overflowBox.moveBy(adjustedPaintOffset);
        return !overflowBox.intersects(paintingRect);
    };

    if (visualContentIsClippedOut(paintInfo.rect))
        return;

    bool pushedClip = pushContentsClip(paintInfo, adjustedPaintOffset);
    paintObject(paintInfo, adjustedPaintOffset);
    if (pushedClip)
        popContentsClip(paintInfo, phase, adjustedPaintOffset);

    // Our scrollbar widgets paint exactly when we tell them to, so that they work properly with
    // z-index. We paint after we painted the background/border, so that the scrollbars will
    // sit above the background/border.
    if (phase == PaintPhase::BlockBackground || phase == PaintPhase::ChildBlockBackground) {
        CheckedPtr layer = this->layer();
        if (hasNonVisibleOverflow() && layer && layer->scrollableArea() && style().usedVisibility() == Visibility::Visible
            && paintInfo.shouldPaintWithinRoot(*this) && !paintInfo.paintRootBackgroundOnly()) {
            layer->checkedScrollableArea()->paintOverflowControls(paintInfo.context(), paintInfo.paintBehavior, roundedIntPoint(adjustedPaintOffset), snappedIntRect(paintInfo.rect));
        }
    }
}

PaintInfo RenderBlock::paintInfoForBlockChildren(const PaintInfo& paintInfo) const
{
    auto phaseForChildren = [&] {
        switch (paintInfo.phase) {
        case PaintPhase::ChildOutlines:
            return PaintPhase::Outline;
        case PaintPhase::ChildBlockBackgrounds:
            return PaintPhase::ChildBlockBackground;
        default:
            return paintInfo.phase;
        }
    };

    PaintInfo paintInfoForChildren(paintInfo);
    paintInfoForChildren.phase = phaseForChildren();
    paintInfoForChildren.updateSubtreePaintRootForChildren(this);

    if (paintInfoForChildren.eventRegionContext())
        paintInfoForChildren.paintBehavior.add(PaintBehavior::EventRegionIncludeBackground);

    return paintInfoForChildren;
}

void RenderBlock::paintContents(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    ASSERT(!isSkippedContentRoot(*this));

    if (childrenInline())
        paintInlineChildren(paintInfo, paintOffset);
    else {
        PaintInfo paintInfoForChildred = paintInfoForBlockChildren(paintInfo);

        // FIXME: Paint-time pagination is obsolete and is now only used by embedded WebViews inside AppKit
        // NSViews. Do not add any more code for this.
        bool usePrintRect = !view().printRect().isEmpty();
        paintChildren(paintInfo, paintOffset, paintInfoForChildred, usePrintRect);
    }
}

void RenderBlock::paintChildren(PaintInfo& paintInfo, const LayoutPoint& paintOffset, PaintInfo& paintInfoForChild, bool usePrintRect)
{
    ASSERT(!isSkippedContentRoot(*this));

    for (auto& child : childrenOfType<RenderBox>(*this)) {
        if (!paintChild(child, paintInfo, paintOffset, paintInfoForChild, usePrintRect))
            return;
    }
}

bool RenderBlock::paintChild(RenderBox& child, PaintInfo& paintInfo, const LayoutPoint& paintOffset, PaintInfo& paintInfoForChild, bool usePrintRect, PaintBlockType paintType)
{
    ASSERT(!isSkippedContentRoot(*this));

    if (child.isExcludedAndPlacedInBorder())
        return true;

    if (child.isSkippedContent()) {
        ASSERT(child.isColumnSpanner());
        return true;
    }

    // Check for page-break-before: always, and if it's set, break and bail.
    bool checkBeforeAlways = !childrenInline() && (usePrintRect && alwaysPageBreak(child.style().breakBefore()));
    LayoutUnit absoluteChildY = paintOffset.y() + child.y();
    if (checkBeforeAlways
        && absoluteChildY > paintInfo.rect.y()
        && absoluteChildY < paintInfo.rect.maxY()) {
        view().setBestTruncatedAt(absoluteChildY, this, true);
        return false;
    }

    if (!child.isFloating() && child.isBlockLevelReplacedOrAtomicInline() && usePrintRect && child.height() <= view().printRect().height()) {
        // Paginate block-level replaced elements.
        if (absoluteChildY + child.height() > view().printRect().maxY()) {
            if (absoluteChildY < view().truncatedAt())
                view().setBestTruncatedAt(absoluteChildY, &child);
            // If we were able to truncate, don't paint.
            if (absoluteChildY >= view().truncatedAt())
                return false;
        }
    }

    LayoutPoint childPoint = flipForWritingModeForChild(child, paintOffset);
    if (!child.hasSelfPaintingLayer() && !child.isFloating()) {
        if (paintType == PaintAsInlineBlock)
            child.paintAsInlineBlock(paintInfoForChild, childPoint);
        else
            child.paint(paintInfoForChild, childPoint);
    }

    // Check for page-break-after: always, and if it's set, break and bail.
    bool checkAfterAlways = !childrenInline() && (usePrintRect && alwaysPageBreak(child.style().breakAfter()));
    if (checkAfterAlways
        && (absoluteChildY + child.height()) > paintInfo.rect.y()
        && (absoluteChildY + child.height()) < paintInfo.rect.maxY()) {
        view().setBestTruncatedAt(absoluteChildY + child.height() + std::max<LayoutUnit>(0, child.collapsedMarginAfter()), this, true);
        return false;
    }

    return true;
}

void RenderBlock::paintCaret(PaintInfo& paintInfo, const LayoutPoint& paintOffset, CaretType type)
{
    auto shouldPaintCaret = [&](RenderBlock* caretPainter, bool isContentEditable) {
        if (caretPainter != this)
            return false;

        return isContentEditable || settings().caretBrowsingEnabled();
    };

    switch (type) {
    case CaretType::CursorCaret: {
        auto caretPainter = frame().selection().caretRendererWithoutUpdatingLayout();
        if (!caretPainter)
            return;

        bool isContentEditable = frame().selection().selection().hasEditableStyle();

        if (shouldPaintCaret(caretPainter, isContentEditable))
            frame().selection().paintCaret(paintInfo.context(), paintOffset);
        break;
    }
    case CaretType::DragCaret: {
        auto caretPainter = page().dragCaretController().caretRenderer();
        if (!caretPainter)
            return;

        bool isContentEditable = page().dragCaretController().isContentEditable();
        if (shouldPaintCaret(caretPainter, isContentEditable))
            page().dragCaretController().paintDragCaret(protectedFrame().ptr(), paintInfo.context(), paintOffset);

        break;
    }
    }
}

void RenderBlock::paintDebugBoxShadowIfApplicable(GraphicsContext& context, const LayoutRect& paintRect) const
{
    // FIXME: Use a more generic, modern-layout wide setting instead.
    if (!settings().legacyLineLayoutVisualCoverageEnabled())
        return;

    auto* flexBox = dynamicDowncast<RenderFlexibleBox>(this);
    if (!flexBox)
        return;

    constexpr size_t shadowExtent = 3;
    GraphicsContextStateSaver stateSaver(context);

    auto shadowRect = paintRect;
    shadowRect.inflate(shadowExtent);
    context.clip(shadowRect);
    context.setDropShadow({ { -shadowRect.width(), 0 }, 30, flexBox->hasModernLayout() ? SRGBA<uint8_t> { 0, 180, 230, 200 } : SRGBA<uint8_t> { 200, 100, 100, 200 }, ShadowRadiusMode::Default });
    context.clipOut(paintRect);
    shadowRect.move(shadowRect.width(), 0);
    context.fillRect(shadowRect, Color::black);
}

void RenderBlock::paintObject(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    PaintPhase paintPhase = paintInfo.phase;
    auto shouldPaintContent = !isSkippedContentRoot(*this);

    // 1. paint background, borders etc
    if ((paintPhase == PaintPhase::BlockBackground || paintPhase == PaintPhase::ChildBlockBackground) && style().usedVisibility() == Visibility::Visible) {
        if (hasVisibleBoxDecorations())
            paintBoxDecorations(paintInfo, paintOffset);
        paintDebugBoxShadowIfApplicable(paintInfo.context(), { paintOffset, size() });
    }
    
    // Paint legends just above the border before we scroll or clip.
    if (shouldPaintContent && (paintPhase == PaintPhase::BlockBackground || paintPhase == PaintPhase::ChildBlockBackground || paintPhase == PaintPhase::Selection))
        paintExcludedChildrenInBorder(paintInfo, paintOffset);
    
    if (paintPhase == PaintPhase::Mask && style().usedVisibility() == Visibility::Visible) {
        paintMask(paintInfo, paintOffset);
        return;
    }

    if (paintPhase == PaintPhase::ClippingMask && style().usedVisibility() == Visibility::Visible) {
        paintClippingMask(paintInfo, paintOffset);
        return;
    }

    // If just painting the root background, then return.
    if (paintInfo.paintRootBackgroundOnly())
        return;

    if (paintPhase == PaintPhase::Accessibility)
        paintInfo.accessibilityRegionContext()->takeBounds(*this, paintOffset);

    if (paintPhase == PaintPhase::EventRegion) {
        auto borderRect = LayoutRect(paintOffset, size());

        Ref document = this->document();
        if (paintInfo.paintBehavior.contains(PaintBehavior::EventRegionIncludeBackground) && visibleToHitTesting()) {
            auto borderShape = BorderShape::shapeForBorderRect(style(), borderRect);
            LOG_WITH_STREAM(EventRegions, stream << "RenderBlock " << *this << " uniting region " << borderShape.deprecatedRoundedRect() << " event listener types " << style().eventListenerRegionTypes());
            bool overrideUserModifyIsEditable = isRenderTextControl() && downcast<RenderTextControl>(*this).protectedTextFormControlElement()->isInnerTextElementEditable();
            paintInfo.eventRegionContext()->unite(borderShape.deprecatedPixelSnappedRoundedRect(document->deviceScaleFactor()), *this, style(), overrideUserModifyIsEditable);
        }

        if (!paintInfo.paintBehavior.contains(PaintBehavior::EventRegionIncludeForeground))
            return;

        bool needsTraverseDescendants = hasVisualOverflow() || containsFloats() || !paintInfo.eventRegionContext()->contains(enclosingIntRect(borderRect)) || view().needsEventRegionUpdateForNonCompositedFrame();
        LOG_WITH_STREAM(EventRegions, stream << "RenderBlock " << *this << " needsTraverseDescendants for event region: hasVisualOverflow: " << hasVisualOverflow() << " containsFloats: "
            << containsFloats() <<  " border box is outside current region: " << !paintInfo.eventRegionContext()->contains(enclosingIntRect(borderRect)) << " needsEventRegionUpdateForNonCompositedFrame:" << view().needsEventRegionUpdateForNonCompositedFrame());
#if ENABLE(TOUCH_ACTION_REGIONS)
        needsTraverseDescendants |= document->mayHaveElementsWithNonAutoTouchAction();
        LOG_WITH_STREAM(EventRegions, stream << "  may have touch-action elements: " << document->mayHaveElementsWithNonAutoTouchAction());
#endif
#if ENABLE(WHEEL_EVENT_REGIONS)
        needsTraverseDescendants |= document->hasWheelEventHandlers();
        LOG_WITH_STREAM(EventRegions, stream << "  has wheel event handlers: " << document->hasWheelEventHandlers());
#endif
#if ENABLE(TOUCH_EVENT_REGIONS)
        needsTraverseDescendants |= document->hasTouchEventHandlers();
        LOG_WITH_STREAM(EventRegions, stream << "  has touch event handlers: " << document->hasTouchEventHandlers());
#endif

#if ENABLE(EDITABLE_REGION)
        // We treat the entire text control as editable to match users' expectation even
        // though it's actually the inner text element of the control that is editable.
        // So, no need to traverse to find the inner text element in this case.
        if (!isRenderTextControl()) {
            needsTraverseDescendants |= document->mayHaveEditableElements() && page().shouldBuildEditableRegion();
            LOG_WITH_STREAM(EventRegions, stream << "  needs editable event region: " << (document->mayHaveEditableElements() && page().shouldBuildEditableRegion()));
        }
#endif

#if ENABLE(INTERACTION_REGIONS_IN_EVENT_REGION)
        needsTraverseDescendants |= page().shouldBuildInteractionRegions();
#endif

        if (!needsTraverseDescendants)
            return;
    }

    // Adjust our painting position if we're inside a scrolled layer (e.g., an overflow:auto div).
    LayoutPoint scrolledOffset = paintOffset;
    scrolledOffset.moveBy(-scrollPosition());

    // Column rules need to account for scrolling and clipping.
    // FIXME: Clipping of column rules does not work. We will need a separate paint phase for column rules I suspect in order to get
    // clipping correct (since it has to paint as background but is still considered "contents").
    if ((paintPhase == PaintPhase::BlockBackground || paintPhase == PaintPhase::ChildBlockBackground) && style().usedVisibility() == Visibility::Visible)
        paintColumnRules(paintInfo, scrolledOffset);

    // Done with backgrounds, borders and column rules.
    if (paintPhase == PaintPhase::BlockBackground)
        return;
    
    // 2. paint contents
    if (shouldPaintContent) {
        if (paintPhase != PaintPhase::SelfOutline)
            paintContents(paintInfo, scrolledOffset);
    }

    // 3. paint selection
    if (!document().printing()) {
        // Fill in gaps in selection on lines, between blocks and "empty space" when content is skipped.
        paintSelection(paintInfo, scrolledOffset);
    }

    if (shouldPaintContent) {
        // 4. paint floats.
        if (paintPhase == PaintPhase::Float || paintPhase == PaintPhase::Selection || paintPhase == PaintPhase::TextClip || paintPhase == PaintPhase::EventRegion || paintPhase == PaintPhase::Accessibility)
            paintFloats(paintInfo, scrolledOffset, paintPhase == PaintPhase::Selection || paintPhase == PaintPhase::TextClip || paintPhase == PaintPhase::EventRegion || paintPhase == PaintPhase::Accessibility);
    }

    // 5. paint outline.
    if ((paintPhase == PaintPhase::Outline || paintPhase == PaintPhase::SelfOutline) && hasOutline() && style().usedVisibility() == Visibility::Visible) {
        // Don't paint focus ring for anonymous block continuation because the
        // inline element having outline-style:auto paints the whole focus ring.
        if (style().outlineStyle() != OutlineStyle::Auto || !isContinuation())
            paintOutline(paintInfo, LayoutRect(paintOffset, size()));
    }

    // 6. paint continuation outlines.
    if ((paintPhase == PaintPhase::Outline || paintPhase == PaintPhase::ChildOutlines)) {
        RenderInline* inlineCont = inlineContinuation();
        if (inlineCont && inlineCont->hasOutline() && inlineCont->style().usedVisibility() == Visibility::Visible) {
            RenderInline* inlineRenderer = downcast<RenderInline>(inlineCont->element()->renderer());
            RenderBlock* containingBlock = this->containingBlock();

            bool inlineEnclosedInSelfPaintingLayer = false;
            for (RenderBoxModelObject* box = inlineRenderer; box != containingBlock; box = &box->parent()->enclosingBoxModelObject()) {
                if (box->hasSelfPaintingLayer()) {
                    inlineEnclosedInSelfPaintingLayer = true;
                    break;
                }
            }

            // Do not add continuations for outline painting by our containing block if we are a relative positioned
            // anonymous block (i.e. have our own layer), paint them straightaway instead. This is because a block depends on renderers in its continuation table being
            // in the same layer. 
            if (!inlineEnclosedInSelfPaintingLayer && !hasLayer())
                containingBlock->addContinuationWithOutline(inlineRenderer);
            else if (!InlineIterator::lineLeftmostInlineBoxFor(*inlineRenderer) || (!inlineEnclosedInSelfPaintingLayer && hasLayer())) {
                auto outlineOffset = paintOffset - locationOffset() + inlineRenderer->containingBlock()->location();
                OutlinePainter { paintInfo }.paintOutline(*inlineRenderer, outlineOffset);
            }
        }
        paintContinuationOutlines(paintInfo, paintOffset);
    }

    // 7. paint caret.
    // If the caret's node's render object's containing block is this block, and the paint action is PaintPhase::Foreground,
    // then paint the caret.
    if (shouldPaintContent)
        paintCarets(paintInfo, paintOffset);
}

static ContinuationOutlineTableMap* continuationOutlineTable()
{
    static NeverDestroyed<ContinuationOutlineTableMap> table;
    return &table.get();
}

void RenderBlock::addContinuationWithOutline(RenderInline* flow)
{
    // We can't make this work if the inline is in a layer.  We'll just rely on the broken
    // way of painting.
    ASSERT(!flow->layer() && !flow->isContinuation());
    
    auto* table = continuationOutlineTable();
    auto* continuations = table->get(*this);
    if (!continuations) {
        continuations = new SingleThreadWeakListHashSet<RenderInline>;
        table->set(*this, std::unique_ptr<SingleThreadWeakListHashSet<RenderInline>>(continuations));
    }
    
    continuations->add(*flow);
}

bool RenderBlock::establishesIndependentFormattingContextIgnoringDisplayType(const RenderStyle& style) const
{
    if (!element()) {
        ASSERT(isAnonymous());
        return false;
    }

    auto isBlockBoxWithPotentiallyScrollableOverflow = [&] {
        return style.isDisplayBlockLevel()
            && style.doesDisplayGenerateBlockContainer()
            && hasNonVisibleOverflow()
            && style.overflowX() != Overflow::Clip
            && style.overflowX() != Overflow::Visible;
    };

    return style.isFloating()
        || style.hasOutOfFlowPosition()
        || isBlockBoxWithPotentiallyScrollableOverflow()
        || style.usedContain().contains(Style::ContainValue::Layout)
        || style.containerType() != ContainerType::Normal
        || WebCore::shouldApplyPaintContainment(style, *protectedElement())
        || (style.isDisplayBlockLevel() && !style.blockStepSize().isNone());
}

bool RenderBlock::establishesIndependentFormattingContext() const
{
    auto& style = this->style();
    if (establishesIndependentFormattingContextIgnoringDisplayType(style))
        return true;

    if (isGridItem()) {
        // Grid items establish a new independent formatting context, unless they're a subgrid
        // https://drafts.csswg.org/css-grid-2/#grid-item-display
        if (!style.gridTemplateColumns().subgrid && !style.gridTemplateRows().subgrid)
            return true;
        // Masonry makes grid items not subgrids.
        if (CheckedPtr parentGridBox = dynamicDowncast<RenderGrid>(parent()))
            return parentGridBox->isMasonry();
    }

    return false;
}

bool RenderBlock::createsNewFormattingContext() const
{
    // Writing-mode changes establish an independent block formatting context
    // if the box is a block-container.
    // https://drafts.csswg.org/css-writing-modes/#block-flow
    if (isWritingModeRoot() && isBlockContainer())
        return true;
    auto& style = this->style();
    if (isBlockContainer() && !style.alignContent().isNormal())
        return true;
    return isNonReplacedAtomicInlineLevelBox()
        || style.isDisplayFlexibleBoxIncludingDeprecatedOrGridFormattingContextBox()
        || isFlexItemIncludingDeprecated()
        || isRenderTable()
        || isRenderTableCell()
        || isRenderTableCaption()
        || isFieldset()
        || isDocumentElementRenderer()
        || isRenderFragmentedFlow()
        || isRenderSVGForeignObject()
        || style.specifiesColumns()
        || style.columnSpan() == ColumnSpan::All
        || style.display() == DisplayType::FlowRoot
        || establishesIndependentFormattingContext();
}

#if ASSERT_ENABLED
bool RenderBlock::paintsContinuationOutline(const RenderInline& renderer)
{
    if (auto* continuations = continuationOutlineTable()->get(*this))
        return continuations->contains(renderer);
    return false;
}
#endif

void RenderBlock::paintContinuationOutlines(PaintInfo& info, const LayoutPoint& paintOffset)
{
    auto* table = continuationOutlineTable();
    auto continuations = table->take(*this);
    if (!continuations)
        return;

    LayoutPoint accumulatedPaintOffset = paintOffset;
    // Paint each continuation outline.
    OutlinePainter outlinePainter { info };
    for (auto& renderInline : *continuations) {
        // Need to add in the coordinates of the intervening blocks.
        auto* block = renderInline.containingBlock();
        for ( ; block && block != this; block = block->containingBlock())
            accumulatedPaintOffset.moveBy(block->location());
        ASSERT(block);
        outlinePainter.paintOutline(renderInline, accumulatedPaintOffset);
    }
}

bool RenderBlock::shouldPaintSelectionGaps() const
{
    return selectionState() != HighlightState::None && style().usedVisibility() == Visibility::Visible && isSelectionRoot();
}

bool RenderBlock::isSelectionRoot() const
{
    if (isPseudoElement())
        return false;
    ASSERT(element() || isAnonymous());
        
    // FIXME: Eventually tables should have to learn how to fill gaps between cells, at least in simple non-spanning cases.
    if (isRenderTable())
        return false;

    if (isBody() || isDocumentElementRenderer() || hasNonVisibleOverflow()
        || isPositioned() || isFloating()
        || isRenderTableCell() || isNonReplacedAtomicInlineLevelBox()
        || isTransformed() || hasReflection() || hasMask() || isWritingModeRoot()
        || isRenderFragmentedFlow() || style().columnSpan() == ColumnSpan::All
        || isFlexItemIncludingDeprecated() || isGridItem())
        return true;
    
    if (view().selection().start()) {
        RefPtr startElement = view().selection().start()->node();
        if (startElement && startElement->rootEditableElement() == element())
            return true;
    }
    
    return false;
}

GapRects RenderBlock::selectionGapRectsForRepaint(const RenderLayerModelObject* repaintContainer)
{
    ASSERT(!needsLayout());

    if (!shouldPaintSelectionGaps())
        return { };

    FloatPoint containerPoint = localToContainerPoint(FloatPoint(), repaintContainer, UseTransforms);
    LayoutPoint offsetFromRepaintContainer(containerPoint - toFloatSize(scrollPosition()));

    LogicalSelectionOffsetCaches cache(*this);
    LayoutUnit lastTop;
    LayoutUnit lastLeft = logicalLeftSelectionOffset(*this, lastTop, cache);
    LayoutUnit lastRight = logicalRightSelectionOffset(*this, lastTop, cache);
    
    return selectionGaps(*this, offsetFromRepaintContainer, IntSize(), lastTop, lastLeft, lastRight, cache);
}

void RenderBlock::paintSelection(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
#if ENABLE(TEXT_SELECTION)
    if (shouldPaintSelectionGaps() && paintInfo.phase == PaintPhase::Foreground) {
        LogicalSelectionOffsetCaches cache(*this);
        LayoutUnit lastTop;
        LayoutUnit lastLeft = logicalLeftSelectionOffset(*this, lastTop, cache);
        LayoutUnit lastRight = logicalRightSelectionOffset(*this, lastTop, cache);
        GraphicsContextStateSaver stateSaver(paintInfo.context());

        LayoutRect gapRectsBounds = selectionGaps(*this, paintOffset, LayoutSize(), lastTop, lastLeft, lastRight, cache, &paintInfo);
        if (!gapRectsBounds.isEmpty()) {
            if (RenderLayer* layer = enclosingLayer()) {
                gapRectsBounds.moveBy(-paintOffset);
                if (!hasLayer()) {
                    LayoutRect localBounds(gapRectsBounds);
                    flipForWritingMode(localBounds);
                    gapRectsBounds = localToContainerQuad(FloatRect(localBounds), &layer->renderer()).enclosingBoundingBox();
                    if (layer->renderer().isRenderBox())
                        gapRectsBounds.moveBy(layer->renderBox()->scrollPosition());
                }
                layer->addBlockSelectionGapsBounds(gapRectsBounds);
            }
        }
    }
#else
    UNUSED_PARAM(paintInfo);
    UNUSED_PARAM(paintOffset);
#endif
}

static void clipOutOutOfFlowBoxes(const PaintInfo* paintInfo, const LayoutPoint& offset, TrackedRendererListHashSet* outOfFlowBoxes)
{
    if (!outOfFlowBoxes)
        return;
    
    for (auto& renderer : *outOfFlowBoxes)
        paintInfo->context().clipOut(IntRect(offset.x() + renderer.x(), offset.y() + renderer.y(), renderer.width(), renderer.height()));
}

LayoutUnit blockDirectionOffset(RenderBlock& rootBlock, const LayoutSize& offsetFromRootBlock)
{
    return rootBlock.isHorizontalWritingMode() ? offsetFromRootBlock.height() : offsetFromRootBlock.width();
}

LayoutUnit inlineDirectionOffset(RenderBlock& rootBlock, const LayoutSize& offsetFromRootBlock)
{
    return rootBlock.isHorizontalWritingMode() ? offsetFromRootBlock.width() : offsetFromRootBlock.height();
}

LayoutRect RenderBlock::logicalRectToPhysicalRect(const LayoutPoint& rootBlockPhysicalPosition, const LayoutRect& logicalRect)
{
    LayoutRect result;
    if (isHorizontalWritingMode())
        result = logicalRect;
    else
        result = LayoutRect(logicalRect.y(), logicalRect.x(), logicalRect.height(), logicalRect.width());
    flipForWritingMode(result);
    result.moveBy(rootBlockPhysicalPosition);
    return result;
}

GapRects RenderBlock::selectionGaps(RenderBlock& rootBlock, const LayoutPoint& rootBlockPhysicalPosition, const LayoutSize& offsetFromRootBlock,
    LayoutUnit& lastLogicalTop, LayoutUnit& lastLogicalLeft, LayoutUnit& lastLogicalRight, const LogicalSelectionOffsetCaches& cache, const PaintInfo* paintInfo)
{
    // IMPORTANT: Callers of this method that intend for painting to happen need to do a save/restore.
    // Clip out floating and positioned objects when painting selection gaps.
    if (paintInfo) {
        // Note that we don't clip out overflow for positioned objects.  We just stick to the border box.
        LayoutRect flippedBlockRect(offsetFromRootBlock.width(), offsetFromRootBlock.height(), width(), height());
        rootBlock.flipForWritingMode(flippedBlockRect);
        flippedBlockRect.moveBy(rootBlockPhysicalPosition);
        clipOutOutOfFlowBoxes(paintInfo, flippedBlockRect.location(), outOfFlowBoxes());
        if (isBody() || isDocumentElementRenderer()) { // The <body> must make sure to examine its containingBlock's positioned objects.
            for (RenderBlock* cb = containingBlock(); cb && !is<RenderView>(*cb); cb = cb->containingBlock())
                clipOutOutOfFlowBoxes(paintInfo, LayoutPoint(cb->x(), cb->y()), cb->outOfFlowBoxes()); // FIXME: Not right for flipped writing modes.
        }
        clipOutFloatingBoxes(rootBlock, paintInfo, rootBlockPhysicalPosition, offsetFromRootBlock);
    }

    // FIXME: overflow: auto/scroll fragments need more math here, since painting in the border box is different from painting in the padding box (one is scrolled, the other is fixed).
    if (!is<RenderBlockFlow>(*this)) {
        // FIXME: Make multi-column selection gap filling work someday.
        return { };
    }

    if (isFlexItem() || isGridItem() || isDeprecatedFlexItem()) {
        // FIXME: Adding a selection gap to these blocks would produce correct (visual) result only if we could also paint selection gaps between them,
        // as we do for blocks in a BFC. Returning an empty gap rect here means we only paint the selection over the content,
        // as opposed to expand it all the way to the end of the container.
        return { };
    }

    if (isTransformed() || style().columnSpan() == ColumnSpan::All || isRenderFragmentedFlow()) {
        // FIXME: We should learn how to gap fill multiple columns and transforms eventually.
        lastLogicalTop = blockDirectionOffset(rootBlock, offsetFromRootBlock) + logicalHeight();
        lastLogicalLeft = logicalLeftSelectionOffset(rootBlock, logicalHeight(), cache);
        lastLogicalRight = logicalRightSelectionOffset(rootBlock, logicalHeight(), cache);
        return { };
    }

    GapRects result;
    if (childrenInline())
        result = inlineSelectionGaps(rootBlock, rootBlockPhysicalPosition, offsetFromRootBlock, lastLogicalTop, lastLogicalLeft, lastLogicalRight, cache, paintInfo);
    else
        result = blockSelectionGaps(rootBlock, rootBlockPhysicalPosition, offsetFromRootBlock, lastLogicalTop, lastLogicalLeft, lastLogicalRight, cache, paintInfo);

    // Fill the vertical gap all the way to the bottom of our block if the selection extends past our block.
    if (&rootBlock == this && (selectionState() != HighlightState::Both && selectionState() != HighlightState::End)) {
        result.uniteCenter(blockSelectionGap(rootBlock, rootBlockPhysicalPosition, offsetFromRootBlock,
            lastLogicalTop, lastLogicalLeft, lastLogicalRight, logicalHeight(), cache, paintInfo));
    }

    return result;
}

GapRects RenderBlock::inlineSelectionGaps(RenderBlock&, const LayoutPoint&, const LayoutSize&, LayoutUnit&, LayoutUnit&, LayoutUnit&, const LogicalSelectionOffsetCaches&, const PaintInfo*)
{
    ASSERT_NOT_REACHED();
    return GapRects();
}

GapRects RenderBlock::blockSelectionGaps(RenderBlock& rootBlock, const LayoutPoint& rootBlockPhysicalPosition, const LayoutSize& offsetFromRootBlock,
    LayoutUnit& lastLogicalTop, LayoutUnit& lastLogicalLeft, LayoutUnit& lastLogicalRight, const LogicalSelectionOffsetCaches& cache, const PaintInfo* paintInfo)
{
    ASSERT(!isSkippedContent());

    if (isSkippedContentRoot(*this))
        return { };

    // Jump right to the first block child that contains some selected objects.
    RenderBox* curr;
    for (curr = firstChildBox(); curr && curr->selectionState() == HighlightState::None; curr = curr->nextSiblingBox()) { }
    
    if (!curr)
        return { };

    LogicalSelectionOffsetCaches childCache(*this, cache);

    GapRects result;
    for (bool sawSelectionEnd = false; curr && !sawSelectionEnd; curr = curr->nextSiblingBox()) {
        HighlightState childState = curr->selectionState();
        if (childState == HighlightState::Both || childState == HighlightState::End)
            sawSelectionEnd = true;

        if (curr->isFloatingOrOutOfFlowPositioned())
            continue; // We must be a normal flow object in order to even be considered.

        if (curr->isInFlowPositioned() && curr->hasLayer()) {
            // If the relposition offset is anything other than 0, then treat this just like an absolute positioned element.
            // Just disregard it completely.
            LayoutSize relOffset = curr->layer()->offsetForInFlowPosition();
            if (relOffset.width() || relOffset.height())
                continue;
        }

        // FIXME: Eventually we won't special-case table and other layout roots like this.
        auto propagatesSelectionToChildren = is<RenderTable>(*curr) || is<RenderFlexibleBox>(*curr) || is<RenderDeprecatedFlexibleBox>(*curr) || is<RenderGrid>(*curr);
        auto paintsOwnSelection = curr->shouldPaintSelectionGaps() || propagatesSelectionToChildren;
        bool fillBlockGaps = paintsOwnSelection || (curr->canBeSelectionLeaf() && childState != HighlightState::None);
        if (fillBlockGaps) {
            // We need to fill the vertical gap above this object.
            if (childState == HighlightState::End || childState == HighlightState::Inside) {
                // Fill the gap above the object.
                result.uniteCenter(blockSelectionGap(rootBlock, rootBlockPhysicalPosition, offsetFromRootBlock,
                    lastLogicalTop, lastLogicalLeft, lastLogicalRight, curr->logicalTop(), cache, paintInfo));
            }

            // Only fill side gaps for objects that paint their own selection if we know for sure the selection is going to extend all the way *past*
            // our object.  We know this if the selection did not end inside our object.
            if (paintsOwnSelection && (childState == HighlightState::Start || sawSelectionEnd))
                childState = HighlightState::None;

            // Fill side gaps on this object based off its state.
            bool leftGap, rightGap;
            getSelectionGapInfo(childState, leftGap, rightGap);

            if (leftGap)
                result.uniteLeft(logicalLeftSelectionGap(rootBlock, rootBlockPhysicalPosition, offsetFromRootBlock, this, curr->logicalLeft(), curr->logicalTop(), curr->logicalHeight(), cache, paintInfo));
            if (rightGap)
                result.uniteRight(logicalRightSelectionGap(rootBlock, rootBlockPhysicalPosition, offsetFromRootBlock, this, curr->logicalRight(), curr->logicalTop(), curr->logicalHeight(), cache, paintInfo));

            // Update lastLogicalTop to be just underneath the object.  lastLogicalLeft and lastLogicalRight extend as far as
            // they can without bumping into floating or positioned objects.  Ideally they will go right up
            // to the border of the root selection block.
            lastLogicalTop = blockDirectionOffset(rootBlock, offsetFromRootBlock) + curr->logicalBottom();
            lastLogicalLeft = logicalLeftSelectionOffset(rootBlock, curr->logicalBottom(), cache);
            lastLogicalRight = logicalRightSelectionOffset(rootBlock, curr->logicalBottom(), cache);
        } else if (childState != HighlightState::None) {
            if (auto* renderBlock = dynamicDowncast<RenderBlock>(*curr)) {
                // We must be a block that has some selected object inside it, so recur.
                result.unite(renderBlock->selectionGaps(rootBlock, rootBlockPhysicalPosition, LayoutSize(offsetFromRootBlock.width() + curr->x(), offsetFromRootBlock.height() + curr->y()),
                    lastLogicalTop, lastLogicalLeft, lastLogicalRight, childCache, paintInfo));
            }
        }
    }
    return result;
}

LayoutRect RenderBlock::blockSelectionGap(RenderBlock& rootBlock, const LayoutPoint& rootBlockPhysicalPosition, const LayoutSize& offsetFromRootBlock,
    LayoutUnit lastLogicalTop, LayoutUnit lastLogicalLeft, LayoutUnit lastLogicalRight, LayoutUnit logicalBottom, const LogicalSelectionOffsetCaches& cache, const PaintInfo* paintInfo)
{
    LayoutUnit logicalTop = lastLogicalTop;
    LayoutUnit logicalHeight = blockDirectionOffset(rootBlock, offsetFromRootBlock) + logicalBottom - logicalTop;
    if (logicalHeight <= 0)
        return LayoutRect();

    // Get the selection offsets for the bottom of the gap
    LayoutUnit logicalLeft = std::max(lastLogicalLeft, logicalLeftSelectionOffset(rootBlock, logicalBottom, cache));
    LayoutUnit logicalRight = std::min(lastLogicalRight, logicalRightSelectionOffset(rootBlock, logicalBottom, cache));
    LayoutUnit logicalWidth = logicalRight - logicalLeft;
    if (logicalWidth <= 0)
        return LayoutRect();

    LayoutRect gapRect = rootBlock.logicalRectToPhysicalRect(rootBlockPhysicalPosition, LayoutRect(logicalLeft, logicalTop, logicalWidth, logicalHeight));
    if (paintInfo)
        paintInfo->context().fillRect(snapRectToDevicePixels(gapRect, protectedDocument()->deviceScaleFactor()), selectionBackgroundColor());
    return gapRect;
}

LayoutRect RenderBlock::logicalLeftSelectionGap(RenderBlock& rootBlock, const LayoutPoint& rootBlockPhysicalPosition, const LayoutSize& offsetFromRootBlock, RenderElement* selObj, LayoutUnit logicalLeft, LayoutUnit logicalTop, LayoutUnit logicalHeight, const LogicalSelectionOffsetCaches& cache, const PaintInfo* paintInfo)
{
    LayoutUnit rootBlockLogicalTop = blockDirectionOffset(rootBlock, offsetFromRootBlock) + logicalTop;
    LayoutUnit rootBlockLogicalLeft = std::max(logicalLeftSelectionOffset(rootBlock, logicalTop, cache), logicalLeftSelectionOffset(rootBlock, logicalTop + logicalHeight, cache));
    LayoutUnit rootBlockLogicalRight = std::min(inlineDirectionOffset(rootBlock, offsetFromRootBlock) + logicalLeft,
        std::min(logicalRightSelectionOffset(rootBlock, logicalTop, cache), logicalRightSelectionOffset(rootBlock, logicalTop + logicalHeight, cache)));
    LayoutUnit rootBlockLogicalWidth = rootBlockLogicalRight - rootBlockLogicalLeft;
    if (rootBlockLogicalWidth <= 0)
        return LayoutRect();

    LayoutRect gapRect = rootBlock.logicalRectToPhysicalRect(rootBlockPhysicalPosition, LayoutRect(rootBlockLogicalLeft, rootBlockLogicalTop, rootBlockLogicalWidth, logicalHeight));
    if (paintInfo)
        paintInfo->context().fillRect(snapRectToDevicePixels(gapRect, protectedDocument()->deviceScaleFactor()), selObj->selectionBackgroundColor());
    return gapRect;
}

LayoutRect RenderBlock::logicalRightSelectionGap(RenderBlock& rootBlock, const LayoutPoint& rootBlockPhysicalPosition, const LayoutSize& offsetFromRootBlock, RenderElement* selObj, LayoutUnit logicalRight, LayoutUnit logicalTop, LayoutUnit logicalHeight, const LogicalSelectionOffsetCaches& cache, const PaintInfo* paintInfo)
{
    LayoutUnit rootBlockLogicalTop = blockDirectionOffset(rootBlock, offsetFromRootBlock) + logicalTop;
    LayoutUnit rootBlockLogicalLeft = std::max(inlineDirectionOffset(rootBlock, offsetFromRootBlock) + logicalRight,
        std::max(logicalLeftSelectionOffset(rootBlock, logicalTop, cache), logicalLeftSelectionOffset(rootBlock, logicalTop + logicalHeight, cache)));
    LayoutUnit rootBlockLogicalRight = std::min(logicalRightSelectionOffset(rootBlock, logicalTop, cache), logicalRightSelectionOffset(rootBlock, logicalTop + logicalHeight, cache));
    LayoutUnit rootBlockLogicalWidth = rootBlockLogicalRight - rootBlockLogicalLeft;
    if (rootBlockLogicalWidth <= 0)
        return LayoutRect();

    LayoutRect gapRect = rootBlock.logicalRectToPhysicalRect(rootBlockPhysicalPosition, LayoutRect(rootBlockLogicalLeft, rootBlockLogicalTop, rootBlockLogicalWidth, logicalHeight));
    if (paintInfo)
        paintInfo->context().fillRect(snapRectToDevicePixels(gapRect, protectedDocument()->deviceScaleFactor()), selObj->selectionBackgroundColor());
    return gapRect;
}

void RenderBlock::getSelectionGapInfo(HighlightState state, bool& leftGap, bool& rightGap)
{
    bool ltr = writingMode().isLogicalLeftInlineStart();
    leftGap = (state == RenderObject::HighlightState::Inside) || (state == RenderObject::HighlightState::End && ltr) || (state == RenderObject::HighlightState::Start && !ltr);
    rightGap = (state == RenderObject::HighlightState::Inside) || (state == RenderObject::HighlightState::Start && ltr) || (state == RenderObject::HighlightState::End && !ltr);
}

LayoutUnit RenderBlock::logicalLeftSelectionOffset(RenderBlock& rootBlock, LayoutUnit position, const LogicalSelectionOffsetCaches& cache)
{
    LayoutUnit logicalLeft = logicalLeftOffsetForLine(position);
    if (logicalLeft == logicalLeftOffsetForContent()) {
        if (&rootBlock != this) // The border can potentially be further extended by our containingBlock().
            return cache.containingBlockInfo(*this).logicalLeftSelectionOffset(rootBlock, position + logicalTop());
        return logicalLeft;
    }

    RenderBlock* cb = this;
    const LogicalSelectionOffsetCaches* currentCache = &cache;
    while (cb != &rootBlock) {
        logicalLeft += cb->logicalLeft();

        ASSERT(currentCache);
        auto info = currentCache->containingBlockInfo(*cb);
        cb = info.block();
        if (!cb)
            break;
        currentCache = info.cache();
    }
    return logicalLeft;
}

LayoutUnit RenderBlock::logicalRightSelectionOffset(RenderBlock& rootBlock, LayoutUnit position, const LogicalSelectionOffsetCaches& cache)
{
    LayoutUnit logicalRight = logicalRightOffsetForLine(position);
    if (logicalRight == logicalRightOffsetForContent()) {
        if (&rootBlock != this) // The border can potentially be further extended by our containingBlock().
            return cache.containingBlockInfo(*this).logicalRightSelectionOffset(rootBlock, position + logicalTop());
        return logicalRight;
    }

    RenderBlock* cb = this;
    const LogicalSelectionOffsetCaches* currentCache = &cache;
    while (cb != &rootBlock) {
        logicalRight += cb->logicalLeft();

        ASSERT(currentCache);
        auto info = currentCache->containingBlockInfo(*cb);
        cb = info.block();
        if (!cb)
            break;
        currentCache = info.cache();
    }
    return logicalRight;
}

TrackedRendererListHashSet* RenderBlock::outOfFlowBoxes() const
{
    return outOfFlowDescendantsMap().positionedRenderers(*this);
}

void RenderBlock::addOutOfFlowBox(RenderBox& outOfFlowBox)
{
    ASSERT(outOfFlowBox.isOutOfFlowPositioned());
    ASSERT(!isAnonymousBlock());

    outOfFlowBox.clearGridAreaContentSize();

    if (outOfFlowBox.isRenderFragmentedFlow())
        return;
    // FIXME: Find out if we can do this as part of outOfFlowBox.setChildNeedsLayout(MarkOnlyThis)
    if (outOfFlowBox.needsLayout()) {
        // We should turn this bit on only while in layout.
        ASSERT(outOfFlowChildNeedsLayout() || view().frameView().layoutContext().isInLayout());
        setOutOfFlowChildNeedsLayoutBit(true);
    }
    outOfFlowDescendantsMap().addDescendant(*this, outOfFlowBox);
}

void RenderBlock::removeOutOfFlowBox(const RenderBox& rendererToRemove)
{
    outOfFlowDescendantsMap().removeDescendant(rendererToRemove);
}

static inline void markRendererAndParentForLayout(RenderBox& renderer)
{
    renderer.setChildNeedsLayout(MarkOnlyThis);
    if (renderer.shouldInvalidatePreferredWidths())
        renderer.setNeedsPreferredWidthsUpdate(MarkOnlyThis);
    auto* parentBlock = RenderObject::containingBlockForPositionType(PositionType::Static, renderer);
    if (!parentBlock) {
        ASSERT_NOT_REACHED();
        return;
    }
    // Parent has to be mark for layout to run static positioning on the out-of-flow content.
    parentBlock->setChildNeedsLayout();
}

void RenderBlock::removeOutOfFlowBoxes(const RenderBlock* newContainingBlockCandidate, ContainingBlockState containingBlockState)
{
    auto* outOfFlowDescendants = outOfFlowBoxes();
    if (!outOfFlowDescendants)
        return;

    Vector<CheckedRef<RenderBox>, 16> renderersToRemove;
    if (!newContainingBlockCandidate) {
        // We don't form containing block for these boxes anymore (either through style change or internal render tree shuffle)
        for (auto& renderer : *outOfFlowDescendants) {
            renderersToRemove.append(renderer);

            markRendererAndParentForLayout(renderer);
            auto markNewContainingBlockForLayout = [&] {
                auto isAbsolutePositioned = renderer.isAbsolutelyPositioned();
                // During style change we can't tell which ancestor is going to be the final containing block, so let's just mark the new candidate dirty.
                auto* newContainingBlock = containingBlock();
                for (; newContainingBlock && (isAbsolutePositioned ? !newContainingBlock->canContainAbsolutelyPositionedObjects() : newContainingBlock->canContainFixedPositionObjects()); newContainingBlock = newContainingBlock->containingBlock()) { }
                if (newContainingBlock)
                    newContainingBlock->setNeedsLayout();
            };
            markNewContainingBlockForLayout();
        }
    } else if (containingBlockState == ContainingBlockState::NewContainingBlock) {
        // Some of the positioned boxes are getting transferred over to newContainingBlockCandidate.
        for (auto& renderer : *outOfFlowDescendants) {
            if (!renderer.isDescendantOf(newContainingBlockCandidate))
                continue;
            renderersToRemove.append(renderer);
            markRendererAndParentForLayout(renderer);
        }
    } else
        ASSERT_NOT_REACHED();

    for (auto& renderer : renderersToRemove)
        removeOutOfFlowBox(renderer);
}

void RenderBlock::addPercentHeightDescendant(RenderBox& descendant)
{
    insertIntoTrackedRendererMaps(*this, descendant);
}

void RenderBlock::removePercentHeightDescendant(RenderBox& descendant)
{
    // We query the map directly, rather than looking at style's
    // logicalHeight()/logicalMinHeight()/logicalMaxHeight() since those
    // can change with writing mode/directional changes.
    removeFromTrackedRendererMaps(descendant);
}

TrackedRendererListHashSet* RenderBlock::percentHeightDescendants() const
{
    return percentHeightDescendantsMap ? percentHeightDescendantsMap->get(*this) : nullptr;
}

bool RenderBlock::hasPercentHeightContainerMap()
{
    return percentHeightContainerMap;
}

void RenderBlock::clearPercentHeightDescendantsFrom(RenderBox& parent)
{
    if (!percentHeightContainerMap)
        return;

    for (RenderObject* child = parent.firstChild(); child; child = child->nextInPreOrder(&parent)) {
        if (CheckedPtr box = dynamicDowncast<RenderBox>(*child))
            removeFromTrackedRendererMaps(*box);
    }
}

bool RenderBlock::isContainingBlockAncestorFor(RenderObject& renderer) const
{
    for (const auto* ancestor = renderer.containingBlock(); ancestor; ancestor = ancestor->containingBlock()) {
        if (ancestor == this)
            return true;
    }
    return false;
}

LayoutUnit RenderBlock::textIndentOffset() const
{
    LayoutUnit cw;
    if (style().textIndent().length.isPercentOrCalculated())
        cw = contentBoxLogicalWidth();
    return Style::evaluate<LayoutUnit>(style().textIndent().length, cw, style().usedZoomForLength());
}

LayoutUnit RenderBlock::logicalLeftOffsetForContent() const
{
    LayoutUnit logicalLeftOffset = writingMode().isHorizontal() ? borderLeft() + paddingLeft() : borderTop() + paddingTop();
    if (isHorizontalWritingMode() && shouldPlaceVerticalScrollbarOnLeft())
        logicalLeftOffset += verticalScrollbarWidth();
    return logicalLeftOffset;
}

LayoutUnit RenderBlock::logicalRightOffsetForContent() const
{
    LayoutUnit logicalRightOffset = writingMode().isHorizontal() ? borderLeft() + paddingLeft() : borderTop() + paddingTop();
    if (isHorizontalWritingMode() && shouldPlaceVerticalScrollbarOnLeft())
        logicalRightOffset += verticalScrollbarWidth();
    logicalRightOffset += contentBoxLogicalWidth();
    return logicalRightOffset;
}

LayoutUnit RenderBlock::adjustLogicalLeftOffsetForLine(LayoutUnit offsetFromFloats) const
{
    LayoutUnit left = offsetFromFloats;

    if (style().lineAlign() == LineAlign::None)
        return left;
    
    // Push in our left offset so that it is aligned with the character grid.
    auto* layoutState = view().frameView().layoutContext().layoutState();
    if (!layoutState)
        return left;

    RenderBlock* lineGrid = layoutState->lineGrid();
    if (!lineGrid || lineGrid->writingMode().computedWritingMode() != writingMode().computedWritingMode())
        return left;

    // FIXME: Should letter-spacing apply? This is complicated since it doesn't apply at the edge?
    float maxCharWidth = lineGrid->style().fontCascade().primaryFont()->maxCharWidth();
    if (!maxCharWidth)
        return left;

    LayoutUnit lineGridOffset = lineGrid->isHorizontalWritingMode() ? layoutState->lineGridOffset().width(): layoutState->lineGridOffset().height();
    LayoutUnit layoutOffset = lineGrid->isHorizontalWritingMode() ? layoutState->layoutOffset().width() : layoutState->layoutOffset().height();
    
    // Push in to the nearest character width (truncated so that we pixel snap left).
    // FIXME: Should be patched when subpixel layout lands, since this calculation doesn't have to pixel snap
    // any more (https://bugs.webkit.org/show_bug.cgi?id=79946).
    // FIXME: This is wrong for RTL (https://bugs.webkit.org/show_bug.cgi?id=79945).
    // FIXME: This doesn't work with columns or fragments (https://bugs.webkit.org/show_bug.cgi?id=79942).
    // FIXME: This doesn't work when the inline position of the object isn't set ahead of time.
    // FIXME: Dynamic changes to the font or to the inline position need to result in a deep relayout.
    // (https://bugs.webkit.org/show_bug.cgi?id=79944)
    float remainder = fmodf(maxCharWidth - fmodf(left + layoutOffset - lineGridOffset, maxCharWidth), maxCharWidth);
    left += remainder;
    return left;
}

LayoutUnit RenderBlock::adjustLogicalRightOffsetForLine(LayoutUnit offsetFromFloats) const
{
    LayoutUnit right = offsetFromFloats;

    if (style().lineAlign() == LineAlign::None)
        return right;
    
    // Push in our right offset so that it is aligned with the character grid.
    auto* layoutState = view().frameView().layoutContext().layoutState();
    if (!layoutState)
        return right;

    RenderBlock* lineGrid = layoutState->lineGrid();
    if (!lineGrid || lineGrid->writingMode().computedWritingMode() != writingMode().computedWritingMode())
        return right;

    // FIXME: Should letter-spacing apply? This is complicated since it doesn't apply at the edge?
    float maxCharWidth = lineGrid->style().fontCascade().primaryFont()->maxCharWidth();
    if (!maxCharWidth)
        return right;

    LayoutUnit lineGridOffset = lineGrid->isHorizontalWritingMode() ? layoutState->lineGridOffset().width(): layoutState->lineGridOffset().height();
    LayoutUnit layoutOffset = lineGrid->isHorizontalWritingMode() ? layoutState->layoutOffset().width() : layoutState->layoutOffset().height();
    
    // Push in to the nearest character width (truncated so that we pixel snap right).
    // FIXME: Should be patched when subpixel layout lands, since this calculation doesn't have to pixel snap
    // any more (https://bugs.webkit.org/show_bug.cgi?id=79946).
    // FIXME: This is wrong for RTL (https://bugs.webkit.org/show_bug.cgi?id=79945).
    // FIXME: This doesn't work with columns or fragments (https://bugs.webkit.org/show_bug.cgi?id=79942).
    // FIXME: This doesn't work when the inline position of the object isn't set ahead of time.
    // FIXME: Dynamic changes to the font or to the inline position need to result in a deep relayout.
    // (https://bugs.webkit.org/show_bug.cgi?id=79944)
    float remainder = fmodf(fmodf(right + layoutOffset - lineGridOffset, maxCharWidth), maxCharWidth);
    right -= ceilf(remainder);
    return right;
}

bool RenderBlock::isPointInOverflowControl(HitTestResult& result, const LayoutPoint& locationInContainer, const LayoutPoint& accumulatedOffset)
{
    if (!scrollsOverflow())
        return false;
    if (CheckedPtr scrollableArea = layer() ? layer()->scrollableArea() : nullptr)
        return scrollableArea->hitTestOverflowControls(result, roundedIntPoint(locationInContainer - toLayoutSize(accumulatedOffset)));
    return false;
}

Node* RenderBlock::nodeForHitTest() const
{
    if (auto type = style().pseudoElementType()) {
        switch (*type) {
        case PseudoElementType::Backdrop:
            // If we're a ::backdrop pseudo-element, we should hit-test to the element that generated it.
            // This matches the behavior that other browsers have.
            for (auto& element : document().topLayerElements()) {
                if (!element->renderer())
                    continue;
                ASSERT(element->renderer()->backdropRenderer());
                if (element->renderer()->backdropRenderer() == this)
                    return element.ptr();
            }
            ASSERT_NOT_REACHED();
            break;

        case PseudoElementType::ViewTransition:
        case PseudoElementType::ViewTransitionGroup:
        case PseudoElementType::ViewTransitionImagePair:
            // The view transition pseudo-elements should hit-test to their originating element (the document element).
            return document().documentElement();

        default:
            break;
        }
    }

    // If we are in the margins of block elements that are part of a
    // continuation we're actually still inside the enclosing element
    // that was split. Use the appropriate inner node.
    if (auto* continuation = this->continuation())
        return continuation->element();
    if (auto inlineBox = dynamicDowncast<RenderInline>(parent()); inlineBox && isAnonymousBlock()) {
        ASSERT(settings().blocksInInlineLayoutEnabled());
        return inlineBox->element();
    }
    return element();
}

bool RenderBlock::hitTestChildren(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& adjustedLocation, HitTestAction hitTestAction)
{
    ASSERT(!isSkippedContentRoot(*this));

    // Hit test descendants first.
    const LayoutSize localOffset = toLayoutSize(adjustedLocation);
    const LayoutSize scrolledOffset(localOffset - toLayoutSize(scrollPosition()));

    if (hitTestAction == HitTestFloat && hitTestFloats(request, result, locationInContainer, toLayoutPoint(scrolledOffset)))
        return true;
    if (hitTestContents(request, result, locationInContainer, toLayoutPoint(scrolledOffset), hitTestAction)) {
        updateHitTestResult(result, flipForWritingMode(locationInContainer.point() - localOffset));
        return true;
    }
    return false;
}

bool RenderBlock::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction hitTestAction)
{
    const LayoutPoint adjustedLocation(accumulatedOffset + location());
    const LayoutSize localOffset = toLayoutSize(adjustedLocation);

    // Check if we need to do anything at all.
    if (!hitTestVisualOverflow(locationInContainer, accumulatedOffset))
        return false;

    if ((hitTestAction == HitTestBlockBackground || hitTestAction == HitTestChildBlockBackground)
        && visibleToHitTesting(request) && isPointInOverflowControl(result, locationInContainer.point(), adjustedLocation)) {
        updateHitTestResult(result, locationInContainer.point() - localOffset);
        // FIXME: isPointInOverflowControl() doesn't handle rect-based tests yet.
        if (result.addNodeToListBasedTestResult(protectedNodeForHitTest().get(), request, locationInContainer) == HitTestProgress::Stop)
           return true;
    }

    if (!hitTestClipPath(locationInContainer, accumulatedOffset))
        return false;

    auto shouldHittestContent = !isSkippedContentRoot(*this);
    if (shouldHittestContent) {
        // If we have clipping, then we can't have any spillout.
        bool useClip = (hasControlClip() || hasNonVisibleOverflow());
        bool checkChildren = !useClip || (hasControlClip() ? locationInContainer.intersects(controlClipRect(adjustedLocation)) : locationInContainer.intersects(overflowClipRect(adjustedLocation, OverlayScrollbarSizeRelevancy::IncludeOverlayScrollbarSize)));
        if (checkChildren && hitTestChildren(request, result, locationInContainer, adjustedLocation, hitTestAction))
            return true;

        if (!checkChildren && hitTestExcludedChildrenInBorder(request, result, locationInContainer, adjustedLocation, hitTestAction))
            return true;
    }

    if (!hitTestBorderRadius(locationInContainer, accumulatedOffset))
        return false;

    // Now hit test our background
    if (hitTestAction == HitTestBlockBackground || hitTestAction == HitTestChildBlockBackground) {
        LayoutRect boundsRect(adjustedLocation, size());
        if (visibleToHitTesting(request) && locationInContainer.intersects(boundsRect)) {
            updateHitTestResult(result, flipForWritingMode(locationInContainer.point() - localOffset));
            if (result.addNodeToListBasedTestResult(protectedNodeForHitTest().get(), request, locationInContainer, boundsRect) == HitTestProgress::Stop)
                return true;
        }
    }

    return false;
}

bool RenderBlock::hitTestContents(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction hitTestAction)
{
    if (childrenInline() && !isRenderTable()) {
        if (style().isSkippedRootOrSkippedContent())
            return false;
        return hitTestInlineChildren(request, result, locationInContainer, accumulatedOffset, hitTestAction);
    }

    // Hit test our children.
    HitTestAction childHitTest = hitTestAction;
    if (hitTestAction == HitTestChildBlockBackgrounds)
        childHitTest = HitTestChildBlockBackground;
    for (auto* child = lastChildBox(); child; child = child->previousSiblingBox()) {
        LayoutPoint childPoint = flipForWritingModeForChild(*child, accumulatedOffset);
        if (!child->hasSelfPaintingLayer() && !child->isFloating() && child->nodeAtPoint(request, result, locationInContainer, childPoint, childHitTest))
            return true;
    }

    return false;
}

static inline bool isEditingBoundary(RenderElement* ancestor, RenderBox& child)
{
    ASSERT(!ancestor || ancestor->nonPseudoElement());
    ASSERT(child.nonPseudoElement());
    return !ancestor || !ancestor->parent() || (ancestor->hasLayer() && ancestor->parent()->isRenderView())
        || ancestor->protectedNonPseudoElement()->hasEditableStyle() == child.protectedNonPseudoElement()->hasEditableStyle();
}

// FIXME: This function should go on RenderObject as an instance method. Then
// all cases in which positionForPoint recurs could call this instead to
// prevent crossing editable boundaries. This would require many tests.
PositionWithAffinity positionForPointRespectingEditingBoundaries(RenderBlock& parent, RenderBox& child, const LayoutPoint& pointInParentCoordinates, HitTestSource source)
{
    LayoutPoint childLocation = child.location();
    if (child.isInFlowPositioned())
        childLocation += child.offsetForInFlowPosition();

    // FIXME: This is wrong if the child's writing-mode is different from the parent's.
    LayoutPoint pointInChildCoordinates(toLayoutPoint(pointInParentCoordinates - childLocation));

    // If this is an anonymous renderer, we just recur normally
    RefPtr childElement= child.nonPseudoElement();
    if (!childElement)
        return child.positionForPoint(pointInChildCoordinates, source, nullptr);

    // Otherwise, first make sure that the editability of the parent and child agree.
    // If they don't agree, then we return a visible position just before or after the child
    RenderElement* ancestor = &parent;
    while (ancestor && !ancestor->nonPseudoElement())
        ancestor = ancestor->parent();

    // If we can't find an ancestor to check editability on, or editability is unchanged, we recur like normal
    if (isEditingBoundary(ancestor, child))
        return child.positionForPoint(pointInChildCoordinates, source, nullptr);

    // Otherwise return before or after the child, depending on if the click was to the logical left or logical right of the child
    LayoutUnit childMiddle = parent.logicalWidthForChild(child) / 2;
    LayoutUnit logicalLeft = parent.isHorizontalWritingMode() ? pointInChildCoordinates.x() : pointInChildCoordinates.y();
    if (logicalLeft < childMiddle)
        return ancestor->createPositionWithAffinity(childElement->computeNodeIndex(), Affinity::Downstream);
    return ancestor->createPositionWithAffinity(childElement->computeNodeIndex() + 1, Affinity::Upstream);
}

PositionWithAffinity RenderBlock::positionForPointWithInlineChildren(const LayoutPoint&, HitTestSource)
{
    ASSERT_NOT_REACHED();
    return PositionWithAffinity();
}

static inline bool isChildHitTestCandidate(const RenderBox& box, HitTestSource source)
{
    auto visibility = source == HitTestSource::Script ? box.style().visibility() : box.style().usedVisibility();
    return box.height() && visibility == Visibility::Visible && !box.isOutOfFlowPositioned() && !box.isRenderFragmentedFlow();
}

// Valid candidates in a FragmentedFlow must be rendered by the fragment.
static inline bool isChildHitTestCandidate(const RenderBox& box, const RenderFragmentContainer* fragment, const LayoutPoint& point, HitTestSource source)
{
    if (!isChildHitTestCandidate(box, source))
        return false;
    if (!fragment)
        return true;
    auto& block = [&]() -> const RenderBlock& {
        if (auto* block = dynamicDowncast<RenderBlock>(box))
            return *block;
        return *box.containingBlock();
    }();
    return block.fragmentAtBlockOffset(point.y()) == fragment;
}

PositionWithAffinity RenderBlock::positionForPoint(const LayoutPoint& point, HitTestSource source, const RenderFragmentContainer* fragment)
{
    if (isRenderTable())
        return RenderBox::positionForPoint(point, source, fragment);

    if (isBlockLevelReplacedOrAtomicInline()) {
        // FIXME: This seems wrong when the object's writing-mode doesn't match the line's writing-mode.
        LayoutUnit pointLogicalLeft = isHorizontalWritingMode() ? point.x() : point.y();
        LayoutUnit pointLogicalTop = isHorizontalWritingMode() ? point.y() : point.x();

        if (pointLogicalTop < 0)
            return createPositionWithAffinity(caretMinOffset(), Affinity::Downstream);
        if (pointLogicalLeft >= logicalWidth())
            return createPositionWithAffinity(caretMaxOffset(), Affinity::Downstream);
        if (pointLogicalTop < 0)
            return createPositionWithAffinity(caretMinOffset(), Affinity::Downstream);
        if (pointLogicalTop >= logicalHeight())
            return createPositionWithAffinity(caretMaxOffset(), Affinity::Downstream);
    }
    if (isFlexibleBoxIncludingDeprecated() || isRenderGrid())
        return RenderBox::positionForPoint(point, source, fragment);

    LayoutPoint pointInContents = point;
    offsetForContents(pointInContents);
    LayoutPoint pointInLogicalContents(pointInContents);
    if (!isHorizontalWritingMode())
        pointInLogicalContents = pointInLogicalContents.transposedPoint();

    if (childrenInline())
        return positionForPointWithInlineChildren(pointInLogicalContents, source);

    RenderBox* lastCandidateBox = lastChildBox();

    if (!fragment)
        fragment = fragmentAtBlockOffset(pointInLogicalContents.y());

    while (lastCandidateBox && !isChildHitTestCandidate(*lastCandidateBox, fragment, pointInLogicalContents, source))
        lastCandidateBox = lastCandidateBox->previousSiblingBox();

    bool blocksAreFlipped = writingMode().isBlockFlipped();
    if (lastCandidateBox) {
        if (pointInLogicalContents.y() > logicalTopForChild(*lastCandidateBox)
            || (!blocksAreFlipped && pointInLogicalContents.y() == logicalTopForChild(*lastCandidateBox)))
            return positionForPointRespectingEditingBoundaries(*this, *lastCandidateBox, pointInContents, source);

        for (auto& childBox : childrenOfType<RenderBox>(*this)) {
            if (!isChildHitTestCandidate(childBox, fragment, pointInLogicalContents, source))
                continue;
            auto childLogicalBottom = logicalTopForChild(childBox) + logicalHeightForChild(childBox);
            if (auto* blockFlow = dynamicDowncast<RenderBlockFlow>(childBox))
                childLogicalBottom = std::max(childLogicalBottom, blockFlow->lowestFloatLogicalBottom());
            // We hit child if our click is above the bottom of its padding box (like IE6/7 and FF3).
            if (pointInLogicalContents.y() < childLogicalBottom || (blocksAreFlipped && pointInLogicalContents.y() == childLogicalBottom))
                return positionForPointRespectingEditingBoundaries(*this, childBox, pointInContents, source);
        }
    }

    // We only get here if there are no hit test candidate children below the click.
    return RenderBox::positionForPoint(point, source, fragment);
}

void RenderBlock::offsetForContents(LayoutPoint& offset) const
{
    offset = flipForWritingMode(offset);
    offset += toLayoutSize(scrollPosition());
    offset = flipForWritingMode(offset);
}

void RenderBlock::computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const
{
    ASSERT(!childrenInline());
    if (shouldApplySizeOrInlineSizeContainment()) {
        if (auto width = explicitIntrinsicInnerLogicalWidth()) {
            minLogicalWidth = width.value();
            maxLogicalWidth = width.value();
        }
    } else if (!shouldApplyInlineSizeContainment())
        computeBlockPreferredLogicalWidths(minLogicalWidth, maxLogicalWidth);

    maxLogicalWidth = std::max(minLogicalWidth, maxLogicalWidth);

    int scrollbarWidth = intrinsicScrollbarLogicalWidthIncludingGutter();
    maxLogicalWidth += scrollbarWidth;
    minLogicalWidth += scrollbarWidth;
}

void RenderBlock::computePreferredLogicalWidths()
{
    ASSERT(needsPreferredLogicalWidthsUpdate());

    m_minPreferredLogicalWidth = 0;
    m_maxPreferredLogicalWidth = 0;

    auto& styleToUse = style();
    auto logicalWidth = overridingLogicalWidthForFlexBasisComputation().value_or(styleToUse.logicalWidth());
    if (auto fixedLogicalWidth = logicalWidth.tryFixed(); !isRenderTableCell() && fixedLogicalWidth && fixedLogicalWidth->isPositiveOrZero() && !(isDeprecatedFlexItem() && !static_cast<int>(fixedLogicalWidth->resolveZoom(style().usedZoomForLength())))) {
        m_minPreferredLogicalWidth = adjustContentBoxLogicalWidthForBoxSizing(*fixedLogicalWidth);
        m_maxPreferredLogicalWidth = m_minPreferredLogicalWidth;
    } else if (logicalWidth.isMaxContent()) {
        computeIntrinsicLogicalWidths(m_minPreferredLogicalWidth, m_maxPreferredLogicalWidth);
        m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth;
    } else if (shouldComputeLogicalWidthFromAspectRatio()) {
        m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth = (computeLogicalWidthFromAspectRatio() - borderAndPaddingLogicalWidth());
        m_minPreferredLogicalWidth = std::max(0_lu, m_minPreferredLogicalWidth);
        m_maxPreferredLogicalWidth = std::max(0_lu, m_maxPreferredLogicalWidth);
    } else 
        computeIntrinsicLogicalWidths(m_minPreferredLogicalWidth, m_maxPreferredLogicalWidth);

    RenderBox::computePreferredLogicalWidths(styleToUse.logicalMinWidth(), styleToUse.logicalMaxWidth(), borderAndPaddingLogicalWidth());

    clearNeedsPreferredWidthsUpdate();
}

void RenderBlock::computeBlockPreferredLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const
{
    ASSERT(!shouldApplyInlineSizeContainment());
    auto* containingBlock = this->containingBlock();
    if (!containingBlock) {
        ASSERT_NOT_REACHED();
        return;
    }

    LayoutUnit childMinWidth;
    LayoutUnit childMaxWidth;
    if (computePreferredWidthsForExcludedChildren(childMinWidth, childMaxWidth)) {
        minLogicalWidth = std::max(childMinWidth, minLogicalWidth);
        maxLogicalWidth = std::max(childMaxWidth, maxLogicalWidth);
    }

    LayoutUnit floatLeftWidth;
    LayoutUnit floatRightWidth;
    auto nowrap = style().textWrapMode() == TextWrapMode::NoWrap && style().whiteSpaceCollapse() == WhiteSpaceCollapse::Collapse;
    for (auto& childBox : childrenOfType<RenderBox>(*this)) {
        // Positioned children don't affect the min/max width. Legends in fieldsets are skipped here
        // since they compute outside of any one layout system. Other children excluded from
        // normal layout are only used with block flows, so it's ok to calculate them here.
        if (childBox.isOutOfFlowPositioned() || childBox.isExcludedAndPlacedInBorder())
            continue;

        auto& childStyle = childBox.style();
        // Either the box itself of its content avoids floats.
        auto childAvoidsFloats = childBox.avoidsFloats() || (childBox.isAnonymousBlock() && childBox.childrenInline());
        if (childBox.isFloating() || childAvoidsFloats) {
            LayoutUnit floatTotalWidth = floatLeftWidth + floatRightWidth;
            auto childUsedClear = RenderStyle::usedClear(childBox);
            if (childUsedClear == UsedClear::Left || childUsedClear == UsedClear::Both) {
                maxLogicalWidth = std::max(floatTotalWidth, maxLogicalWidth);
                floatLeftWidth = 0.f;
            }
            if (childUsedClear == UsedClear::Right || childUsedClear == UsedClear::Both) {
                maxLogicalWidth = std::max(floatTotalWidth, maxLogicalWidth);
                floatRightWidth = 0.f;
            }
        }

        // A margin basically has three types: fixed, percentage, and auto (variable).
        // Auto and percentage margins simply become 0 when computing min/max width.
        // Fixed margins can be added in as is.
        LayoutUnit marginStart;
        LayoutUnit marginEnd;
        const auto& childZoomFactor = childStyle.usedZoomForLength();
        if (auto fixedMarginStart = childStyle.marginStart(writingMode()).tryFixed())
            marginStart += fixedMarginStart->resolveZoom(childZoomFactor);
        if (auto fixedMarginEnd = childStyle.marginEnd(writingMode()).tryFixed())
            marginEnd += fixedMarginEnd->resolveZoom(childZoomFactor);
        auto margin = marginStart + marginEnd;

        LayoutUnit childMinPreferredLogicalWidth;
        LayoutUnit childMaxPreferredLogicalWidth;
        computeChildPreferredLogicalWidths(const_cast<RenderBox&>(childBox), childMinPreferredLogicalWidth, childMaxPreferredLogicalWidth);

        auto logicalWidth = childMinPreferredLogicalWidth + margin;
        minLogicalWidth = std::max(logicalWidth, minLogicalWidth);

        // IE ignores tables for calculation of nowrap. Makes some sense.
        if (nowrap && !childBox.isRenderTable())
            maxLogicalWidth = std::max(logicalWidth, maxLogicalWidth);

        logicalWidth = childMaxPreferredLogicalWidth + margin;

        if (!childBox.isFloating()) {
            if (childAvoidsFloats) {
                // Determine a left and right max value based off whether or not the floats can fit in the
                // margins of the object.  For negative margins, we will attempt to overlap the float if the negative margin
                // is smaller than the float width.
                bool ltr = containingBlock->writingMode().isLogicalLeftInlineStart();
                LayoutUnit marginLogicalLeft = ltr ? marginStart : marginEnd;
                LayoutUnit marginLogicalRight = ltr ? marginEnd : marginStart;
                LayoutUnit maxLeft = marginLogicalLeft > 0 ? std::max(floatLeftWidth, marginLogicalLeft) : floatLeftWidth + marginLogicalLeft;
                LayoutUnit maxRight = marginLogicalRight > 0 ? std::max(floatRightWidth, marginLogicalRight) : floatRightWidth + marginLogicalRight;
                logicalWidth = childMaxPreferredLogicalWidth + maxLeft + maxRight;
                logicalWidth = std::max(logicalWidth, floatLeftWidth + floatRightWidth);
            } else
                maxLogicalWidth = std::max(floatLeftWidth + floatRightWidth, maxLogicalWidth);
            floatLeftWidth = 0.f;
            floatRightWidth = 0.f;
        }

        if (childBox.isFloating()) {
            if (RenderStyle::usedFloat(childBox) == UsedFloat::Left)
                floatLeftWidth += logicalWidth;
            else
                floatRightWidth += logicalWidth;
        } else
            maxLogicalWidth = std::max(logicalWidth, maxLogicalWidth);
    }

    // Always make sure these values are non-negative.
    minLogicalWidth = std::max(0_lu, minLogicalWidth);
    maxLogicalWidth = std::max(0_lu, maxLogicalWidth);

    maxLogicalWidth = std::max(floatLeftWidth + floatRightWidth, maxLogicalWidth);
}

void RenderBlock::computeChildIntrinsicLogicalWidths(RenderBox& child, LayoutUnit& minPreferredLogicalWidth, LayoutUnit& maxPreferredLogicalWidth) const
{
    minPreferredLogicalWidth = child.minPreferredLogicalWidth();
    maxPreferredLogicalWidth = child.maxPreferredLogicalWidth();
}

void RenderBlock::computeChildPreferredLogicalWidths(RenderBox& childBox, LayoutUnit& minPreferredLogicalWidth, LayoutUnit& maxPreferredLogicalWidth) const
{
    if (childBox.isHorizontalWritingMode() != isHorizontalWritingMode()) {
        // If the child is an orthogonal flow, child's height determines the width,
        // but the height is not available until layout.
        // http://dev.w3.org/csswg/css-writing-modes-3/#orthogonal-shrink-to-fit
        if (!childBox.needsLayout()) {
            minPreferredLogicalWidth = childBox.logicalHeight();
            maxPreferredLogicalWidth = childBox.logicalHeight();
            return;
        }
        auto& childBoxStyle = childBox.style();
        if (auto fixedChildBoxStyleLogicalWidth = childBoxStyle.logicalWidth().tryFixed(); childBox.shouldComputeLogicalHeightFromAspectRatio() && fixedChildBoxStyleLogicalWidth) {
            auto aspectRatioSize = blockSizeFromAspectRatio(
                childBox.horizontalBorderAndPaddingExtent(),
                childBox.verticalBorderAndPaddingExtent(),
                LayoutUnit { childBoxStyle.logicalAspectRatio() },
                childBoxStyle.boxSizingForAspectRatio(),
                LayoutUnit { fixedChildBoxStyleLogicalWidth->resolveZoom(childBoxStyle.usedZoomForLength()) },
                style().aspectRatio(),
                isRenderReplaced()
            );
            minPreferredLogicalWidth = aspectRatioSize;
            maxPreferredLogicalWidth = aspectRatioSize;
            return;
        }
        auto logicalHeightWithoutLayout = childBox.computeLogicalHeightWithoutLayout();
        minPreferredLogicalWidth = logicalHeightWithoutLayout;
        maxPreferredLogicalWidth = logicalHeightWithoutLayout;
        return;
    }
    
    computeChildIntrinsicLogicalWidths(childBox, minPreferredLogicalWidth, maxPreferredLogicalWidth);

    // For non-replaced blocks if the inline size is min|max-content or a definite
    // size the min|max-content contribution is that size plus border, padding and
    // margin https://drafts.csswg.org/css-sizing/#block-intrinsic
    if (!is<RenderBlock>(childBox))
        return;
    auto& computedInlineSize = childBox.style().logicalWidth();
    if (computedInlineSize.isMaxContent())
        minPreferredLogicalWidth = maxPreferredLogicalWidth;
    else if (computedInlineSize.isMinContent())
        maxPreferredLogicalWidth = minPreferredLogicalWidth;
}

bool RenderBlock::hasLineIfEmpty() const
{
    RefPtr element = this->element();
    return element && element->isRootEditableElement();
}

std::optional<LayoutUnit> RenderBlock::firstLineBaseline() const
{
    if (shouldApplyLayoutContainment())
        return { };

    if (isWritingModeRoot() && !isFlexItem())
        return { };

    for (RenderBox* child = firstInFlowChildBox(); child; child = child->nextInFlowSiblingBox()) {
        if (child->isLegend() && child->isExcludedFromNormalLayout())
            continue;
        if (auto baseline = child->firstLineBaseline())
            return LayoutUnit { floorToInt(child->logicalTop() + baseline.value()) };
    }
    return { };
}

std::optional<LayoutUnit> RenderBlock::lastLineBaseline() const
{
    if (shouldApplyLayoutContainment())
        return { };

    if (isWritingModeRoot())
        return { };

    for (RenderBox* child = lastInFlowChildBox(); child; child = child->previousInFlowSiblingBox()) {
        if (child->isLegend() && child->isExcludedFromNormalLayout())
            continue;
        if (auto baseline = child->lastLineBaseline())
            return LayoutUnit { floorToInt(child->logicalTop() + baseline.value()) };
    } 
    return { };
}

static inline bool isRenderBlockFlowOrRenderButton(RenderElement& renderElement)
{
    // We include isRenderButton in this check because buttons are implemented
    // using flex box but should still support first-line|first-letter.
    // The flex box and specs require that flex box and grid do not support
    // first-line|first-letter, though.
    // FIXME: Remove when buttons are implemented with align-items instead of
    // flex box.
    return renderElement.isRenderBlockFlow() || renderElement.isRenderButton();
}

static inline RenderBlock* findFirstLetterBlock(RenderBlock* start)
{
    RenderBlock* firstLetterBlock = start;
    while (true) {
        bool canHaveFirstLetterRenderer = firstLetterBlock->style().hasPseudoStyle(PseudoElementType::FirstLetter)
            && firstLetterBlock->canHaveGeneratedChildren()
            && isRenderBlockFlowOrRenderButton(*firstLetterBlock);
        if (canHaveFirstLetterRenderer)
            return firstLetterBlock;

        RenderElement* parentBlock = firstLetterBlock->parent();
        if (firstLetterBlock->isBlockLevelReplacedOrAtomicInline() || !parentBlock || parentBlock->firstChild() != firstLetterBlock
            || !isRenderBlockFlowOrRenderButton(*parentBlock))
            return nullptr;
        firstLetterBlock = downcast<RenderBlock>(parentBlock);
    } 

    return nullptr;
}

std::pair<RenderObject*, RenderElement*> RenderBlock::firstLetterAndContainer(RenderObject* skipThisAsFirstLetter)
{
    // Don't recur
    if (style().pseudoElementType() == PseudoElementType::FirstLetter)
        return { };
    
    // FIXME: We need to destroy the first-letter object if it is no longer the first child. Need to find
    // an efficient way to check for that situation though before implementing anything.
    RenderElement* firstLetterContainer = findFirstLetterBlock(this);
    if (!firstLetterContainer)
        return { };
    
    // Drill into inlines looking for our first text descendant.
    auto* firstLetter = firstLetterContainer->firstChild();
    while (firstLetter) {
        if (is<RenderText>(*firstLetter)) {
            if (firstLetter == skipThisAsFirstLetter) {
                firstLetter = firstLetter->nextSibling();
                continue;
            }
            break;
        }

        RenderElement& current = downcast<RenderElement>(*firstLetter);
        if (is<RenderListMarker>(current))
            firstLetter = current.nextSibling();
        else if (current.isFloatingOrOutOfFlowPositioned()) {
            if (current.style().pseudoElementType() == PseudoElementType::FirstLetter) {
                firstLetter = current.firstChild();
                break;
            }
            firstLetter = current.nextSibling();
        } else if (current.isBlockLevelReplacedOrAtomicInline() || is<RenderButton>(current) || is<RenderMenuList>(current))
            break;
        else if (current.isFlexibleBoxIncludingDeprecated() || current.isRenderGrid())
            return { };
        else if (current.style().hasPseudoStyle(PseudoElementType::FirstLetter) && current.canHaveGeneratedChildren())  {
            // We found a lower-level node with first-letter, which supersedes the higher-level style
            firstLetterContainer = &current;
            firstLetter = current.firstChild();
        } else
            firstLetter = current.firstChild();
    }
    
    if (!firstLetter)
        return { };

    return { firstLetter, firstLetterContainer };
}

RenderFragmentedFlow* RenderBlock::cachedEnclosingFragmentedFlow() const
{
    RenderBlockRareData* rareData = blockRareData();

    if (!rareData || !rareData->m_enclosingFragmentedFlow)
        return nullptr;

    return rareData->m_enclosingFragmentedFlow.value().get();
}

bool RenderBlock::cachedEnclosingFragmentedFlowNeedsUpdate() const
{
    RenderBlockRareData* rareData = blockRareData();

    if (!rareData || !rareData->m_enclosingFragmentedFlow)
        return true;

    return false;
}

void RenderBlock::setCachedEnclosingFragmentedFlowNeedsUpdate()
{
    RenderBlockRareData& rareData = ensureBlockRareData();
    rareData.m_enclosingFragmentedFlow = std::nullopt;
}

RenderFragmentedFlow* RenderBlock::updateCachedEnclosingFragmentedFlow(RenderFragmentedFlow* fragmentedFlow) const
{
    if (!fragmentedFlow) {
        if (auto* rareData = const_cast<RenderBlock&>(*this).blockRareData())
            rareData->m_enclosingFragmentedFlow = { };
        return { };
    }

    const_cast<RenderBlock&>(*this).ensureBlockRareData().m_enclosingFragmentedFlow = fragmentedFlow;
    return fragmentedFlow;
}

RenderFragmentedFlow* RenderBlock::locateEnclosingFragmentedFlow() const
{
    RenderBlockRareData* rareData = blockRareData();
    if (!rareData || !rareData->m_enclosingFragmentedFlow)
        return updateCachedEnclosingFragmentedFlow(RenderBox::locateEnclosingFragmentedFlow());

    ASSERT(rareData->m_enclosingFragmentedFlow.value() == RenderBox::locateEnclosingFragmentedFlow());
    return rareData->m_enclosingFragmentedFlow.value().get();
}

void RenderBlock::resetEnclosingFragmentedFlowAndChildInfoIncludingDescendants(RenderFragmentedFlow* fragmentedFlow)
{
    if (fragmentedFlowState() == FragmentedFlowState::NotInsideFlow)
        return;

    if (auto* cachedFragmentedFlow = cachedEnclosingFragmentedFlow())
        fragmentedFlow = cachedFragmentedFlow;
    setCachedEnclosingFragmentedFlowNeedsUpdate();
    RenderElement::resetEnclosingFragmentedFlowAndChildInfoIncludingDescendants(fragmentedFlow);
}

LayoutUnit RenderBlock::paginationStrut() const
{
    RenderBlockRareData* rareData = blockRareData();
    return rareData ? rareData->m_paginationStrut : 0_lu;
}

LayoutUnit RenderBlock::pageLogicalOffset() const
{
    RenderBlockRareData* rareData = blockRareData();
    return rareData ? rareData->m_pageLogicalOffset : 0_lu;
}

void RenderBlock::setPaginationStrut(LayoutUnit strut)
{
    RenderBlockRareData* rareData = blockRareData();
    if (!rareData) {
        if (!strut)
            return;
        rareData = &ensureBlockRareData();
    }
    rareData->m_paginationStrut = strut;
}

void RenderBlock::setPageLogicalOffset(LayoutUnit logicalOffset)
{
    RenderBlockRareData* rareData = blockRareData();
    if (!rareData) {
        if (!logicalOffset)
            return;
        rareData = &ensureBlockRareData();
    }
    rareData->m_pageLogicalOffset = logicalOffset;
}

void RenderBlock::boundingRects(Vector<LayoutRect>& rects, const LayoutPoint& accumulatedOffset) const
{
    // For blocks inside inlines, we include margins so that we run right up to the inline boxes
    // above and below us (thus getting merged with them to form a single irregular shape).
    if (auto* continuation = this->continuation()) {
        // FIXME: This is wrong for block-flows that are horizontal.
        // https://bugs.webkit.org/show_bug.cgi?id=46781
        rects.append(LayoutRect(accumulatedOffset.x(), accumulatedOffset.y() - collapsedMarginBefore(), width(), height() + collapsedMarginBefore() + collapsedMarginAfter()));
        auto* containingBlock = inlineContinuation()->containingBlock();
        continuation->boundingRects(rects, accumulatedOffset - locationOffset() + containingBlock->locationOffset());
    } else
        rects.append({ accumulatedOffset, size() });
}

void RenderBlock::absoluteQuads(Vector<FloatQuad>& quads, bool* wasFixed) const
{
    if (!continuation()) {
        absoluteQuadsIgnoringContinuation({ { }, size() }, quads, wasFixed);
        return;
    }
    // For blocks inside inlines, we include margins so that we run right up to the inline boxes
    // above and below us (thus getting merged with them to form a single irregular shape).
    auto logicalRect = FloatRect { 0, -collapsedMarginBefore(), width(), height() + collapsedMarginBefore() + collapsedMarginAfter() };
    absoluteQuadsIgnoringContinuation(logicalRect, quads, wasFixed);
    collectAbsoluteQuadsForContinuation(quads, wasFixed);
}

void RenderBlock::absoluteQuadsIgnoringContinuation(const FloatRect& logicalRect, Vector<FloatQuad>& quads, bool* wasFixed) const
{
    // FIXME: This is wrong for block-flows that are horizontal.
    // https://bugs.webkit.org/show_bug.cgi?id=46781
    CheckedPtr fragmentedFlow = enclosingFragmentedFlow();
    if (!fragmentedFlow || !fragmentedFlow->absoluteQuadsForBox(quads, wasFixed, *this))
        quads.append(localToAbsoluteQuad(logicalRect, UseTransforms, wasFixed));
}

LayoutRect RenderBlock::rectWithOutlineForRepaint(const RenderLayerModelObject* repaintContainer, LayoutUnit outlineWidth) const
{
    LayoutRect r(RenderBox::rectWithOutlineForRepaint(repaintContainer, outlineWidth));
    if (isContinuation())
        r.inflateY(collapsedMarginBefore()); // FIXME: This is wrong for block-flows that are horizontal.
    return r;
}

const RenderStyle& RenderBlock::outlineStyleForRepaint() const
{
    if (auto* continuation = this->continuation())
        return continuation->style();
    return RenderElement::outlineStyleForRepaint();
}

void RenderBlock::updateHitTestResult(HitTestResult& result, const LayoutPoint& point) const
{
    if (result.innerNode())
        return;

    if (RefPtr node = nodeForHitTest()) {
        result.setInnerNode(node.get());
        if (!result.innerNonSharedNode())
            result.setInnerNonSharedNode(node.get());
        result.setPseudoElementIdentifier(style().pseudoElementIdentifier());
        result.setLocalPoint(point);
    }
}

LayoutUnit RenderBlock::offsetFromLogicalTopOfFirstPage() const
{
    auto* layoutState = view().frameView().layoutContext().layoutState();
    if (layoutState && !layoutState->isPaginated())
        return 0;

    if (CheckedPtr fragmentedFlow = enclosingFragmentedFlow())
        return fragmentedFlow->offsetFromLogicalTopOfFirstFragment(this);

    if (layoutState) {
        ASSERT(layoutState->renderer() == this);

        LayoutSize offsetDelta = layoutState->layoutOffset() - layoutState->pageOffset();
        return isHorizontalWritingMode() ? offsetDelta.height() : offsetDelta.width();
    }
    
    ASSERT_NOT_REACHED();
    return 0;
}

RenderFragmentContainer* RenderBlock::fragmentAtBlockOffset(LayoutUnit blockOffset) const
{
    if (CheckedPtr fragmentedFlow = enclosingFragmentedFlow(); fragmentedFlow && fragmentedFlow->hasValidFragmentInfo())
        return fragmentedFlow->fragmentAtBlockOffset(this, offsetFromLogicalTopOfFirstPage() + blockOffset, true);

    return nullptr;
}

static bool canComputeFragmentRangeForBox(const RenderBlock& parentBlock, const RenderBox& childBox, const RenderFragmentedFlow& enclosingFragmentedFlow)
{
    if (!enclosingFragmentedFlow.hasFragments())
        return false;

    if (!childBox.canHaveOutsideFragmentRange())
        return false;

    return enclosingFragmentedFlow.hasCachedFragmentRangeForBox(parentBlock);
}

bool RenderBlock::childBoxIsUnsplittableForFragmentation(const RenderBox& child) const
{
    CheckedPtr fragmentedFlow = enclosingFragmentedFlow();
    bool checkColumnBreaks = fragmentedFlow && fragmentedFlow->shouldCheckColumnBreaks();
    bool checkPageBreaks = !checkColumnBreaks && view().frameView().layoutContext().layoutState()->pageLogicalHeight();
    return child.isUnsplittableForPagination() || child.style().breakInside() == BreakInside::Avoid
        || (checkColumnBreaks && child.style().breakInside() == BreakInside::AvoidColumn)
        || (checkPageBreaks && child.style().breakInside() == BreakInside::AvoidPage);
}

void RenderBlock::computeFragmentRangeForBoxChild(const RenderBox& box) const
{
    CheckedPtr fragmentedFlow = enclosingFragmentedFlow();
    ASSERT(fragmentedFlow && canComputeFragmentRangeForBox(*this, box, *fragmentedFlow));
    ASSERT(box.fragmentedFlowState() == FragmentedFlowState::InsideFlow);

    RenderFragmentContainer* startFragment;
    RenderFragmentContainer* endFragment;
    LayoutUnit offsetFromLogicalTopOfFirstFragment = box.offsetFromLogicalTopOfFirstPage();
    if (childBoxIsUnsplittableForFragmentation(box))
        startFragment = endFragment = fragmentedFlow->fragmentAtBlockOffset(this, offsetFromLogicalTopOfFirstFragment, true);
    else {
        startFragment = fragmentedFlow->fragmentAtBlockOffset(this, offsetFromLogicalTopOfFirstFragment, true);
        endFragment = fragmentedFlow->fragmentAtBlockOffset(this, offsetFromLogicalTopOfFirstFragment + logicalHeightForChild(box), true);
    }

    fragmentedFlow->setFragmentRangeForBox(box, startFragment, endFragment);
}

void RenderBlock::estimateFragmentRangeForBoxChild(const RenderBox& box) const
{
    CheckedPtr fragmentedFlow = enclosingFragmentedFlow();
    if (!fragmentedFlow || box.fragmentedFlowState() == FragmentedFlowState::NotInsideFlow || !canComputeFragmentRangeForBox(*this, box, *fragmentedFlow))
        return;

    if (childBoxIsUnsplittableForFragmentation(box)) {
        computeFragmentRangeForBoxChild(box);
        return;
    }

    auto estimatedValues = box.computeLogicalHeight(RenderFragmentedFlow::maxLogicalHeight(), logicalTopForChild(box));
    LayoutUnit offsetFromLogicalTopOfFirstFragment = box.offsetFromLogicalTopOfFirstPage();
    RenderFragmentContainer* startFragment = fragmentedFlow->fragmentAtBlockOffset(this, offsetFromLogicalTopOfFirstFragment, true);
    RenderFragmentContainer* endFragment = fragmentedFlow->fragmentAtBlockOffset(this, offsetFromLogicalTopOfFirstFragment + estimatedValues.m_extent, true);

    fragmentedFlow->setFragmentRangeForBox(box, startFragment, endFragment);
}

bool RenderBlock::updateFragmentRangeForBoxChild(const RenderBox& box) const
{
    CheckedPtr fragmentedFlow = enclosingFragmentedFlow();
    if (!fragmentedFlow || box.fragmentedFlowState() == FragmentedFlowState::NotInsideFlow || !canComputeFragmentRangeForBox(*this, box, *fragmentedFlow))
        return false;

    RenderFragmentContainer* startFragment = nullptr;
    RenderFragmentContainer* endFragment = nullptr;
    fragmentedFlow->getFragmentRangeForBox(box, startFragment, endFragment);

    computeFragmentRangeForBoxChild(box);

    RenderFragmentContainer* newStartFragment = nullptr;
    RenderFragmentContainer* newEndFragment = nullptr;
    fragmentedFlow->getFragmentRangeForBox(box, newStartFragment, newEndFragment);


    // Changing the start fragment means we shift everything and a relayout is needed.
    if (newStartFragment != startFragment)
        return true;

    // The fragment range of the box has changed. Some boxes (e.g floats) may have been positioned assuming
    // a different range.
    if (box.needsLayoutAfterFragmentRangeChange() && newEndFragment != endFragment)
        return true;

    return false;
}

void RenderBlock::setTrimmedMarginForChild(RenderBox& child, Style::MarginTrimSide side)
{
    switch (side) {
    case Style::MarginTrimSide::BlockStart:
        setMarginBeforeForChild(child, 0_lu);
        break;
    case Style::MarginTrimSide::BlockEnd:
        setMarginAfterForChild(child, 0_lu);
        break;
    case Style::MarginTrimSide::InlineStart:
        setMarginStartForChild(child, 0_lu);
        break;
    case Style::MarginTrimSide::InlineEnd:
        setMarginEndForChild(child, 0_lu);
        break;
    default:
        ASSERT_NOT_IMPLEMENTED_YET();
    }

    child.markMarginAsTrimmed(side);
}

LayoutUnit RenderBlock::collapsedMarginBeforeForChild(const RenderBox& child) const
{
    // If the child has the same directionality as we do, then we can just return its
    // collapsed margin.
    if (!child.isWritingModeRoot())
        return child.collapsedMarginBefore();
    
    // The child has a different directionality.  If the child is parallel, then it's just
    // flipped relative to us.  We can use the collapsed margin for the opposite edge.
    if (child.isHorizontalWritingMode() == isHorizontalWritingMode())
        return child.collapsedMarginAfter();
    
    // The child is perpendicular to us, which means its margins don't collapse but are on the
    // "logical left/right" sides of the child box.  We can just return the raw margin in this case.  
    return marginBeforeForChild(child);
}

LayoutUnit RenderBlock::collapsedMarginAfterForChild(const RenderBox& child) const
{
    // If the child has the same directionality as we do, then we can just return its
    // collapsed margin.
    if (!child.isWritingModeRoot())
        return child.collapsedMarginAfter();
    
    // The child has a different directionality.  If the child is parallel, then it's just
    // flipped relative to us.  We can use the collapsed margin for the opposite edge.
    if (child.isHorizontalWritingMode() == isHorizontalWritingMode())
        return child.collapsedMarginBefore();
    
    // The child is perpendicular to us, which means its margins don't collapse but are on the
    // "logical left/right" side of the child box.  We can just return the raw margin in this case.  
    return marginAfterForChild(child);
}

bool RenderBlock::hasMarginBeforeQuirk(const RenderBox& child) const
{
    // If the child has the same directionality as we do, then we can just return its
    // margin quirk.
    if (!child.isWritingModeRoot()) {
        auto* childBlock = dynamicDowncast<RenderBlock>(child);
        return childBlock ? childBlock->hasMarginBeforeQuirk() : child.style().marginBefore().hasQuirk();
    }

    // The child has a different directionality. If the child is parallel, then it's just
    // flipped relative to us. We can use the opposite edge.
    if (child.isHorizontalWritingMode() == isHorizontalWritingMode()) {
        auto* childBlock = dynamicDowncast<RenderBlock>(child);
        return childBlock ? childBlock->hasMarginAfterQuirk() : child.style().marginAfter().hasQuirk();
    }

    // The child is perpendicular to us and box sides are never quirky in html.css, and we don't really care about
    // whether or not authors specified quirky ems, since they're an implementation detail.
    return false;
}

bool RenderBlock::hasMarginAfterQuirk(const RenderBox& child) const
{
    // If the child has the same directionality as we do, then we can just return its
    // margin quirk.
    if (!child.isWritingModeRoot()) {
        auto* childBlock = dynamicDowncast<RenderBlock>(child);
        return childBlock ? childBlock->hasMarginAfterQuirk() : child.style().marginAfter().hasQuirk();
    }

    // The child has a different directionality. If the child is parallel, then it's just
    // flipped relative to us. We can use the opposite edge.
    if (child.isHorizontalWritingMode() == isHorizontalWritingMode()) {
        auto* childBlock = dynamicDowncast<RenderBlock>(child);
        return childBlock ? childBlock->hasMarginBeforeQuirk() : child.style().marginBefore().hasQuirk();
    }

    // The child is perpendicular to us and box sides are never quirky in html.css, and we don't really care about
    // whether or not authors specified quirky ems, since they're an implementation detail.
    return false;
}

ASCIILiteral RenderBlock::renderName() const
{
    if (isBody())
        return "RenderBody"_s; // FIXME: Temporary hack until we know that the regression tests pass.
    if (isFieldset())
        return "RenderFieldSet"_s; // FIXME: Remove eventually, but done to keep tests from breaking.
    if (isFloating())
        return "RenderBlock (floating)"_s;
    if (isOutOfFlowPositioned())
        return "RenderBlock (positioned)"_s;
    if (isAnonymousBlock())
        return "RenderBlock (anonymous)"_s;
    // FIXME: Temporary hack while the new generated content system is being implemented.
    if (isPseudoElement())
        return "RenderBlock (generated)"_s;
    if (isAnonymous())
        return "RenderBlock (generated)"_s;
    if (isRelativelyPositioned())
        return "RenderBlock (relative positioned)"_s;
    if (isStickilyPositioned())
        return "RenderBlock (sticky positioned)"_s;
    return "RenderBlock"_s;
}

String RenderBlock::debugDescription() const
{
    if (isViewTransitionPseudo()) {
        StringBuilder builder;

        builder.append(renderName(), " 0x"_s, hex(reinterpret_cast<uintptr_t>(this), Lowercase));

        builder.append(" ::view-transition"_s);
        if (style().pseudoElementType() != PseudoElementType::ViewTransition) {
            builder.append("-"_s, style().pseudoElementType() == PseudoElementType::ViewTransitionGroup ? "group("_s : "image-pair("_s);
            builder.append(style().pseudoElementNameArgument(), ')');
        }
        return builder.toString();
    }

    return RenderObject::debugDescription();
}

TextRun RenderBlock::constructTextRun(StringView stringView, const RenderStyle& style, ExpansionBehavior expansion, TextRunFlags flags)
{
    auto textDirection = TextDirection::LTR;
    bool directionalOverride = style.rtlOrdering() == Order::Visual;
    if (flags != DefaultTextRunFlags) {
        if (flags & RespectDirection)
            textDirection = style.writingMode().bidiDirection();
        if (flags & RespectDirectionOverride)
            directionalOverride |= isOverride(style.unicodeBidi());
    }

    // This works because:
    // 1. TextRun owns its text string. Its member is a String, not a StringView
    // 2. This replacement doesn't affect string indices. We're replacing a single Unicode code unit with another Unicode code unit.
    // How convenient.
    auto updatedString = RenderBlock::updateSecurityDiscCharacters(style, stringView.toStringWithoutCopying());
    return TextRun(WTFMove(updatedString), 0, 0, expansion, textDirection, directionalOverride);
}

TextRun RenderBlock::constructTextRun(const String& string, const RenderStyle& style, ExpansionBehavior expansion, TextRunFlags flags)
{
    return constructTextRun(StringView(string), style, expansion, flags);
}

TextRun RenderBlock::constructTextRun(const AtomString& atomString, const RenderStyle& style, ExpansionBehavior expansion, TextRunFlags flags)
{
    return constructTextRun(StringView(atomString), style, expansion, flags);
}

TextRun RenderBlock::constructTextRun(const RenderText& text, const RenderStyle& style, ExpansionBehavior expansion)
{
    return constructTextRun(text.stringView(), style, expansion);
}

TextRun RenderBlock::constructTextRun(const RenderText& text, unsigned offset, unsigned length, const RenderStyle& style, ExpansionBehavior expansion)
{
    unsigned stop = offset + length;
    ASSERT(stop <= text.text().length());
    return constructTextRun(text.stringView(offset, stop), style, expansion);
}

TextRun RenderBlock::constructTextRun(std::span<const Latin1Character> characters, const RenderStyle& style, ExpansionBehavior expansion)
{
    return constructTextRun(StringView { characters }, style, expansion);
}

TextRun RenderBlock::constructTextRun(std::span<const char16_t> characters, const RenderStyle& style, ExpansionBehavior expansion)
{
    return constructTextRun(StringView { characters }, style, expansion);
}

bool RenderBlock::hasDefiniteLogicalHeight() const
{
    return (bool)availableLogicalHeightForPercentageComputation();
}

std::optional<LayoutUnit> RenderBlock::availableLogicalHeightForPercentageComputation() const
{
    // For anonymous blocks that are skipped during percentage height calculation,
    // we consider them to have an indefinite height.
    if (skipContainingBlockForPercentHeightCalculation(*this, false))
        return { };

    auto availableHeight = [&]() -> std::optional<LayoutUnit> {
        if (auto overridingLogicalHeightForFlex = (isFlexItem() ? downcast<RenderFlexibleBox>(parent())->usedFlexItemOverridingLogicalHeightForPercentageResolution(*this) : std::nullopt))
            return contentBoxLogicalHeight(*overridingLogicalHeightForFlex);

        if (auto overridingLogicalHeightForGrid = (isGridItem() ? overridingBorderBoxLogicalHeight() : std::nullopt))
            return contentBoxLogicalHeight(*overridingLogicalHeightForGrid);

        auto& style = this->style();
        if (auto fixedLogicalHeight = style.logicalHeight().tryFixed()) {
            auto contentBoxHeight = adjustContentBoxLogicalHeightForBoxSizing(LayoutUnit { fixedLogicalHeight->resolveZoom(style.usedZoomForLength()) });
            return std::max(0_lu, constrainContentBoxLogicalHeightByMinMax(contentBoxHeight - scrollbarLogicalHeight(), { }));
        }

        if (shouldComputeLogicalHeightFromAspectRatio()) {
            // Only grid is expected to be in a state where it is calculating pref width and having unknown logical width.
            if (isRenderGrid() && needsPreferredLogicalWidthsUpdate() && !style.logicalWidth().isSpecified())
                return { };
            return blockSizeFromAspectRatio(
                horizontalBorderAndPaddingExtent(),
                verticalBorderAndPaddingExtent(),
                LayoutUnit { style.logicalAspectRatio() },
                style.boxSizingForAspectRatio(),
                logicalWidth(),
                style.aspectRatio(),
                isRenderReplaced()
            );
        }

        // A positioned element that specified both top/bottom or that specifies
        // height should be treated as though it has a height explicitly specified
        // that can be used for any percentage computations.
        auto isOutOfFlowPositionedWithSpecifiedHeight = isOutOfFlowPositioned() && (!style.logicalHeight().isAuto() || (!style.logicalTop().isAuto() && !style.logicalBottom().isAuto()));
        if (isOutOfFlowPositionedWithSpecifiedHeight) {
            // Don't allow this to affect the block' size() member variable, since this
            // can get called while the block is still laying out its kids.
            return std::max(0_lu, computeLogicalHeight(logicalHeight(), 0_lu).m_extent - borderAndPaddingLogicalHeight() - scrollbarLogicalHeight());
        }

        if (style.logicalHeight().isPercentOrCalculated()) {
            if (auto heightWithScrollbar = computePercentageLogicalHeight(style.logicalHeight())) {
                auto contentBoxHeightWithScrollbar = adjustContentBoxLogicalHeightForBoxSizing(*heightWithScrollbar);
                // We need to adjust for min/max height because this method does not handle the min/max of the current block, its caller does.
                // So the return value from the recursive call will not have been adjusted yet.
                return std::max(0_lu, constrainContentBoxLogicalHeightByMinMax(contentBoxHeightWithScrollbar - scrollbarLogicalHeight(), { }));
            }
            return { };
        }

        if (isRenderView())
            return view().pageOrViewLogicalHeight();

        return { };
    };
    return availableHeight();
}
    
void RenderBlock::layoutExcludedChildren(RelayoutChildren relayoutChildren)
{
    if (!isFieldset())
        return;

    setIntrinsicBorderForFieldset(0);

    RenderBox* box = findFieldsetLegend();
    if (!box)
        return;

    box->setIsExcludedFromNormalLayout(true);
    for (auto& child : childrenOfType<RenderBox>(*this)) {
        if (&child == box || !child.isLegend())
            continue;
        child.setIsExcludedFromNormalLayout(false);
    }

    RenderBox& legend = *box;
    if (relayoutChildren == RelayoutChildren::Yes)
        legend.setChildNeedsLayout(MarkOnlyThis);
    legend.layoutIfNeeded();
    
    LayoutUnit logicalLeft;
    if (writingMode().isBidiLTR()) {
        switch (legend.style().textAlign()) {
        case Style::TextAlign::Center:
            logicalLeft = (logicalWidth() - logicalWidthForChild(legend)) / 2;
            break;
        case Style::TextAlign::Right:
            logicalLeft = logicalWidth() - borderAndPaddingEnd() - logicalWidthForChild(legend);
            break;
        default:
            logicalLeft = borderAndPaddingStart() + marginStartForChild(legend);
            break;
        }
    } else {
        switch (legend.style().textAlign()) {
        case Style::TextAlign::Left:
            logicalLeft = borderAndPaddingStart();
            break;
        case Style::TextAlign::Center: {
            // Make sure that the extra pixel goes to the end side in RTL (since it went to the end side
            // in LTR).
            LayoutUnit centeredWidth = logicalWidth() - logicalWidthForChild(legend);
            logicalLeft = centeredWidth - centeredWidth / 2;
            break;
        }
        default:
            logicalLeft = logicalWidth() - borderAndPaddingStart() - marginStartForChild(legend) - logicalWidthForChild(legend);
            break;
        }
    }
    
    setLogicalLeftForChild(legend, logicalLeft);
    
    LayoutUnit fieldsetBorderBefore = borderBefore();
    LayoutUnit legendLogicalHeight = logicalHeightForChild(legend);
    LayoutUnit legendAfterMargin = marginAfterForChild(legend);
    LayoutUnit topPositionForLegend = std::max(0_lu, (fieldsetBorderBefore - legendLogicalHeight) / 2);
    LayoutUnit bottomPositionForLegend = topPositionForLegend + legendLogicalHeight + legendAfterMargin;

    // Place the legend now.
    setLogicalTopForChild(legend, topPositionForLegend);

    // If the bottom of the legend (including its after margin) is below the fieldset border,
    // then we need to add in sufficient intrinsic border to account for this gap.
    // FIXME: Should we support the before margin of the legend? Not entirely clear.
    // FIXME: Consider dropping support for the after margin of the legend. Not sure other
    // browsers support that anyway.
    if (bottomPositionForLegend > fieldsetBorderBefore)
        setIntrinsicBorderForFieldset(bottomPositionForLegend - fieldsetBorderBefore);
    
    // Now that the legend is included in the border extent, we can set our logical height
    // to the borderBefore (which includes the legend and its after margin if they were bigger
    // than the actual fieldset border) and then add in our padding before.
    setLogicalHeight(borderAndPaddingBefore());
}

RenderBox* RenderBlock::findFieldsetLegend(FieldsetFindLegendOption option) const
{
    if (isSkippedContentRoot(*this))
        return { };

    for (auto& legend : childrenOfType<RenderBox>(*this)) {
        if (option == FieldsetIgnoreFloatingOrOutOfFlow && legend.isFloatingOrOutOfFlowPositioned())
            continue;
        if (legend.isLegend())
            return const_cast<RenderBox*>(&legend);
    }
    return { };
}

void RenderBlock::adjustBorderBoxRectForPainting(LayoutRect& paintRect)
{
    if (!isFieldset() || !intrinsicBorderForFieldset())
        return;
    
    auto* legend = findFieldsetLegend();
    if (!legend)
        return;

    if (writingMode().isHorizontal()) {
        LayoutUnit yOff = std::max(0_lu, (legend->height() - RenderBox::borderBefore()) / 2);
        paintRect.setHeight(paintRect.height() - yOff);
        if (writingMode().isBlockTopToBottom())
            paintRect.setY(paintRect.y() + yOff);
    } else {
        LayoutUnit xOff = std::max(0_lu, (legend->width() - RenderBox::borderBefore()) / 2);
        paintRect.setWidth(paintRect.width() - xOff);
        if (writingMode().isBlockLeftToRight())
            paintRect.setX(paintRect.x() + xOff);
    }
}

LayoutRect RenderBlock::paintRectToClipOutFromBorder(const LayoutRect& paintRect)
{
    LayoutRect clipRect;
    if (!isFieldset())
        return clipRect;
    auto* legend = findFieldsetLegend();
    if (!legend)
        return clipRect;

    LayoutUnit borderExtent = RenderBox::borderBefore();
    if (writingMode().isHorizontal()) {
        clipRect.setX(paintRect.x() + legend->x());
        clipRect.setY(writingMode().isBlockTopToBottom() ? paintRect.y() : paintRect.y() + paintRect.height() - borderExtent);
        clipRect.setWidth(legend->width());
        clipRect.setHeight(borderExtent);
    } else {
        clipRect.setX(writingMode().isBlockLeftToRight() ? paintRect.x() : paintRect.x() + paintRect.width() - borderExtent);
        clipRect.setY(paintRect.y() + legend->y());
        clipRect.setWidth(borderExtent);
        clipRect.setHeight(legend->height());
    }
    return clipRect;
}

LayoutUnit RenderBlock::intrinsicBorderForFieldset() const
{
    auto* rareData = blockRareData();
    return rareData ? rareData->m_intrinsicBorderForFieldset : 0_lu;
}

void RenderBlock::setIntrinsicBorderForFieldset(LayoutUnit padding)
{
    auto* rareData = blockRareData();
    if (!rareData) {
        if (!padding)
            return;
        rareData = &ensureBlockRareData();
    }
    rareData->m_intrinsicBorderForFieldset = padding;
}

RectEdges<LayoutUnit> RenderBlock::borderWidths() const
{
    if (!intrinsicBorderForFieldset())
        return RenderBox::borderWidths();

    return {
        borderTop(),
        borderRight(),
        borderBottom(),
        borderLeft()
    };
}

LayoutUnit RenderBlock::borderTop() const
{
    if (!writingMode().isBlockTopToBottom() || !intrinsicBorderForFieldset())
        return RenderBox::borderTop();
    return RenderBox::borderTop() + intrinsicBorderForFieldset();
}

LayoutUnit RenderBlock::borderLeft() const
{
    if (!writingMode().isBlockLeftToRight() || !intrinsicBorderForFieldset())
        return RenderBox::borderLeft();
    return RenderBox::borderLeft() + intrinsicBorderForFieldset();
}

LayoutUnit RenderBlock::borderBottom() const
{
    if (writingMode().blockDirection() != FlowDirection::BottomToTop || !intrinsicBorderForFieldset())
        return RenderBox::borderBottom();
    return RenderBox::borderBottom() + intrinsicBorderForFieldset();
}

LayoutUnit RenderBlock::borderRight() const
{
    if (writingMode().blockDirection() != FlowDirection::RightToLeft || !intrinsicBorderForFieldset())
        return RenderBox::borderRight();
    return RenderBox::borderRight() + intrinsicBorderForFieldset();
}

LayoutUnit RenderBlock::borderBefore() const
{
    return RenderBox::borderBefore() + intrinsicBorderForFieldset();
}

bool RenderBlock::computePreferredWidthsForExcludedChildren(LayoutUnit& minWidth, LayoutUnit& maxWidth) const
{
    if (!isFieldset())
        return false;
    
    auto* legend = findFieldsetLegend();
    if (!legend)
        return false;
    
    legend->setIsExcludedFromNormalLayout(true);

    computeChildPreferredLogicalWidths(*legend, minWidth, maxWidth);
    
    // These are going to be added in later, so we subtract them out to reflect the
    // fact that the legend is outside the scrollable area.
    auto scrollbarWidth = intrinsicScrollbarLogicalWidthIncludingGutter();
    minWidth -= scrollbarWidth;
    maxWidth -= scrollbarWidth;
    
    const auto& childStyle = legend->style();
    const auto& childZoomFactor = childStyle.usedZoomForLength();

    LayoutUnit marginStart;
    LayoutUnit marginEnd;
    if (auto fixedMarginStart = childStyle.marginStart(writingMode()).tryFixed())
        marginStart += fixedMarginStart->resolveZoom(childZoomFactor);
    if (auto fixedMarginEnd = childStyle.marginEnd(writingMode()).tryFixed())
        marginEnd += fixedMarginEnd->resolveZoom(childZoomFactor);

    auto margin = marginStart + marginEnd;

    minWidth += margin;
    maxWidth += margin;

    return true;
}

LayoutUnit RenderBlock::adjustBorderBoxLogicalHeightForBoxSizing(LayoutUnit height) const
{
    // FIXME: We're doing this to match other browsers even though it's questionable.
    // Shouldn't height:100px mean the fieldset content gets 100px of height even if the
    // resulting fieldset becomes much taller because of the legend?
    LayoutUnit bordersPlusPadding = borderAndPaddingLogicalHeight();
    if (style().boxSizing() == BoxSizing::ContentBox)
        return height + bordersPlusPadding - intrinsicBorderForFieldset();
    return std::max(height, bordersPlusPadding);
}

LayoutUnit RenderBlock::adjustContentBoxLogicalHeightForBoxSizing(std::optional<LayoutUnit> height) const
{
    // FIXME: We're doing this to match other browsers even though it's questionable.
    // Shouldn't height:100px mean the fieldset content gets 100px of height even if the
    // resulting fieldset becomes much taller because of the legend?
    if (!height)
        return 0;
    LayoutUnit result = height.value();
    if (style().boxSizing() == BoxSizing::BorderBox)
        result -= borderAndPaddingLogicalHeight();
    else
        result -= intrinsicBorderForFieldset();
    return std::max(0_lu, result);
}

LayoutUnit RenderBlock::adjustIntrinsicLogicalHeightForBoxSizing(LayoutUnit height) const
{
    if (style().boxSizing() == BoxSizing::BorderBox)
        return height + borderAndPaddingLogicalHeight();
    return height + intrinsicBorderForFieldset();
}

void RenderBlock::paintExcludedChildrenInBorder(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (!isFieldset())
        return;
    
    RenderBox* box = findFieldsetLegend();
    if (!box || !box->isExcludedFromNormalLayout() || box->hasSelfPaintingLayer())
        return;
    
    LayoutPoint childPoint = flipForWritingModeForChild(*box, paintOffset);
    box->paintAsInlineBlock(paintInfo, childPoint);
}

bool RenderBlock::hitTestExcludedChildrenInBorder(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction hitTestAction)
{
    if (!isFieldset())
        return false;

    auto* legend = findFieldsetLegend();
    if (!legend || !legend->isExcludedFromNormalLayout() || legend->hasSelfPaintingLayer())
        return false;

    HitTestAction childHitTest = hitTestAction;
    if (hitTestAction == HitTestChildBlockBackgrounds)
        childHitTest = HitTestChildBlockBackground;
    LayoutPoint childPoint = flipForWritingModeForChild(*legend, accumulatedOffset);
    return legend->nodeAtPoint(request, result, locationInContainer, childPoint, childHitTest);
}

String RenderBlock::updateSecurityDiscCharacters(const RenderStyle& style, String&& string)
{
#if !PLATFORM(COCOA)
    UNUSED_PARAM(style);
    return WTFMove(string);
#else
    if (style.textSecurity() == TextSecurity::None)
        return WTFMove(string);
    // This PUA character in the system font is used to render password field dots on Cocoa platforms.
    constexpr char16_t textSecurityDiscPUACodePoint = 0xF79A;
    Ref font = style.fontCascade().primaryFont();
    if (!(font->platformData().isSystemFont() && font->glyphForCharacter(textSecurityDiscPUACodePoint)))
        return WTFMove(string);

    // See RenderText::setRenderedText()
#if PLATFORM(IOS_FAMILY)
    constexpr char16_t discCharacterToReplace = blackCircle;
#else
    constexpr char16_t discCharacterToReplace = bullet;
#endif

    return makeStringByReplacingAll(string, discCharacterToReplace, textSecurityDiscPUACodePoint);
#endif
}

LayoutUnit RenderBlock::layoutOverflowLogicalBottom(const RenderBlock& renderer)
{
    ASSERT(is<RenderGrid>(renderer) || is<RenderFlexibleBox>(renderer));
    auto maxChildLogicalBottom = LayoutUnit { };
    for (auto& child : childrenOfType<RenderBox>(renderer)) {
        if (child.isOutOfFlowPositioned())
            continue;
        auto childLogicalBottom = renderer.logicalTopForChild(child) + renderer.logicalHeightForChild(child) + renderer.marginAfterForChild(child);
        maxChildLogicalBottom = std::max(maxChildLogicalBottom, childLogicalBottom);
    }
    return std::max(renderer.clientLogicalBottom(), maxChildLogicalBottom + renderer.paddingAfter());
}

void RenderBlock::updateDescendantTransformsAfterLayout()
{
    auto boxes = view().frameView().layoutContext().takeBoxesNeedingTransformUpdateAfterContainerLayout(*this);
    for (auto& box : boxes) {
        if (box && box->hasLayer())
            box->layer()->updateTransform();
    }
}

} // namespace WebCore
