/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2005 Allan Sandfeld Jensen (kde@carewolf.com)
 *           (C) 2005, 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
#include "RenderBoxModelObject.h"

#include "BitmapImage.h"
#include "BorderEdge.h"
#include "CachedImage.h"
#include "ColorBlending.h"
#include "Document.h"
#include "FloatRoundedRect.h"
#include "Frame.h"
#include "FrameView.h"
#include "GeometryUtilities.h"
#include "GraphicsContext.h"
#include "HTMLImageElement.h"
#include "HTMLNames.h"
#include "ImageBuffer.h"
#include "ImageQualityController.h"
#include "InlineIteratorInlineBox.h"
#include "Path.h"
#include "RenderBlock.h"
#include "RenderFlexibleBox.h"
#include "RenderFragmentContainer.h"
#include "RenderInline.h"
#include "RenderLayer.h"
#include "RenderLayerBacking.h"
#include "RenderLayerCompositor.h"
#include "RenderLayerScrollableArea.h"
#include "RenderMultiColumnFlow.h"
#include "RenderTable.h"
#include "RenderTableRow.h"
#include "RenderText.h"
#include "RenderTextFragment.h"
#include "RenderTreeBuilder.h"
#include "RenderView.h"
#include "ScrollingConstraints.h"
#include "Settings.h"
#include "Styleable.h"
#include "TextBoxPainter.h"
#include "TransformState.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/NeverDestroyed.h>
#if ASSERT_ENABLED
#include <wtf/SetForScope.h>
#endif

#if PLATFORM(IOS_FAMILY)
#include "RuntimeApplicationChecks.h"
#endif

namespace WebCore {

using namespace HTMLNames;

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderBoxModelObject);

// The HashMap for storing continuation pointers.
// An inline can be split with blocks occuring in between the inline content.
// When this occurs we need a pointer to the next object. We can basically be
// split into a sequence of inlines and blocks. The continuation will either be
// an anonymous block (that houses other blocks) or it will be an inline flow.
// <b><i><p>Hello</p></i></b>. In this example the <i> will have a block as
// its continuation but the <b> will just have an inline as its continuation.
RenderBoxModelObject::ContinuationChainNode::ContinuationChainNode(RenderBoxModelObject& renderer)
    : renderer(renderer)
{
}

RenderBoxModelObject::ContinuationChainNode::~ContinuationChainNode()
{
    if (next) {
        ASSERT(previous);
        ASSERT(next->previous == this);
        next->previous = previous;
    }
    if (previous) {
        ASSERT(previous->next == this);
        previous->next = next;
    }
}

void RenderBoxModelObject::ContinuationChainNode::insertAfter(ContinuationChainNode& after)
{
    ASSERT(!previous);
    ASSERT(!next);
    if ((next = after.next)) {
        ASSERT(next->previous == &after);
        next->previous = this;
    }
    previous = &after;
    after.next = this;
}

using ContinuationChainNodeMap = HashMap<const RenderBoxModelObject*, std::unique_ptr<RenderBoxModelObject::ContinuationChainNode>>;

static ContinuationChainNodeMap& continuationChainNodeMap()
{
    static NeverDestroyed<ContinuationChainNodeMap> map;
    return map;
}

using FirstLetterRemainingTextMap = HashMap<const RenderBoxModelObject*, WeakPtr<RenderTextFragment>>;

static FirstLetterRemainingTextMap& firstLetterRemainingTextMap()
{
    static NeverDestroyed<FirstLetterRemainingTextMap> map;
    return map;
}

void RenderBoxModelObject::setSelectionState(HighlightState state)
{
    if (state == HighlightState::Inside && selectionState() != HighlightState::None)
        return;

    if ((state == HighlightState::Start && selectionState() == HighlightState::End)
        || (state == HighlightState::End && selectionState() == HighlightState::Start))
        RenderLayerModelObject::setSelectionState(HighlightState::Both);
    else
        RenderLayerModelObject::setSelectionState(state);

    // FIXME: We should consider whether it is OK propagating to ancestor RenderInlines.
    // This is a workaround for http://webkit.org/b/32123
    // The containing block can be null in case of an orphaned tree.
    RenderBlock* containingBlock = this->containingBlock();
    if (containingBlock && !containingBlock->isRenderView())
        containingBlock->setSelectionState(state);
}

void RenderBoxModelObject::contentChanged(ContentChangeType changeType)
{
    if (!hasLayer())
        return;

    layer()->contentChanged(changeType);
}

bool RenderBoxModelObject::hasAcceleratedCompositing() const
{
    return view().compositor().hasAcceleratedCompositing();
}

RenderBoxModelObject::RenderBoxModelObject(Element& element, RenderStyle&& style, BaseTypeFlags baseTypeFlags)
    : RenderLayerModelObject(element, WTFMove(style), baseTypeFlags | RenderBoxModelObjectFlag)
{
}

RenderBoxModelObject::RenderBoxModelObject(Document& document, RenderStyle&& style, BaseTypeFlags baseTypeFlags)
    : RenderLayerModelObject(document, WTFMove(style), baseTypeFlags | RenderBoxModelObjectFlag)
{
}

RenderBoxModelObject::~RenderBoxModelObject()
{
    // Do not add any code here. Add it to willBeDestroyed() instead.
    ASSERT(!continuation());
}

void RenderBoxModelObject::willBeDestroyed()
{
    if (hasContinuationChainNode())
        removeFromContinuationChain();

    if (isFirstLetter())
        clearFirstLetterRemainingText();

    if (!renderTreeBeingDestroyed())
        view().imageQualityController().rendererWillBeDestroyed(*this);

    RenderLayerModelObject::willBeDestroyed();
}

bool RenderBoxModelObject::hasVisibleBoxDecorationStyle() const
{
    return hasBackground() || style().hasVisibleBorderDecoration() || style().hasEffectiveAppearance() || style().boxShadow();
}

void RenderBoxModelObject::updateFromStyle()
{
    RenderLayerModelObject::updateFromStyle();

    // Set the appropriate bits for a box model object.  Since all bits are cleared in styleWillChange,
    // we only check for bits that could possibly be set to true.
    const RenderStyle& styleToUse = style();
    setHasVisibleBoxDecorations(hasVisibleBoxDecorationStyle());
    setInline(styleToUse.isDisplayInlineType());
    setPositionState(styleToUse.position());
    setHorizontalWritingMode(styleToUse.isHorizontalWritingMode());
    if (styleToUse.isFlippedBlocksWritingMode())
        view().frameView().setHasFlippedBlockRenderers(true);
    setPaintContainmentApplies(shouldApplyPaintContainment(*this));
}

static LayoutSize accumulateInFlowPositionOffsets(const RenderObject* child)
{
    if (!child->isAnonymousBlock() || !child->isInFlowPositioned())
        return LayoutSize();
    LayoutSize offset;
    for (RenderElement* parent = downcast<RenderBlock>(*child).inlineContinuation(); is<RenderInline>(parent); parent = parent->parent()) {
        if (parent->isInFlowPositioned())
            offset += downcast<RenderInline>(*parent).offsetForInFlowPosition();
    }
    return offset;
}
    
static inline bool isOutOfFlowPositionedWithImplicitHeight(const RenderBoxModelObject& child)
{
    return child.isOutOfFlowPositioned() && !child.style().logicalTop().isAuto() && !child.style().logicalBottom().isAuto();
}
    
RenderBlock* RenderBoxModelObject::containingBlockForAutoHeightDetection(Length logicalHeight) const
{
    // For percentage heights: The percentage is calculated with respect to the
    // height of the generated box's containing block. If the height of the
    // containing block is not specified explicitly (i.e., it depends on content
    // height), and this element is not absolutely positioned, the used height is
    // calculated as if 'auto' was specified.
    if (!logicalHeight.isPercentOrCalculated() || isOutOfFlowPositioned())
        return nullptr;
    
    // Anonymous block boxes are ignored when resolving percentage values that
    // would refer to it: the closest non-anonymous ancestor box is used instead.
    auto* cb = containingBlock();
    while (cb && cb->isAnonymous() && !is<RenderView>(cb))
        cb = cb->containingBlock();
    if (!cb)
        return nullptr;

    // Matching RenderBox::percentageLogicalHeightIsResolvable() by
    // ignoring table cell's attribute value, where it says that table cells
    // violate what the CSS spec says to do with heights. Basically we don't care
    // if the cell specified a height or not.
    if (cb->isTableCell())
        return nullptr;
    
    // Match RenderBox::availableLogicalHeightUsing by special casing the layout
    // view. The available height is taken from the frame.
    if (cb->isRenderView())
        return nullptr;
    
    if (isOutOfFlowPositionedWithImplicitHeight(*cb))
        return nullptr;
    
    return cb;
}
    
bool RenderBoxModelObject::hasAutoHeightOrContainingBlockWithAutoHeight() const
{
    const auto* thisBox = isBox() ? downcast<RenderBox>(this) : nullptr;
    Length logicalHeightLength = style().logicalHeight();
    auto* cb = containingBlockForAutoHeightDetection(logicalHeightLength);
    
    if (logicalHeightLength.isPercentOrCalculated() && cb && isBox())
        cb->addPercentHeightDescendant(*const_cast<RenderBox*>(downcast<RenderBox>(this)));

    if (thisBox && thisBox->isFlexItem() && downcast<RenderFlexibleBox>(*parent()).useChildOverridingLogicalHeightForPercentageResolution(*thisBox))
        return false;
    
    if (thisBox && thisBox->isGridItem() && thisBox->hasOverridingContainingBlockContentLogicalHeight())
        return thisBox->overridingContainingBlockContentLogicalHeight() == std::nullopt;
    
    if (logicalHeightLength.isAuto() && !isOutOfFlowPositionedWithImplicitHeight(*this))
        return true;

    // We need the containing block to have a definite block-size in order to resolve the block-size of the descendant,
    // except when in quirks mode. Flexboxes follow strict behavior even in quirks mode, though.
    if (!cb || (document().inQuirksMode() && !cb->isFlexibleBoxIncludingDeprecated()))
        return false;
    if (thisBox && thisBox->hasOverridingContainingBlockContentLogicalHeight())
        return thisBox->overridingContainingBlockContentLogicalHeight() == std::nullopt;
    return !cb->hasDefiniteLogicalHeight();
}

DecodingMode RenderBoxModelObject::decodingModeForImageDraw(const Image& image, const PaintInfo& paintInfo) const
{
    if (!is<BitmapImage>(image))
        return DecodingMode::Synchronous;
    
    const BitmapImage& bitmapImage = downcast<BitmapImage>(image);
    if (bitmapImage.canAnimate()) {
        // The DecodingMode for the current frame has to be Synchronous. The DecodingMode
        // for the next frame will be calculated in BitmapImage::internalStartAnimation().
        return DecodingMode::Synchronous;
    }

    // Large image case.
#if PLATFORM(IOS_FAMILY)
    if (IOSApplication::isIBooksStorytime())
        return DecodingMode::Synchronous;
#endif
    if (is<HTMLImageElement>(element())) {
        auto decodingMode = downcast<HTMLImageElement>(*element()).decodingMode();
        if (decodingMode != DecodingMode::Auto)
            return decodingMode;
    }
    if (bitmapImage.isLargeImageAsyncDecodingEnabledForTesting())
        return DecodingMode::Asynchronous;
    if (document().isImageDocument())
        return DecodingMode::Synchronous;
    if (paintInfo.paintBehavior.contains(PaintBehavior::Snapshotting))
        return DecodingMode::Synchronous;
    if (!settings().largeImageAsyncDecodingEnabled())
        return DecodingMode::Synchronous;
    if (!bitmapImage.canUseAsyncDecodingForLargeImages())
        return DecodingMode::Synchronous;
    if (paintInfo.paintBehavior.contains(PaintBehavior::TileFirstPaint))
        return DecodingMode::Asynchronous;
    // FIXME: isVisibleInViewport() is not cheap. Find a way to make this condition faster.
    if (!isVisibleInViewport())
        return DecodingMode::Asynchronous;
    return DecodingMode::Synchronous;
}

LayoutSize RenderBoxModelObject::relativePositionOffset() const
{
    // This function has been optimized to avoid calls to containingBlock() in the common case
    // where all values are either auto or fixed.

    LayoutSize offset = accumulateInFlowPositionOffsets(this);

    // Objects that shrink to avoid floats normally use available line width when computing containing block width.  However
    // in the case of relative positioning using percentages, we can't do this.  The offset should always be resolved using the
    // available width of the containing block.  Therefore we don't use containingBlockLogicalWidthForContent() here, but instead explicitly
    // call availableWidth on our containing block.
    // However for grid items the containing block is the grid area, so offsets should be resolved against that:
    // https://drafts.csswg.org/css-grid/#grid-item-sizing
    if (!style().left().isAuto() || !style().right().isAuto()) {
        LayoutUnit availableWidth = hasOverridingContainingBlockContentWidth()
            ? valueOrDefault(overridingContainingBlockContentWidth()) : containingBlock()->availableWidth();
        if (!style().left().isAuto()) {
            if (!style().right().isAuto() && !containingBlock()->style().isLeftToRightDirection())
                offset.setWidth(-valueForLength(style().right(), !style().right().isFixed() ? availableWidth : 0_lu));
            else
                offset.expand(valueForLength(style().left(), !style().left().isFixed() ? availableWidth : 0_lu), 0_lu);
        } else if (!style().right().isAuto())
            offset.expand(-valueForLength(style().right(), !style().right().isFixed() ? availableWidth : 0_lu), 0_lu);
    }

    // If the containing block of a relatively positioned element does not
    // specify a height, a percentage top or bottom offset should be resolved as
    // auto. An exception to this is if the containing block has the WinIE quirk
    // where <html> and <body> assume the size of the viewport. In this case,
    // calculate the percent offset based on this height.
    // See <https://bugs.webkit.org/show_bug.cgi?id=26396>.
    // Another exception is a grid item, as the containing block is the grid area:
    // https://drafts.csswg.org/css-grid/#grid-item-sizing
    if (!style().top().isAuto()
        && (!style().top().isPercentOrCalculated()
            || !containingBlock()->hasAutoHeightOrContainingBlockWithAutoHeight()
            || containingBlock()->stretchesToViewport()
            || hasOverridingContainingBlockContentHeight())) {
        // FIXME: The computation of the available height is repeated later for "bottom".
        // We could refactor this and move it to some common code for both ifs, however moving it outside of the ifs
        // is not possible as it'd cause performance regressions.
        offset.expand(0_lu, valueForLength(style().top(), !style().top().isFixed()
            ? (hasOverridingContainingBlockContentHeight() ? overridingContainingBlockContentHeight().value_or(0_lu) : containingBlock()->availableHeight())
            : LayoutUnit()));
    } else if (!style().bottom().isAuto()
        && (!style().bottom().isPercentOrCalculated()
            || !containingBlock()->hasAutoHeightOrContainingBlockWithAutoHeight()
            || containingBlock()->stretchesToViewport()
            || hasOverridingContainingBlockContentHeight())) {
        // FIXME: Check comment above for "top", it applies here too.
        offset.expand(0_lu, -valueForLength(style().bottom(), !style().bottom().isFixed()
            ? (hasOverridingContainingBlockContentHeight() ? overridingContainingBlockContentHeight().value_or(0_lu) : containingBlock()->availableHeight())
            : LayoutUnit()));
    }

    return offset;
}

LayoutPoint RenderBoxModelObject::adjustedPositionRelativeToOffsetParent(const LayoutPoint& startPoint) const
{
    // If the element is the HTML body element or doesn't have a parent
    // return 0 and stop this algorithm.
    if (isBody() || !parent())
        return LayoutPoint();

    LayoutPoint referencePoint = startPoint;
    
    // If the offsetParent of the element is null, or is the HTML body element,
    // return the distance between the canvas origin and the left border edge 
    // of the element and stop this algorithm.
    if (const RenderBoxModelObject* offsetParent = this->offsetParent()) {
        if (is<RenderBox>(*offsetParent) && !offsetParent->isBody() && !is<RenderTable>(*offsetParent))
            referencePoint.move(-downcast<RenderBox>(*offsetParent).borderLeft(), -downcast<RenderBox>(*offsetParent).borderTop());
        if (!isOutOfFlowPositioned() || enclosingFragmentedFlow()) {
            if (isRelativelyPositioned())
                referencePoint.move(relativePositionOffset());
            else if (isStickilyPositioned())
                referencePoint.move(stickyPositionOffset());
            
            // CSS regions specification says that region flows should return the body element as their offsetParent.
            // Since we will bypass the body’s renderer anyway, just end the loop if we encounter a region flow (named flow thread).
            // See http://dev.w3.org/csswg/css-regions/#cssomview-offset-attributes
            auto* ancestor = parent();
            while (ancestor != offsetParent) {
                // FIXME: What are we supposed to do inside SVG content?
                
                if (is<RenderMultiColumnFlow>(*ancestor)) {
                    // We need to apply a translation based off what region we are inside.
                    RenderFragmentContainer* fragment = downcast<RenderMultiColumnFlow>(*ancestor).physicalTranslationFromFlowToFragment(referencePoint);
                    if (fragment)
                        referencePoint.moveBy(fragment->topLeftLocation());
                } else if (!isOutOfFlowPositioned()) {
                    if (is<RenderBox>(*ancestor) && !is<RenderTableRow>(*ancestor))
                        referencePoint.moveBy(downcast<RenderBox>(*ancestor).topLeftLocation());
                }
                
                ancestor = ancestor->parent();
            }
            
            if (is<RenderBox>(*offsetParent) && offsetParent->isBody() && !offsetParent->isPositioned())
                referencePoint.moveBy(downcast<RenderBox>(*offsetParent).topLeftLocation());
        }
    }

    return referencePoint;
}

std::pair<const RenderBox&, const RenderLayer*> RenderBoxModelObject::enclosingClippingBoxForStickyPosition() const
{
    ASSERT(isStickilyPositioned());
    RenderLayer* clipLayer = hasLayer() ? layer()->enclosingOverflowClipLayer(ExcludeSelf) : nullptr;
    const RenderBox& box = clipLayer ? downcast<RenderBox>(clipLayer->renderer()) : view();
    return { box, clipLayer };
}

void RenderBoxModelObject::computeStickyPositionConstraints(StickyPositionViewportConstraints& constraints, const FloatRect& constrainingRect) const
{
    constraints.setConstrainingRectAtLastLayout(constrainingRect);

    // Do not use anonymous containing blocks to determine sticky constraints. We want the size
    // of the first true containing block, because that is what imposes the limitation on the
    // movement of stickily positioned items.
    RenderBlock* containingBlock = this->containingBlock();
    while (containingBlock && (!is<RenderBlock>(*containingBlock) || containingBlock->isAnonymousBlock()))
        containingBlock = containingBlock->containingBlock();
    ASSERT(containingBlock);

    auto [enclosingClippingBox, enclosingClippingLayer] = enclosingClippingBoxForStickyPosition();

    LayoutRect containerContentRect;
    if (!enclosingClippingLayer || (containingBlock != &enclosingClippingBox)) {
        // In this case either the scrolling element is the view or there is another containing block in
        // the hierarchy between this stickily positioned item and its scrolling ancestor. In both cases,
        // we use the content box rectangle of the containing block, which is what should constrain the
        // movement.
        containerContentRect = containingBlock->computedCSSContentBoxRect();
    } else {
        containerContentRect = containingBlock->layoutOverflowRect();
        containerContentRect.contract(LayoutBoxExtent {
            containingBlock->computedCSSPaddingTop(), containingBlock->computedCSSPaddingRight(),
            containingBlock->computedCSSPaddingBottom(), containingBlock->computedCSSPaddingLeft() });
    }

    LayoutUnit maxWidth = containingBlock->availableLogicalWidth();

    // Sticky positioned element ignore any override logical width on the containing block (as they don't call
    // containingBlockLogicalWidthForContent). It's unclear whether this is totally fine.
    LayoutBoxExtent minMargin(minimumValueForLength(style().marginTop(), maxWidth),
        minimumValueForLength(style().marginRight(), maxWidth),
        minimumValueForLength(style().marginBottom(), maxWidth),
        minimumValueForLength(style().marginLeft(), maxWidth));

    // Compute the container-relative area within which the sticky element is allowed to move.
    containerContentRect.contract(minMargin);

    // Finally compute container rect relative to the scrolling ancestor. We pass an empty
    // mode here, because sticky positioning should ignore transforms.
    FloatRect containerRectRelativeToScrollingAncestor = containingBlock->localToContainerQuad(FloatRect(containerContentRect), &enclosingClippingBox, { } /* ignore transforms */).boundingBox();
    if (enclosingClippingLayer) {
        FloatPoint containerLocationRelativeToScrollingAncestor = containerRectRelativeToScrollingAncestor.location() -
            FloatSize(enclosingClippingBox.borderLeft() + enclosingClippingBox.paddingLeft(),
            enclosingClippingBox.borderTop() + enclosingClippingBox.paddingTop());
        if (&enclosingClippingBox != containingBlock) {
            if (auto* scrollableArea = enclosingClippingLayer->scrollableArea())
                containerLocationRelativeToScrollingAncestor += scrollableArea->scrollOffset();
        }
        containerRectRelativeToScrollingAncestor.setLocation(containerLocationRelativeToScrollingAncestor);
    }
    constraints.setContainingBlockRect(containerRectRelativeToScrollingAncestor);

    // Now compute the sticky box rect, also relative to the scrolling ancestor.
    LayoutRect stickyBoxRect = frameRectForStickyPositioning();

    // Ideally, it would be possible to call this->localToContainerQuad to determine the frame
    // rectangle in the coordinate system of the scrolling ancestor, but localToContainerQuad
    // itself depends on sticky positioning! Instead, start from the parent but first adjusting
    // the rectangle for the writing mode of this stickily-positioned element. We also pass an
    // empty mode here because sticky positioning should ignore transforms.
    //
    // FIXME: It would also be nice to not have to call localToContainerQuad again since we
    // have already done a similar call to move from the containing block to the scrolling
    // ancestor above, but localToContainerQuad takes care of a lot of complex situations
    // involving inlines, tables, and transformations.
    if (parent()->isBox())
        downcast<RenderBox>(parent())->flipForWritingMode(stickyBoxRect);
    auto stickyBoxRelativeToScrollingAncestor = parent()->localToContainerQuad(FloatRect(stickyBoxRect), &enclosingClippingBox, { } /* ignore transforms */).boundingBox();

    if (enclosingClippingLayer) {
        stickyBoxRelativeToScrollingAncestor.move(-FloatSize(enclosingClippingBox.borderLeft() + enclosingClippingBox.paddingLeft(),
            enclosingClippingBox.borderTop() + enclosingClippingBox.paddingTop()));

        if (&enclosingClippingBox != parent()) {
            if (auto* scrollableArea = enclosingClippingLayer->scrollableArea())
                stickyBoxRelativeToScrollingAncestor.moveBy(scrollableArea->scrollOffset());
        }
    }
    constraints.setStickyBoxRect(stickyBoxRelativeToScrollingAncestor);

    if (!style().left().isAuto()) {
        constraints.setLeftOffset(valueForLength(style().left(), constrainingRect.width()));
        constraints.addAnchorEdge(ViewportConstraints::AnchorEdgeLeft);
    }

    if (!style().right().isAuto()) {
        constraints.setRightOffset(valueForLength(style().right(), constrainingRect.width()));
        constraints.addAnchorEdge(ViewportConstraints::AnchorEdgeRight);
    }

    if (!style().top().isAuto()) {
        constraints.setTopOffset(valueForLength(style().top(), constrainingRect.height()));
        constraints.addAnchorEdge(ViewportConstraints::AnchorEdgeTop);
    }

    if (!style().bottom().isAuto()) {
        constraints.setBottomOffset(valueForLength(style().bottom(), constrainingRect.height()));
        constraints.addAnchorEdge(ViewportConstraints::AnchorEdgeBottom);
    }
}

FloatRect RenderBoxModelObject::constrainingRectForStickyPosition() const
{
    RenderLayer* enclosingClippingLayer = hasLayer() ? layer()->enclosingOverflowClipLayer(ExcludeSelf) : nullptr;

    if (enclosingClippingLayer) {
        RenderBox& enclosingClippingBox = downcast<RenderBox>(enclosingClippingLayer->renderer());
        LayoutRect clipRect = enclosingClippingBox.overflowClipRect(LayoutPoint(), nullptr); // FIXME: make this work in regions.
        clipRect.contract(LayoutSize(enclosingClippingBox.paddingLeft() + enclosingClippingBox.paddingRight(),
            enclosingClippingBox.paddingTop() + enclosingClippingBox.paddingBottom()));

        FloatRect constrainingRect = enclosingClippingBox.localToContainerQuad(FloatRect(clipRect), &view()).boundingBox();

        auto* scrollableArea = enclosingClippingLayer->scrollableArea();
        FloatPoint scrollOffset;
        if (scrollableArea)
            scrollOffset = FloatPoint() + scrollableArea->scrollOffset();

        float scrollbarOffset = 0;
        if (enclosingClippingBox.hasLayer() && enclosingClippingBox.shouldPlaceVerticalScrollbarOnLeft() && scrollableArea)
            scrollbarOffset = scrollableArea->verticalScrollbarWidth(IgnoreOverlayScrollbarSize);

        constrainingRect.setLocation(FloatPoint(scrollOffset.x() + scrollbarOffset, scrollOffset.y()));
        return constrainingRect;
    }
    
    return view().frameView().rectForFixedPositionLayout();
}

LayoutSize RenderBoxModelObject::stickyPositionOffset() const
{
    FloatRect constrainingRect = constrainingRectForStickyPosition();
    StickyPositionViewportConstraints constraints;
    computeStickyPositionConstraints(constraints, constrainingRect);
    
    // The sticky offset is physical, so we can just return the delta computed in absolute coords (though it may be wrong with transforms).
    return LayoutSize(constraints.computeStickyOffset(constrainingRect));
}

LayoutSize RenderBoxModelObject::offsetForInFlowPosition() const
{
    if (isRelativelyPositioned())
        return relativePositionOffset();

    if (isStickilyPositioned())
        return stickyPositionOffset();

    return LayoutSize();
}

LayoutUnit RenderBoxModelObject::offsetLeft() const
{
    // Note that RenderInline and RenderBox override this to pass a different
    // startPoint to adjustedPositionRelativeToOffsetParent.
    return adjustedPositionRelativeToOffsetParent(LayoutPoint()).x();
}

LayoutUnit RenderBoxModelObject::offsetTop() const
{
    // Note that RenderInline and RenderBox override this to pass a different
    // startPoint to adjustedPositionRelativeToOffsetParent.
    return adjustedPositionRelativeToOffsetParent(LayoutPoint()).y();
}

LayoutUnit RenderBoxModelObject::computedCSSPadding(const Length& padding) const
{
    LayoutUnit w;
    if (padding.isPercentOrCalculated())
        w = containingBlockLogicalWidthForContent();
    return minimumValueForLength(padding, w);
}

RoundedRect RenderBoxModelObject::getBackgroundRoundedRect(const LayoutRect& borderRect, const InlineIterator::InlineBoxIterator& box,
    bool includeLogicalLeftEdge, bool includeLogicalRightEdge) const
{
    RoundedRect border = style().getRoundedBorderFor(borderRect, includeLogicalLeftEdge, includeLogicalRightEdge);
    if (box && (box->nextInlineBox() || box->previousInlineBox())) {
        RoundedRect segmentBorder = style().getRoundedBorderFor(LayoutRect(0_lu, 0_lu, borderRect.width(), borderRect.height()), includeLogicalLeftEdge, includeLogicalRightEdge);
        border.setRadii(segmentBorder.radii());
    }
    return border;
}

void RenderBoxModelObject::clipRoundedInnerRect(GraphicsContext& context, const FloatRect& rect, const FloatRoundedRect& clipRect)
{
    if (clipRect.isRenderable())
        context.clipRoundedRect(clipRect);
    else {
        // We create a rounded rect for each of the corners and clip it, while making sure we clip opposing corners together.
        if (!clipRect.radii().topLeft().isEmpty() || !clipRect.radii().bottomRight().isEmpty()) {
            FloatRect topCorner(clipRect.rect().x(), clipRect.rect().y(), rect.maxX() - clipRect.rect().x(), rect.maxY() - clipRect.rect().y());
            FloatRoundedRect::Radii topCornerRadii;
            topCornerRadii.setTopLeft(clipRect.radii().topLeft());
            context.clipRoundedRect(FloatRoundedRect(topCorner, topCornerRadii));

            FloatRect bottomCorner(rect.x(), rect.y(), clipRect.rect().maxX() - rect.x(), clipRect.rect().maxY() - rect.y());
            FloatRoundedRect::Radii bottomCornerRadii;
            bottomCornerRadii.setBottomRight(clipRect.radii().bottomRight());
            context.clipRoundedRect(FloatRoundedRect(bottomCorner, bottomCornerRadii));
        } 

        if (!clipRect.radii().topRight().isEmpty() || !clipRect.radii().bottomLeft().isEmpty()) {
            FloatRect topCorner(rect.x(), clipRect.rect().y(), clipRect.rect().maxX() - rect.x(), rect.maxY() - clipRect.rect().y());
            FloatRoundedRect::Radii topCornerRadii;
            topCornerRadii.setTopRight(clipRect.radii().topRight());
            context.clipRoundedRect(FloatRoundedRect(topCorner, topCornerRadii));

            FloatRect bottomCorner(clipRect.rect().x(), rect.y(), rect.maxX() - clipRect.rect().x(), clipRect.rect().maxY() - rect.y());
            FloatRoundedRect::Radii bottomCornerRadii;
            bottomCornerRadii.setBottomLeft(clipRect.radii().bottomLeft());
            context.clipRoundedRect(FloatRoundedRect(bottomCorner, bottomCornerRadii));
        }
    }
}

static LayoutRect shrinkRectByOneDevicePixel(const GraphicsContext& context, const LayoutRect& rect, float devicePixelRatio)
{
    LayoutRect shrunkRect = rect;
    AffineTransform transform = context.getCTM();
    shrunkRect.inflateX(-ceilToDevicePixel(1_lu / transform.xScale(), devicePixelRatio));
    shrunkRect.inflateY(-ceilToDevicePixel(1_lu / transform.yScale(), devicePixelRatio));
    return shrunkRect;
}

LayoutRect RenderBoxModelObject::borderInnerRectAdjustedForBleedAvoidance(const GraphicsContext& context, const LayoutRect& rect, BackgroundBleedAvoidance bleedAvoidance) const
{
    if (bleedAvoidance != BackgroundBleedBackgroundOverBorder)
        return rect;

    // We shrink the rectangle by one device pixel on each side to make it fully overlap the anti-aliased background border
    return shrinkRectByOneDevicePixel(context, rect, document().deviceScaleFactor());
}

RoundedRect RenderBoxModelObject::backgroundRoundedRectAdjustedForBleedAvoidance(const GraphicsContext& context, const LayoutRect& borderRect, BackgroundBleedAvoidance bleedAvoidance, const InlineIterator::InlineBoxIterator& box, bool includeLogicalLeftEdge, bool includeLogicalRightEdge) const
{
    if (bleedAvoidance == BackgroundBleedShrinkBackground) {
        // We shrink the rectangle by one device pixel on each side because the bleed is one pixel maximum.
        return getBackgroundRoundedRect(shrinkRectByOneDevicePixel(context, borderRect, document().deviceScaleFactor()), box,
            includeLogicalLeftEdge, includeLogicalRightEdge);
    }
    if (bleedAvoidance == BackgroundBleedBackgroundOverBorder)
        return style().getRoundedInnerBorderFor(borderRect, includeLogicalLeftEdge, includeLogicalRightEdge);

    return getBackgroundRoundedRect(borderRect, box, includeLogicalLeftEdge, includeLogicalRightEdge);
}

static void applyBoxShadowForBackground(GraphicsContext& context, const RenderStyle& style)
{
    const ShadowData* boxShadow = style.boxShadow();
    while (boxShadow->style() != ShadowStyle::Normal)
        boxShadow = boxShadow->next();

    FloatSize shadowOffset(boxShadow->x().value(), boxShadow->y().value());
    context.setShadow(shadowOffset, boxShadow->radius().value(), style.colorByApplyingColorFilter(boxShadow->color()), boxShadow->isWebkitBoxShadow() ? ShadowRadiusMode::Legacy : ShadowRadiusMode::Default);
}

InterpolationQuality RenderBoxModelObject::chooseInterpolationQuality(GraphicsContext& context, Image& image, const void* layer, const LayoutSize& size)
{
    return view().imageQualityController().chooseInterpolationQuality(context, this, image, layer, size);
}

void RenderBoxModelObject::paintMaskForTextFillBox(ImageBuffer* maskImage, const FloatRect& maskRect, const InlineIterator::InlineBoxIterator& inlineBox, const LayoutRect& scrolledPaintRect)
{
    GraphicsContext& maskImageContext = maskImage->context();
    maskImageContext.translate(-maskRect.location());

    // Now add the text to the clip. We do this by painting using a special paint phase that signals to
    // the painter it should just modify the clip.
    PaintInfo maskInfo(maskImageContext, LayoutRect { maskRect }, PaintPhase::TextClip, PaintBehavior::ForceBlackText);
    if (inlineBox) {
        auto paintOffset = scrolledPaintRect.location() - toLayoutSize(LayoutPoint(inlineBox->rect().location()));

        for (auto box = inlineBox->firstLeafBox(), end = inlineBox->endLeafBox(); box != end; box.traverseNextOnLine()) {
            if (!box->isText())
                continue;
            TextBoxPainter textBoxPainter(downcast<InlineIterator::TextBoxIterator>(box), maskInfo, paintOffset);
            textBoxPainter.paint();
        }
        return;
    }

    LayoutSize localOffset = is<RenderBox>(*this) ? downcast<RenderBox>(*this).locationOffset() : LayoutSize();
    paint(maskInfo, scrolledPaintRect.location() - localOffset);
}

void RenderBoxModelObject::paintFillLayerExtended(const PaintInfo& paintInfo, const Color& color, const FillLayer& bgLayer, const LayoutRect& rect,
    BackgroundBleedAvoidance bleedAvoidance, const InlineIterator::InlineBoxIterator& box, const LayoutRect& backgroundImageStrip, CompositeOperator op, RenderElement* backgroundObject, BaseBackgroundColorUsage baseBgColorUsage)
{
    GraphicsContext& context = paintInfo.context();

    if ((context.paintingDisabled() && !context.detectingContentfulPaint()) || rect.isEmpty())
        return;

    auto [includeLeftEdge, includeRightEdge] = box ? box->hasClosedLeftAndRightEdge() : std::pair(true, true);

    bool hasRoundedBorder = style().hasBorderRadius() && (includeLeftEdge || includeRightEdge);
    bool clippedWithLocalScrolling = hasNonVisibleOverflow() && bgLayer.attachment() == FillAttachment::LocalBackground;
    bool isBorderFill = bgLayer.clip() == FillBox::Border;
    bool isRoot = this->isDocumentElementRenderer();

    Color bgColor = color;
    StyleImage* bgImage = bgLayer.image();
    bool shouldPaintBackgroundImage = bgImage && bgImage->canRender(this, style().effectiveZoom());
    
    if (context.detectingContentfulPaint()) {
        if (!context.contenfulPaintDetected() && shouldPaintBackgroundImage && bgImage->cachedImage()) {
            if (style().backgroundSizeType() != FillSizeType::Size || !style().backgroundSizeLength().isEmpty())
                context.setContentfulPaintDetected();
            return;
        }
    }

    if (context.invalidatingImagesWithAsyncDecodes()) {
        if (shouldPaintBackgroundImage && bgImage->cachedImage()->isClientWaitingForAsyncDecoding(*this))
            bgImage->cachedImage()->removeAllClientsWaitingForAsyncDecoding();
        return;
    }
    
    bool forceBackgroundToWhite = false;
    if (document().printing()) {
        if (style().printColorAdjust() == PrintColorAdjust::Economy)
            forceBackgroundToWhite = true;
        if (settings().shouldPrintBackgrounds())
            forceBackgroundToWhite = false;
    }

    // When printing backgrounds is disabled or using economy mode,
    // change existing background colors and images to a solid white background.
    // If there's no bg color or image, leave it untouched to avoid affecting transparency.
    // We don't try to avoid loading the background images, because this style flag is only set
    // when printing, and at that point we've already loaded the background images anyway. (To avoid
    // loading the background images we'd have to do this check when applying styles rather than
    // while rendering.)
    if (forceBackgroundToWhite) {
        // Note that we can't reuse this variable below because the bgColor might be changed
        bool shouldPaintBackgroundColor = !bgLayer.next() && bgColor.isVisible();
        if (shouldPaintBackgroundImage || shouldPaintBackgroundColor) {
            bgColor = Color::white;
            shouldPaintBackgroundImage = false;
        }
    }

    bool baseBgColorOnly = (baseBgColorUsage == BaseBackgroundColorOnly);
    if (baseBgColorOnly && (!isRoot || bgLayer.next() || bgColor.isOpaque()))
        return;

    bool colorVisible = bgColor.isVisible();
    float deviceScaleFactor = document().deviceScaleFactor();
    FloatRect pixelSnappedRect = snapRectToDevicePixels(rect, deviceScaleFactor);

    // Fast path for drawing simple color backgrounds.
    if (!isRoot && !clippedWithLocalScrolling && !shouldPaintBackgroundImage && isBorderFill && !bgLayer.next()) {
        if (!colorVisible)
            return;

        bool boxShadowShouldBeAppliedToBackground = this->boxShadowShouldBeAppliedToBackground(rect.location(), bleedAvoidance, box);
        GraphicsContextStateSaver shadowStateSaver(context, boxShadowShouldBeAppliedToBackground);
        if (boxShadowShouldBeAppliedToBackground)
            applyBoxShadowForBackground(context, style());

        if (hasRoundedBorder && bleedAvoidance != BackgroundBleedUseTransparencyLayer) {
            FloatRoundedRect pixelSnappedBorder = backgroundRoundedRectAdjustedForBleedAvoidance(context, rect, bleedAvoidance, box,
                includeLeftEdge, includeRightEdge).pixelSnappedRoundedRectForPainting(deviceScaleFactor);
            if (pixelSnappedBorder.isRenderable()) {
                CompositeOperator previousOperator = context.compositeOperation();
                bool saveRestoreCompositeOp = op != previousOperator;
                if (saveRestoreCompositeOp)
                    context.setCompositeOperation(op);

                context.fillRoundedRect(pixelSnappedBorder, bgColor);

                if (saveRestoreCompositeOp)
                    context.setCompositeOperation(previousOperator);
            } else {
                context.save();
                clipRoundedInnerRect(context, pixelSnappedRect, pixelSnappedBorder);
                context.fillRect(pixelSnappedBorder.rect(), bgColor, op);
                context.restore();
            }
        } else
            context.fillRect(pixelSnappedRect, bgColor, op);

        return;
    }

    // FillBox::Border radius clipping is taken care of by BackgroundBleedUseTransparencyLayer
    bool clipToBorderRadius = hasRoundedBorder && !(isBorderFill && bleedAvoidance == BackgroundBleedUseTransparencyLayer);
    GraphicsContextStateSaver clipToBorderStateSaver(context, clipToBorderRadius);
    if (clipToBorderRadius) {
        RoundedRect border = isBorderFill ? backgroundRoundedRectAdjustedForBleedAvoidance(context, rect, bleedAvoidance, box, includeLeftEdge, includeRightEdge) : getBackgroundRoundedRect(rect, box, includeLeftEdge, includeRightEdge);

        // Clip to the padding or content boxes as necessary.
        if (bgLayer.clip() == FillBox::Content) {
            border = style().getRoundedInnerBorderFor(border.rect(),
                paddingTop() + borderTop(), paddingBottom() + borderBottom(), paddingLeft() + borderLeft(), paddingRight() + borderRight(), includeLeftEdge, includeRightEdge);
        } else if (bgLayer.clip() == FillBox::Padding)
            border = style().getRoundedInnerBorderFor(border.rect(), includeLeftEdge, includeRightEdge);

        clipRoundedInnerRect(context, pixelSnappedRect, border.pixelSnappedRoundedRectForPainting(deviceScaleFactor));
    }
    
    LayoutUnit bLeft = includeLeftEdge ? borderLeft() : 0_lu;
    LayoutUnit bRight = includeRightEdge ? borderRight() : 0_lu;
    LayoutUnit pLeft = includeLeftEdge ? paddingLeft() : 0_lu;
    LayoutUnit pRight = includeRightEdge ? paddingRight() : 0_lu;

    GraphicsContextStateSaver clipWithScrollingStateSaver(context, clippedWithLocalScrolling);
    LayoutRect scrolledPaintRect = rect;
    if (clippedWithLocalScrolling) {
        // Clip to the overflow area.
        auto& thisBox = downcast<RenderBox>(*this);
        context.clip(thisBox.overflowClipRect(rect.location()));
        
        // Adjust the paint rect to reflect a scrolled content box with borders at the ends.
        scrolledPaintRect.moveBy(-thisBox.scrollPosition());
        scrolledPaintRect.setWidth(bLeft + layer()->scrollWidth() + bRight);
        scrolledPaintRect.setHeight(borderTop() + layer()->scrollHeight() + borderBottom());
    }
    
    GraphicsContextStateSaver backgroundClipStateSaver(context, false);
    RefPtr<ImageBuffer> maskImage;
    FloatRect maskRect;

    if (bgLayer.clip() == FillBox::Padding || bgLayer.clip() == FillBox::Content) {
        // Clip to the padding or content boxes as necessary.
        if (!clipToBorderRadius) {
            bool includePadding = bgLayer.clip() == FillBox::Content;
            LayoutRect clipRect = LayoutRect(scrolledPaintRect.x() + bLeft + (includePadding ? pLeft : 0_lu),
                scrolledPaintRect.y() + borderTop() + (includePadding ? paddingTop() : 0_lu),
                scrolledPaintRect.width() - bLeft - bRight - (includePadding ? pLeft + pRight : 0_lu),
                scrolledPaintRect.height() - borderTop() - borderBottom() - (includePadding ? paddingTop() + paddingBottom() : 0_lu));
            backgroundClipStateSaver.save();
            context.clip(clipRect);
        }
    } else if (bgLayer.clip() == FillBox::Text) {
        // We have to draw our text into a mask that can then be used to clip background drawing.
        // First figure out how big the mask has to be.  It should be no bigger than what we need
        // to actually render, so we should intersect the dirty rect with the border box of the background.
        maskRect = snapRectToDevicePixels(rect, deviceScaleFactor);
        maskRect.intersect(snapRectToDevicePixels(paintInfo.rect, deviceScaleFactor));

        // Now create the mask.
        maskImage = context.createCompatibleImageBuffer(maskRect.size());
        if (!maskImage)
            return;
        paintMaskForTextFillBox(maskImage.get(), maskRect, box, scrolledPaintRect);

        // The mask has been created.  Now we just need to clip to it.
        backgroundClipStateSaver.save();
        context.clip(maskRect);
        context.beginTransparencyLayer(1);
    }

    auto isOpaqueRoot = false;
    if (isRoot) {
        isOpaqueRoot = bgLayer.next() || bgColor.isOpaque() || view().shouldPaintBaseBackground();
        view().frameView().setContentIsOpaque(isOpaqueRoot);
    }

    // Paint the color first underneath all images, culled if background image occludes it.
    // FIXME: In the bgLayer.hasFiniteBounds() case, we could improve the culling test
    // by verifying whether the background image covers the entire layout rect.
    if (!bgLayer.next()) {
        LayoutRect backgroundRect(scrolledPaintRect);
        bool boxShadowShouldBeAppliedToBackground = this->boxShadowShouldBeAppliedToBackground(rect.location(), bleedAvoidance, box);
        if (boxShadowShouldBeAppliedToBackground || !shouldPaintBackgroundImage || !bgLayer.hasOpaqueImage(*this) || !bgLayer.hasRepeatXY() || bgLayer.isEmpty()) {
            if (!boxShadowShouldBeAppliedToBackground)
                backgroundRect.intersect(paintInfo.rect);

            // If we have an alpha and we are painting the root element, blend with the base background color.
            Color baseColor;
            bool shouldClearBackground = false;
            if ((baseBgColorUsage != BaseBackgroundColorSkip) && isOpaqueRoot) {
                baseColor = view().frameView().baseBackgroundColor();
                if (!baseColor.isVisible())
                    shouldClearBackground = true;
            }

            GraphicsContextStateSaver shadowStateSaver(context, boxShadowShouldBeAppliedToBackground);
            if (boxShadowShouldBeAppliedToBackground)
                applyBoxShadowForBackground(context, style());

            FloatRect backgroundRectForPainting = snapRectToDevicePixels(backgroundRect, deviceScaleFactor);
            if (baseColor.isVisible()) {
                if (!baseBgColorOnly && bgColor.isVisible())
                    baseColor = blendSourceOver(baseColor, bgColor);
                context.fillRect(backgroundRectForPainting, baseColor, CompositeOperator::Copy);
            } else if (!baseBgColorOnly && bgColor.isVisible()) {
                auto operation = context.compositeOperation();
                if (shouldClearBackground) {
                    if (op == CompositeOperator::DestinationOut) // We're punching out the background.
                        operation = op;
                    else
                        operation = CompositeOperator::Copy;
                }
                context.fillRect(backgroundRectForPainting, bgColor, operation);
            } else if (shouldClearBackground)
                context.clearRect(backgroundRectForPainting);
        }
    }

    // no progressive loading of the background image
    if (!baseBgColorOnly && shouldPaintBackgroundImage) {
        // Multiline inline boxes paint like the image was one long strip spanning lines. The backgroundImageStrip is this fictional rectangle.
        auto imageRect = backgroundImageStrip.isEmpty() ? scrolledPaintRect : backgroundImageStrip;
        auto paintOffset = backgroundImageStrip.isEmpty() ? rect.location() : backgroundImageStrip.location();
        auto geometry = calculateBackgroundImageGeometry(paintInfo.paintContainer, bgLayer, paintOffset, imageRect, backgroundObject);
        geometry.clip(LayoutRect(pixelSnappedRect));
        RefPtr<Image> image;
        if (!geometry.destRect().isEmpty() && (image = bgImage->image(backgroundObject ? backgroundObject : this, geometry.tileSize()))) {
            context.setDrawLuminanceMask(bgLayer.maskMode() == MaskMode::Luminance);

            if (is<BitmapImage>(image))
                downcast<BitmapImage>(*image).updateFromSettings(settings());

            ImagePaintingOptions options = {
                op == CompositeOperator::SourceOver ? bgLayer.composite() : op,
                bgLayer.blendMode(),
                decodingModeForImageDraw(*image, paintInfo),
                ImageOrientation::FromImage,
                chooseInterpolationQuality(context, *image, &bgLayer, geometry.tileSize())
            };

            auto drawResult = context.drawTiledImage(*image, geometry.destRect(), toLayoutPoint(geometry.relativePhase()), geometry.tileSize(), geometry.spaceSize(), options);
            if (drawResult == ImageDrawResult::DidRequestDecoding) {
                ASSERT(bgImage->hasCachedImage());
                bgImage->cachedImage()->addClientWaitingForAsyncDecoding(*this);
            }
        }
    }

    if (maskImage && bgLayer.clip() == FillBox::Text) {
        context.drawConsumingImageBuffer(WTFMove(maskImage), maskRect, CompositeOperator::DestinationIn);
        context.endTransparencyLayer();
    }
}

static inline LayoutUnit resolveWidthForRatio(LayoutUnit height, const LayoutSize& intrinsicRatio)
{
    return height * intrinsicRatio.width() / intrinsicRatio.height();
}

static inline LayoutUnit resolveHeightForRatio(LayoutUnit width, const LayoutSize& intrinsicRatio)
{
    return width * intrinsicRatio.height() / intrinsicRatio.width();
}

static inline LayoutSize resolveAgainstIntrinsicWidthOrHeightAndRatio(const LayoutSize& size, const LayoutSize& intrinsicRatio, LayoutUnit useWidth, LayoutUnit useHeight)
{
    if (intrinsicRatio.isEmpty()) {
        if (useWidth)
            return LayoutSize(useWidth, size.height());
        return LayoutSize(size.width(), useHeight);
    }

    if (useWidth)
        return LayoutSize(useWidth, resolveHeightForRatio(useWidth, intrinsicRatio));
    return LayoutSize(resolveWidthForRatio(useHeight, intrinsicRatio), useHeight);
}

static inline LayoutSize resolveAgainstIntrinsicRatio(const LayoutSize& size, const LayoutSize& intrinsicRatio)
{
    // Two possible solutions: (size.width(), solutionHeight) or (solutionWidth, size.height())
    // "... must be assumed to be the largest dimensions..." = easiest answer: the rect with the largest surface area.

    LayoutUnit solutionWidth = resolveWidthForRatio(size.height(), intrinsicRatio);
    LayoutUnit solutionHeight = resolveHeightForRatio(size.width(), intrinsicRatio);
    if (solutionWidth <= size.width()) {
        if (solutionHeight <= size.height()) {
            // If both solutions fit, choose the one covering the larger area.
            LayoutUnit areaOne = solutionWidth * size.height();
            LayoutUnit areaTwo = size.width() * solutionHeight;
            if (areaOne < areaTwo)
                return LayoutSize(size.width(), solutionHeight);
            return LayoutSize(solutionWidth, size.height());
        }

        // Only the first solution fits.
        return LayoutSize(solutionWidth, size.height());
    }

    // Only the second solution fits, assert that.
    ASSERT(solutionHeight <= size.height());
    return LayoutSize(size.width(), solutionHeight);
}

LayoutSize RenderBoxModelObject::calculateImageIntrinsicDimensions(StyleImage* image, const LayoutSize& positioningAreaSize, ScaleByEffectiveZoomOrNot shouldScaleOrNot) const
{
    // A generated image without a fixed size, will always return the container size as intrinsic size.
    if (image->isGeneratedImage() && image->usesImageContainerSize())
        return LayoutSize(positioningAreaSize.width(), positioningAreaSize.height());

    Length intrinsicWidth;
    Length intrinsicHeight;
    FloatSize intrinsicRatio;
    image->computeIntrinsicDimensions(this, intrinsicWidth, intrinsicHeight, intrinsicRatio);

    ASSERT(!intrinsicWidth.isPercentOrCalculated());
    ASSERT(!intrinsicHeight.isPercentOrCalculated());

    LayoutSize resolvedSize(intrinsicWidth.value(), intrinsicHeight.value());
    LayoutSize minimumSize(resolvedSize.width() > 0 ? 1 : 0, resolvedSize.height() > 0 ? 1 : 0);

    if (shouldScaleOrNot == ScaleByEffectiveZoom)
        resolvedSize.scale(style().effectiveZoom());
    resolvedSize.clampToMinimumSize(minimumSize);

    if (!resolvedSize.isEmpty())
        return resolvedSize;

    // If the image has one of either an intrinsic width or an intrinsic height:
    // * and an intrinsic aspect ratio, then the missing dimension is calculated from the given dimension and the ratio.
    // * and no intrinsic aspect ratio, then the missing dimension is assumed to be the size of the rectangle that
    //   establishes the coordinate system for the 'background-position' property.
    if (resolvedSize.width() > 0 || resolvedSize.height() > 0)
        return resolveAgainstIntrinsicWidthOrHeightAndRatio(positioningAreaSize, LayoutSize(intrinsicRatio), resolvedSize.width(), resolvedSize.height());

    // If the image has no intrinsic dimensions and has an intrinsic ratio the dimensions must be assumed to be the
    // largest dimensions at that ratio such that neither dimension exceeds the dimensions of the rectangle that
    // establishes the coordinate system for the 'background-position' property.
    if (!intrinsicRatio.isEmpty())
        return resolveAgainstIntrinsicRatio(positioningAreaSize, LayoutSize(intrinsicRatio));

    // If the image has no intrinsic ratio either, then the dimensions must be assumed to be the rectangle that
    // establishes the coordinate system for the 'background-position' property.
    return positioningAreaSize;
}

LayoutSize RenderBoxModelObject::calculateFillTileSize(const FillLayer& fillLayer, const LayoutSize& positioningAreaSize) const
{
    StyleImage* image = fillLayer.image();
    FillSizeType type = fillLayer.size().type;
    auto devicePixelSize = LayoutUnit { 1.0 / document().deviceScaleFactor() };

    LayoutSize imageIntrinsicSize;
    if (image) {
        imageIntrinsicSize = calculateImageIntrinsicDimensions(image, positioningAreaSize, ScaleByEffectiveZoom);
        imageIntrinsicSize.scale(1 / image->imageScaleFactor(), 1 / image->imageScaleFactor());
    } else
        imageIntrinsicSize = positioningAreaSize;

    switch (type) {
    case FillSizeType::Size: {
        LayoutSize tileSize = positioningAreaSize;

        Length layerWidth = fillLayer.size().size.width;
        Length layerHeight = fillLayer.size().size.height;

        if (layerWidth.isFixed())
            tileSize.setWidth(layerWidth.value());
        else if (layerWidth.isPercentOrCalculated()) {
            auto resolvedWidth = valueForLength(layerWidth, positioningAreaSize.width());
            // Non-zero resolved value should always produce some content.
            tileSize.setWidth(!resolvedWidth ? resolvedWidth : std::max(devicePixelSize, resolvedWidth));
        }
        
        if (layerHeight.isFixed())
            tileSize.setHeight(layerHeight.value());
        else if (layerHeight.isPercentOrCalculated()) {
            auto resolvedHeight = valueForLength(layerHeight, positioningAreaSize.height());
            // Non-zero resolved value should always produce some content.
            tileSize.setHeight(!resolvedHeight ? resolvedHeight : std::max(devicePixelSize, resolvedHeight));
        }

        // If one of the values is auto we have to use the appropriate
        // scale to maintain our aspect ratio.
        if (layerWidth.isAuto() && !layerHeight.isAuto()) {
            if (imageIntrinsicSize.height())
                tileSize.setWidth(imageIntrinsicSize.width() * tileSize.height() / imageIntrinsicSize.height());
        } else if (!layerWidth.isAuto() && layerHeight.isAuto()) {
            if (imageIntrinsicSize.width())
                tileSize.setHeight(imageIntrinsicSize.height() * tileSize.width() / imageIntrinsicSize.width());
        } else if (layerWidth.isAuto() && layerHeight.isAuto()) {
            // If both width and height are auto, use the image's intrinsic size.
            tileSize = imageIntrinsicSize;
        }

        tileSize.clampNegativeToZero();
        return tileSize;
    }
    case FillSizeType::None: {
        // If both values are ‘auto’ then the intrinsic width and/or height of the image should be used, if any.
        if (!imageIntrinsicSize.isEmpty())
            return imageIntrinsicSize;

        // If the image has neither an intrinsic width nor an intrinsic height, its size is determined as for ‘contain’.
        type = FillSizeType::Contain;
    }
    FALLTHROUGH;
    case FillSizeType::Contain:
    case FillSizeType::Cover: {
        // Scale computation needs higher precision than what LayoutUnit can offer.
        FloatSize localImageIntrinsicSize = imageIntrinsicSize;
        FloatSize localPositioningAreaSize = positioningAreaSize;

        float horizontalScaleFactor = localImageIntrinsicSize.width() ? (localPositioningAreaSize.width() / localImageIntrinsicSize.width()) : 1;
        float verticalScaleFactor = localImageIntrinsicSize.height() ? (localPositioningAreaSize.height() / localImageIntrinsicSize.height()) : 1;
        float scaleFactor = type == FillSizeType::Contain ? std::min(horizontalScaleFactor, verticalScaleFactor) : std::max(horizontalScaleFactor, verticalScaleFactor);
        
        if (localImageIntrinsicSize.isEmpty())
            return { };
        
        return LayoutSize(localImageIntrinsicSize.scaled(scaleFactor).expandedTo({ devicePixelSize, devicePixelSize }));
    }
    }

    ASSERT_NOT_REACHED();
    return { };
}

static void pixelSnapBackgroundImageGeometryForPainting(LayoutRect& destinationRect, LayoutSize& tileSize, LayoutSize& phase, LayoutSize& space, float scaleFactor)
{
    tileSize = LayoutSize(snapRectToDevicePixels(LayoutRect(destinationRect.location(), tileSize), scaleFactor).size());
    phase = LayoutSize(snapRectToDevicePixels(LayoutRect(destinationRect.location(), phase), scaleFactor).size());
    space = LayoutSize(snapRectToDevicePixels(LayoutRect(LayoutPoint(), space), scaleFactor).size());
    destinationRect = LayoutRect(snapRectToDevicePixels(destinationRect, scaleFactor));
}

bool RenderBoxModelObject::fixedBackgroundPaintsInLocalCoordinates() const
{
    if (!isDocumentElementRenderer())
        return false;

    if (view().frameView().paintBehavior().contains(PaintBehavior::FlattenCompositingLayers))
        return false;

    RenderLayer* rootLayer = view().layer();
    if (!rootLayer || !rootLayer->isComposited())
        return false;

    return rootLayer->backing()->backgroundLayerPaintsFixedRootBackground();
}

static inline LayoutUnit getSpace(LayoutUnit areaSize, LayoutUnit tileSize)
{
    int numberOfTiles = areaSize / tileSize;
    LayoutUnit space = -1;

    if (numberOfTiles > 1)
        space = (areaSize - numberOfTiles * tileSize) / (numberOfTiles - 1);

    return space;
}

static LayoutUnit resolveEdgeRelativeLength(const Length& length, Edge edge, LayoutUnit availableSpace, const LayoutSize& areaSize, const LayoutSize& tileSize)
{
    LayoutUnit result = minimumValueForLength(length, availableSpace);

    if (edge == Edge::Right)
        return areaSize.width() - tileSize.width() - result;
    
    if (edge == Edge::Bottom)
        return areaSize.height() - tileSize.height() - result;

    return result;
}

BackgroundImageGeometry RenderBoxModelObject::calculateBackgroundImageGeometry(const RenderLayerModelObject* paintContainer, const FillLayer& fillLayer, const LayoutPoint& paintOffset,
    const LayoutRect& borderBoxRect, RenderElement* backgroundObject) const
{
    LayoutUnit left;
    LayoutUnit top;
    LayoutSize positioningAreaSize;
    // Determine the background positioning area and set destination rect to the background painting area.
    // Destination rect will be adjusted later if the background is non-repeating.
    // FIXME: transforms spec says that fixed backgrounds behave like scroll inside transforms. https://bugs.webkit.org/show_bug.cgi?id=15679
    LayoutRect destinationRect(borderBoxRect);
    bool fixedAttachment = fillLayer.attachment() == FillAttachment::FixedBackground;
    float deviceScaleFactor = document().deviceScaleFactor();
    if (!fixedAttachment) {
        LayoutUnit right;
        LayoutUnit bottom;
        // Scroll and Local.
        if (fillLayer.origin() != FillBox::Border) {
            left = borderLeft();
            right = borderRight();
            top = borderTop();
            bottom = borderBottom();
            if (fillLayer.origin() == FillBox::Content) {
                left += paddingLeft();
                right += paddingRight();
                top += paddingTop();
                bottom += paddingBottom();
            }
        }

        // The background of the box generated by the root element covers the entire canvas including
        // its margins. Since those were added in already, we have to factor them out when computing
        // the background positioning area.
        if (isDocumentElementRenderer()) {
            positioningAreaSize = downcast<RenderBox>(*this).size() - LayoutSize(left + right, top + bottom);
            positioningAreaSize = LayoutSize(snapSizeToDevicePixel(positioningAreaSize, LayoutPoint(), deviceScaleFactor));
            if (view().frameView().hasExtendedBackgroundRectForPainting()) {
                LayoutRect extendedBackgroundRect = view().frameView().extendedBackgroundRectForPainting();
                left += (marginLeft() - extendedBackgroundRect.x());
                top += (marginTop() - extendedBackgroundRect.y());
            }
        } else {
            positioningAreaSize = borderBoxRect.size() - LayoutSize(left + right, top + bottom);
            positioningAreaSize = LayoutSize(snapRectToDevicePixels(LayoutRect(paintOffset, positioningAreaSize), deviceScaleFactor).size());
        }
    } else {
        LayoutRect viewportRect;
        float topContentInset = 0;
        if (settings().fixedBackgroundsPaintRelativeToDocument())
            viewportRect = view().unscaledDocumentRect();
        else {
            FrameView& frameView = view().frameView();
            bool useFixedLayout = frameView.useFixedLayout() && !frameView.fixedLayoutSize().isEmpty();

            if (useFixedLayout) {
                // Use the fixedLayoutSize() when useFixedLayout() because the rendering will scale
                // down the frameView to to fit in the current viewport.
                viewportRect.setSize(frameView.fixedLayoutSize());
            } else
                viewportRect.setSize(frameView.sizeForVisibleContent());

            if (fixedBackgroundPaintsInLocalCoordinates()) {
                if (!useFixedLayout) {
                    // Shifting location up by topContentInset is needed for layout tests which expect
                    // layout to be shifted down when calling window.internals.setTopContentInset().
                    topContentInset = frameView.topContentInset(ScrollView::TopContentInsetType::WebCoreOrPlatformContentInset);
                    viewportRect.setLocation(LayoutPoint(0, -topContentInset));
                }
            } else if (useFixedLayout || frameView.frameScaleFactor() != 1) {
                // scrollPositionForFixedPosition() is adjusted for page scale and it does not include
                // topContentInset so do not add it to the calculation below.
                viewportRect.setLocation(frameView.scrollPositionForFixedPosition());
            } else {
                // documentScrollPositionRelativeToViewOrigin() includes -topContentInset in its height
                // so we need to account for that in calculating the phase size
                topContentInset = frameView.topContentInset(ScrollView::TopContentInsetType::WebCoreOrPlatformContentInset);
                viewportRect.setLocation(frameView.documentScrollPositionRelativeToViewOrigin());
            }

            top += topContentInset;
        }
        
        if (paintContainer)
            viewportRect.moveBy(LayoutPoint(-paintContainer->localToAbsolute(FloatPoint())));

        destinationRect = viewportRect;
        positioningAreaSize = destinationRect.size();
        positioningAreaSize.setHeight(positioningAreaSize.height() - topContentInset);
        positioningAreaSize = LayoutSize(snapRectToDevicePixels(LayoutRect(destinationRect.location(), positioningAreaSize), deviceScaleFactor).size());
    }

    auto clientForBackgroundImage = backgroundObject ? backgroundObject : this;
    LayoutSize tileSize = calculateFillTileSize(fillLayer, positioningAreaSize);
    if (StyleImage* layerImage = fillLayer.image())
        layerImage->setContainerContextForRenderer(*clientForBackgroundImage, tileSize, style().effectiveZoom());
    
    FillRepeat backgroundRepeatX = fillLayer.repeatX();
    FillRepeat backgroundRepeatY = fillLayer.repeatY();
    LayoutUnit availableWidth = positioningAreaSize.width() - tileSize.width();
    LayoutUnit availableHeight = positioningAreaSize.height() - tileSize.height();

    LayoutSize spaceSize;
    LayoutSize phase;
    LayoutSize noRepeat;
    LayoutUnit computedXPosition = resolveEdgeRelativeLength(fillLayer.xPosition(), fillLayer.backgroundXOrigin(), availableWidth, positioningAreaSize, tileSize);
    if (backgroundRepeatX == FillRepeat::Round && positioningAreaSize.width() > 0 && tileSize.width() > 0) {
        int numTiles = std::max(1, roundToInt(positioningAreaSize.width() / tileSize.width()));
        if (fillLayer.size().size.height.isAuto() && backgroundRepeatY != FillRepeat::Round)
            tileSize.setHeight(tileSize.height() * positioningAreaSize.width() / (numTiles * tileSize.width()));

        tileSize.setWidth(positioningAreaSize.width() / numTiles);
        phase.setWidth(tileSize.width() ? tileSize.width() - fmodf((computedXPosition + left), tileSize.width()) : 0);
    }

    LayoutUnit computedYPosition = resolveEdgeRelativeLength(fillLayer.yPosition(), fillLayer.backgroundYOrigin(), availableHeight, positioningAreaSize, tileSize);
    if (backgroundRepeatY == FillRepeat::Round && positioningAreaSize.height() > 0 && tileSize.height() > 0) {
        int numTiles = std::max(1, roundToInt(positioningAreaSize.height() / tileSize.height()));
        if (fillLayer.size().size.width.isAuto() && backgroundRepeatX != FillRepeat::Round)
            tileSize.setWidth(tileSize.width() * positioningAreaSize.height() / (numTiles * tileSize.height()));

        tileSize.setHeight(positioningAreaSize.height() / numTiles);
        phase.setHeight(tileSize.height() ? tileSize.height() - fmodf((computedYPosition + top), tileSize.height()) : 0);
    }

    if (backgroundRepeatX == FillRepeat::Repeat) {
        phase.setWidth(tileSize.width() ? tileSize.width() - fmodf(computedXPosition + left, tileSize.width()) : 0);
        spaceSize.setWidth(0);
    } else if (backgroundRepeatX == FillRepeat::Space && tileSize.width() > 0) {
        LayoutUnit space = getSpace(positioningAreaSize.width(), tileSize.width());
        if (space >= 0) {
            LayoutUnit actualWidth = tileSize.width() + space;
            computedXPosition = minimumValueForLength(Length(), availableWidth);
            spaceSize.setWidth(space);
            spaceSize.setHeight(0);
            phase.setWidth(actualWidth ? actualWidth - fmodf((computedXPosition + left), actualWidth) : 0);
        } else
            backgroundRepeatX = FillRepeat::NoRepeat;
    }

    if (backgroundRepeatX == FillRepeat::NoRepeat) {
        LayoutUnit xOffset = left + computedXPosition;
        if (xOffset > 0)
            destinationRect.move(xOffset, 0_lu);
        xOffset = std::min<LayoutUnit>(xOffset, 0);
        phase.setWidth(-xOffset);
        destinationRect.setWidth(tileSize.width() + xOffset);
        spaceSize.setWidth(0);
    }

    if (backgroundRepeatY == FillRepeat::Repeat) {
        phase.setHeight(tileSize.height() ? tileSize.height() - fmodf(computedYPosition + top, tileSize.height()) : 0);
        spaceSize.setHeight(0);
    } else if (backgroundRepeatY == FillRepeat::Space && tileSize.height() > 0) {
        LayoutUnit space = getSpace(positioningAreaSize.height(), tileSize.height());

        if (space >= 0) {
            LayoutUnit actualHeight = tileSize.height() + space;
            computedYPosition = minimumValueForLength(Length(), availableHeight);
            spaceSize.setHeight(space);
            phase.setHeight(actualHeight ? actualHeight - fmodf((computedYPosition + top), actualHeight) : 0);
        } else
            backgroundRepeatY = FillRepeat::NoRepeat;
    }
    if (backgroundRepeatY == FillRepeat::NoRepeat) {
        LayoutUnit yOffset = top + computedYPosition;
        if (yOffset > 0)
            destinationRect.move(0_lu, yOffset);
        yOffset = std::min<LayoutUnit>(yOffset, 0);
        phase.setHeight(-yOffset);
        destinationRect.setHeight(tileSize.height() + yOffset);
        spaceSize.setHeight(0);
    }

    if (fixedAttachment) {
        LayoutPoint attachmentPoint = borderBoxRect.location();
        phase.expand(std::max<LayoutUnit>(attachmentPoint.x() - destinationRect.x(), 0), std::max<LayoutUnit>(attachmentPoint.y() - destinationRect.y(), 0));
    }

    destinationRect.intersect(borderBoxRect);
    pixelSnapBackgroundImageGeometryForPainting(destinationRect, tileSize, phase, spaceSize, deviceScaleFactor);
    return BackgroundImageGeometry(destinationRect, tileSize, phase, spaceSize, fixedAttachment);
}

void RenderBoxModelObject::getGeometryForBackgroundImage(const RenderLayerModelObject* paintContainer, const LayoutPoint& paintOffset, FloatRect& destRect, FloatSize& phase, FloatSize& tileSize) const
{
    LayoutRect paintRect(destRect);
    auto geometry = calculateBackgroundImageGeometry(paintContainer, style().backgroundLayers(), paintOffset, paintRect);
    phase = geometry.phase();
    tileSize = geometry.tileSize();
    destRect = geometry.destRect();
}

bool RenderBoxModelObject::paintNinePieceImage(GraphicsContext& graphicsContext, const LayoutRect& rect, const RenderStyle& style,
                                               const NinePieceImage& ninePieceImage, CompositeOperator op)
{
    StyleImage* styleImage = ninePieceImage.image();
    if (!styleImage)
        return false;

    if (!styleImage->isLoaded())
        return true; // Never paint a nine-piece image incrementally, but don't paint the fallback borders either.

    if (!styleImage->canRender(this, style.effectiveZoom()))
        return false;

    // FIXME: border-image is broken with full page zooming when tiling has to happen, since the tiling function
    // doesn't have any understanding of the zoom that is in effect on the tile.
    float deviceScaleFactor = document().deviceScaleFactor();

    LayoutRect rectWithOutsets = rect;
    rectWithOutsets.expand(style.imageOutsets(ninePieceImage));
    LayoutRect destination = LayoutRect(snapRectToDevicePixels(rectWithOutsets, deviceScaleFactor));

    LayoutSize source = calculateImageIntrinsicDimensions(styleImage, destination.size(), DoNotScaleByEffectiveZoom);

    // If both values are ‘auto’ then the intrinsic width and/or height of the image should be used, if any.
    styleImage->setContainerContextForRenderer(*this, source, style.effectiveZoom());

    ninePieceImage.paint(graphicsContext, this, style, destination, source, deviceScaleFactor, op);
    return true;
}

static bool allCornersClippedOut(const RoundedRect& border, const LayoutRect& clipRect)
{
    LayoutRect boundingRect = border.rect();
    if (clipRect.contains(boundingRect))
        return false;

    RoundedRect::Radii radii = border.radii();

    LayoutRect topLeftRect(boundingRect.location(), radii.topLeft());
    if (clipRect.intersects(topLeftRect))
        return false;

    LayoutRect topRightRect(boundingRect.location(), radii.topRight());
    topRightRect.setX(boundingRect.maxX() - topRightRect.width());
    if (clipRect.intersects(topRightRect))
        return false;

    LayoutRect bottomLeftRect(boundingRect.location(), radii.bottomLeft());
    bottomLeftRect.setY(boundingRect.maxY() - bottomLeftRect.height());
    if (clipRect.intersects(bottomLeftRect))
        return false;

    LayoutRect bottomRightRect(boundingRect.location(), radii.bottomRight());
    bottomRightRect.setX(boundingRect.maxX() - bottomRightRect.width());
    bottomRightRect.setY(boundingRect.maxY() - bottomRightRect.height());
    if (clipRect.intersects(bottomRightRect))
        return false;

    return true;
}

static bool borderWillArcInnerEdge(const LayoutSize& firstRadius, const LayoutSize& secondRadius)
{
    return !firstRadius.isZero() || !secondRadius.isZero();
}

inline bool styleRequiresClipPolygon(BorderStyle style)
{
    return style == BorderStyle::Dotted || style == BorderStyle::Dashed; // These are drawn with a stroke, so we have to clip to get corner miters.
}

static bool borderStyleFillsBorderArea(BorderStyle style)
{
    return !(style == BorderStyle::Dotted || style == BorderStyle::Dashed || style == BorderStyle::Double);
}

static bool borderStyleHasInnerDetail(BorderStyle style)
{
    return style == BorderStyle::Groove || style == BorderStyle::Ridge || style == BorderStyle::Double;
}

static bool borderStyleIsDottedOrDashed(BorderStyle style)
{
    return style == BorderStyle::Dotted || style == BorderStyle::Dashed;
}

// BorderStyle::Outset darkens the bottom and right (and maybe lightens the top and left)
// BorderStyle::Inset darkens the top and left (and maybe lightens the bottom and right)
static inline bool borderStyleHasUnmatchedColorsAtCorner(BorderStyle style, BoxSide side, BoxSide adjacentSide)
{
    // These styles match at the top/left and bottom/right.
    if (style == BorderStyle::Inset || style == BorderStyle::Groove || style == BorderStyle::Ridge || style == BorderStyle::Outset) {
        BoxSideSet topRightSides = { BoxSideFlag::Top, BoxSideFlag::Right };
        BoxSideSet bottomLeftSides = { BoxSideFlag::Bottom, BoxSideFlag::Left };

        BoxSideSet usedSides { edgeFlagForSide(side), edgeFlagForSide(adjacentSide) };
        return usedSides == topRightSides || usedSides == bottomLeftSides;
    }
    return false;
}

static inline bool colorsMatchAtCorner(BoxSide side, BoxSide adjacentSide, const BorderEdges& edges)
{
    auto& edge = edges.at(side);
    auto& adjacentEdge = edges.at(adjacentSide);

    if (edge.shouldRender() != adjacentEdge.shouldRender())
        return false;

    if (!edgesShareColor(edge, adjacentEdge))
        return false;

    return !borderStyleHasUnmatchedColorsAtCorner(edge.style(), side, adjacentSide);
}


static inline bool colorNeedsAntiAliasAtCorner(BoxSide side, BoxSide adjacentSide, const BorderEdges& edges)
{
    auto& edge = edges.at(side);
    auto& adjacentEdge = edges.at(adjacentSide);

    if (edge.color().isOpaque())
        return false;

    if (edge.shouldRender() != adjacentEdge.shouldRender())
        return false;

    if (!edgesShareColor(edge, adjacentEdge))
        return true;

    return borderStyleHasUnmatchedColorsAtCorner(edge.style(), side, adjacentSide);
}

// This assumes that we draw in order: top, bottom, left, right.
static inline bool willBeOverdrawn(BoxSide side, BoxSide adjacentSide, const BorderEdges& edges)
{
    switch (side) {
    case BoxSide::Top:
    case BoxSide::Bottom: {
        auto& edge = edges.at(side);
        auto& adjacentEdge = edges.at(adjacentSide);

        if (adjacentEdge.presentButInvisible())
            return false;

        if (!edgesShareColor(edge, adjacentEdge) && !adjacentEdge.color().isOpaque())
            return false;
        
        if (!borderStyleFillsBorderArea(adjacentEdge.style()))
            return false;

        return true;
    }
    case BoxSide::Left:
    case BoxSide::Right:
        // These draw last, so are never overdrawn.
        return false;
    }
    return false;
}

static inline bool borderStylesRequireMitre(BoxSide side, BoxSide adjacentSide, BorderStyle style, BorderStyle adjacentStyle)
{
    if (style == BorderStyle::Double || adjacentStyle == BorderStyle::Double || adjacentStyle == BorderStyle::Groove || adjacentStyle == BorderStyle::Ridge)
        return true;

    if (borderStyleIsDottedOrDashed(style) != borderStyleIsDottedOrDashed(adjacentStyle))
        return true;

    if (style != adjacentStyle)
        return true;

    return borderStyleHasUnmatchedColorsAtCorner(style, side, adjacentSide);
}

static bool joinRequiresMitre(BoxSide side, BoxSide adjacentSide, const BorderEdges& edges, bool allowOverdraw)
{
    auto& edge = edges.at(side);
    auto& adjacentEdge = edges.at(adjacentSide);

    if ((edge.isTransparent() && adjacentEdge.isTransparent()) || !adjacentEdge.isPresent())
        return false;

    if (allowOverdraw && willBeOverdrawn(side, adjacentSide, edges))
        return false;

    if (!edgesShareColor(edge, adjacentEdge))
        return true;

    if (borderStylesRequireMitre(side, adjacentSide, edge.style(), adjacentEdge.style()))
        return true;
    
    return false;
}

static RoundedRect calculateAdjustedInnerBorder(const RoundedRect&innerBorder, BoxSide side)
{
    // Expand the inner border as necessary to make it a rounded rect (i.e. radii contained within each edge).
    // This function relies on the fact we only get radii not contained within each edge if one of the radii
    // for an edge is zero, so we can shift the arc towards the zero radius corner.
    RoundedRect::Radii newRadii = innerBorder.radii();
    LayoutRect newRect = innerBorder.rect();

    float overshoot;
    float maxRadii;

    switch (side) {
    case BoxSide::Top:
        overshoot = newRadii.topLeft().width() + newRadii.topRight().width() - newRect.width();
        if (overshoot > 0) {
            ASSERT(!(newRadii.topLeft().width() && newRadii.topRight().width()));
            newRect.setWidth(newRect.width() + overshoot);
            if (!newRadii.topLeft().width())
                newRect.move(-overshoot, 0);
        }
        newRadii.setBottomLeft({ });
        newRadii.setBottomRight({ });
        maxRadii = std::max(newRadii.topLeft().height(), newRadii.topRight().height());
        if (maxRadii > newRect.height())
            newRect.setHeight(maxRadii);
        break;

    case BoxSide::Bottom:
        overshoot = newRadii.bottomLeft().width() + newRadii.bottomRight().width() - newRect.width();
        if (overshoot > 0) {
            ASSERT(!(newRadii.bottomLeft().width() && newRadii.bottomRight().width()));
            newRect.setWidth(newRect.width() + overshoot);
            if (!newRadii.bottomLeft().width())
                newRect.move(-overshoot, 0);
        }
        newRadii.setTopLeft({ });
        newRadii.setTopRight({ });
        maxRadii = std::max(newRadii.bottomLeft().height(), newRadii.bottomRight().height());
        if (maxRadii > newRect.height()) {
            newRect.move(0, newRect.height() - maxRadii);
            newRect.setHeight(maxRadii);
        }
        break;

    case BoxSide::Left:
        overshoot = newRadii.topLeft().height() + newRadii.bottomLeft().height() - newRect.height();
        if (overshoot > 0) {
            ASSERT(!(newRadii.topLeft().height() && newRadii.bottomLeft().height()));
            newRect.setHeight(newRect.height() + overshoot);
            if (!newRadii.topLeft().height())
                newRect.move(0, -overshoot);
        }
        newRadii.setTopRight({ });
        newRadii.setBottomRight({ });
        maxRadii = std::max(newRadii.topLeft().width(), newRadii.bottomLeft().width());
        if (maxRadii > newRect.width())
            newRect.setWidth(maxRadii);
        break;

    case BoxSide::Right:
        overshoot = newRadii.topRight().height() + newRadii.bottomRight().height() - newRect.height();
        if (overshoot > 0) {
            ASSERT(!(newRadii.topRight().height() && newRadii.bottomRight().height()));
            newRect.setHeight(newRect.height() + overshoot);
            if (!newRadii.topRight().height())
                newRect.move(0, -overshoot);
        }
        newRadii.setTopLeft({ });
        newRadii.setBottomLeft({ });
        maxRadii = std::max(newRadii.topRight().width(), newRadii.bottomRight().width());
        if (maxRadii > newRect.width()) {
            newRect.move(newRect.width() - maxRadii, 0);
            newRect.setWidth(maxRadii);
        }
        break;
    }

    return RoundedRect(newRect, newRadii);
}

void RenderBoxModelObject::paintOneBorderSide(GraphicsContext& graphicsContext, const RenderStyle& style, const RoundedRect& outerBorder, const RoundedRect& innerBorder,
    const LayoutRect& sideRect, BoxSide side, BoxSide adjacentSide1, BoxSide adjacentSide2, const BorderEdges& edges, const Path* path,
    BackgroundBleedAvoidance bleedAvoidance, bool includeLogicalLeftEdge, bool includeLogicalRightEdge, bool antialias, const Color* overrideColor)
{
    auto& edgeToRender = edges.at(side);
    ASSERT(edgeToRender.widthForPainting());
    auto& adjacentEdge1 = edges.at(adjacentSide1);
    auto& adjacentEdge2 = edges.at(adjacentSide2);

    bool mitreAdjacentSide1 = joinRequiresMitre(side, adjacentSide1, edges, !antialias);
    bool mitreAdjacentSide2 = joinRequiresMitre(side, adjacentSide2, edges, !antialias);
    
    bool adjacentSide1StylesMatch = colorsMatchAtCorner(side, adjacentSide1, edges);
    bool adjacentSide2StylesMatch = colorsMatchAtCorner(side, adjacentSide2, edges);

    const Color& colorToPaint = overrideColor ? *overrideColor : edgeToRender.color();

    if (path) {
        GraphicsContextStateSaver stateSaver(graphicsContext);

        clipBorderSidePolygon(graphicsContext, outerBorder, innerBorder, side, adjacentSide1StylesMatch, adjacentSide2StylesMatch);

        if (!innerBorder.isRenderable())
            graphicsContext.clipOutRoundedRect(FloatRoundedRect(calculateAdjustedInnerBorder(innerBorder, side)));

        float thickness = std::max(std::max(edgeToRender.widthForPainting(), adjacentEdge1.widthForPainting()), adjacentEdge2.widthForPainting());
        drawBoxSideFromPath(graphicsContext, outerBorder.rect(), *path, edges, edgeToRender.widthForPainting(), thickness, side, style,
            colorToPaint, edgeToRender.style(), bleedAvoidance, includeLogicalLeftEdge, includeLogicalRightEdge);
    } else {
        bool clipForStyle = styleRequiresClipPolygon(edgeToRender.style()) && (mitreAdjacentSide1 || mitreAdjacentSide2);
        bool clipAdjacentSide1 = colorNeedsAntiAliasAtCorner(side, adjacentSide1, edges) && mitreAdjacentSide1;
        bool clipAdjacentSide2 = colorNeedsAntiAliasAtCorner(side, adjacentSide2, edges) && mitreAdjacentSide2;
        bool shouldClip = clipForStyle || clipAdjacentSide1 || clipAdjacentSide2;
        
        GraphicsContextStateSaver clipStateSaver(graphicsContext, shouldClip);
        if (shouldClip) {
            bool aliasAdjacentSide1 = clipAdjacentSide1 || (clipForStyle && mitreAdjacentSide1);
            bool aliasAdjacentSide2 = clipAdjacentSide2 || (clipForStyle && mitreAdjacentSide2);
            clipBorderSidePolygon(graphicsContext, outerBorder, innerBorder, side, !aliasAdjacentSide1, !aliasAdjacentSide2);
            // Since we clipped, no need to draw with a mitre.
            mitreAdjacentSide1 = false;
            mitreAdjacentSide2 = false;
        }
        drawLineForBoxSide(graphicsContext, sideRect, side, colorToPaint, edgeToRender.style(), mitreAdjacentSide1 ? adjacentEdge1.widthForPainting() : 0, mitreAdjacentSide2 ? adjacentEdge2.widthForPainting() : 0, antialias);
    }
}

static LayoutRect calculateSideRect(const RoundedRect& outerBorder, const BorderEdges& edges, BoxSide side)
{
    LayoutRect sideRect = outerBorder.rect();
    float width = edges.at(side).widthForPainting();

    switch (side) {
    case BoxSide::Top:
        sideRect.setHeight(width);
        break;
    case BoxSide::Right:
        sideRect.shiftXEdgeTo(sideRect.maxX() - width);
        break;
    case BoxSide::Bottom:
        sideRect.shiftYEdgeTo(sideRect.maxY() - width);
        break;
    case BoxSide::Left:
        sideRect.setWidth(width);
        break;
    }

    return sideRect;
}

void RenderBoxModelObject::paintBorderSides(GraphicsContext& graphicsContext, const RenderStyle& style, const RoundedRect& outerBorder, const RoundedRect& innerBorder,
    const IntPoint& innerBorderAdjustment, const BorderEdges& edges, BoxSideSet edgeSet, BackgroundBleedAvoidance bleedAvoidance,
    bool includeLogicalLeftEdge, bool includeLogicalRightEdge, bool antialias, const Color* overrideColor)
{
    bool renderRadii = outerBorder.isRounded();

    Path roundedPath;
    if (renderRadii)
        roundedPath.addRoundedRect(outerBorder);
    
    // The inner border adjustment for bleed avoidance mode BackgroundBleedBackgroundOverBorder
    // is only applied to sideRect, which is okay since BackgroundBleedBackgroundOverBorder
    // is only to be used for solid borders and the shape of the border painted by drawBoxSideFromPath
    // only depends on sideRect when painting solid borders.

    auto paintOneSide = [&](BoxSide side, BoxSide adjacentSide1, BoxSide adjacentSide2) {
        auto& edge = edges.at(side);
        if (!edge.shouldRender() || !edgeSet.contains(edgeFlagForSide(side)))
            return;

        LayoutRect sideRect = outerBorder.rect();
        LayoutSize firstRadius;
        LayoutSize secondRadius;

        switch (side) {
        case BoxSide::Top:
            sideRect.setHeight(edge.widthForPainting() + innerBorderAdjustment.y());
            firstRadius = innerBorder.radii().topLeft();
            secondRadius = innerBorder.radii().topRight();
            break;
        case BoxSide::Right:
            sideRect.shiftXEdgeTo(sideRect.maxX() - edge.widthForPainting() - innerBorderAdjustment.x());
            firstRadius = innerBorder.radii().bottomRight();
            secondRadius = innerBorder.radii().topRight();
            break;
        case BoxSide::Bottom:
            sideRect.shiftYEdgeTo(sideRect.maxY() - edge.widthForPainting() - innerBorderAdjustment.y());
            firstRadius = innerBorder.radii().bottomLeft();
            secondRadius = innerBorder.radii().bottomRight();
            break;
        case BoxSide::Left:
            sideRect.setWidth(edge.widthForPainting() + innerBorderAdjustment.x());
            firstRadius = innerBorder.radii().bottomLeft();
            secondRadius = innerBorder.radii().topLeft();
            break;
        }

        bool usePath = renderRadii && (borderStyleHasInnerDetail(edge.style()) || borderWillArcInnerEdge(firstRadius, secondRadius));
        paintOneBorderSide(graphicsContext, style, outerBorder, innerBorder, sideRect, side, adjacentSide1, adjacentSide2, edges, usePath ? &roundedPath : nullptr, bleedAvoidance, includeLogicalLeftEdge, includeLogicalRightEdge, antialias, overrideColor);
    };

    paintOneSide(BoxSide::Top, BoxSide::Left, BoxSide::Right);
    paintOneSide(BoxSide::Bottom, BoxSide::Left, BoxSide::Right);
    paintOneSide(BoxSide::Left, BoxSide::Top, BoxSide::Bottom);
    paintOneSide(BoxSide::Right, BoxSide::Top, BoxSide::Bottom);
}

void RenderBoxModelObject::paintTranslucentBorderSides(GraphicsContext& graphicsContext, const RenderStyle& style, const RoundedRect& outerBorder, const RoundedRect& innerBorder, const IntPoint& innerBorderAdjustment,
    const BorderEdges& edges, BoxSideSet edgesToDraw, BackgroundBleedAvoidance bleedAvoidance, bool includeLogicalLeftEdge, bool includeLogicalRightEdge, bool antialias)
{
    // willBeOverdrawn assumes that we draw in order: top, bottom, left, right.
    // This is different from BoxSide enum order.
    static constexpr std::array<BoxSide, 4> paintOrderSides = { BoxSide::Top, BoxSide::Bottom, BoxSide::Left, BoxSide::Right };

    while (edgesToDraw) {
        // Find undrawn edges sharing a color.
        Color commonColor;
        
        BoxSideSet commonColorEdgeSet;
        for (auto side : paintOrderSides) {
            if (!edgesToDraw.contains(edgeFlagForSide(side)))
                continue;

            auto& edge = edges.at(side);
            bool includeEdge;
            if (commonColorEdgeSet.isEmpty()) {
                commonColor = edge.color();
                includeEdge = true;
            } else
                includeEdge = edge.color() == commonColor;

            if (includeEdge)
                commonColorEdgeSet.add(edgeFlagForSide(side));
        }

        bool useTransparencyLayer = includesAdjacentEdges(commonColorEdgeSet) && !commonColor.isOpaque();
        if (useTransparencyLayer) {
            graphicsContext.beginTransparencyLayer(commonColor.alphaAsFloat());
            commonColor = commonColor.opaqueColor();
        }

        paintBorderSides(graphicsContext, style, outerBorder, innerBorder, innerBorderAdjustment, edges, commonColorEdgeSet, bleedAvoidance, includeLogicalLeftEdge, includeLogicalRightEdge, antialias, &commonColor);
            
        if (useTransparencyLayer)
            graphicsContext.endTransparencyLayer();
        
        edgesToDraw.remove(commonColorEdgeSet);
    }
}

void RenderBoxModelObject::paintBorder(const PaintInfo& info, const LayoutRect& rect, const RenderStyle& style,
                                       BackgroundBleedAvoidance bleedAvoidance, bool includeLogicalLeftEdge, bool includeLogicalRightEdge)
{
    GraphicsContext& graphicsContext = info.context();

    if (graphicsContext.paintingDisabled())
        return;

    auto paintsBorderImage = [&](LayoutRect rect, const NinePieceImage& ninePieceImage) {
        auto* styleImage = ninePieceImage.image();
        if (!styleImage)
            return false;

        if (!styleImage->isLoaded())
            return false;

        if (!styleImage->canRender(this, style.effectiveZoom()))
            return false;

        auto rectWithOutsets = rect;
        rectWithOutsets.expand(style.imageOutsets(ninePieceImage));
        return !rectWithOutsets.isEmpty();
    };

    if (rect.isEmpty() && !paintsBorderImage(rect, style.borderImage()))
        return;

    auto rectToClipOut = paintRectToClipOutFromBorder(rect);
    bool appliedClipAlready = !rectToClipOut.isEmpty();
    GraphicsContextStateSaver stateSave(graphicsContext, appliedClipAlready);
    if (!rectToClipOut.isEmpty())
        graphicsContext.clipOut(snapRectToDevicePixels(rectToClipOut, document().deviceScaleFactor()));

    // border-image is not affected by border-radius.
    if (paintNinePieceImage(graphicsContext, rect, style, style.borderImage()))
        return;

    auto edges = borderEdges(style, document().deviceScaleFactor(), includeLogicalLeftEdge, includeLogicalRightEdge);
    RoundedRect outerBorder = style.getRoundedBorderFor(rect, includeLogicalLeftEdge, includeLogicalRightEdge);
    RoundedRect innerBorder = style.getRoundedInnerBorderFor(borderInnerRectAdjustedForBleedAvoidance(graphicsContext, rect, bleedAvoidance), includeLogicalLeftEdge, includeLogicalRightEdge);

    // If no borders intersects with the dirty area, we can skip the border painting.
    if (innerBorder.contains(info.rect))
        return;

    bool haveAlphaColor = false;
    bool haveAllSolidEdges = true;
    bool haveAllDoubleEdges = true;
    int numEdgesVisible = 4;
    bool allEdgesShareColor = true;
    std::optional<BoxSide> firstVisibleSide;
    BoxSideSet edgesToDraw;

    for (auto side : allBoxSides) {
        auto& currEdge = edges.at(side);

        if (currEdge.shouldRender())
            edgesToDraw.add(edgeFlagForSide(side));

        if (currEdge.presentButInvisible()) {
            --numEdgesVisible;
            allEdgesShareColor = false;
            continue;
        }
        
        if (!currEdge.widthForPainting()) {
            --numEdgesVisible;
            continue;
        }

        if (!firstVisibleSide)
            firstVisibleSide = side;
        else if (currEdge.color() != edges.at(*firstVisibleSide).color())
            allEdgesShareColor = false;

        if (!currEdge.color().isOpaque())
            haveAlphaColor = true;
        
        if (currEdge.style() != BorderStyle::Solid)
            haveAllSolidEdges = false;

        if (currEdge.style() != BorderStyle::Double)
            haveAllDoubleEdges = false;
    }

    // If no corner intersects the clip region, we can pretend outerBorder is
    // rectangular to improve performance.
    if (haveAllSolidEdges && outerBorder.isRounded() && allCornersClippedOut(outerBorder, info.rect))
        outerBorder.setRadii(RoundedRect::Radii());

    float deviceScaleFactor = document().deviceScaleFactor();
    // isRenderable() check avoids issue described in https://bugs.webkit.org/show_bug.cgi?id=38787
    if ((haveAllSolidEdges || haveAllDoubleEdges) && allEdgesShareColor && innerBorder.isRenderable()) {
        // Fast path for drawing all solid edges and all unrounded double edges
        if (numEdgesVisible == 4 && (outerBorder.isRounded() || haveAlphaColor)
            && (haveAllSolidEdges || (!outerBorder.isRounded() && !innerBorder.isRounded()))) {
            Path path;
            
            FloatRoundedRect pixelSnappedOuterBorder = outerBorder.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
            if (pixelSnappedOuterBorder.isRounded() && bleedAvoidance != BackgroundBleedUseTransparencyLayer)
                path.addRoundedRect(pixelSnappedOuterBorder);
            else
                path.addRect(pixelSnappedOuterBorder.rect());

            if (haveAllDoubleEdges) {
                LayoutRect innerThirdRect = outerBorder.rect();
                LayoutRect outerThirdRect = outerBorder.rect();
                for (auto side : allBoxSides) {
                    LayoutUnit outerWidth;
                    LayoutUnit innerWidth;
                    edges.at(side).getDoubleBorderStripeWidths(outerWidth, innerWidth);
                    switch (side) {
                    case BoxSide::Top:
                        innerThirdRect.shiftYEdgeTo(innerThirdRect.y() + innerWidth);
                        outerThirdRect.shiftYEdgeTo(outerThirdRect.y() + outerWidth);
                        break;
                    case BoxSide::Right:
                        innerThirdRect.setWidth(innerThirdRect.width() - innerWidth);
                        outerThirdRect.setWidth(outerThirdRect.width() - outerWidth);
                        break;
                    case BoxSide::Bottom:
                        innerThirdRect.setHeight(innerThirdRect.height() - innerWidth);
                        outerThirdRect.setHeight(outerThirdRect.height() - outerWidth);
                        break;
                    case BoxSide::Left:
                        innerThirdRect.shiftXEdgeTo(innerThirdRect.x() + innerWidth);
                        outerThirdRect.shiftXEdgeTo(outerThirdRect.x() + outerWidth);
                        break;
                    }
                }

                FloatRoundedRect pixelSnappedOuterThird = outerBorder.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
                pixelSnappedOuterThird.setRect(snapRectToDevicePixels(outerThirdRect, deviceScaleFactor));

                if (pixelSnappedOuterThird.isRounded() && bleedAvoidance != BackgroundBleedUseTransparencyLayer)
                    path.addRoundedRect(pixelSnappedOuterThird);
                else
                    path.addRect(pixelSnappedOuterThird.rect());

                FloatRoundedRect pixelSnappedInnerThird = innerBorder.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
                pixelSnappedInnerThird.setRect(snapRectToDevicePixels(innerThirdRect, deviceScaleFactor));
                if (pixelSnappedInnerThird.isRounded() && bleedAvoidance != BackgroundBleedUseTransparencyLayer)
                    path.addRoundedRect(pixelSnappedInnerThird);
                else
                    path.addRect(pixelSnappedInnerThird.rect());
            }

            FloatRoundedRect pixelSnappedInnerBorder = innerBorder.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
            if (pixelSnappedInnerBorder.isRounded())
                path.addRoundedRect(pixelSnappedInnerBorder);
            else
                path.addRect(pixelSnappedInnerBorder.rect());
            
            graphicsContext.setFillRule(WindRule::EvenOdd);
            graphicsContext.setFillColor(edges.at(*firstVisibleSide).color());
            graphicsContext.fillPath(path);
            return;
        } 
        // Avoid creating transparent layers
        if (haveAllSolidEdges && numEdgesVisible != 4 && !outerBorder.isRounded() && haveAlphaColor) {
            Path path;

            for (auto side : allBoxSides) {
                if (edges.at(side).shouldRender()) {
                    auto sideRect = calculateSideRect(outerBorder, edges, side);
                    path.addRect(sideRect); // FIXME: Need pixel snapping here.
                }
            }

            graphicsContext.setFillRule(WindRule::NonZero);
            graphicsContext.setFillColor(edges.at(*firstVisibleSide).color());
            graphicsContext.fillPath(path);
            return;
        }
    }

    bool clipToOuterBorder = outerBorder.isRounded();
    GraphicsContextStateSaver stateSaver(graphicsContext, clipToOuterBorder && !appliedClipAlready);
    if (clipToOuterBorder) {
        // Clip to the inner and outer radii rects.
        if (bleedAvoidance != BackgroundBleedUseTransparencyLayer)
            graphicsContext.clipRoundedRect(outerBorder.pixelSnappedRoundedRectForPainting(deviceScaleFactor));
        // isRenderable() check avoids issue described in https://bugs.webkit.org/show_bug.cgi?id=38787
        // The inside will be clipped out later (in clipBorderSideForComplexInnerPath)
        if (innerBorder.isRenderable())
            graphicsContext.clipOutRoundedRect(innerBorder.pixelSnappedRoundedRectForPainting(deviceScaleFactor));
    }

    // If only one edge visible antialiasing doesn't create seams
    bool antialias = shouldAntialiasLines(graphicsContext) || numEdgesVisible == 1;
    RoundedRect unadjustedInnerBorder = (bleedAvoidance == BackgroundBleedBackgroundOverBorder) ? style.getRoundedInnerBorderFor(rect, includeLogicalLeftEdge, includeLogicalRightEdge) : innerBorder;
    IntPoint innerBorderAdjustment(innerBorder.rect().x() - unadjustedInnerBorder.rect().x(), innerBorder.rect().y() - unadjustedInnerBorder.rect().y());
    if (haveAlphaColor)
        paintTranslucentBorderSides(graphicsContext, style, outerBorder, unadjustedInnerBorder, innerBorderAdjustment, edges, edgesToDraw, bleedAvoidance, includeLogicalLeftEdge, includeLogicalRightEdge, antialias);
    else
        paintBorderSides(graphicsContext, style, outerBorder, unadjustedInnerBorder, innerBorderAdjustment, edges, edgesToDraw, bleedAvoidance, includeLogicalLeftEdge, includeLogicalRightEdge, antialias);
}

void RenderBoxModelObject::drawBoxSideFromPath(GraphicsContext& graphicsContext, const LayoutRect& borderRect, const Path& borderPath, const BorderEdges& edges,
    float thickness, float drawThickness, BoxSide side, const RenderStyle& style, Color color, BorderStyle borderStyle, BackgroundBleedAvoidance bleedAvoidance,
    bool includeLogicalLeftEdge, bool includeLogicalRightEdge)
{
    if (thickness <= 0)
        return;

    if (borderStyle == BorderStyle::Double && thickness < 3)
        borderStyle = BorderStyle::Solid;

    switch (borderStyle) {
    case BorderStyle::None:
    case BorderStyle::Hidden:
        return;
    case BorderStyle::Dotted:
    case BorderStyle::Dashed: {
        graphicsContext.setStrokeColor(color);

        // The stroke is doubled here because the provided path is the 
        // outside edge of the border so half the stroke is clipped off. 
        // The extra multiplier is so that the clipping mask can antialias
        // the edges to prevent jaggies.
        graphicsContext.setStrokeThickness(drawThickness * 2 * 1.1f);
        graphicsContext.setStrokeStyle(borderStyle == BorderStyle::Dashed ? DashedStroke : DottedStroke);

        // If the number of dashes that fit in the path is odd and non-integral then we
        // will have an awkwardly-sized dash at the end of the path. To try to avoid that
        // here, we simply make the whitespace dashes ever so slightly bigger.
        // FIXME: This could be even better if we tried to manipulate the dash offset
        // and possibly the gapLength to get the corners dash-symmetrical.
        float dashLength = thickness * ((borderStyle == BorderStyle::Dashed) ? 3.0f : 1.0f);
        float gapLength = dashLength;
        float numberOfDashes = borderPath.length() / dashLength;
        // Don't try to show dashes if we have less than 2 dashes + 2 gaps.
        // FIXME: should do this test per side.
        if (numberOfDashes >= 4) {
            bool evenNumberOfFullDashes = !((int)numberOfDashes % 2);
            bool integralNumberOfDashes = !(numberOfDashes - (int)numberOfDashes);
            if (!evenNumberOfFullDashes && !integralNumberOfDashes) {
                float numberOfGaps = numberOfDashes / 2;
                gapLength += (dashLength  / numberOfGaps);
            }

            DashArray lineDash;
            lineDash.append(dashLength);
            lineDash.append(gapLength);
            graphicsContext.setLineDash(lineDash, dashLength);
        }
        
        // FIXME: stroking the border path causes issues with tight corners:
        // https://bugs.webkit.org/show_bug.cgi?id=58711
        // Also, to get the best appearance we should stroke a path between the two borders.
        graphicsContext.strokePath(borderPath);
        return;
    }
    case BorderStyle::Double: {
        // Get the inner border rects for both the outer border line and the inner border line
        LayoutUnit outerBorderTopWidth;
        LayoutUnit innerBorderTopWidth;
        edges.top().getDoubleBorderStripeWidths(outerBorderTopWidth, innerBorderTopWidth);

        LayoutUnit outerBorderRightWidth;
        LayoutUnit innerBorderRightWidth;
        edges.right().getDoubleBorderStripeWidths(outerBorderRightWidth, innerBorderRightWidth);

        LayoutUnit outerBorderBottomWidth;
        LayoutUnit innerBorderBottomWidth;
        edges.bottom().getDoubleBorderStripeWidths(outerBorderBottomWidth, innerBorderBottomWidth);

        LayoutUnit outerBorderLeftWidth;
        LayoutUnit innerBorderLeftWidth;
        edges.left().getDoubleBorderStripeWidths(outerBorderLeftWidth, innerBorderLeftWidth);

        // Draw inner border line
        {
            GraphicsContextStateSaver stateSaver(graphicsContext);
            RoundedRect innerClip = style.getRoundedInnerBorderFor(borderRect,
                innerBorderTopWidth, innerBorderBottomWidth, innerBorderLeftWidth, innerBorderRightWidth,
                includeLogicalLeftEdge, includeLogicalRightEdge);
            
            graphicsContext.clipRoundedRect(FloatRoundedRect(innerClip));
            drawBoxSideFromPath(graphicsContext, borderRect, borderPath, edges, thickness, drawThickness, side, style, color, BorderStyle::Solid, bleedAvoidance, includeLogicalLeftEdge, includeLogicalRightEdge);
        }

        // Draw outer border line
        {
            GraphicsContextStateSaver stateSaver(graphicsContext);
            LayoutRect outerRect = borderRect;
            if (bleedAvoidance == BackgroundBleedUseTransparencyLayer) {
                outerRect.inflate(1);
                ++outerBorderTopWidth;
                ++outerBorderBottomWidth;
                ++outerBorderLeftWidth;
                ++outerBorderRightWidth;
            }
                
            RoundedRect outerClip = style.getRoundedInnerBorderFor(outerRect,
                outerBorderTopWidth, outerBorderBottomWidth, outerBorderLeftWidth, outerBorderRightWidth,
                includeLogicalLeftEdge, includeLogicalRightEdge);
            graphicsContext.clipOutRoundedRect(FloatRoundedRect(outerClip));
            drawBoxSideFromPath(graphicsContext, borderRect, borderPath, edges, thickness, drawThickness, side, style, color, BorderStyle::Solid, bleedAvoidance, includeLogicalLeftEdge, includeLogicalRightEdge);
        }
        return;
    }
    case BorderStyle::Ridge:
    case BorderStyle::Groove:
    {
        BorderStyle s1;
        BorderStyle s2;
        if (borderStyle == BorderStyle::Groove) {
            s1 = BorderStyle::Inset;
            s2 = BorderStyle::Outset;
        } else {
            s1 = BorderStyle::Outset;
            s2 = BorderStyle::Inset;
        }
        
        // Paint full border
        drawBoxSideFromPath(graphicsContext, borderRect, borderPath, edges, thickness, drawThickness, side, style, color, s1, bleedAvoidance, includeLogicalLeftEdge, includeLogicalRightEdge);

        // Paint inner only
        GraphicsContextStateSaver stateSaver(graphicsContext);
        LayoutUnit topWidth { edges.top().widthForPainting() / 2 };
        LayoutUnit bottomWidth { edges.bottom().widthForPainting() / 2 };
        LayoutUnit leftWidth { edges.left().widthForPainting() / 2 };
        LayoutUnit rightWidth { edges.right().widthForPainting() / 2 };

        RoundedRect clipRect = style.getRoundedInnerBorderFor(borderRect,
            topWidth, bottomWidth, leftWidth, rightWidth,
            includeLogicalLeftEdge, includeLogicalRightEdge);

        graphicsContext.clipRoundedRect(FloatRoundedRect(clipRect));
        drawBoxSideFromPath(graphicsContext, borderRect, borderPath, edges, thickness, drawThickness, side, style, color, s2, bleedAvoidance, includeLogicalLeftEdge, includeLogicalRightEdge);
        return;
    }
    case BorderStyle::Inset:
    case BorderStyle::Outset:
        calculateBorderStyleColor(borderStyle, side, color);
        break;
    default:
        break;
    }

    graphicsContext.setStrokeStyle(NoStroke);
    graphicsContext.setFillColor(color);
    graphicsContext.drawRect(snapRectToDevicePixels(borderRect, document().deviceScaleFactor()));
}

void RenderBoxModelObject::clipBorderSidePolygon(GraphicsContext& graphicsContext, const RoundedRect& outerBorder, const RoundedRect& innerBorder,
                                                 BoxSide side, bool firstEdgeMatches, bool secondEdgeMatches)
{
    float deviceScaleFactor = document().deviceScaleFactor();
    const FloatRect& outerRect = snapRectToDevicePixels(outerBorder.rect(), deviceScaleFactor);
    const FloatRect& innerRect = snapRectToDevicePixels(innerBorder.rect(), deviceScaleFactor);

    // For each side, create a quad that encompasses all parts of that side that may draw,
    // including areas inside the innerBorder.
    //
    //         0----------------3
    //       0  \              /  0
    //       |\  1----------- 2  /|
    //       | 1                1 |   
    //       | |                | |
    //       | |                | |  
    //       | 2                2 |  
    //       |/  1------------2  \| 
    //       3  /              \  3   
    //         0----------------3
    //
    Vector<FloatPoint> quad;
    switch (side) {
    case BoxSide::Top:
        quad = { outerRect.minXMinYCorner(), innerRect.minXMinYCorner(), innerRect.maxXMinYCorner(), outerRect.maxXMinYCorner() };

        if (!innerBorder.radii().topLeft().isZero())
            findIntersection(outerRect.minXMinYCorner(), innerRect.minXMinYCorner(), innerRect.minXMaxYCorner(), innerRect.maxXMinYCorner(), quad[1]);

        if (!innerBorder.radii().topRight().isZero())
            findIntersection(outerRect.maxXMinYCorner(), innerRect.maxXMinYCorner(), innerRect.minXMinYCorner(), innerRect.maxXMaxYCorner(), quad[2]);
        break;

    case BoxSide::Left:
        quad = { outerRect.minXMinYCorner(), innerRect.minXMinYCorner(), innerRect.minXMaxYCorner(), outerRect.minXMaxYCorner() };

        if (!innerBorder.radii().topLeft().isZero())
            findIntersection(outerRect.minXMinYCorner(), innerRect.minXMinYCorner(), innerRect.minXMaxYCorner(), innerRect.maxXMinYCorner(), quad[1]);

        if (!innerBorder.radii().bottomLeft().isZero())
            findIntersection(outerRect.minXMaxYCorner(), innerRect.minXMaxYCorner(), innerRect.minXMinYCorner(), innerRect.maxXMaxYCorner(), quad[2]);
        break;

    case BoxSide::Bottom:
        quad = { outerRect.minXMaxYCorner(), innerRect.minXMaxYCorner(), innerRect.maxXMaxYCorner(), outerRect.maxXMaxYCorner() };

        if (!innerBorder.radii().bottomLeft().isZero())
            findIntersection(outerRect.minXMaxYCorner(), innerRect.minXMaxYCorner(), innerRect.minXMinYCorner(), innerRect.maxXMaxYCorner(), quad[1]);

        if (!innerBorder.radii().bottomRight().isZero())
            findIntersection(outerRect.maxXMaxYCorner(), innerRect.maxXMaxYCorner(), innerRect.maxXMinYCorner(), innerRect.minXMaxYCorner(), quad[2]);
        break;

    case BoxSide::Right:
        quad = { outerRect.maxXMinYCorner(), innerRect.maxXMinYCorner(), innerRect.maxXMaxYCorner(), outerRect.maxXMaxYCorner() };

        if (!innerBorder.radii().topRight().isZero())
            findIntersection(outerRect.maxXMinYCorner(), innerRect.maxXMinYCorner(), innerRect.minXMinYCorner(), innerRect.maxXMaxYCorner(), quad[1]);

        if (!innerBorder.radii().bottomRight().isZero())
            findIntersection(outerRect.maxXMaxYCorner(), innerRect.maxXMaxYCorner(), innerRect.maxXMinYCorner(), innerRect.minXMaxYCorner(), quad[2]);
        break;
    }

    // If the border matches both of its adjacent sides, don't anti-alias the clip, and
    // if neither side matches, anti-alias the clip.
    if (firstEdgeMatches == secondEdgeMatches) {
        bool wasAntialiased = graphicsContext.shouldAntialias();
        graphicsContext.setShouldAntialias(!firstEdgeMatches);
        graphicsContext.clipPath(Path::polygonPathFromPoints(quad), WindRule::NonZero);
        graphicsContext.setShouldAntialias(wasAntialiased);
        return;
    }

    // Square off the end which shouldn't be affected by antialiasing, and clip.
    Vector<FloatPoint> firstQuad = {
        quad[0],
        quad[1],
        quad[2],
        side == BoxSide::Top || side == BoxSide::Bottom ? FloatPoint(quad[3].x(), quad[2].y()) : FloatPoint(quad[2].x(), quad[3].y()),
        quad[3]
    };
    bool wasAntialiased = graphicsContext.shouldAntialias();
    graphicsContext.setShouldAntialias(!firstEdgeMatches);
    graphicsContext.clipPath(Path::polygonPathFromPoints(firstQuad), WindRule::NonZero);

    Vector<FloatPoint> secondQuad = {
        quad[0],
        side == BoxSide::Top || side == BoxSide::Bottom ? FloatPoint(quad[0].x(), quad[1].y()) : FloatPoint(quad[1].x(), quad[0].y()),
        quad[1],
        quad[2],
        quad[3]
    };
    // Antialiasing affects the second side.
    graphicsContext.setShouldAntialias(!secondEdgeMatches);
    graphicsContext.clipPath(Path::polygonPathFromPoints(secondQuad), WindRule::NonZero);

    graphicsContext.setShouldAntialias(wasAntialiased);
}

bool RenderBoxModelObject::borderObscuresBackgroundEdge(const FloatSize& contextScale) const
{
    auto edges = borderEdges(style(), document().deviceScaleFactor());

    for (auto side : allBoxSides) {
        auto& currEdge = edges.at(side);
        // FIXME: for vertical text
        float axisScale = (side == BoxSide::Top || side == BoxSide::Bottom) ? contextScale.height() : contextScale.width();
        if (!currEdge.obscuresBackgroundEdge(axisScale))
            return false;
    }

    return true;
}

bool RenderBoxModelObject::borderObscuresBackground() const
{
    if (!style().hasBorder())
        return false;

    // Bail if we have any border-image for now. We could look at the image alpha to improve this.
    if (style().borderImage().image())
        return false;

    auto edges = borderEdges(style(), document().deviceScaleFactor());

    for (auto side : allBoxSides) {
        if (!edges.at(side).obscuresBackground())
            return false;
    }

    return true;
}

bool RenderBoxModelObject::boxShadowShouldBeAppliedToBackground(const LayoutPoint&, BackgroundBleedAvoidance bleedAvoidance, const InlineIterator::InlineBoxIterator& inlineBox) const
{
    if (bleedAvoidance != BackgroundBleedNone)
        return false;

    if (style().hasEffectiveAppearance())
        return false;

    bool hasOneNormalBoxShadow = false;
    for (const ShadowData* currentShadow = style().boxShadow(); currentShadow; currentShadow = currentShadow->next()) {
        if (currentShadow->style() != ShadowStyle::Normal)
            continue;

        if (hasOneNormalBoxShadow)
            return false;
        hasOneNormalBoxShadow = true;

        if (!currentShadow->spread().isZero())
            return false;
    }

    if (!hasOneNormalBoxShadow)
        return false;

    Color backgroundColor = style().visitedDependentColorWithColorFilter(CSSPropertyBackgroundColor);
    if (!backgroundColor.isOpaque())
        return false;

    auto* lastBackgroundLayer = &style().backgroundLayers();
    while (auto* next = lastBackgroundLayer->next())
        lastBackgroundLayer = next;

    if (lastBackgroundLayer->clip() != FillBox::Border)
        return false;

    if (lastBackgroundLayer->image() && style().hasBorderRadius())
        return false;

    auto applyToInlineBox = [&] {
        // The checks here match how paintFillLayer() decides whether to clip (if it does, the shadow
        // would be clipped out, so it has to be drawn separately).
        if (inlineBox->isRootInlineBox())
            return true;
        if (!inlineBox->previousInlineBox() && !inlineBox->nextInlineBox())
            return true;
        auto* image = lastBackgroundLayer->image();
        auto& renderer = inlineBox->renderer();
        bool hasFillImage = image && image->canRender(&renderer, renderer.style().effectiveZoom());
        return !hasFillImage && !renderer.style().hasBorderRadius();
    };

    if (inlineBox && !applyToInlineBox())
        return false;

    if (hasNonVisibleOverflow() && lastBackgroundLayer->attachment() == FillAttachment::LocalBackground)
        return false;

    return true;
}

static inline LayoutRect areaCastingShadowInHole(const LayoutRect& holeRect, LayoutUnit shadowExtent, LayoutUnit shadowSpread, const LayoutSize& shadowOffset)
{
    LayoutRect bounds(holeRect);
    
    bounds.inflate(shadowExtent);

    if (shadowSpread < 0)
        bounds.inflate(-shadowSpread);
    
    LayoutRect offsetBounds = bounds;
    offsetBounds.move(-shadowOffset);
    return unionRect(bounds, offsetBounds);
}

void RenderBoxModelObject::paintBoxShadow(const PaintInfo& info, const LayoutRect& paintRect, const RenderStyle& style, ShadowStyle shadowStyle, bool includeLogicalLeftEdge, bool includeLogicalRightEdge)
{
    // FIXME: Deal with border-image.  Would be great to use border-image as a mask.
    GraphicsContext& context = info.context();
    if (context.paintingDisabled() || !style.boxShadow())
        return;

    RoundedRect borderRect = (shadowStyle == ShadowStyle::Inset) ? style.getRoundedInnerBorderFor(paintRect, includeLogicalLeftEdge, includeLogicalRightEdge)
        : style.getRoundedBorderFor(paintRect, includeLogicalLeftEdge, includeLogicalRightEdge);

    if (!borderRect.isRenderable())
        borderRect.adjustRadii();

    bool hasBorderRadius = style.hasBorderRadius();
    float deviceScaleFactor = document().deviceScaleFactor();

    bool hasOpaqueBackground = style.visitedDependentColorWithColorFilter(CSSPropertyBackgroundColor).isOpaque();
    for (const ShadowData* shadow = style.boxShadow(); shadow; shadow = shadow->next()) {
        if (shadow->style() != shadowStyle)
            continue;

        LayoutSize shadowOffset(shadow->x().value(), shadow->y().value());
        LayoutUnit shadowPaintingExtent = shadow->paintingExtent();
        LayoutUnit shadowSpread = LayoutUnit(shadow->spread().value());
        auto shadowRadius = shadow->radius().value();

        if (shadowOffset.isZero() && !shadowRadius && !shadowSpread)
            continue;
        
        Color shadowColor = style.colorByApplyingColorFilter(shadow->color());

        if (shadow->style() == ShadowStyle::Normal) {
            auto fillRect = borderRect;
            fillRect.inflate(shadowSpread);
            if (fillRect.isEmpty())
                continue;

            auto shadowRect = borderRect.rect();
            shadowRect.inflate(shadowPaintingExtent + shadowSpread);
            shadowRect.move(shadowOffset);
            auto pixelSnappedShadowRect = snapRectToDevicePixels(shadowRect, deviceScaleFactor);

            GraphicsContextStateSaver stateSaver(context);
            context.clip(pixelSnappedShadowRect);

            // Move the fill just outside the clip, adding at least 1 pixel of separation so that the fill does not
            // bleed in (due to antialiasing) if the context is transformed.
            LayoutUnit xOffset = paintRect.width() + std::max<LayoutUnit>(0, shadowOffset.width()) + shadowPaintingExtent + 2 * shadowSpread + LayoutUnit(1);
            LayoutSize extraOffset(xOffset.ceil(), 0);
            shadowOffset -= extraOffset;
            fillRect.move(extraOffset);

            auto pixelSnappedRectToClipOut = borderRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
            auto pixelSnappedFillRect = fillRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
            
            LayoutPoint shadowRectOrigin = fillRect.rect().location() + shadowOffset;
            FloatPoint snappedShadowOrigin = FloatPoint(roundToDevicePixel(shadowRectOrigin.x(), deviceScaleFactor), roundToDevicePixel(shadowRectOrigin.y(), deviceScaleFactor));
            FloatSize snappedShadowOffset = snappedShadowOrigin - pixelSnappedFillRect.rect().location();

            context.setShadow(snappedShadowOffset, shadowRadius, shadowColor, shadow->isWebkitBoxShadow() ? ShadowRadiusMode::Legacy : ShadowRadiusMode::Default);

            if (hasBorderRadius) {
                // If the box is opaque, it is unnecessary to clip it out. However, doing so saves time
                // when painting the shadow. On the other hand, it introduces subpixel gaps along the
                // corners. Those are avoided by insetting the clipping path by one pixel.
                if (hasOpaqueBackground)
                    pixelSnappedRectToClipOut.inflateWithRadii(-1.0f);

                if (!pixelSnappedRectToClipOut.isEmpty())
                    context.clipOutRoundedRect(pixelSnappedRectToClipOut);

                RoundedRect influenceRect(LayoutRect(pixelSnappedShadowRect), borderRect.radii());
                influenceRect.expandRadii(2 * shadowPaintingExtent + shadowSpread);

                if (allCornersClippedOut(influenceRect, info.rect))
                    context.fillRect(pixelSnappedFillRect.rect(), Color::black);
                else {
                    pixelSnappedFillRect.expandRadii(shadowSpread);
                    if (!pixelSnappedFillRect.isRenderable())
                        pixelSnappedFillRect.adjustRadii();
                    context.fillRoundedRect(pixelSnappedFillRect, Color::black);
                }
            } else {
                // If the box is opaque, it is unnecessary to clip it out. However, doing so saves time
                // when painting the shadow. On the other hand, it introduces subpixel gaps along the
                // edges if they are not pixel-aligned. Those are avoided by insetting the clipping path
                // by one pixel.
                if (hasOpaqueBackground) {
                    // FIXME: The function to decide on the policy based on the transform should be a named function.
                    // FIXME: It's not clear if this check is right. What about integral scale factors?
                    AffineTransform transform = context.getCTM();
                    if (transform.a() != 1 || (transform.d() != 1 && transform.d() != -1) || transform.b() || transform.c())
                        pixelSnappedRectToClipOut.inflate(-1.0f);
                }

                if (!pixelSnappedRectToClipOut.isEmpty())
                    context.clipOut(pixelSnappedRectToClipOut.rect());

                context.fillRect(pixelSnappedFillRect.rect(), Color::black);
            }
        } else {
            // Inset shadow.
            auto holeRect = borderRect.rect();
            holeRect.inflate(-shadowSpread);

            bool isHorizontal = style.isHorizontalWritingMode();
            if (!includeLogicalLeftEdge) {
                if (isHorizontal)
                    holeRect.shiftXEdgeBy(-(std::max<LayoutUnit>(shadowOffset.width(), 0) + shadowPaintingExtent + shadowSpread));
                else
                    holeRect.shiftYEdgeBy(-(std::max<LayoutUnit>(shadowOffset.height(), 0) + shadowPaintingExtent + shadowSpread));
            }

            if (!includeLogicalRightEdge) {
                if (isHorizontal)
                    holeRect.setWidth(holeRect.width() - std::min<LayoutUnit>(shadowOffset.width(), 0) + shadowPaintingExtent + shadowSpread);
                else
                    holeRect.setHeight(holeRect.height() - std::min<LayoutUnit>(shadowOffset.height(), 0) + shadowPaintingExtent + shadowSpread);
            }

            auto roundedHoleRect = RoundedRect { holeRect, borderRect.radii() };
            if (shadowSpread && roundedHoleRect.isRounded()) {
                auto rounedRectCorrectingForSpread = [&]() {
                    bool horizontal = style.isHorizontalWritingMode();
                    LayoutUnit leftWidth { (!horizontal || includeLogicalLeftEdge) ? style.borderLeftWidth() + shadowSpread : 0 };
                    LayoutUnit rightWidth { (!horizontal || includeLogicalRightEdge) ? style.borderRightWidth() + shadowSpread : 0 };
                    LayoutUnit topWidth { (horizontal || includeLogicalLeftEdge) ? style.borderTopWidth() + shadowSpread : 0 };
                    LayoutUnit bottomWidth { (horizontal || includeLogicalRightEdge) ? style.borderBottomWidth() + shadowSpread : 0 };

                    return style.getRoundedInnerBorderFor(paintRect, topWidth, bottomWidth, leftWidth, rightWidth, includeLogicalLeftEdge, includeLogicalRightEdge);
                }();
                roundedHoleRect.setRadii(rounedRectCorrectingForSpread.radii());
            }

            auto pixelSnappedHoleRect = roundedHoleRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
            auto pixelSnappedBorderRect = borderRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
            if (pixelSnappedHoleRect.isEmpty()) {
                if (hasBorderRadius)
                    context.fillRoundedRect(pixelSnappedBorderRect, shadowColor);
                else
                    context.fillRect(pixelSnappedBorderRect.rect(), shadowColor);
                continue;
            }

            Color fillColor = shadowColor.opaqueColor();
            auto shadowCastingRect = areaCastingShadowInHole(borderRect.rect(), shadowPaintingExtent, shadowSpread, shadowOffset);
            auto pixelSnappedOuterRect = snapRectToDevicePixels(shadowCastingRect, deviceScaleFactor);

            GraphicsContextStateSaver stateSaver(context);
            if (hasBorderRadius)
                context.clipRoundedRect(pixelSnappedBorderRect);
            else
                context.clip(pixelSnappedBorderRect.rect());

            LayoutUnit xOffset = 2 * paintRect.width() + std::max<LayoutUnit>(0, shadowOffset.width()) + shadowPaintingExtent - 2 * shadowSpread + LayoutUnit(1);
            LayoutSize extraOffset(xOffset.ceil(), 0);

            context.translate(extraOffset);
            shadowOffset -= extraOffset;

            auto snappedShadowOffset = roundSizeToDevicePixels(shadowOffset, deviceScaleFactor);
            context.setShadow(snappedShadowOffset, shadowRadius, shadowColor, shadow->isWebkitBoxShadow() ? ShadowRadiusMode::Legacy : ShadowRadiusMode::Default);
            context.fillRectWithRoundedHole(pixelSnappedOuterRect, pixelSnappedHoleRect, fillColor);
        }
    }
}

LayoutUnit RenderBoxModelObject::containingBlockLogicalWidthForContent() const
{
    if (auto* containingBlock = this->containingBlock())
        return containingBlock->availableLogicalWidth();
    return { };
}

RenderBoxModelObject* RenderBoxModelObject::continuation() const
{
    if (!hasContinuationChainNode())
        return nullptr;

    auto& continuationChainNode = *continuationChainNodeMap().get(this);
    if (!continuationChainNode.next)
        return nullptr;
    return continuationChainNode.next->renderer.get();
}

RenderInline* RenderBoxModelObject::inlineContinuation() const
{
    if (!hasContinuationChainNode())
        return nullptr;

    for (auto* next = continuationChainNodeMap().get(this)->next; next; next = next->next) {
        if (is<RenderInline>(*next->renderer))
            return downcast<RenderInline>(next->renderer.get());
    }
    return nullptr;
}

void RenderBoxModelObject::forRendererAndContinuations(RenderBoxModelObject& renderer, const std::function<void(RenderBoxModelObject&)>& function)
{
    function(renderer);
    if (!renderer.hasContinuationChainNode())
        return;

    for (auto* next = continuationChainNodeMap().get(&renderer)->next; next; next = next->next) {
        if (!next->renderer)
            continue;
        function(*next->renderer);
    }
}

RenderBoxModelObject::ContinuationChainNode* RenderBoxModelObject::continuationChainNode() const
{
    return continuationChainNodeMap().get(this);
}

void RenderBoxModelObject::insertIntoContinuationChainAfter(RenderBoxModelObject& afterRenderer)
{
    ASSERT(isContinuation());
    ASSERT(!continuationChainNodeMap().contains(this));

    auto& after = afterRenderer.ensureContinuationChainNode();
    ensureContinuationChainNode().insertAfter(after);
}

void RenderBoxModelObject::removeFromContinuationChain()
{
    ASSERT(hasContinuationChainNode());
    ASSERT(continuationChainNodeMap().contains(this));
    setHasContinuationChainNode(false);
    continuationChainNodeMap().remove(this);
}

auto RenderBoxModelObject::ensureContinuationChainNode() -> ContinuationChainNode&
{
    setHasContinuationChainNode(true);
    return *continuationChainNodeMap().ensure(this, [&] {
        return makeUnique<ContinuationChainNode>(*this);
    }).iterator->value;
}

RenderTextFragment* RenderBoxModelObject::firstLetterRemainingText() const
{
    if (!isFirstLetter())
        return nullptr;
    return firstLetterRemainingTextMap().get(this).get();
}

void RenderBoxModelObject::setFirstLetterRemainingText(RenderTextFragment& remainingText)
{
    ASSERT(isFirstLetter());
    firstLetterRemainingTextMap().set(this, remainingText);
}

void RenderBoxModelObject::clearFirstLetterRemainingText()
{
    ASSERT(isFirstLetter());
    firstLetterRemainingTextMap().remove(this);
}

bool RenderBoxModelObject::shouldAntialiasLines(GraphicsContext& context)
{
    // FIXME: We may want to not antialias when scaled by an integral value,
    // and we may want to antialias when translated by a non-integral value.
    return !context.getCTM().isIdentityOrTranslationOrFlipped();
}

void RenderBoxModelObject::mapAbsoluteToLocalPoint(OptionSet<MapCoordinatesMode> mode, TransformState& transformState) const
{
    RenderElement* container = this->container();
    if (!container)
        return;
    
    container->mapAbsoluteToLocalPoint(mode, transformState);

    LayoutSize containerOffset = offsetFromContainer(*container, LayoutPoint());

    bool preserve3D = mode.contains(UseTransforms) && (container->style().preserves3D() || style().preserves3D());
    if (mode.contains(UseTransforms) && shouldUseTransformFromContainer(container)) {
        TransformationMatrix t;
        getTransformFromContainer(container, containerOffset, t);
        transformState.applyTransform(t, preserve3D ? TransformState::AccumulateTransform : TransformState::FlattenTransform);
    } else
        transformState.move(containerOffset.width(), containerOffset.height(), preserve3D ? TransformState::AccumulateTransform : TransformState::FlattenTransform);
}

bool RenderBoxModelObject::hasRunningAcceleratedAnimations() const
{
    if (auto styleable = Styleable::fromRenderer(*this))
        return styleable->runningAnimationsAreAllAccelerated();
    return false;
}

void RenderBoxModelObject::collectAbsoluteQuadsForContinuation(Vector<FloatQuad>& quads, bool* wasFixed) const
{
    ASSERT(continuation());
    for (auto* nextInContinuation = this->continuation(); nextInContinuation; nextInContinuation = nextInContinuation->continuation()) {
        if (is<RenderBlock>(*nextInContinuation)) {
            auto& blockBox = downcast<RenderBlock>(*nextInContinuation);
            // For blocks inside inlines, we include margins so that we run right up to the inline boxes
            // above and below us (thus getting merged with them to form a single irregular shape).
            auto logicalRect = FloatRect { 0, -blockBox.collapsedMarginBefore(), blockBox.width(),
                blockBox.height() + blockBox.collapsedMarginBefore() + blockBox.collapsedMarginAfter() };
            nextInContinuation->absoluteQuadsIgnoringContinuation(logicalRect, quads, wasFixed);
            continue;
        }
        nextInContinuation->absoluteQuadsIgnoringContinuation({ }, quads, wasFixed);
    }
}

void RenderBoxModelObject::applyTransform(TransformationMatrix&, const FloatRect&, OptionSet<RenderStyle::TransformOperationOption>) const
{
    // applyTransform() is only used through RenderLayer*, which only invokes this for RenderBox derived renderers, thus not for
    // RenderInline/RenderLineBreak - the other two renderers that inherit from RenderBoxModelObject.
    ASSERT_NOT_REACHED();
}

} // namespace WebCore
