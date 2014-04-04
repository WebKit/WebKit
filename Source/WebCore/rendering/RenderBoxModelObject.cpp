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

#include "BorderEdge.h"
#include "FloatRoundedRect.h"
#include "Frame.h"
#include "FrameView.h"
#include "GeometryUtilities.h"
#include "GraphicsContext.h"
#include "HTMLFrameOwnerElement.h"
#include "HTMLNames.h"
#include "ImageBuffer.h"
#include "ImageQualityController.h"
#include "Page.h"
#include "Path.h"
#include "RenderBlock.h"
#include "RenderInline.h"
#include "RenderLayer.h"
#include "RenderLayerBacking.h"
#include "RenderLayerCompositor.h"
#include "RenderNamedFlowFragment.h"
#include "RenderNamedFlowThread.h"
#include "RenderRegion.h"
#include "RenderTable.h"
#include "RenderView.h"
#include "ScrollingConstraints.h"
#include "Settings.h"
#include "TransformState.h"

namespace WebCore {

using namespace HTMLNames;

// The HashMap for storing continuation pointers.
// An inline can be split with blocks occuring in between the inline content.
// When this occurs we need a pointer to the next object. We can basically be
// split into a sequence of inlines and blocks. The continuation will either be
// an anonymous block (that houses other blocks) or it will be an inline flow.
// <b><i><p>Hello</p></i></b>. In this example the <i> will have a block as
// its continuation but the <b> will just have an inline as its continuation.
typedef HashMap<const RenderBoxModelObject*, RenderBoxModelObject*> ContinuationMap;
static ContinuationMap* continuationMap = 0;

// This HashMap is similar to the continuation map, but connects first-letter
// renderers to their remaining text fragments.
typedef HashMap<const RenderBoxModelObject*, RenderTextFragment*> FirstLetterRemainingTextMap;
static FirstLetterRemainingTextMap* firstLetterRemainingTextMap = 0;

void RenderBoxModelObject::setSelectionState(SelectionState state)
{
    if (state == SelectionInside && selectionState() != SelectionNone)
        return;

    if ((state == SelectionStart && selectionState() == SelectionEnd)
        || (state == SelectionEnd && selectionState() == SelectionStart))
        RenderLayerModelObject::setSelectionState(SelectionBoth);
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

bool RenderBoxModelObject::startTransition(double timeOffset, CSSPropertyID propertyId, const RenderStyle* fromStyle, const RenderStyle* toStyle)
{
    ASSERT(hasLayer());
    ASSERT(isComposited());
    return layer()->backing()->startTransition(timeOffset, propertyId, fromStyle, toStyle);
}

void RenderBoxModelObject::transitionPaused(double timeOffset, CSSPropertyID propertyId)
{
    ASSERT(hasLayer());
    ASSERT(isComposited());
    layer()->backing()->transitionPaused(timeOffset, propertyId);
}

void RenderBoxModelObject::transitionFinished(CSSPropertyID propertyId)
{
    ASSERT(hasLayer());
    ASSERT(isComposited());
    layer()->backing()->transitionFinished(propertyId);
}

bool RenderBoxModelObject::startAnimation(double timeOffset, const Animation* animation, const KeyframeList& keyframes)
{
    ASSERT(hasLayer());
    ASSERT(isComposited());
    return layer()->backing()->startAnimation(timeOffset, animation, keyframes);
}

void RenderBoxModelObject::animationPaused(double timeOffset, const String& name)
{
    ASSERT(hasLayer());
    ASSERT(isComposited());
    layer()->backing()->animationPaused(timeOffset, name);
}

void RenderBoxModelObject::animationFinished(const String& name)
{
    ASSERT(hasLayer());
    ASSERT(isComposited());
    layer()->backing()->animationFinished(name);
}

void RenderBoxModelObject::suspendAnimations(double time)
{
    ASSERT(hasLayer());
    ASSERT(isComposited());
    layer()->backing()->suspendAnimations(time);
}

bool RenderBoxModelObject::shouldPaintAtLowQuality(GraphicsContext* context, Image* image, const void* layer, const LayoutSize& size)
{
    return view().imageQualityController().shouldPaintAtLowQuality(context, this, image, layer, size);
}

RenderBoxModelObject::RenderBoxModelObject(Element& element, PassRef<RenderStyle> style, unsigned baseTypeFlags)
    : RenderLayerModelObject(element, std::move(style), baseTypeFlags | RenderBoxModelObjectFlag)
{
}

RenderBoxModelObject::RenderBoxModelObject(Document& document, PassRef<RenderStyle> style, unsigned baseTypeFlags)
    : RenderLayerModelObject(document, std::move(style), baseTypeFlags | RenderBoxModelObjectFlag)
{
}

RenderBoxModelObject::~RenderBoxModelObject()
{
}

void RenderBoxModelObject::willBeDestroyed()
{
    // A continuation of this RenderObject should be destroyed at subclasses.
    ASSERT(!continuation());

    // If this is a first-letter object with a remaining text fragment then the
    // entry needs to be cleared from the map.
    if (firstLetterRemainingText())
        setFirstLetterRemainingText(0);

    if (!documentBeingDestroyed())
        view().imageQualityController().rendererWillBeDestroyed(*this);

    RenderLayerModelObject::willBeDestroyed();
}

void RenderBoxModelObject::updateFromStyle()
{
    RenderLayerModelObject::updateFromStyle();

    // Set the appropriate bits for a box model object.  Since all bits are cleared in styleWillChange,
    // we only check for bits that could possibly be set to true.
    const RenderStyle& styleToUse = style();
    setHasBoxDecorations(hasBackground() || styleToUse.hasBorder() || styleToUse.hasAppearance() || styleToUse.boxShadow());
    setInline(styleToUse.isDisplayInlineType());
    setPositionState(styleToUse.position());
    setHorizontalWritingMode(styleToUse.isHorizontalWritingMode());
}

static LayoutSize accumulateInFlowPositionOffsets(const RenderObject* child)
{
    if (!child->isAnonymousBlock() || !child->isInFlowPositioned())
        return LayoutSize();
    LayoutSize offset;
    RenderElement* p = toRenderBlock(child)->inlineElementContinuation();
    while (p && p->isRenderInline()) {
        if (p->isInFlowPositioned()) {
            RenderInline* renderInline = toRenderInline(p);
            offset += renderInline->offsetForInFlowPosition();
        }
        p = p->parent();
    }
    return offset;
}

bool RenderBoxModelObject::hasAutoHeightOrContainingBlockWithAutoHeight() const
{
    Length logicalHeightLength = style().logicalHeight();
    if (logicalHeightLength.isAuto())
        return true;

    // For percentage heights: The percentage is calculated with respect to the height of the generated box's
    // containing block. If the height of the containing block is not specified explicitly (i.e., it depends
    // on content height), and this element is not absolutely positioned, the value computes to 'auto'.
    if (!logicalHeightLength.isPercent() || isOutOfFlowPositioned() || document().inQuirksMode())
        return false;

    // Anonymous block boxes are ignored when resolving percentage values that would refer to it:
    // the closest non-anonymous ancestor box is used instead.
    RenderBlock* cb = containingBlock(); 
    while (cb->isAnonymous() && !cb->isRenderView())
        cb = cb->containingBlock();

    // Matching RenderBox::percentageLogicalHeightIsResolvableFromBlock() by
    // ignoring table cell's attribute value, where it says that table cells violate
    // what the CSS spec says to do with heights. Basically we
    // don't care if the cell specified a height or not.
    if (cb->isTableCell())
        return false;
    
    if (!cb->style().logicalHeight().isAuto() || (!cb->style().logicalTop().isAuto() && !cb->style().logicalBottom().isAuto()))
        return false;

    return true;
}

LayoutSize RenderBoxModelObject::relativePositionOffset() const
{
    LayoutSize offset = accumulateInFlowPositionOffsets(this);

    RenderBlock* containingBlock = this->containingBlock();

    // Objects that shrink to avoid floats normally use available line width when computing containing block width.  However
    // in the case of relative positioning using percentages, we can't do this.  The offset should always be resolved using the
    // available width of the containing block.  Therefore we don't use containingBlockLogicalWidthForContent() here, but instead explicitly
    // call availableWidth on our containing block.
    if (!style().left().isAuto()) {
        if (!style().right().isAuto() && !containingBlock->style().isLeftToRightDirection())
            offset.setWidth(-valueForLength(style().right(), containingBlock->availableWidth()));
        else
            offset.expand(valueForLength(style().left(), containingBlock->availableWidth()), 0);
    } else if (!style().right().isAuto()) {
        offset.expand(-valueForLength(style().right(), containingBlock->availableWidth()), 0);
    }

    // If the containing block of a relatively positioned element does not
    // specify a height, a percentage top or bottom offset should be resolved as
    // auto. An exception to this is if the containing block has the WinIE quirk
    // where <html> and <body> assume the size of the viewport. In this case,
    // calculate the percent offset based on this height.
    // See <https://bugs.webkit.org/show_bug.cgi?id=26396>.
    if (!style().top().isAuto()
        && (!containingBlock->hasAutoHeightOrContainingBlockWithAutoHeight()
            || !style().top().isPercent()
            || containingBlock->stretchesToViewport()))
        offset.expand(0, valueForLength(style().top(), containingBlock->availableHeight()));

    else if (!style().bottom().isAuto()
        && (!containingBlock->hasAutoHeightOrContainingBlockWithAutoHeight()
            || !style().bottom().isPercent()
            || containingBlock->stretchesToViewport()))
        offset.expand(0, -valueForLength(style().bottom(), containingBlock->availableHeight()));

    return offset;
}

LayoutPoint RenderBoxModelObject::adjustedPositionRelativeToOffsetParent(const LayoutPoint& startPoint) const
{
    // If the element is the HTML body element or doesn't have a parent
    // return 0 and stop this algorithm.
    if (isBody() || !parent())
        return LayoutPoint();

    LayoutPoint referencePoint = startPoint;
    referencePoint.move(parent()->offsetForColumns(referencePoint));
    
    // If the offsetParent of the element is null, or is the HTML body element,
    // return the distance between the canvas origin and the left border edge 
    // of the element and stop this algorithm.
    if (const RenderBoxModelObject* offsetParent = this->offsetParent()) {
        if (offsetParent->isBox() && !offsetParent->isBody() && !offsetParent->isTable())
            referencePoint.move(-toRenderBox(offsetParent)->borderLeft(), -toRenderBox(offsetParent)->borderTop());
        if (!isOutOfFlowPositioned() || flowThreadContainingBlock()) {
            if (isRelPositioned())
                referencePoint.move(relativePositionOffset());
            else if (isStickyPositioned())
                referencePoint.move(stickyPositionOffset());
            
            // CSS regions specification says that region flows should return the body element as their offsetParent.
            // Since we will bypass the body’s renderer anyway, just end the loop if we encounter a region flow (named flow thread).
            // See http://dev.w3.org/csswg/css-regions/#cssomview-offset-attributes
            auto curr = parent();
            while (curr != offsetParent && !curr->isRenderNamedFlowThread()) {
                // FIXME: What are we supposed to do inside SVG content?
                if (!isOutOfFlowPositioned()) {
                    if (curr->isBox() && !curr->isTableRow())
                        referencePoint.moveBy(toRenderBox(curr)->topLeftLocation());
                    referencePoint.move(curr->parent()->offsetForColumns(referencePoint));
                }
                curr = curr->parent();
            }
            
            // Compute the offset position for elements inside named flow threads for which the offsetParent was the body.
            // See https://bugs.webkit.org/show_bug.cgi?id=115899
            if (curr->isRenderNamedFlowThread())
                referencePoint = toRenderNamedFlowThread(curr)->adjustedPositionRelativeToOffsetParent(*this, referencePoint);
            else if (offsetParent->isBox() && offsetParent->isBody() && !offsetParent->isPositioned())
                referencePoint.moveBy(toRenderBox(offsetParent)->topLeftLocation());
        }
    }

    return referencePoint;
}

void RenderBoxModelObject::computeStickyPositionConstraints(StickyPositionViewportConstraints& constraints, const FloatRect& constrainingRect) const
{
    constraints.setConstrainingRectAtLastLayout(constrainingRect);

    RenderBlock* containingBlock = this->containingBlock();
    RenderLayer* enclosingClippingLayer = layer()->enclosingOverflowClipLayer(ExcludeSelf);
    RenderBox& enclosingClippingBox = enclosingClippingLayer ? toRenderBox(enclosingClippingLayer->renderer()) : view();

    LayoutRect containerContentRect;
    if (!enclosingClippingLayer || (containingBlock != &enclosingClippingBox))
        containerContentRect = containingBlock->contentBoxRect();
    else {
        containerContentRect = containingBlock->layoutOverflowRect();
        LayoutPoint containerLocation = containerContentRect.location() + LayoutPoint(containingBlock->borderLeft() + containingBlock->paddingLeft(),
            containingBlock->borderTop() + containingBlock->paddingTop());
        containerContentRect.setLocation(containerLocation);
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

    // Finally compute container rect relative to the scrolling ancestor.
    FloatRect containerRectRelativeToScrollingAncestor = containingBlock->localToContainerQuad(FloatRect(containerContentRect), &enclosingClippingBox).boundingBox();
    if (enclosingClippingLayer) {
        FloatPoint containerLocationRelativeToScrollingAncestor = containerRectRelativeToScrollingAncestor.location() -
            FloatSize(enclosingClippingBox.borderLeft() + enclosingClippingBox.paddingLeft(),
            enclosingClippingBox.borderTop() + enclosingClippingBox.paddingTop());
        if (&enclosingClippingBox != containingBlock)
            containerLocationRelativeToScrollingAncestor += enclosingClippingLayer->scrollOffset();
        containerRectRelativeToScrollingAncestor.setLocation(containerLocationRelativeToScrollingAncestor);
    }
    constraints.setContainingBlockRect(containerRectRelativeToScrollingAncestor);

    // Now compute the sticky box rect, also relative to the scrolling ancestor.
    LayoutRect stickyBoxRect = frameRectForStickyPositioning();
    LayoutRect flippedStickyBoxRect = stickyBoxRect;
    containingBlock->flipForWritingMode(flippedStickyBoxRect);
    FloatRect stickyBoxRelativeToScrollingAnecstor = flippedStickyBoxRect;

    // FIXME: sucks to call localToContainerQuad again, but we can't just offset from the previously computed rect if there are transforms.
    // Map to the view to avoid including page scale factor.
    FloatPoint stickyLocationRelativeToScrollingAncestor = flippedStickyBoxRect.location() + containingBlock->localToContainerQuad(FloatRect(FloatPoint(), containingBlock->size()), &enclosingClippingBox).boundingBox().location();
    if (enclosingClippingLayer) {
        stickyLocationRelativeToScrollingAncestor -= FloatSize(enclosingClippingBox.borderLeft() + enclosingClippingBox.paddingLeft(),
            enclosingClippingBox.borderTop() + enclosingClippingBox.paddingTop());
        if (&enclosingClippingBox != containingBlock)
            stickyLocationRelativeToScrollingAncestor += enclosingClippingLayer->scrollOffset();
    }
    // FIXME: For now, assume that |this| is not transformed.
    stickyBoxRelativeToScrollingAnecstor.setLocation(stickyLocationRelativeToScrollingAncestor);
    constraints.setStickyBoxRect(stickyBoxRelativeToScrollingAnecstor);

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

LayoutSize RenderBoxModelObject::stickyPositionOffset() const
{
    FloatRect constrainingRect;

    ASSERT(hasLayer());
    RenderLayer* enclosingClippingLayer = layer()->enclosingOverflowClipLayer(ExcludeSelf);
    if (enclosingClippingLayer) {
        RenderBox& enclosingClippingBox = toRenderBox(enclosingClippingLayer->renderer());
        LayoutRect clipRect = enclosingClippingBox.overflowClipRect(LayoutPoint(), 0); // FIXME: make this work in regions.
        clipRect.contract(LayoutSize(enclosingClippingBox.paddingLeft() + enclosingClippingBox.paddingRight(),
            enclosingClippingBox.paddingTop() + enclosingClippingBox.paddingBottom()));
        constrainingRect = enclosingClippingBox.localToContainerQuad(FloatRect(clipRect), &view()).boundingBox();

        FloatPoint scrollOffset = FloatPoint() + enclosingClippingLayer->scrollOffset();
        constrainingRect.setLocation(scrollOffset);
    } else {
        LayoutRect viewportRect = view().frameView().viewportConstrainedVisibleContentRect();
        float scale = view().frameView().frame().frameScaleFactor();
        viewportRect.scale(1 / scale);
        constrainingRect = viewportRect;
    }
    
    StickyPositionViewportConstraints constraints;
    computeStickyPositionConstraints(constraints, constrainingRect);
    
    // The sticky offset is physical, so we can just return the delta computed in absolute coords (though it may be wrong with transforms).
    return LayoutSize(constraints.computeStickyOffset(constrainingRect));
}

LayoutSize RenderBoxModelObject::offsetForInFlowPosition() const
{
    if (isRelPositioned())
        return relativePositionOffset();

    if (isStickyPositioned())
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

int RenderBoxModelObject::pixelSnappedOffsetWidth() const
{
    return snapSizeToPixel(offsetWidth(), offsetLeft());
}

int RenderBoxModelObject::pixelSnappedOffsetHeight() const
{
    return snapSizeToPixel(offsetHeight(), offsetTop());
}

LayoutUnit RenderBoxModelObject::computedCSSPadding(const Length& padding) const
{
    LayoutUnit w = 0;
    if (padding.isPercent())
        w = containingBlockLogicalWidthForContent();
    return minimumValueForLength(padding, w);
}

RoundedRect RenderBoxModelObject::getBackgroundRoundedRect(const LayoutRect& borderRect, InlineFlowBox* box, LayoutUnit inlineBoxWidth, LayoutUnit inlineBoxHeight,
    bool includeLogicalLeftEdge, bool includeLogicalRightEdge) const
{
    RoundedRect border = style().getRoundedBorderFor(borderRect, &view(), includeLogicalLeftEdge, includeLogicalRightEdge);
    if (box && (box->nextLineBox() || box->prevLineBox())) {
        RoundedRect segmentBorder = style().getRoundedBorderFor(LayoutRect(0, 0, inlineBoxWidth, inlineBoxHeight), &view(), includeLogicalLeftEdge, includeLogicalRightEdge);
        border.setRadii(segmentBorder.radii());
    }
    return border;
}

void RenderBoxModelObject::clipRoundedInnerRect(GraphicsContext * context, const FloatRect& rect, const FloatRoundedRect& clipRect)
{
    if (clipRect.isRenderable())
        context->clipRoundedRect(clipRect);
    else {
        // We create a rounded rect for each of the corners and clip it, while making sure we clip opposing corners together.
        if (!clipRect.radii().topLeft().isEmpty() || !clipRect.radii().bottomRight().isEmpty()) {
            FloatRect topCorner(clipRect.rect().x(), clipRect.rect().y(), rect.maxX() - clipRect.rect().x(), rect.maxY() - clipRect.rect().y());
            FloatRoundedRect::Radii topCornerRadii;
            topCornerRadii.setTopLeft(clipRect.radii().topLeft());
            context->clipRoundedRect(FloatRoundedRect(topCorner, topCornerRadii));

            FloatRect bottomCorner(rect.x(), rect.y(), clipRect.rect().maxX() - rect.x(), clipRect.rect().maxY() - rect.y());
            FloatRoundedRect::Radii bottomCornerRadii;
            bottomCornerRadii.setBottomRight(clipRect.radii().bottomRight());
            context->clipRoundedRect(FloatRoundedRect(bottomCorner, bottomCornerRadii));
        } 

        if (!clipRect.radii().topRight().isEmpty() || !clipRect.radii().bottomLeft().isEmpty()) {
            FloatRect topCorner(rect.x(), clipRect.rect().y(), clipRect.rect().maxX() - rect.x(), rect.maxY() - clipRect.rect().y());
            FloatRoundedRect::Radii topCornerRadii;
            topCornerRadii.setTopRight(clipRect.radii().topRight());
            context->clipRoundedRect(FloatRoundedRect(topCorner, topCornerRadii));

            FloatRect bottomCorner(clipRect.rect().x(), rect.y(), rect.maxX() - clipRect.rect().x(), clipRect.rect().maxY() - rect.y());
            FloatRoundedRect::Radii bottomCornerRadii;
            bottomCornerRadii.setBottomLeft(clipRect.radii().bottomLeft());
            context->clipRoundedRect(FloatRoundedRect(bottomCorner, bottomCornerRadii));
        }
    }
}

static LayoutRect shrinkRectByOneDevicePixel(const GraphicsContext& context, const LayoutRect& rect, float devicePixelRatio)
{
    LayoutRect shrunkRect = rect;
    AffineTransform transform = context.getCTM();
    shrunkRect.inflateX(-ceilToDevicePixel(LayoutUnit::fromPixel(1) / transform.xScale(), devicePixelRatio));
    shrunkRect.inflateY(-ceilToDevicePixel(LayoutUnit::fromPixel(1) / transform.yScale(), devicePixelRatio));
    return shrunkRect;
}

LayoutRect RenderBoxModelObject::borderInnerRectAdjustedForBleedAvoidance(const GraphicsContext& context, const LayoutRect& rect, BackgroundBleedAvoidance bleedAvoidance) const
{
    if (bleedAvoidance != BackgroundBleedBackgroundOverBorder)
        return rect;

    // We shrink the rectangle by one device pixel on each side to make it fully overlap the anti-aliased background border
    return shrinkRectByOneDevicePixel(context, rect, document().deviceScaleFactor());
}

RoundedRect RenderBoxModelObject::backgroundRoundedRectAdjustedForBleedAvoidance(const GraphicsContext& context, const LayoutRect& borderRect, BackgroundBleedAvoidance bleedAvoidance, InlineFlowBox* box, const LayoutSize& boxSize, bool includeLogicalLeftEdge, bool includeLogicalRightEdge) const
{
    if (bleedAvoidance == BackgroundBleedShrinkBackground) {
        // We shrink the rectangle by one device pixel on each side because the bleed is one pixel maximum.
        return getBackgroundRoundedRect(shrinkRectByOneDevicePixel(context, borderRect, document().deviceScaleFactor()), box, boxSize.width(), boxSize.height(),
            includeLogicalLeftEdge, includeLogicalRightEdge);
    }
    if (bleedAvoidance == BackgroundBleedBackgroundOverBorder)
        return style().getRoundedInnerBorderFor(borderRect, includeLogicalLeftEdge, includeLogicalRightEdge);

    return getBackgroundRoundedRect(borderRect, box, boxSize.width(), boxSize.height(), includeLogicalLeftEdge, includeLogicalRightEdge);
}

static void applyBoxShadowForBackground(GraphicsContext* context, RenderStyle* style)
{
    const ShadowData* boxShadow = style->boxShadow();
    while (boxShadow->style() != Normal)
        boxShadow = boxShadow->next();

    FloatSize shadowOffset(boxShadow->x(), boxShadow->y());
    if (!boxShadow->isWebkitBoxShadow())
        context->setShadow(shadowOffset, boxShadow->radius(), boxShadow->color(), style->colorSpace());
    else
        context->setLegacyShadow(shadowOffset, boxShadow->radius(), boxShadow->color(), style->colorSpace());
}

void RenderBoxModelObject::paintMaskForTextFillBox(ImageBuffer* maskImage, const IntRect& maskRect, InlineFlowBox* box, const LayoutRect& scrolledPaintRect, RenderNamedFlowFragment* namedFlowFragment)
{
    GraphicsContext* maskImageContext = maskImage->context();
    maskImageContext->translate(-maskRect.x(), -maskRect.y());

    // Now add the text to the clip. We do this by painting using a special paint phase that signals to
    // InlineTextBoxes that they should just add their contents to the clip.
    PaintInfo info(maskImageContext, maskRect, PaintPhaseTextClip, PaintBehaviorForceBlackText, 0, namedFlowFragment);
    if (box) {
        const RootInlineBox& rootBox = box->root();
        box->paint(info, LayoutPoint(scrolledPaintRect.x() - box->x(), scrolledPaintRect.y() - box->y()), rootBox.lineTop(), rootBox.lineBottom());
    } else if (isRenderNamedFlowFragmentContainer()) {
        RenderNamedFlowFragment* region = toRenderBlockFlow(this)->renderNamedFlowFragment();
        if (!region->flowThread())
            return;
        region->flowThread()->layer()->paintNamedFlowThreadInsideRegion(maskImageContext, region, maskRect, maskRect.location(), PaintBehaviorForceBlackText, RenderLayer::PaintLayerTemporaryClipRects);
    } else {
        LayoutSize localOffset = isBox() ? toRenderBox(this)->locationOffset() : LayoutSize();
        paint(info, scrolledPaintRect.location() - localOffset);
    }
}

void RenderBoxModelObject::paintFillLayerExtended(const PaintInfo& paintInfo, const Color& color, const FillLayer* bgLayer, const LayoutRect& rect,
    BackgroundBleedAvoidance bleedAvoidance, InlineFlowBox* box, const LayoutSize& boxSize, CompositeOperator op, RenderElement* backgroundObject)
{
    GraphicsContext* context = paintInfo.context;
    if (context->paintingDisabled() || rect.isEmpty())
        return;

    bool includeLeftEdge = box ? box->includeLogicalLeftEdge() : true;
    bool includeRightEdge = box ? box->includeLogicalRightEdge() : true;

    bool hasRoundedBorder = style().hasBorderRadius() && (includeLeftEdge || includeRightEdge);
    bool clippedWithLocalScrolling = hasOverflowClip() && bgLayer->attachment() == LocalBackgroundAttachment;
    bool isBorderFill = bgLayer->clip() == BorderFillBox;
    bool isRoot = this->isRoot();

    Color bgColor = color;
    StyleImage* bgImage = bgLayer->image();
    bool shouldPaintBackgroundImage = bgImage && bgImage->canRender(this, style().effectiveZoom());
    
    bool forceBackgroundToWhite = false;
    if (document().printing()) {
        if (style().printColorAdjust() == PrintColorAdjustEconomy)
            forceBackgroundToWhite = true;
        if (frame().settings().shouldPrintBackgrounds())
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
        bool shouldPaintBackgroundColor = !bgLayer->next() && bgColor.isValid() && bgColor.alpha();
        if (shouldPaintBackgroundImage || shouldPaintBackgroundColor) {
            bgColor = Color::white;
            shouldPaintBackgroundImage = false;
        }
    }

    bool colorVisible = bgColor.isValid() && bgColor.alpha();
    float deviceScaleFactor = document().deviceScaleFactor();
    FloatRect pixelSnappedRect = pixelSnappedForPainting(rect, deviceScaleFactor);

    // Fast path for drawing simple color backgrounds.
    if (!isRoot && !clippedWithLocalScrolling && !shouldPaintBackgroundImage && isBorderFill && !bgLayer->next()) {
        if (!colorVisible)
            return;

        bool boxShadowShouldBeAppliedToBackground = this->boxShadowShouldBeAppliedToBackground(bleedAvoidance, box);
        GraphicsContextStateSaver shadowStateSaver(*context, boxShadowShouldBeAppliedToBackground);
        if (boxShadowShouldBeAppliedToBackground)
            applyBoxShadowForBackground(context, &style());

        if (hasRoundedBorder && bleedAvoidance != BackgroundBleedUseTransparencyLayer) {
            FloatRoundedRect pixelSnappedBorder = backgroundRoundedRectAdjustedForBleedAvoidance(*context, rect, bleedAvoidance, box, boxSize,
                includeLeftEdge, includeRightEdge).pixelSnappedRoundedRectForPainting(deviceScaleFactor);
            if (pixelSnappedBorder.isRenderable())
                context->fillRoundedRect(pixelSnappedBorder, bgColor, style().colorSpace());
            else {
                context->save();
                clipRoundedInnerRect(context, pixelSnappedRect, pixelSnappedBorder);
                context->fillRect(pixelSnappedBorder.rect(), bgColor, style().colorSpace());
                context->restore();
            }
        } else
            context->fillRect(pixelSnappedRect, bgColor, style().colorSpace());

        return;
    }

    // BorderFillBox radius clipping is taken care of by BackgroundBleedUseTransparencyLayer
    bool clipToBorderRadius = hasRoundedBorder && !(isBorderFill && bleedAvoidance == BackgroundBleedUseTransparencyLayer);
    GraphicsContextStateSaver clipToBorderStateSaver(*context, clipToBorderRadius);
    if (clipToBorderRadius) {
        RoundedRect border = isBorderFill ? backgroundRoundedRectAdjustedForBleedAvoidance(*context, rect, bleedAvoidance, box, boxSize, includeLeftEdge, includeRightEdge) : getBackgroundRoundedRect(rect, box, boxSize.width(), boxSize.height(), includeLeftEdge, includeRightEdge);

        // Clip to the padding or content boxes as necessary.
        if (bgLayer->clip() == ContentFillBox) {
            border = style().getRoundedInnerBorderFor(border.rect(),
                paddingTop() + borderTop(), paddingBottom() + borderBottom(), paddingLeft() + borderLeft(), paddingRight() + borderRight(), includeLeftEdge, includeRightEdge);
        } else if (bgLayer->clip() == PaddingFillBox)
            border = style().getRoundedInnerBorderFor(border.rect(), includeLeftEdge, includeRightEdge);

        clipRoundedInnerRect(context, pixelSnappedRect, border.pixelSnappedRoundedRectForPainting(deviceScaleFactor));
    }
    
    LayoutUnit bLeft = includeLeftEdge ? borderLeft() : LayoutUnit::fromPixel(0);
    LayoutUnit bRight = includeRightEdge ? borderRight() : LayoutUnit::fromPixel(0);
    LayoutUnit pLeft = includeLeftEdge ? paddingLeft() : LayoutUnit();
    LayoutUnit pRight = includeRightEdge ? paddingRight() : LayoutUnit();

    GraphicsContextStateSaver clipWithScrollingStateSaver(*context, clippedWithLocalScrolling);
    LayoutRect scrolledPaintRect = rect;
    if (clippedWithLocalScrolling) {
        // Clip to the overflow area.
        RenderBox* thisBox = toRenderBox(this);
        context->clip(thisBox->overflowClipRect(rect.location(), paintInfo.renderNamedFlowFragment));
        
        // Adjust the paint rect to reflect a scrolled content box with borders at the ends.
        IntSize offset = thisBox->scrolledContentOffset();
        scrolledPaintRect.move(-offset);
        scrolledPaintRect.setWidth(bLeft + layer()->scrollWidth() + bRight);
        scrolledPaintRect.setHeight(borderTop() + layer()->scrollHeight() + borderBottom());
    }
    
    GraphicsContextStateSaver backgroundClipStateSaver(*context, false);
    std::unique_ptr<ImageBuffer> maskImage;
    IntRect maskRect;

    if (bgLayer->clip() == PaddingFillBox || bgLayer->clip() == ContentFillBox) {
        // Clip to the padding or content boxes as necessary.
        if (!clipToBorderRadius) {
            bool includePadding = bgLayer->clip() == ContentFillBox;
            LayoutRect clipRect = LayoutRect(scrolledPaintRect.x() + bLeft + (includePadding ? pLeft : LayoutUnit()),
                scrolledPaintRect.y() + borderTop() + (includePadding ? paddingTop() : LayoutUnit()),
                scrolledPaintRect.width() - bLeft - bRight - (includePadding ? pLeft + pRight : LayoutUnit()),
                scrolledPaintRect.height() - borderTop() - borderBottom() - (includePadding ? paddingTop() + paddingBottom() : LayoutUnit()));
            backgroundClipStateSaver.save();
            context->clip(clipRect);
        }
    } else if (bgLayer->clip() == TextFillBox) {
        // We have to draw our text into a mask that can then be used to clip background drawing.
        // First figure out how big the mask has to be.  It should be no bigger than what we need
        // to actually render, so we should intersect the dirty rect with the border box of the background.
        maskRect = pixelSnappedIntRect(rect);
        maskRect.intersect(pixelSnappedIntRect(paintInfo.rect));

        // Now create the mask.
        maskImage = context->createCompatibleBuffer(maskRect.size());
        if (!maskImage)
            return;
        paintMaskForTextFillBox(maskImage.get(), maskRect, box, scrolledPaintRect, paintInfo.renderNamedFlowFragment);

        // The mask has been created.  Now we just need to clip to it.
        backgroundClipStateSaver.save();
        context->clip(maskRect);
        context->beginTransparencyLayer(1);
    }

    // Only fill with a base color (e.g., white) if we're the root document, since iframes/frames with
    // no background in the child document should show the parent's background.
    bool isOpaqueRoot = false;
    if (isRoot) {
        isOpaqueRoot = true;
        if (!bgLayer->next() && !(bgColor.isValid() && bgColor.alpha() == 255)) {
            Element* ownerElement = document().ownerElement();
            if (ownerElement) {
                if (!ownerElement->hasTagName(frameTag)) {
                    // Locate the <body> element using the DOM.  This is easier than trying
                    // to crawl around a render tree with potential :before/:after content and
                    // anonymous blocks created by inline <body> tags etc.  We can locate the <body>
                    // render object very easily via the DOM.
                    HTMLElement* body = document().body();
                    if (body) {
                        // Can't scroll a frameset document anyway.
                        isOpaqueRoot = body->hasTagName(framesetTag);
                    } else {
                        // SVG documents and XML documents with SVG root nodes are transparent.
                        isOpaqueRoot = !document().hasSVGRootNode();
                    }
                }
            } else
                isOpaqueRoot = !view().frameView().isTransparent();
        }
        view().frameView().setContentIsOpaque(isOpaqueRoot);
    }

    // Paint the color first underneath all images, culled if background image occludes it.
    // FIXME: In the bgLayer->hasFiniteBounds() case, we could improve the culling test
    // by verifying whether the background image covers the entire layout rect.
    if (!bgLayer->next()) {
        LayoutRect backgroundRect(scrolledPaintRect);
        bool boxShadowShouldBeAppliedToBackground = this->boxShadowShouldBeAppliedToBackground(bleedAvoidance, box);
        if (boxShadowShouldBeAppliedToBackground || !shouldPaintBackgroundImage || !bgLayer->hasOpaqueImage(this) || !bgLayer->hasRepeatXY()) {
            if (!boxShadowShouldBeAppliedToBackground)
                backgroundRect.intersect(paintInfo.rect);

            // If we have an alpha and we are painting the root element, go ahead and blend with the base background color.
            Color baseColor;
            bool shouldClearBackground = false;
            if (isOpaqueRoot) {
                baseColor = view().frameView().baseBackgroundColor();
                if (!baseColor.alpha())
                    shouldClearBackground = true;
            }

            GraphicsContextStateSaver shadowStateSaver(*context, boxShadowShouldBeAppliedToBackground);
            if (boxShadowShouldBeAppliedToBackground)
                applyBoxShadowForBackground(context, &style());

            FloatRect backgroundRectForPainting = pixelSnappedForPainting(backgroundRect, deviceScaleFactor);
            if (baseColor.alpha()) {
                if (bgColor.alpha())
                    baseColor = baseColor.blend(bgColor);

                context->fillRect(backgroundRectForPainting, baseColor, style().colorSpace(), CompositeCopy);
            } else if (bgColor.alpha()) {
                CompositeOperator operation = shouldClearBackground ? CompositeCopy : context->compositeOperation();
                context->fillRect(backgroundRectForPainting, bgColor, style().colorSpace(), operation);
            } else if (shouldClearBackground)
                context->clearRect(backgroundRectForPainting);
        }
    }

    // no progressive loading of the background image
    if (shouldPaintBackgroundImage) {
        BackgroundImageGeometry geometry;
        calculateBackgroundImageGeometry(paintInfo.paintContainer, bgLayer, scrolledPaintRect, geometry, backgroundObject);
        geometry.clip(LayoutRect(pixelSnappedRect));
        if (!geometry.destRect().isEmpty()) {
            CompositeOperator compositeOp = op == CompositeSourceOver ? bgLayer->composite() : op;
            auto clientForBackgroundImage = backgroundObject ? backgroundObject : this;
            RefPtr<Image> image = bgImage->image(clientForBackgroundImage, geometry.tileSize());
            context->setDrawLuminanceMask(bgLayer->maskSourceType() == MaskLuminance);
            bool useLowQualityScaling = shouldPaintAtLowQuality(context, image.get(), bgLayer, geometry.tileSize());
            if (image.get())
                image->setSpaceSize(geometry.spaceSize());
            context->drawTiledImage(image.get(), style().colorSpace(), geometry.destRect(), geometry.relativePhase(), geometry.tileSize(), compositeOp, useLowQualityScaling, bgLayer->blendMode());
        }
    }

    if (bgLayer->clip() == TextFillBox) {
        context->drawImageBuffer(maskImage.get(), ColorSpaceDeviceRGB, maskRect, CompositeDestinationIn);
        context->endTransparencyLayer();
    }
}

static inline int resolveWidthForRatio(LayoutUnit height, const FloatSize& intrinsicRatio)
{
    return height * intrinsicRatio.width() / intrinsicRatio.height();
}

static inline int resolveHeightForRatio(LayoutUnit width, const FloatSize& intrinsicRatio)
{
    return width * intrinsicRatio.height() / intrinsicRatio.width();
}

static inline LayoutSize resolveAgainstIntrinsicWidthOrHeightAndRatio(const LayoutSize& size, const FloatSize& intrinsicRatio, LayoutUnit useWidth, LayoutUnit useHeight)
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

static inline LayoutSize resolveAgainstIntrinsicRatio(const LayoutSize& size, const FloatSize& intrinsicRatio)
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

    // Intrinsic dimensions expressed as percentages must be resolved relative to the dimensions of the rectangle
    // that establishes the coordinate system for the 'background-position' property. 
    
    // FIXME: Remove unnecessary rounding when layout is off ints: webkit.org/b/63656
    if (intrinsicWidth.isPercent() && intrinsicHeight.isPercent() && intrinsicRatio.isEmpty()) {
        // Resolve width/height percentages against positioningAreaSize, only if no intrinsic ratio is provided.
        float resolvedWidth = positioningAreaSize.width() * intrinsicWidth.percent() / 100;
        float resolvedHeight = positioningAreaSize.height() * intrinsicHeight.percent() / 100;
        return LayoutSize(resolvedWidth, resolvedHeight);
    }

    LayoutSize resolvedSize(intrinsicWidth.isFixed() ? intrinsicWidth.value() : 0, intrinsicHeight.isFixed() ? intrinsicHeight.value() : 0);
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
        return resolveAgainstIntrinsicWidthOrHeightAndRatio(positioningAreaSize, intrinsicRatio, resolvedSize.width(), resolvedSize.height());

    // If the image has no intrinsic dimensions and has an intrinsic ratio the dimensions must be assumed to be the
    // largest dimensions at that ratio such that neither dimension exceeds the dimensions of the rectangle that
    // establishes the coordinate system for the 'background-position' property.
    if (!intrinsicRatio.isEmpty())
        return resolveAgainstIntrinsicRatio(positioningAreaSize, intrinsicRatio);

    // If the image has no intrinsic ratio either, then the dimensions must be assumed to be the rectangle that
    // establishes the coordinate system for the 'background-position' property.
    return positioningAreaSize;
}

LayoutSize RenderBoxModelObject::calculateFillTileSize(const FillLayer* fillLayer, const LayoutSize& positioningAreaSize) const
{
    StyleImage* image = fillLayer->image();
    EFillSizeType type = fillLayer->size().type;

    LayoutSize imageIntrinsicSize = calculateImageIntrinsicDimensions(image, positioningAreaSize, ScaleByEffectiveZoom);
    imageIntrinsicSize.scale(1 / image->imageScaleFactor(), 1 / image->imageScaleFactor());
    switch (type) {
        case SizeLength: {
            LayoutSize tileSize = positioningAreaSize;

            Length layerWidth = fillLayer->size().size.width();
            Length layerHeight = fillLayer->size().size.height();

            if (layerWidth.isFixed())
                tileSize.setWidth(layerWidth.value());
            else if (layerWidth.isPercent() || layerWidth.isViewportPercentage())
                tileSize.setWidth(valueForLength(layerWidth, positioningAreaSize.width()));
            
            if (layerHeight.isFixed())
                tileSize.setHeight(layerHeight.value());
            else if (layerHeight.isPercent() || layerHeight.isViewportPercentage())
                tileSize.setHeight(valueForLength(layerHeight, positioningAreaSize.height()));

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
        case SizeNone: {
            // If both values are ‘auto’ then the intrinsic width and/or height of the image should be used, if any.
            if (!imageIntrinsicSize.isEmpty())
                return imageIntrinsicSize;

            // If the image has neither an intrinsic width nor an intrinsic height, its size is determined as for ‘contain’.
            type = Contain;
        }
        FALLTHROUGH;
        case Contain:
        case Cover: {
            float horizontalScaleFactor = imageIntrinsicSize.width() ? (positioningAreaSize.width() / imageIntrinsicSize.width()).toFloat() : 1;
            float verticalScaleFactor = imageIntrinsicSize.height() ? (positioningAreaSize.height() / imageIntrinsicSize.height()).toFloat() : 1;
            float scaleFactor = type == Contain ? std::min(horizontalScaleFactor, verticalScaleFactor) : std::max(horizontalScaleFactor, verticalScaleFactor);
            float deviceScaleFactor = document().deviceScaleFactor();
            return LayoutSize(std::max<LayoutUnit>(1 / deviceScaleFactor, imageIntrinsicSize.width() * scaleFactor),
                std::max<LayoutUnit>(1 / deviceScaleFactor, imageIntrinsicSize.height() * scaleFactor));
       }
    }

    ASSERT_NOT_REACHED();
    return LayoutSize();
}

void RenderBoxModelObject::BackgroundImageGeometry::setNoRepeatX(LayoutUnit xOffset)
{
    m_destRect.move(std::max<LayoutUnit>(xOffset, 0), 0);
    m_phase.setX(-std::min<LayoutUnit>(xOffset, 0));
    m_destRect.setWidth(m_tileSize.width() + std::min<float>(xOffset, 0));
}
void RenderBoxModelObject::BackgroundImageGeometry::setNoRepeatY(LayoutUnit yOffset)
{
    m_destRect.move(0, std::max<LayoutUnit>(yOffset, 0));
    m_phase.setY(-std::min<LayoutUnit>(yOffset, 0));
    m_destRect.setHeight(m_tileSize.height() + std::min<float>(yOffset, 0));
}

void RenderBoxModelObject::BackgroundImageGeometry::useFixedAttachment(const LayoutPoint& attachmentPoint)
{
    FloatPoint alignedPoint = attachmentPoint;
    m_phase.move(std::max<LayoutUnit>(alignedPoint.x() - m_destRect.x(), 0), std::max<LayoutUnit>(alignedPoint.y() - m_destRect.y(), 0));
}

void RenderBoxModelObject::BackgroundImageGeometry::clip(const LayoutRect& clipRect)
{
    m_destRect.intersect(clipRect);
}

LayoutPoint RenderBoxModelObject::BackgroundImageGeometry::relativePhase() const
{
    LayoutPoint phase = m_phase;
    phase += m_destRect.location() - m_destOrigin;
    return phase;
}

bool RenderBoxModelObject::fixedBackgroundPaintsInLocalCoordinates() const
{
    if (!isRoot())
        return false;

    if (view().frameView().paintBehavior() & PaintBehaviorFlattenCompositingLayers)
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

void RenderBoxModelObject::pixelSnapBackgroundImageGeometryForPainting(BackgroundImageGeometry& geometry) const
{
    float deviceScaleFactor = document().deviceScaleFactor();
    // FIXME: We need a better rounding strategy to round/space out tiles.
    geometry.setTileSize(LayoutSize(pixelSnappedForPainting(LayoutRect(geometry.destRect().location(), geometry.tileSize()), deviceScaleFactor).size()));
    geometry.setSpaceSize(LayoutSize(pixelSnappedForPainting(LayoutRect(LayoutPoint(), geometry.spaceSize()), deviceScaleFactor).size()));
    geometry.setDestOrigin(LayoutPoint(roundedForPainting(geometry.destOrigin(), deviceScaleFactor)));
    geometry.setDestRect(LayoutRect(pixelSnappedForPainting(geometry.destRect(), deviceScaleFactor)));
    geometry.setPhase(LayoutPoint(roundedForPainting(geometry.phase(), deviceScaleFactor)));
}

void RenderBoxModelObject::calculateBackgroundImageGeometry(const RenderLayerModelObject* paintContainer, const FillLayer* fillLayer, const LayoutRect& paintRect,
    BackgroundImageGeometry& geometry, RenderElement* backgroundObject) const
{
    LayoutUnit left = 0;
    LayoutUnit top = 0;
    LayoutSize positioningAreaSize;
    // Determine the background positioning area and set destRect to the background painting area.
    // destRect will be adjusted later if the background is non-repeating.
    // FIXME: transforms spec says that fixed backgrounds behave like scroll inside transforms. https://bugs.webkit.org/show_bug.cgi?id=15679
    bool fixedAttachment = fillLayer->attachment() == FixedBackgroundAttachment;
    
#if ENABLE(FAST_MOBILE_SCROLLING)
    if (view().frameView().canBlitOnScroll()) {
        // As a side effect of an optimization to blit on scroll, we do not honor the CSS
        // property "background-attachment: fixed" because it may result in rendering
        // artifacts. Note, these artifacts only appear if we are blitting on scroll of
        // a page that has fixed background images.
        fixedAttachment = false;
    }
#endif

    if (!fixedAttachment) {
        geometry.setDestRect(paintRect);

        LayoutUnit right = 0;
        LayoutUnit bottom = 0;
        // Scroll and Local.
        if (fillLayer->origin() != BorderFillBox) {
            left = borderLeft();
            right = borderRight();
            top = borderTop();
            bottom = borderBottom();
            if (fillLayer->origin() == ContentFillBox) {
                left += paddingLeft();
                right += paddingRight();
                top += paddingTop();
                bottom += paddingBottom();
            }
        }

        // The background of the box generated by the root element covers the entire canvas including
        // its margins. Since those were added in already, we have to factor them out when computing
        // the background positioning area.
        if (isRoot()) {
            positioningAreaSize = toRenderBox(this)->size() - LayoutSize(left + right, top + bottom);
            if (view().frameView().hasExtendedBackgroundRectForPainting()) {
                LayoutRect extendedBackgroundRect = view().frameView().extendedBackgroundRectForPainting();
                left += (marginLeft() - extendedBackgroundRect.x());
                top += (marginTop() - extendedBackgroundRect.y());
            }
        } else
            positioningAreaSize = paintRect.size() - LayoutSize(left + right, top + bottom);
    } else {
        geometry.setHasNonLocalGeometry();

        LayoutRect viewportRect = view().viewRect();
        if (fixedBackgroundPaintsInLocalCoordinates())
            viewportRect.setLocation(LayoutPoint());
        else
            viewportRect.setLocation(toLayoutPoint(view().frameView().scrollOffsetForFixedPosition()));

        if (paintContainer)
            viewportRect.moveBy(LayoutPoint(-paintContainer->localToAbsolute(FloatPoint())));

        geometry.setDestRect(viewportRect);
        positioningAreaSize = geometry.destRect().size();
    }

    auto clientForBackgroundImage = backgroundObject ? backgroundObject : this;
    LayoutSize fillTileSize = calculateFillTileSize(fillLayer, positioningAreaSize);
    fillLayer->image()->setContainerSizeForRenderer(clientForBackgroundImage, fillTileSize, style().effectiveZoom());
    geometry.setTileSize(fillTileSize);

    EFillRepeat backgroundRepeatX = fillLayer->repeatX();
    EFillRepeat backgroundRepeatY = fillLayer->repeatY();
    LayoutUnit availableWidth = positioningAreaSize.width() - geometry.tileSize().width();
    LayoutUnit availableHeight = positioningAreaSize.height() - geometry.tileSize().height();

    LayoutUnit computedXPosition = minimumValueForLength(fillLayer->xPosition(), availableWidth, true);
    if (backgroundRepeatX == RoundFill && positioningAreaSize.width() > 0 && fillTileSize.width() > 0) {
        int numTiles = std::max(1, roundToInt(positioningAreaSize.width() / fillTileSize.width()));
        if (fillLayer->size().size.height().isAuto() && backgroundRepeatY != RoundFill)
            fillTileSize.setHeight(fillTileSize.height() * positioningAreaSize.width() / (numTiles * fillTileSize.width()));

        fillTileSize.setWidth(positioningAreaSize.width() / numTiles);
        geometry.setTileSize(fillTileSize);
        geometry.setPhaseX(geometry.tileSize().width() ? geometry.tileSize().width() - fmodf((computedXPosition + left), geometry.tileSize().width()) : 0);
        geometry.setSpaceSize(LayoutSize());
    }

    LayoutUnit computedYPosition = minimumValueForLength(fillLayer->yPosition(), availableHeight, true);
    if (backgroundRepeatY == RoundFill && positioningAreaSize.height() > 0 && fillTileSize.height() > 0) {
        int numTiles = std::max(1, roundToInt(positioningAreaSize.height() / fillTileSize.height()));
        if (fillLayer->size().size.width().isAuto() && backgroundRepeatX != RoundFill)
            fillTileSize.setWidth(fillTileSize.width() * positioningAreaSize.height() / (numTiles * fillTileSize.height()));

        fillTileSize.setHeight(positioningAreaSize.height() / numTiles);
        geometry.setTileSize(fillTileSize);
        geometry.setPhaseY(geometry.tileSize().height() ? geometry.tileSize().height() - fmodf((computedYPosition + top), geometry.tileSize().height()) : 0);
        geometry.setSpaceSize(LayoutSize());
    }

    if (backgroundRepeatX == RepeatFill) {
        geometry.setPhaseX(geometry.tileSize().width() ? geometry.tileSize().width() -  fmodf(computedXPosition + left, geometry.tileSize().width()): 0);
        geometry.setSpaceSize(LayoutSize(0, geometry.spaceSize().height()));
    } else if (backgroundRepeatX == SpaceFill && fillTileSize.width() > 0) {
        LayoutUnit space = getSpace(positioningAreaSize.width(), geometry.tileSize().width());
        LayoutUnit actualWidth = geometry.tileSize().width() + space;

        if (space >= 0) {
            computedXPosition = minimumValueForLength(Length(), availableWidth, true);
            geometry.setSpaceSize(LayoutSize(space, 0));
            geometry.setPhaseX(actualWidth ? actualWidth - fmodf((computedXPosition + left), actualWidth) : 0);
        } else
            backgroundRepeatX = NoRepeatFill;
    }
    if (backgroundRepeatX == NoRepeatFill) {
        LayoutUnit xOffset = fillLayer->backgroundXOrigin() == RightEdge ? availableWidth - computedXPosition : computedXPosition;
        geometry.setNoRepeatX(left + xOffset);
        geometry.setSpaceSize(LayoutSize(0, geometry.spaceSize().height()));
    }

    if (backgroundRepeatY == RepeatFill) {
        geometry.setPhaseY(geometry.tileSize().height() ? geometry.tileSize().height() - fmodf(computedYPosition + top, geometry.tileSize().height()) : 0);
        geometry.setSpaceSize(LayoutSize(geometry.spaceSize().width(), 0));
    } else if (backgroundRepeatY == SpaceFill && fillTileSize.height() > 0) {
        LayoutUnit space = getSpace(positioningAreaSize.height(), geometry.tileSize().height());
        LayoutUnit actualHeight = geometry.tileSize().height() + space;

        if (space >= 0) {
            computedYPosition = minimumValueForLength(Length(), availableHeight, true);
            geometry.setSpaceSize(LayoutSize(geometry.spaceSize().width(), space));
            geometry.setPhaseY(actualHeight ? actualHeight - fmodf((computedYPosition + top), actualHeight) : 0);
        } else
            backgroundRepeatY = NoRepeatFill;
    }
    if (backgroundRepeatY == NoRepeatFill) {
        LayoutUnit yOffset = fillLayer->backgroundYOrigin() == BottomEdge ? availableHeight - computedYPosition : computedYPosition;
        geometry.setNoRepeatY(top + yOffset);
        geometry.setSpaceSize(LayoutSize(geometry.spaceSize().width(), 0));
    }

    if (fixedAttachment)
        geometry.useFixedAttachment(paintRect.location());

    geometry.clip(paintRect);
    geometry.setDestOrigin(geometry.destRect().location());

    pixelSnapBackgroundImageGeometryForPainting(geometry);
}

void RenderBoxModelObject::getGeometryForBackgroundImage(const RenderLayerModelObject* paintContainer, FloatRect& destRect, FloatPoint& phase, FloatSize& tileSize) const
{
    const FillLayer* backgroundLayer = style().backgroundLayers();
    BackgroundImageGeometry geometry;
    LayoutRect paintRect = LayoutRect(destRect);
    calculateBackgroundImageGeometry(paintContainer, backgroundLayer, paintRect, geometry);
    phase = geometry.phase();
    tileSize = geometry.tileSize();
    destRect = geometry.destRect();
}

static LayoutUnit computeBorderImageSide(Length borderSlice, LayoutUnit borderSide, LayoutUnit imageSide, LayoutUnit boxExtent, RenderView* renderView)
{
    if (borderSlice.isRelative())
        return borderSlice.value() * borderSide;
    if (borderSlice.isAuto())
        return imageSide;
    return valueForLength(borderSlice, boxExtent, renderView);
}

bool RenderBoxModelObject::paintNinePieceImage(GraphicsContext* graphicsContext, const LayoutRect& rect, const RenderStyle& style,
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
    LayoutRect rectWithOutsets = rect;
    rectWithOutsets.expand(style.imageOutsets(ninePieceImage));
    IntRect borderImageRect = pixelSnappedIntRect(rectWithOutsets);

    LayoutSize imageSize = calculateImageIntrinsicDimensions(styleImage, borderImageRect.size(), DoNotScaleByEffectiveZoom);

    // If both values are ‘auto’ then the intrinsic width and/or height of the image should be used, if any.
    styleImage->setContainerSizeForRenderer(this, imageSize, style.effectiveZoom());

    int imageWidth = imageSize.width();
    int imageHeight = imageSize.height();
    RenderView* renderView = &view();

    float imageScaleFactor = styleImage->imageScaleFactor();
    int topSlice = std::min<int>(imageHeight, valueForLength(ninePieceImage.imageSlices().top(), imageHeight)) * imageScaleFactor;
    int rightSlice = std::min<int>(imageWidth, valueForLength(ninePieceImage.imageSlices().right(), imageWidth)) * imageScaleFactor;
    int bottomSlice = std::min<int>(imageHeight, valueForLength(ninePieceImage.imageSlices().bottom(), imageHeight)) * imageScaleFactor;
    int leftSlice = std::min<int>(imageWidth, valueForLength(ninePieceImage.imageSlices().left(), imageWidth)) * imageScaleFactor;

    ENinePieceImageRule hRule = ninePieceImage.horizontalRule();
    ENinePieceImageRule vRule = ninePieceImage.verticalRule();

    int topWidth = computeBorderImageSide(ninePieceImage.borderSlices().top(), style.borderTopWidth(), topSlice, borderImageRect.height(), renderView);
    int rightWidth = computeBorderImageSide(ninePieceImage.borderSlices().right(), style.borderRightWidth(), rightSlice, borderImageRect.width(), renderView);
    int bottomWidth = computeBorderImageSide(ninePieceImage.borderSlices().bottom(), style.borderBottomWidth(), bottomSlice, borderImageRect.height(), renderView);
    int leftWidth = computeBorderImageSide(ninePieceImage.borderSlices().left(), style.borderLeftWidth(), leftSlice, borderImageRect.width(), renderView);
    
    // Reduce the widths if they're too large.
    // The spec says: Given Lwidth as the width of the border image area, Lheight as its height, and Wside as the border image width
    // offset for the side, let f = min(Lwidth/(Wleft+Wright), Lheight/(Wtop+Wbottom)). If f < 1, then all W are reduced by
    // multiplying them by f.
    int borderSideWidth = std::max(1, leftWidth + rightWidth);
    int borderSideHeight = std::max(1, topWidth + bottomWidth);
    float borderSideScaleFactor = std::min((float)borderImageRect.width() / borderSideWidth, (float)borderImageRect.height() / borderSideHeight);
    if (borderSideScaleFactor < 1) {
        topWidth *= borderSideScaleFactor;
        rightWidth *= borderSideScaleFactor;
        bottomWidth *= borderSideScaleFactor;
        leftWidth *= borderSideScaleFactor;
    }

    bool drawLeft = leftSlice > 0 && leftWidth > 0;
    bool drawTop = topSlice > 0 && topWidth > 0;
    bool drawRight = rightSlice > 0 && rightWidth > 0;
    bool drawBottom = bottomSlice > 0 && bottomWidth > 0;
    bool drawMiddle = ninePieceImage.fill() && (imageWidth - leftSlice - rightSlice) > 0 && (borderImageRect.width() - leftWidth - rightWidth) > 0
                      && (imageHeight - topSlice - bottomSlice) > 0 && (borderImageRect.height() - topWidth - bottomWidth) > 0;

    RefPtr<Image> image = styleImage->image(this, imageSize);
    ColorSpace colorSpace = style.colorSpace();
    
    float destinationWidth = borderImageRect.width() - leftWidth - rightWidth;
    float destinationHeight = borderImageRect.height() - topWidth - bottomWidth;
    
    float sourceWidth = imageWidth - leftSlice - rightSlice;
    float sourceHeight = imageHeight - topSlice - bottomSlice;
    
    float leftSideScale = drawLeft ? (float)leftWidth / leftSlice : 1;
    float rightSideScale = drawRight ? (float)rightWidth / rightSlice : 1;
    float topSideScale = drawTop ? (float)topWidth / topSlice : 1;
    float bottomSideScale = drawBottom ? (float)bottomWidth / bottomSlice : 1;
    
    if (drawLeft) {
        // Paint the top and bottom left corners.

        // The top left corner rect is (tx, ty, leftWidth, topWidth)
        // The rect to use from within the image is obtained from our slice, and is (0, 0, leftSlice, topSlice)
        if (drawTop)
            graphicsContext->drawImage(image.get(), colorSpace, IntRect(borderImageRect.location(), IntSize(leftWidth, topWidth)),
                LayoutRect(0, 0, leftSlice, topSlice), op, ImageOrientationDescription());

        // The bottom left corner rect is (tx, ty + h - bottomWidth, leftWidth, bottomWidth)
        // The rect to use from within the image is (0, imageHeight - bottomSlice, leftSlice, botomSlice)
        if (drawBottom)
            graphicsContext->drawImage(image.get(), colorSpace, IntRect(borderImageRect.x(), borderImageRect.maxY() - bottomWidth, leftWidth, bottomWidth),
                LayoutRect(0, imageHeight - bottomSlice, leftSlice, bottomSlice), op, ImageOrientationDescription());

        // Paint the left edge.
        // Have to scale and tile into the border rect.
        if (sourceHeight > 0)
            graphicsContext->drawTiledImage(image.get(), colorSpace, IntRect(borderImageRect.x(), borderImageRect.y() + topWidth, leftWidth,
                                            destinationHeight),
                                            IntRect(0, topSlice, leftSlice, sourceHeight),
                                            FloatSize(leftSideScale, leftSideScale), Image::StretchTile, (Image::TileRule)vRule, op);
    }

    if (drawRight) {
        // Paint the top and bottom right corners
        // The top right corner rect is (tx + w - rightWidth, ty, rightWidth, topWidth)
        // The rect to use from within the image is obtained from our slice, and is (imageWidth - rightSlice, 0, rightSlice, topSlice)
        if (drawTop)
            graphicsContext->drawImage(image.get(), colorSpace, IntRect(borderImageRect.maxX() - rightWidth, borderImageRect.y(), rightWidth, topWidth),
                LayoutRect(imageWidth - rightSlice, 0, rightSlice, topSlice), op, ImageOrientationDescription());

        // The bottom right corner rect is (tx + w - rightWidth, ty + h - bottomWidth, rightWidth, bottomWidth)
        // The rect to use from within the image is (imageWidth - rightSlice, imageHeight - bottomSlice, rightSlice, bottomSlice)
        if (drawBottom)
            graphicsContext->drawImage(image.get(), colorSpace, IntRect(borderImageRect.maxX() - rightWidth, borderImageRect.maxY() - bottomWidth, rightWidth, bottomWidth),
                LayoutRect(imageWidth - rightSlice, imageHeight - bottomSlice, rightSlice, bottomSlice), op, ImageOrientationDescription());

        // Paint the right edge.
        if (sourceHeight > 0)
            graphicsContext->drawTiledImage(image.get(), colorSpace, IntRect(borderImageRect.maxX() - rightWidth, borderImageRect.y() + topWidth, rightWidth,
                                            destinationHeight),
                                            IntRect(imageWidth - rightSlice, topSlice, rightSlice, sourceHeight),
                                            FloatSize(rightSideScale, rightSideScale),
                                            Image::StretchTile, (Image::TileRule)vRule, op);
    }

    // Paint the top edge.
    if (drawTop && sourceWidth > 0)
        graphicsContext->drawTiledImage(image.get(), colorSpace, IntRect(borderImageRect.x() + leftWidth, borderImageRect.y(), destinationWidth, topWidth),
                                        IntRect(leftSlice, 0, sourceWidth, topSlice),
                                        FloatSize(topSideScale, topSideScale), (Image::TileRule)hRule, Image::StretchTile, op);

    // Paint the bottom edge.
    if (drawBottom && sourceWidth > 0)
        graphicsContext->drawTiledImage(image.get(), colorSpace, IntRect(borderImageRect.x() + leftWidth, borderImageRect.maxY() - bottomWidth,
                                        destinationWidth, bottomWidth),
                                        IntRect(leftSlice, imageHeight - bottomSlice, sourceWidth, bottomSlice),
                                        FloatSize(bottomSideScale, bottomSideScale),
                                        (Image::TileRule)hRule, Image::StretchTile, op);

    // Paint the middle.
    if (drawMiddle) {
        FloatSize middleScaleFactor(1, 1);
        if (drawTop)
            middleScaleFactor.setWidth(topSideScale);
        else if (drawBottom)
            middleScaleFactor.setWidth(bottomSideScale);
        if (drawLeft)
            middleScaleFactor.setHeight(leftSideScale);
        else if (drawRight)
            middleScaleFactor.setHeight(rightSideScale);
            
        // For "stretch" rules, just override the scale factor and replace. We only had to do this for the
        // center tile, since sides don't even use the scale factor unless they have a rule other than "stretch".
        // The middle however can have "stretch" specified in one axis but not the other, so we have to
        // correct the scale here.
        if (hRule == StretchImageRule)
            middleScaleFactor.setWidth(destinationWidth / sourceWidth);
            
        if (vRule == StretchImageRule)
            middleScaleFactor.setHeight(destinationHeight / sourceHeight);
        
        graphicsContext->drawTiledImage(image.get(), colorSpace,
            IntRect(borderImageRect.x() + leftWidth, borderImageRect.y() + topWidth, destinationWidth, destinationHeight),
            IntRect(leftSlice, topSlice, sourceWidth, sourceHeight),
            middleScaleFactor, (Image::TileRule)hRule, (Image::TileRule)vRule, op);
    }

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

static bool borderWillArcInnerEdge(const LayoutSize& firstRadius, const FloatSize& secondRadius)
{
    return !firstRadius.isZero() || !secondRadius.isZero();
}

inline bool styleRequiresClipPolygon(EBorderStyle style)
{
    return style == DOTTED || style == DASHED; // These are drawn with a stroke, so we have to clip to get corner miters.
}

static bool borderStyleFillsBorderArea(EBorderStyle style)
{
    return !(style == DOTTED || style == DASHED || style == DOUBLE);
}

static bool borderStyleHasInnerDetail(EBorderStyle style)
{
    return style == GROOVE || style == RIDGE || style == DOUBLE;
}

static bool borderStyleIsDottedOrDashed(EBorderStyle style)
{
    return style == DOTTED || style == DASHED;
}

// OUTSET darkens the bottom and right (and maybe lightens the top and left)
// INSET darkens the top and left (and maybe lightens the bottom and right)
static inline bool borderStyleHasUnmatchedColorsAtCorner(EBorderStyle style, BoxSide side, BoxSide adjacentSide)
{
    // These styles match at the top/left and bottom/right.
    if (style == INSET || style == GROOVE || style == RIDGE || style == OUTSET) {
        const BorderEdgeFlags topRightFlags = edgeFlagForSide(BSTop) | edgeFlagForSide(BSRight);
        const BorderEdgeFlags bottomLeftFlags = edgeFlagForSide(BSBottom) | edgeFlagForSide(BSLeft);

        BorderEdgeFlags flags = edgeFlagForSide(side) | edgeFlagForSide(adjacentSide);
        return flags == topRightFlags || flags == bottomLeftFlags;
    }
    return false;
}

static inline bool colorsMatchAtCorner(BoxSide side, BoxSide adjacentSide, const BorderEdge edges[])
{
    if (edges[side].shouldRender() != edges[adjacentSide].shouldRender())
        return false;

    if (!edgesShareColor(edges[side], edges[adjacentSide]))
        return false;

    return !borderStyleHasUnmatchedColorsAtCorner(edges[side].style(), side, adjacentSide);
}


static inline bool colorNeedsAntiAliasAtCorner(BoxSide side, BoxSide adjacentSide, const BorderEdge edges[])
{
    if (!edges[side].color().hasAlpha())
        return false;

    if (edges[side].shouldRender() != edges[adjacentSide].shouldRender())
        return false;

    if (!edgesShareColor(edges[side], edges[adjacentSide]))
        return true;

    return borderStyleHasUnmatchedColorsAtCorner(edges[side].style(), side, adjacentSide);
}

// This assumes that we draw in order: top, bottom, left, right.
static inline bool willBeOverdrawn(BoxSide side, BoxSide adjacentSide, const BorderEdge edges[])
{
    switch (side) {
    case BSTop:
    case BSBottom:
        if (edges[adjacentSide].presentButInvisible())
            return false;

        if (!edgesShareColor(edges[side], edges[adjacentSide]) && edges[adjacentSide].color().hasAlpha())
            return false;
        
        if (!borderStyleFillsBorderArea(edges[adjacentSide].style()))
            return false;

        return true;

    case BSLeft:
    case BSRight:
        // These draw last, so are never overdrawn.
        return false;
    }
    return false;
}

static inline bool borderStylesRequireMitre(BoxSide side, BoxSide adjacentSide, EBorderStyle style, EBorderStyle adjacentStyle)
{
    if (style == DOUBLE || adjacentStyle == DOUBLE || adjacentStyle == GROOVE || adjacentStyle == RIDGE)
        return true;

    if (borderStyleIsDottedOrDashed(style) != borderStyleIsDottedOrDashed(adjacentStyle))
        return true;

    if (style != adjacentStyle)
        return true;

    return borderStyleHasUnmatchedColorsAtCorner(style, side, adjacentSide);
}

static bool joinRequiresMitre(BoxSide side, BoxSide adjacentSide, const BorderEdge edges[], bool allowOverdraw)
{
    if ((edges[side].isTransparent() && edges[adjacentSide].isTransparent()) || !edges[adjacentSide].isPresent())
        return false;

    if (allowOverdraw && willBeOverdrawn(side, adjacentSide, edges))
        return false;

    if (!edgesShareColor(edges[side], edges[adjacentSide]))
        return true;

    if (borderStylesRequireMitre(side, adjacentSide, edges[side].style(), edges[adjacentSide].style()))
        return true;
    
    return false;
}

void RenderBoxModelObject::paintOneBorderSide(GraphicsContext* graphicsContext, const RenderStyle& style, const RoundedRect& outerBorder, const RoundedRect& innerBorder,
    const LayoutRect& sideRect, BoxSide side, BoxSide adjacentSide1, BoxSide adjacentSide2, const BorderEdge edges[], const Path* path,
    BackgroundBleedAvoidance bleedAvoidance, bool includeLogicalLeftEdge, bool includeLogicalRightEdge, bool antialias, const Color* overrideColor)
{
    const BorderEdge& edgeToRender = edges[side];
    ASSERT(edgeToRender.widthForPainting());
    const BorderEdge& adjacentEdge1 = edges[adjacentSide1];
    const BorderEdge& adjacentEdge2 = edges[adjacentSide2];

    bool mitreAdjacentSide1 = joinRequiresMitre(side, adjacentSide1, edges, !antialias);
    bool mitreAdjacentSide2 = joinRequiresMitre(side, adjacentSide2, edges, !antialias);
    
    bool adjacentSide1StylesMatch = colorsMatchAtCorner(side, adjacentSide1, edges);
    bool adjacentSide2StylesMatch = colorsMatchAtCorner(side, adjacentSide2, edges);

    const Color& colorToPaint = overrideColor ? *overrideColor : edgeToRender.color();

    if (path) {
        GraphicsContextStateSaver stateSaver(*graphicsContext);
        if (innerBorder.isRenderable())
            clipBorderSidePolygon(graphicsContext, outerBorder, innerBorder, side, adjacentSide1StylesMatch, adjacentSide2StylesMatch);
        else
            clipBorderSideForComplexInnerPath(graphicsContext, outerBorder, innerBorder, side, edges);
        float thickness = std::max(std::max(edgeToRender.widthForPainting(), adjacentEdge1.widthForPainting()), adjacentEdge2.widthForPainting());
        drawBoxSideFromPath(graphicsContext, outerBorder.rect(), *path, edges, edgeToRender.widthForPainting(), thickness, side, style,
            colorToPaint, edgeToRender.style(), bleedAvoidance, includeLogicalLeftEdge, includeLogicalRightEdge);
    } else {
        bool clipForStyle = styleRequiresClipPolygon(edgeToRender.style()) && (mitreAdjacentSide1 || mitreAdjacentSide2);
        bool clipAdjacentSide1 = colorNeedsAntiAliasAtCorner(side, adjacentSide1, edges) && mitreAdjacentSide1;
        bool clipAdjacentSide2 = colorNeedsAntiAliasAtCorner(side, adjacentSide2, edges) && mitreAdjacentSide2;
        bool shouldClip = clipForStyle || clipAdjacentSide1 || clipAdjacentSide2;
        
        GraphicsContextStateSaver clipStateSaver(*graphicsContext, shouldClip);
        if (shouldClip) {
            bool aliasAdjacentSide1 = clipAdjacentSide1 || (clipForStyle && mitreAdjacentSide1);
            bool aliasAdjacentSide2 = clipAdjacentSide2 || (clipForStyle && mitreAdjacentSide2);
            clipBorderSidePolygon(graphicsContext, outerBorder, innerBorder, side, !aliasAdjacentSide1, !aliasAdjacentSide2);
            // Since we clipped, no need to draw with a mitre.
            mitreAdjacentSide1 = false;
            mitreAdjacentSide2 = false;
        }
        
        drawLineForBoxSide(graphicsContext, sideRect.x(), sideRect.y(), sideRect.maxX(), sideRect.maxY(), side, colorToPaint, edgeToRender.style(),
            mitreAdjacentSide1 ? adjacentEdge1.widthForPainting() : 0, mitreAdjacentSide2 ? adjacentEdge2.widthForPainting() : 0, antialias);
    }
}

static LayoutRect calculateSideRect(const RoundedRect& outerBorder, const BorderEdge edges[], int side)
{
    LayoutRect sideRect = outerBorder.rect();
    float width = edges[side].widthForPainting();

    if (side == BSTop)
        sideRect.setHeight(width);
    else if (side == BSBottom)
        sideRect.shiftYEdgeTo(sideRect.maxY() - width);
    else if (side == BSLeft)
        sideRect.setWidth(width);
    else
        sideRect.shiftXEdgeTo(sideRect.maxX() - width);

    return sideRect;
}

void RenderBoxModelObject::paintBorderSides(GraphicsContext* graphicsContext, const RenderStyle& style, const RoundedRect& outerBorder, const RoundedRect& innerBorder,
    const IntPoint& innerBorderAdjustment, const BorderEdge edges[], BorderEdgeFlags edgeSet, BackgroundBleedAvoidance bleedAvoidance,
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

    if (edges[BSTop].shouldRender() && includesEdge(edgeSet, BSTop)) {
        LayoutRect sideRect = outerBorder.rect();
        sideRect.setHeight(edges[BSTop].widthForPainting() + innerBorderAdjustment.y());

        bool usePath = renderRadii && (borderStyleHasInnerDetail(edges[BSTop].style()) || borderWillArcInnerEdge(innerBorder.radii().topLeft(), innerBorder.radii().topRight()));
        paintOneBorderSide(graphicsContext, style, outerBorder, innerBorder, sideRect, BSTop, BSLeft, BSRight, edges, usePath ? &roundedPath : 0, bleedAvoidance, includeLogicalLeftEdge, includeLogicalRightEdge, antialias, overrideColor);
    }

    if (edges[BSBottom].shouldRender() && includesEdge(edgeSet, BSBottom)) {
        LayoutRect sideRect = outerBorder.rect();
        sideRect.shiftYEdgeTo(sideRect.maxY() - edges[BSBottom].widthForPainting() - innerBorderAdjustment.y());

        bool usePath = renderRadii && (borderStyleHasInnerDetail(edges[BSBottom].style()) || borderWillArcInnerEdge(innerBorder.radii().bottomLeft(), innerBorder.radii().bottomRight()));
        paintOneBorderSide(graphicsContext, style, outerBorder, innerBorder, sideRect, BSBottom, BSLeft, BSRight, edges, usePath ? &roundedPath : 0, bleedAvoidance, includeLogicalLeftEdge, includeLogicalRightEdge, antialias, overrideColor);
    }

    if (edges[BSLeft].shouldRender() && includesEdge(edgeSet, BSLeft)) {
        LayoutRect sideRect = outerBorder.rect();
        sideRect.setWidth(edges[BSLeft].widthForPainting() + innerBorderAdjustment.x());

        bool usePath = renderRadii && (borderStyleHasInnerDetail(edges[BSLeft].style()) || borderWillArcInnerEdge(innerBorder.radii().bottomLeft(), innerBorder.radii().topLeft()));
        paintOneBorderSide(graphicsContext, style, outerBorder, innerBorder, sideRect, BSLeft, BSTop, BSBottom, edges, usePath ? &roundedPath : 0, bleedAvoidance, includeLogicalLeftEdge, includeLogicalRightEdge, antialias, overrideColor);
    }

    if (edges[BSRight].shouldRender() && includesEdge(edgeSet, BSRight)) {
        LayoutRect sideRect = outerBorder.rect();
        sideRect.shiftXEdgeTo(sideRect.maxX() - edges[BSRight].widthForPainting() - innerBorderAdjustment.x());

        bool usePath = renderRadii && (borderStyleHasInnerDetail(edges[BSRight].style()) || borderWillArcInnerEdge(innerBorder.radii().bottomRight(), innerBorder.radii().topRight()));
        paintOneBorderSide(graphicsContext, style, outerBorder, innerBorder, sideRect, BSRight, BSTop, BSBottom, edges, usePath ? &roundedPath : 0, bleedAvoidance, includeLogicalLeftEdge, includeLogicalRightEdge, antialias, overrideColor);
    }
}

void RenderBoxModelObject::paintTranslucentBorderSides(GraphicsContext* graphicsContext, const RenderStyle& style, const RoundedRect& outerBorder, const RoundedRect& innerBorder, const IntPoint& innerBorderAdjustment,
    const BorderEdge edges[], BorderEdgeFlags edgesToDraw, BackgroundBleedAvoidance bleedAvoidance, bool includeLogicalLeftEdge, bool includeLogicalRightEdge, bool antialias)
{
    // willBeOverdrawn assumes that we draw in order: top, bottom, left, right.
    // This is different from BoxSide enum order.
    static BoxSide paintOrder[] = { BSTop, BSBottom, BSLeft, BSRight };

    while (edgesToDraw) {
        // Find undrawn edges sharing a color.
        Color commonColor;
        
        BorderEdgeFlags commonColorEdgeSet = 0;
        for (size_t i = 0; i < sizeof(paintOrder) / sizeof(paintOrder[0]); ++i) {
            BoxSide currSide = paintOrder[i];
            if (!includesEdge(edgesToDraw, currSide))
                continue;

            bool includeEdge;
            if (!commonColorEdgeSet) {
                commonColor = edges[currSide].color();
                includeEdge = true;
            } else
                includeEdge = edges[currSide].color() == commonColor;

            if (includeEdge)
                commonColorEdgeSet |= edgeFlagForSide(currSide);
        }

        bool useTransparencyLayer = includesAdjacentEdges(commonColorEdgeSet) && commonColor.hasAlpha();
        if (useTransparencyLayer) {
            graphicsContext->beginTransparencyLayer(static_cast<float>(commonColor.alpha()) / 255);
            commonColor = Color(commonColor.red(), commonColor.green(), commonColor.blue());
        }

        paintBorderSides(graphicsContext, style, outerBorder, innerBorder, innerBorderAdjustment, edges, commonColorEdgeSet, bleedAvoidance, includeLogicalLeftEdge, includeLogicalRightEdge, antialias, &commonColor);
            
        if (useTransparencyLayer)
            graphicsContext->endTransparencyLayer();
        
        edgesToDraw &= ~commonColorEdgeSet;
    }
}

void RenderBoxModelObject::paintBorder(const PaintInfo& info, const LayoutRect& rect, const RenderStyle& style,
                                       BackgroundBleedAvoidance bleedAvoidance, bool includeLogicalLeftEdge, bool includeLogicalRightEdge)
{
    GraphicsContext* graphicsContext = info.context;
    // border-image is not affected by border-radius.
    if (paintNinePieceImage(graphicsContext, rect, style, style.borderImage()))
        return;

    if (graphicsContext->paintingDisabled())
        return;

    BorderEdge edges[4];
    BorderEdge::getBorderEdgeInfo(edges, style, document().deviceScaleFactor(), includeLogicalLeftEdge, includeLogicalRightEdge);
    RoundedRect outerBorder = style.getRoundedBorderFor(rect, &view(), includeLogicalLeftEdge, includeLogicalRightEdge);
    RoundedRect innerBorder = style.getRoundedInnerBorderFor(borderInnerRectAdjustedForBleedAvoidance(*graphicsContext, rect, bleedAvoidance), includeLogicalLeftEdge, includeLogicalRightEdge);

    bool haveAlphaColor = false;
    bool haveAllSolidEdges = true;
    bool haveAllDoubleEdges = true;
    int numEdgesVisible = 4;
    bool allEdgesShareColor = true;
    int firstVisibleEdge = -1;
    BorderEdgeFlags edgesToDraw = 0;

    for (int i = BSTop; i <= BSLeft; ++i) {
        const BorderEdge& currEdge = edges[i];

        if (edges[i].shouldRender())
            edgesToDraw |= edgeFlagForSide(static_cast<BoxSide>(i));

        if (currEdge.presentButInvisible()) {
            --numEdgesVisible;
            allEdgesShareColor = false;
            continue;
        }
        
        if (!currEdge.widthForPainting()) {
            --numEdgesVisible;
            continue;
        }

        if (firstVisibleEdge == -1)
            firstVisibleEdge = i;
        else if (currEdge.color() != edges[firstVisibleEdge].color())
            allEdgesShareColor = false;

        if (currEdge.color().hasAlpha())
            haveAlphaColor = true;
        
        if (currEdge.style() != SOLID)
            haveAllSolidEdges = false;

        if (currEdge.style() != DOUBLE)
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
                for (int side = BSTop; side <= BSLeft; ++side) {
                    LayoutUnit outerWidth;
                    LayoutUnit innerWidth;
                    edges[side].getDoubleBorderStripeWidths(outerWidth, innerWidth);

                    if (side == BSTop) {
                        innerThirdRect.shiftYEdgeTo(innerThirdRect.y() + innerWidth);
                        outerThirdRect.shiftYEdgeTo(outerThirdRect.y() + outerWidth);
                    } else if (side == BSBottom) {
                        innerThirdRect.setHeight(innerThirdRect.height() - innerWidth);
                        outerThirdRect.setHeight(outerThirdRect.height() - outerWidth);
                    } else if (side == BSLeft) {
                        innerThirdRect.shiftXEdgeTo(innerThirdRect.x() + innerWidth);
                        outerThirdRect.shiftXEdgeTo(outerThirdRect.x() + outerWidth);
                    } else {
                        innerThirdRect.setWidth(innerThirdRect.width() - innerWidth);
                        outerThirdRect.setWidth(outerThirdRect.width() - outerWidth);
                    }
                }

                FloatRoundedRect pixelSnappedOuterThird = outerBorder.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
                pixelSnappedOuterThird.setRect(pixelSnappedForPainting(outerThirdRect, deviceScaleFactor));

                if (pixelSnappedOuterThird.isRounded() && bleedAvoidance != BackgroundBleedUseTransparencyLayer)
                    path.addRoundedRect(pixelSnappedOuterThird);
                else
                    path.addRect(pixelSnappedOuterThird.rect());

                FloatRoundedRect pixelSnappedInnerThird = innerBorder.pixelSnappedRoundedRectForPainting(deviceScaleFactor);
                pixelSnappedInnerThird.setRect(pixelSnappedForPainting(innerThirdRect, deviceScaleFactor));
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
            
            graphicsContext->setFillRule(RULE_EVENODD);
            graphicsContext->setFillColor(edges[firstVisibleEdge].color(), style.colorSpace());
            graphicsContext->fillPath(path);
            return;
        } 
        // Avoid creating transparent layers
        if (haveAllSolidEdges && numEdgesVisible != 4 && !outerBorder.isRounded() && haveAlphaColor) {
            Path path;

            for (int i = BSTop; i <= BSLeft; ++i) {
                const BorderEdge& currEdge = edges[i];
                if (currEdge.shouldRender()) {
                    LayoutRect sideRect = calculateSideRect(outerBorder, edges, i);
                    path.addRect(sideRect);
                }
            }

            graphicsContext->setFillRule(RULE_NONZERO);
            graphicsContext->setFillColor(edges[firstVisibleEdge].color(), style.colorSpace());
            graphicsContext->fillPath(path);
            return;
        }
    }

    bool clipToOuterBorder = outerBorder.isRounded();
    GraphicsContextStateSaver stateSaver(*graphicsContext, clipToOuterBorder);
    if (clipToOuterBorder) {
        // Clip to the inner and outer radii rects.
        if (bleedAvoidance != BackgroundBleedUseTransparencyLayer)
            graphicsContext->clipRoundedRect(FloatRoundedRect(outerBorder));
        // isRenderable() check avoids issue described in https://bugs.webkit.org/show_bug.cgi?id=38787
        // The inside will be clipped out later (in clipBorderSideForComplexInnerPath)
        if (innerBorder.isRenderable())
            graphicsContext->clipOutRoundedRect(innerBorder.pixelSnappedRoundedRectForPainting(deviceScaleFactor));
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

void RenderBoxModelObject::drawBoxSideFromPath(GraphicsContext* graphicsContext, const LayoutRect& borderRect, const Path& borderPath, const BorderEdge edges[],
    float thickness, float drawThickness, BoxSide side, const RenderStyle& style, Color color, EBorderStyle borderStyle, BackgroundBleedAvoidance bleedAvoidance,
    bool includeLogicalLeftEdge, bool includeLogicalRightEdge)
{
    if (thickness <= 0)
        return;

    if (borderStyle == DOUBLE && thickness < 3)
        borderStyle = SOLID;

    switch (borderStyle) {
    case BNONE:
    case BHIDDEN:
        return;
    case DOTTED:
    case DASHED: {
        graphicsContext->setStrokeColor(color, style.colorSpace());

        // The stroke is doubled here because the provided path is the 
        // outside edge of the border so half the stroke is clipped off. 
        // The extra multiplier is so that the clipping mask can antialias
        // the edges to prevent jaggies.
        graphicsContext->setStrokeThickness(drawThickness * 2 * 1.1f);
        graphicsContext->setStrokeStyle(borderStyle == DASHED ? DashedStroke : DottedStroke);

        // If the number of dashes that fit in the path is odd and non-integral then we
        // will have an awkwardly-sized dash at the end of the path. To try to avoid that
        // here, we simply make the whitespace dashes ever so slightly bigger.
        // FIXME: This could be even better if we tried to manipulate the dash offset
        // and possibly the gapLength to get the corners dash-symmetrical.
        float dashLength = thickness * ((borderStyle == DASHED) ? 3.0f : 1.0f);
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
            graphicsContext->setLineDash(lineDash, dashLength);
        }
        
        // FIXME: stroking the border path causes issues with tight corners:
        // https://bugs.webkit.org/show_bug.cgi?id=58711
        // Also, to get the best appearance we should stroke a path between the two borders.
        graphicsContext->strokePath(borderPath);
        return;
    }
    case DOUBLE: {
        // Get the inner border rects for both the outer border line and the inner border line
        LayoutUnit outerBorderTopWidth;
        LayoutUnit innerBorderTopWidth;
        edges[BSTop].getDoubleBorderStripeWidths(outerBorderTopWidth, innerBorderTopWidth);

        LayoutUnit outerBorderRightWidth;
        LayoutUnit innerBorderRightWidth;
        edges[BSRight].getDoubleBorderStripeWidths(outerBorderRightWidth, innerBorderRightWidth);

        LayoutUnit outerBorderBottomWidth;
        LayoutUnit innerBorderBottomWidth;
        edges[BSBottom].getDoubleBorderStripeWidths(outerBorderBottomWidth, innerBorderBottomWidth);

        LayoutUnit outerBorderLeftWidth;
        LayoutUnit innerBorderLeftWidth;
        edges[BSLeft].getDoubleBorderStripeWidths(outerBorderLeftWidth, innerBorderLeftWidth);

        // Draw inner border line
        {
            GraphicsContextStateSaver stateSaver(*graphicsContext);
            RoundedRect innerClip = style.getRoundedInnerBorderFor(borderRect,
                innerBorderTopWidth, innerBorderBottomWidth, innerBorderLeftWidth, innerBorderRightWidth,
                includeLogicalLeftEdge, includeLogicalRightEdge);
            
            graphicsContext->clipRoundedRect(FloatRoundedRect(innerClip));
            drawBoxSideFromPath(graphicsContext, borderRect, borderPath, edges, thickness, drawThickness, side, style, color, SOLID, bleedAvoidance, includeLogicalLeftEdge, includeLogicalRightEdge);
        }

        // Draw outer border line
        {
            GraphicsContextStateSaver stateSaver(*graphicsContext);
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
            graphicsContext->clipOutRoundedRect(FloatRoundedRect(outerClip));
            drawBoxSideFromPath(graphicsContext, borderRect, borderPath, edges, thickness, drawThickness, side, style, color, SOLID, bleedAvoidance, includeLogicalLeftEdge, includeLogicalRightEdge);
        }
        return;
    }
    case RIDGE:
    case GROOVE:
    {
        EBorderStyle s1;
        EBorderStyle s2;
        if (borderStyle == GROOVE) {
            s1 = INSET;
            s2 = OUTSET;
        } else {
            s1 = OUTSET;
            s2 = INSET;
        }
        
        // Paint full border
        drawBoxSideFromPath(graphicsContext, borderRect, borderPath, edges, thickness, drawThickness, side, style, color, s1, bleedAvoidance, includeLogicalLeftEdge, includeLogicalRightEdge);

        // Paint inner only
        GraphicsContextStateSaver stateSaver(*graphicsContext);
        LayoutUnit topWidth = edges[BSTop].widthForPainting() / 2;
        LayoutUnit bottomWidth = edges[BSBottom].widthForPainting() / 2;
        LayoutUnit leftWidth = edges[BSLeft].widthForPainting() / 2;
        LayoutUnit rightWidth = edges[BSRight].widthForPainting() / 2;

        RoundedRect clipRect = style.getRoundedInnerBorderFor(borderRect,
            topWidth, bottomWidth, leftWidth, rightWidth,
            includeLogicalLeftEdge, includeLogicalRightEdge);

        graphicsContext->clipRoundedRect(FloatRoundedRect(clipRect));
        drawBoxSideFromPath(graphicsContext, borderRect, borderPath, edges, thickness, drawThickness, side, style, color, s2, bleedAvoidance, includeLogicalLeftEdge, includeLogicalRightEdge);
        return;
    }
    case INSET:
        if (side == BSTop || side == BSLeft)
            color = color.dark();
        break;
    case OUTSET:
        if (side == BSBottom || side == BSRight)
            color = color.dark();
        break;
    default:
        break;
    }

    graphicsContext->setStrokeStyle(NoStroke);
    graphicsContext->setFillColor(color, style.colorSpace());
    graphicsContext->drawRect(pixelSnappedForPainting(borderRect, document().deviceScaleFactor()));
}

static void findInnerVertex(const FloatPoint& outerCorner, const FloatPoint& innerCorner, const FloatPoint& centerPoint, FloatPoint& result)
{
    // If the line between outer and inner corner is towards the horizontal, intersect with a vertical line through the center,
    // otherwise with a horizontal line through the center. The points that form this line are arbitrary (we use 0, 100).
    // Note that if findIntersection fails, it will leave result untouched.
    float diffInnerOuterX = fabs(innerCorner.x() - outerCorner.x());
    float diffInnerOuterY = fabs(innerCorner.y() - outerCorner.y());
    float diffCenterOuterX = fabs(centerPoint.x() - outerCorner.x());
    float diffCenterOuterY = fabs(centerPoint.y() - outerCorner.y());
    if (diffInnerOuterY * diffCenterOuterX < diffCenterOuterY * diffInnerOuterX)
        findIntersection(outerCorner, innerCorner, FloatPoint(centerPoint.x(), 0), FloatPoint(centerPoint.x(), 100), result);
    else
        findIntersection(outerCorner, innerCorner, FloatPoint(0, centerPoint.y()), FloatPoint(100, centerPoint.y()), result);
}

void RenderBoxModelObject::clipBorderSidePolygon(GraphicsContext* graphicsContext, const RoundedRect& outerBorder, const RoundedRect& innerBorder,
                                                 BoxSide side, bool firstEdgeMatches, bool secondEdgeMatches)
{
    FloatPoint quad[4];

    const LayoutRect& outerRect = outerBorder.rect();
    const LayoutRect& innerRect = innerBorder.rect();

    FloatPoint centerPoint(innerRect.location().x() + static_cast<float>(innerRect.width()) / 2, innerRect.location().y() + static_cast<float>(innerRect.height()) / 2);

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
    switch (side) {
    case BSTop:
        quad[0] = outerRect.minXMinYCorner();
        quad[1] = innerRect.minXMinYCorner();
        quad[2] = innerRect.maxXMinYCorner();
        quad[3] = outerRect.maxXMinYCorner();

        if (!innerBorder.radii().topLeft().isZero())
            findInnerVertex(outerRect.minXMinYCorner(), innerRect.minXMinYCorner(), centerPoint, quad[1]);

        if (!innerBorder.radii().topRight().isZero())
            findInnerVertex(outerRect.maxXMinYCorner(), innerRect.maxXMinYCorner(), centerPoint, quad[2]);
        break;

    case BSLeft:
        quad[0] = outerRect.minXMinYCorner();
        quad[1] = innerRect.minXMinYCorner();
        quad[2] = innerRect.minXMaxYCorner();
        quad[3] = outerRect.minXMaxYCorner();

        if (!innerBorder.radii().topLeft().isZero())
            findInnerVertex(outerRect.minXMinYCorner(), innerRect.minXMinYCorner(), centerPoint, quad[1]);

        if (!innerBorder.radii().bottomLeft().isZero())
            findInnerVertex(outerRect.minXMaxYCorner(), innerRect.minXMaxYCorner(), centerPoint, quad[2]);
        break;

    case BSBottom:
        quad[0] = outerRect.minXMaxYCorner();
        quad[1] = innerRect.minXMaxYCorner();
        quad[2] = innerRect.maxXMaxYCorner();
        quad[3] = outerRect.maxXMaxYCorner();

        if (!innerBorder.radii().bottomLeft().isZero())
            findInnerVertex(outerRect.minXMaxYCorner(), innerRect.minXMaxYCorner(), centerPoint, quad[1]);

        if (!innerBorder.radii().bottomRight().isZero())
            findInnerVertex(outerRect.maxXMaxYCorner(), innerRect.maxXMaxYCorner(), centerPoint, quad[2]);
        break;

    case BSRight:
        quad[0] = outerRect.maxXMinYCorner();
        quad[1] = innerRect.maxXMinYCorner();
        quad[2] = innerRect.maxXMaxYCorner();
        quad[3] = outerRect.maxXMaxYCorner();

        if (!innerBorder.radii().topRight().isZero())
            findInnerVertex(outerRect.maxXMinYCorner(), innerRect.maxXMinYCorner(), centerPoint, quad[1]);

        if (!innerBorder.radii().bottomRight().isZero())
            findInnerVertex(outerRect.maxXMaxYCorner(), innerRect.maxXMaxYCorner(), centerPoint, quad[2]);
        break;
    }

    // If the border matches both of its adjacent sides, don't anti-alias the clip, and
    // if neither side matches, anti-alias the clip.
    if (firstEdgeMatches == secondEdgeMatches) {
        graphicsContext->clipConvexPolygon(4, quad, !firstEdgeMatches);
        return;
    }

    // Square off the end which shouldn't be affected by antialiasing, and clip.
    FloatPoint firstQuad[4];
    firstQuad[0] = quad[0];
    firstQuad[1] = quad[1];
    firstQuad[2] = side == BSTop || side == BSBottom ? FloatPoint(quad[3].x(), quad[2].y()) : FloatPoint(quad[2].x(), quad[3].y());
    firstQuad[3] = quad[3];
    graphicsContext->clipConvexPolygon(4, firstQuad, !firstEdgeMatches);

    FloatPoint secondQuad[4];
    secondQuad[0] = quad[0];
    secondQuad[1] = side == BSTop || side == BSBottom ? FloatPoint(quad[0].x(), quad[1].y()) : FloatPoint(quad[1].x(), quad[0].y());
    secondQuad[2] = quad[2];
    secondQuad[3] = quad[3];
    // Antialiasing affects the second side.
    graphicsContext->clipConvexPolygon(4, secondQuad, !secondEdgeMatches);
}

static LayoutRect calculateSideRectIncludingInner(const RoundedRect& outerBorder, const BorderEdge edges[], BoxSide side)
{
    LayoutRect sideRect = outerBorder.rect();
    LayoutUnit width;

    switch (side) {
    case BSTop:
        width = sideRect.height() - edges[BSBottom].widthForPainting();
        sideRect.setHeight(width);
        break;
    case BSBottom:
        width = sideRect.height() - edges[BSTop].widthForPainting();
        sideRect.shiftYEdgeTo(sideRect.maxY() - width);
        break;
    case BSLeft:
        width = sideRect.width() - edges[BSRight].widthForPainting();
        sideRect.setWidth(width);
        break;
    case BSRight:
        width = sideRect.width() - edges[BSLeft].widthForPainting();
        sideRect.shiftXEdgeTo(sideRect.maxX() - width);
        break;
    }

    return sideRect;
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
    case BSTop:
        overshoot = newRadii.topLeft().width() + newRadii.topRight().width() - newRect.width();
        if (overshoot > 0) {
            ASSERT(!(newRadii.topLeft().width() && newRadii.topRight().width()));
            newRect.setWidth(newRect.width() + overshoot);
            if (!newRadii.topLeft().width())
                newRect.move(-overshoot, 0);
        }
        newRadii.setBottomLeft(IntSize(0, 0));
        newRadii.setBottomRight(IntSize(0, 0));
        maxRadii = std::max(newRadii.topLeft().height(), newRadii.topRight().height());
        if (maxRadii > newRect.height())
            newRect.setHeight(maxRadii);
        break;

    case BSBottom:
        overshoot = newRadii.bottomLeft().width() + newRadii.bottomRight().width() - newRect.width();
        if (overshoot > 0) {
            ASSERT(!(newRadii.bottomLeft().width() && newRadii.bottomRight().width()));
            newRect.setWidth(newRect.width() + overshoot);
            if (!newRadii.bottomLeft().width())
                newRect.move(-overshoot, 0);
        }
        newRadii.setTopLeft(IntSize(0, 0));
        newRadii.setTopRight(IntSize(0, 0));
        maxRadii = std::max(newRadii.bottomLeft().height(), newRadii.bottomRight().height());
        if (maxRadii > newRect.height()) {
            newRect.move(0, newRect.height() - maxRadii);
            newRect.setHeight(maxRadii);
        }
        break;

    case BSLeft:
        overshoot = newRadii.topLeft().height() + newRadii.bottomLeft().height() - newRect.height();
        if (overshoot > 0) {
            ASSERT(!(newRadii.topLeft().height() && newRadii.bottomLeft().height()));
            newRect.setHeight(newRect.height() + overshoot);
            if (!newRadii.topLeft().height())
                newRect.move(0, -overshoot);
        }
        newRadii.setTopRight(IntSize(0, 0));
        newRadii.setBottomRight(IntSize(0, 0));
        maxRadii = std::max(newRadii.topLeft().width(), newRadii.bottomLeft().width());
        if (maxRadii > newRect.width())
            newRect.setWidth(maxRadii);
        break;

    case BSRight:
        overshoot = newRadii.topRight().height() + newRadii.bottomRight().height() - newRect.height();
        if (overshoot > 0) {
            ASSERT(!(newRadii.topRight().height() && newRadii.bottomRight().height()));
            newRect.setHeight(newRect.height() + overshoot);
            if (!newRadii.topRight().height())
                newRect.move(0, -overshoot);
        }
        newRadii.setTopLeft(IntSize(0, 0));
        newRadii.setBottomLeft(IntSize(0, 0));
        maxRadii = std::max(newRadii.topRight().width(), newRadii.bottomRight().width());
        if (maxRadii > newRect.width()) {
            newRect.move(newRect.width() - maxRadii, 0);
            newRect.setWidth(maxRadii);
        }
        break;
    }

    return RoundedRect(newRect, newRadii);
}

void RenderBoxModelObject::clipBorderSideForComplexInnerPath(GraphicsContext* graphicsContext, const RoundedRect& outerBorder, const RoundedRect& innerBorder,
    BoxSide side, const class BorderEdge edges[])
{
    graphicsContext->clip(calculateSideRectIncludingInner(outerBorder, edges, side));
    graphicsContext->clipOutRoundedRect(FloatRoundedRect(calculateAdjustedInnerBorder(innerBorder, side)));
}

bool RenderBoxModelObject::borderObscuresBackgroundEdge(const FloatSize& contextScale) const
{
    BorderEdge edges[4];
    BorderEdge::getBorderEdgeInfo(edges, style(), document().deviceScaleFactor());

    for (int i = BSTop; i <= BSLeft; ++i) {
        const BorderEdge& currEdge = edges[i];
        // FIXME: for vertical text
        float axisScale = (i == BSTop || i == BSBottom) ? contextScale.height() : contextScale.width();
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

    BorderEdge edges[4];
    BorderEdge::getBorderEdgeInfo(edges, style(), document().deviceScaleFactor());

    for (int i = BSTop; i <= BSLeft; ++i) {
        const BorderEdge& currEdge = edges[i];
        if (!currEdge.obscuresBackground())
            return false;
    }

    return true;
}

bool RenderBoxModelObject::boxShadowShouldBeAppliedToBackground(BackgroundBleedAvoidance bleedAvoidance, InlineFlowBox* inlineFlowBox) const
{
    if (bleedAvoidance != BackgroundBleedNone)
        return false;

    if (style().hasAppearance())
        return false;

    bool hasOneNormalBoxShadow = false;
    for (const ShadowData* currentShadow = style().boxShadow(); currentShadow; currentShadow = currentShadow->next()) {
        if (currentShadow->style() != Normal)
            continue;

        if (hasOneNormalBoxShadow)
            return false;
        hasOneNormalBoxShadow = true;

        if (currentShadow->spread())
            return false;
    }

    if (!hasOneNormalBoxShadow)
        return false;

    Color backgroundColor = style().visitedDependentColor(CSSPropertyBackgroundColor);
    if (!backgroundColor.isValid() || backgroundColor.hasAlpha())
        return false;

    const FillLayer* lastBackgroundLayer = style().backgroundLayers();
    for (const FillLayer* next = lastBackgroundLayer->next(); next; next = lastBackgroundLayer->next())
        lastBackgroundLayer = next;

    if (lastBackgroundLayer->clip() != BorderFillBox)
        return false;

    if (lastBackgroundLayer->image() && style().hasBorderRadius())
        return false;

    if (inlineFlowBox && !inlineFlowBox->boxShadowCanBeAppliedToBackground(*lastBackgroundLayer))
        return false;

    if (hasOverflowClip() && lastBackgroundLayer->attachment() == LocalBackgroundAttachment)
        return false;

    return true;
}

static inline LayoutRect areaCastingShadowInHole(const LayoutRect& holeRect, int shadowExtent, int shadowSpread, const IntSize& shadowOffset)
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
    GraphicsContext* context = info.context;
    if (context->paintingDisabled() || !style.boxShadow())
        return;

    RoundedRect border = (shadowStyle == Inset)
        ? style.getRoundedInnerBorderFor(paintRect, includeLogicalLeftEdge, includeLogicalRightEdge)
        : style.getRoundedBorderFor(paintRect, &view(), includeLogicalLeftEdge, includeLogicalRightEdge);

    bool hasBorderRadius = style.hasBorderRadius();
    bool isHorizontal = style.isHorizontalWritingMode();
    
    bool hasOpaqueBackground = style.visitedDependentColor(CSSPropertyBackgroundColor).isValid() && style.visitedDependentColor(CSSPropertyBackgroundColor).alpha() == 255;
    for (const ShadowData* shadow = style.boxShadow(); shadow; shadow = shadow->next()) {
        if (shadow->style() != shadowStyle)
            continue;

        IntSize shadowOffset(shadow->x(), shadow->y());
        int shadowRadius = shadow->radius();
        int shadowPaintingExtent = shadow->paintingExtent();
        int shadowSpread = shadow->spread();
        
        if (shadowOffset.isZero() && !shadowRadius && !shadowSpread)
            continue;
        
        const Color& shadowColor = shadow->color();

        if (shadow->style() == Normal) {
            RoundedRect fillRect = border;
            fillRect.inflate(shadowSpread);
            if (fillRect.isEmpty())
                continue;

            IntRect shadowRect(border.rect());
            shadowRect.inflate(shadowPaintingExtent + shadowSpread);
            shadowRect.move(shadowOffset);

            GraphicsContextStateSaver stateSaver(*context);
            context->clip(shadowRect);

            // Move the fill just outside the clip, adding 1 pixel separation so that the fill does not
            // bleed in (due to antialiasing) if the context is transformed.
            IntSize extraOffset(paintRect.pixelSnappedWidth() + std::max(0, shadowOffset.width()) + shadowPaintingExtent + 2 * shadowSpread + 1, 0);
            shadowOffset -= extraOffset;
            fillRect.move(extraOffset);

            if (shadow->isWebkitBoxShadow())
                context->setLegacyShadow(shadowOffset, shadowRadius, shadowColor, style.colorSpace());
            else
                context->setShadow(shadowOffset, shadowRadius, shadowColor, style.colorSpace());

            if (hasBorderRadius) {
                RoundedRect rectToClipOut = border;

                // If the box is opaque, it is unnecessary to clip it out. However, doing so saves time
                // when painting the shadow. On the other hand, it introduces subpixel gaps along the
                // corners. Those are avoided by insetting the clipping path by one pixel.
                if (hasOpaqueBackground) {
                    rectToClipOut.inflateWithRadii(-1);
                }

                if (!rectToClipOut.isEmpty())
                    context->clipOutRoundedRect(FloatRoundedRect(rectToClipOut));

                RoundedRect influenceRect(shadowRect, border.radii());
                influenceRect.expandRadii(2 * shadowPaintingExtent + shadowSpread);
                if (allCornersClippedOut(influenceRect, info.rect))
                    context->fillRect(fillRect.rect(), Color::black, style.colorSpace());
                else {
                    fillRect.expandRadii(shadowSpread);
                    if (!fillRect.isRenderable())
                        fillRect.adjustRadii();
                    context->fillRoundedRect(FloatRoundedRect(fillRect), Color::black, style.colorSpace());
                }
            } else {
                LayoutRect rectToClipOut = border.rect();

                // If the box is opaque, it is unnecessary to clip it out. However, doing so saves time
                // when painting the shadow. On the other hand, it introduces subpixel gaps along the
                // edges if they are not pixel-aligned. Those are avoided by insetting the clipping path
                // by one pixel.
                if (hasOpaqueBackground) {
                    // FIXME: The function to decide on the policy based on the transform should be a named function.
                    // FIXME: It's not clear if this check is right. What about integral scale factors?
                    AffineTransform transform = context->getCTM();
                    if (transform.a() != 1 || (transform.d() != 1 && transform.d() != -1) || transform.b() || transform.c())
                        rectToClipOut.inflate(-1);
                }

                if (!rectToClipOut.isEmpty())
                    context->clipOut(rectToClipOut);
                context->fillRect(fillRect.rect(), Color::black, style.colorSpace());
            }
        } else {
            // Inset shadow.
            IntRect holeRect(border.rect());
            holeRect.inflate(-shadowSpread);

            if (holeRect.isEmpty()) {
                if (hasBorderRadius)
                    context->fillRoundedRect(FloatRoundedRect(border), shadowColor, style.colorSpace());
                else
                    context->fillRect(border.rect(), shadowColor, style.colorSpace());
                continue;
            }

            if (!includeLogicalLeftEdge) {
                if (isHorizontal) {
                    holeRect.move(-std::max(shadowOffset.width(), 0) - shadowPaintingExtent, 0);
                    holeRect.setWidth(holeRect.width() + std::max(shadowOffset.width(), 0) + shadowPaintingExtent);
                } else {
                    holeRect.move(0, -std::max(shadowOffset.height(), 0) - shadowPaintingExtent);
                    holeRect.setHeight(holeRect.height() + std::max(shadowOffset.height(), 0) + shadowPaintingExtent);
                }
            }
            if (!includeLogicalRightEdge) {
                if (isHorizontal)
                    holeRect.setWidth(holeRect.width() - std::min(shadowOffset.width(), 0) + shadowPaintingExtent);
                else
                    holeRect.setHeight(holeRect.height() - std::min(shadowOffset.height(), 0) + shadowPaintingExtent);
            }

            Color fillColor(shadowColor.red(), shadowColor.green(), shadowColor.blue(), 255);

            LayoutRect outerRect = areaCastingShadowInHole(border.rect(), shadowPaintingExtent, shadowSpread, shadowOffset);
            RoundedRect roundedHole(holeRect, border.radii());

            GraphicsContextStateSaver stateSaver(*context);
            if (hasBorderRadius) {
                Path path;
                path.addRoundedRect(border);
                context->clip(path);
                roundedHole.shrinkRadii(shadowSpread);
            } else
                context->clip(border.rect());

            IntSize extraOffset(2 * paintRect.pixelSnappedWidth() + std::max(0, shadowOffset.width()) + shadowPaintingExtent - 2 * shadowSpread + 1, 0);
            context->translate(extraOffset.width(), extraOffset.height());
            shadowOffset -= extraOffset;

            if (shadow->isWebkitBoxShadow())
                context->setLegacyShadow(shadowOffset, shadowRadius, shadowColor, style.colorSpace());
            else
                context->setShadow(shadowOffset, shadowRadius, shadowColor, style.colorSpace());

            context->fillRectWithRoundedHole(outerRect, FloatRoundedRect(roundedHole), fillColor, style.colorSpace());
        }
    }
}

LayoutUnit RenderBoxModelObject::containingBlockLogicalWidthForContent() const
{
    return containingBlock()->availableLogicalWidth();
}

RenderBoxModelObject* RenderBoxModelObject::continuation() const
{
    if (!continuationMap)
        return 0;
    return continuationMap->get(this);
}

void RenderBoxModelObject::setContinuation(RenderBoxModelObject* continuation)
{
    if (continuation) {
        if (!continuationMap)
            continuationMap = new ContinuationMap;
        continuationMap->set(this, continuation);
    } else {
        if (continuationMap)
            continuationMap->remove(this);
    }
}

RenderTextFragment* RenderBoxModelObject::firstLetterRemainingText() const
{
    if (!firstLetterRemainingTextMap)
        return 0;
    return firstLetterRemainingTextMap->get(this);
}

void RenderBoxModelObject::setFirstLetterRemainingText(RenderTextFragment* remainingText)
{
    if (remainingText) {
        if (!firstLetterRemainingTextMap)
            firstLetterRemainingTextMap = new FirstLetterRemainingTextMap;
        firstLetterRemainingTextMap->set(this, remainingText);
    } else if (firstLetterRemainingTextMap)
        firstLetterRemainingTextMap->remove(this);
}

LayoutRect RenderBoxModelObject::localCaretRectForEmptyElement(LayoutUnit width, LayoutUnit textIndentOffset)
{
    ASSERT(!firstChild());

    // FIXME: This does not take into account either :first-line or :first-letter
    // However, as soon as some content is entered, the line boxes will be
    // constructed and this kludge is not called any more. So only the caret size
    // of an empty :first-line'd block is wrong. I think we can live with that.
    const RenderStyle& currentStyle = firstLineStyle();
    LayoutUnit height = lineHeight(true, currentStyle.isHorizontalWritingMode() ? HorizontalLine : VerticalLine);

    enum CaretAlignment { alignLeft, alignRight, alignCenter };

    CaretAlignment alignment = alignLeft;

    switch (currentStyle.textAlign()) {
    case LEFT:
    case WEBKIT_LEFT:
        break;
    case CENTER:
    case WEBKIT_CENTER:
        alignment = alignCenter;
        break;
    case RIGHT:
    case WEBKIT_RIGHT:
        alignment = alignRight;
        break;
    case JUSTIFY:
    case TASTART:
        if (!currentStyle.isLeftToRightDirection())
            alignment = alignRight;
        break;
    case TAEND:
        if (currentStyle.isLeftToRightDirection())
            alignment = alignRight;
        break;
    }

    LayoutUnit x = borderLeft() + paddingLeft();
    LayoutUnit maxX = width - borderRight() - paddingRight();

    switch (alignment) {
    case alignLeft:
        if (currentStyle.isLeftToRightDirection())
            x += textIndentOffset;
        break;
    case alignCenter:
        x = (x + maxX) / 2;
        if (currentStyle.isLeftToRightDirection())
            x += textIndentOffset / 2;
        else
            x -= textIndentOffset / 2;
        break;
    case alignRight:
        x = maxX - caretWidth;
        if (!currentStyle.isLeftToRightDirection())
            x -= textIndentOffset;
        break;
    }
    x = std::min(x, std::max<LayoutUnit>(maxX - caretWidth, 0));

    LayoutUnit y = paddingTop() + borderTop();

    return currentStyle.isHorizontalWritingMode() ? LayoutRect(x, y, caretWidth, height) : LayoutRect(y, x, height, caretWidth);
}

bool RenderBoxModelObject::shouldAntialiasLines(GraphicsContext* context)
{
    // FIXME: We may want to not antialias when scaled by an integral value,
    // and we may want to antialias when translated by a non-integral value.
    return !context->getCTM().isIdentityOrTranslationOrFlipped();
}

void RenderBoxModelObject::mapAbsoluteToLocalPoint(MapCoordinatesFlags mode, TransformState& transformState) const
{
    auto o = container();
    if (!o)
        return;

    // The point inside a box that's inside a region has its coordinates relative to the region,
    // not the FlowThread that is its container in the RenderObject tree.
    if (o->isRenderFlowThread() && isBox()) {
        RenderRegion* startRegion = nullptr;
        RenderRegion* endRegion = nullptr;
        if (toRenderFlowThread(o)->getRegionRangeForBox(toRenderBox(this), startRegion, endRegion))
            o = startRegion;
    }

    o->mapAbsoluteToLocalPoint(mode, transformState);

    LayoutSize containerOffset = offsetFromContainer(o, LayoutPoint());

    if (!style().hasOutOfFlowPosition() && o->hasColumns()) {
        RenderBlock* block = toRenderBlock(o);
        LayoutPoint point(roundedLayoutPoint(transformState.mappedPoint()));
        point -= containerOffset;
        block->adjustForColumnRect(containerOffset, point);
    }

    bool preserve3D = mode & UseTransforms && (o->style().preserves3D() || style().preserves3D());
    if (mode & UseTransforms && shouldUseTransformFromContainer(o)) {
        TransformationMatrix t;
        getTransformFromContainer(o, containerOffset, t);
        transformState.applyTransform(t, preserve3D ? TransformState::AccumulateTransform : TransformState::FlattenTransform);
    } else
        transformState.move(containerOffset.width(), containerOffset.height(), preserve3D ? TransformState::AccumulateTransform : TransformState::FlattenTransform);
}

void RenderBoxModelObject::moveChildTo(RenderBoxModelObject* toBoxModelObject, RenderObject* child, RenderObject* beforeChild, bool fullRemoveInsert)
{
    // We assume that callers have cleared their positioned objects list for child moves (!fullRemoveInsert) so the
    // positioned renderer maps don't become stale. It would be too slow to do the map lookup on each call.
    ASSERT(!fullRemoveInsert || !isRenderBlock() || !toRenderBlock(this)->hasPositionedObjects());

    ASSERT(this == child->parent());
    ASSERT(!beforeChild || toBoxModelObject == beforeChild->parent());
    if (fullRemoveInsert && (toBoxModelObject->isRenderBlock() || toBoxModelObject->isRenderInline())) {
        // Takes care of adding the new child correctly if toBlock and fromBlock
        // have different kind of children (block vs inline).
        removeChildInternal(*child, NotifyChildren);
        toBoxModelObject->addChild(child, beforeChild);
    } else {
        NotifyChildrenType notifyType = fullRemoveInsert ? NotifyChildren : DontNotifyChildren;
        removeChildInternal(*child, notifyType);
        toBoxModelObject->insertChildInternal(child, beforeChild, notifyType);
    }
}

void RenderBoxModelObject::moveChildrenTo(RenderBoxModelObject* toBoxModelObject, RenderObject* startChild, RenderObject* endChild, RenderObject* beforeChild, bool fullRemoveInsert)
{
    // This condition is rarely hit since this function is usually called on
    // anonymous blocks which can no longer carry positioned objects (see r120761)
    // or when fullRemoveInsert is false.
    if (fullRemoveInsert && isRenderBlock()) {
        toRenderBlock(this)->removePositionedObjects(0);
        if (isRenderBlockFlow())
            toRenderBlockFlow(this)->removeFloatingObjects(); 
    }

    ASSERT(!beforeChild || toBoxModelObject == beforeChild->parent());
    for (RenderObject* child = startChild; child && child != endChild; ) {
        // Save our next sibling as moveChildTo will clear it.
        RenderObject* nextSibling = child->nextSibling();
        moveChildTo(toBoxModelObject, child, beforeChild, fullRemoveInsert);
        child = nextSibling;
    }
}

} // namespace WebCore
