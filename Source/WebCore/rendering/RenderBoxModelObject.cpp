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
#include "BorderPainter.h"
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
    setPaintContainmentApplies(shouldApplyPaintContainment());
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

InterpolationQuality RenderBoxModelObject::chooseInterpolationQuality(GraphicsContext& context, Image& image, const void* layer, const LayoutSize& size) const
{
    return view().imageQualityController().chooseInterpolationQuality(context, const_cast<RenderBoxModelObject*>(this), image, layer, size);
}

void RenderBoxModelObject::paintMaskForTextFillBox(ImageBuffer* maskImage, const FloatRect& maskRect, const InlineIterator::InlineBoxIterator& inlineBox, const LayoutRect& scrolledPaintRect)
{
    GraphicsContext& maskImageContext = maskImage->context();
    maskImageContext.translate(-maskRect.location());

    // Now add the text to the clip. We do this by painting using a special paint phase that signals to
    // the painter it should just modify the clip.
    PaintInfo maskInfo(maskImageContext, LayoutRect { maskRect }, PaintPhase::TextClip, PaintBehavior::ForceBlackText);
    if (inlineBox) {
        auto paintOffset = scrolledPaintRect.location() - toLayoutSize(LayoutPoint(inlineBox->visualRectIgnoringBlockDirection().location()));

        for (auto box = inlineBox->firstLeafBox(), end = inlineBox->endLeafBox(); box != end; box.traverseNextOnLine()) {
            if (!box->isText())
                continue;
            if (auto* legacyTextBox = downcast<LegacyInlineTextBox>(box->legacyInlineBox())) {
                LegacyTextBoxPainter textBoxPainter(*legacyTextBox, maskInfo, paintOffset);
                textBoxPainter.paint();
                continue;
            }
            ModernTextBoxPainter textBoxPainter(box->modernPath().inlineContent(), box->modernPath().box(), maskInfo, paintOffset);
            textBoxPainter.paint();
        }
        return;
    }

    LayoutSize localOffset = is<RenderBox>(*this) ? downcast<RenderBox>(*this).locationOffset() : LayoutSize();
    paint(maskInfo, scrolledPaintRect.location() - localOffset);
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
    if (!image->imageHasNaturalDimensions())
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

void RenderBoxModelObject::applyTransform(TransformationMatrix&, const RenderStyle&, const FloatRect&, OptionSet<RenderStyle::TransformOperationOption>) const
{
    // applyTransform() is only used through RenderLayer*, which only invokes this for RenderBox derived renderers, thus not for
    // RenderInline/RenderLineBreak - the other two renderers that inherit from RenderBoxModelObject.
    ASSERT_NOT_REACHED();
}

} // namespace WebCore
