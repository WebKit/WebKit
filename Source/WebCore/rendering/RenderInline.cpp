/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003-2025 Apple Inc. All rights reserved.
 * Copyright (C) 2014 Google Inc. All rights reserved.
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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
#include "RenderInline.h"

#include "Chrome.h"
#include "FloatQuad.h"
#include "FontCascadeInlines.h"
#include "FrameSelection.h"
#include "GraphicsContext.h"
#include "HitTestResult.h"
#include "InlineIteratorBoxInlines.h"
#include "InlineIteratorInlineBox.h"
#include "InlineIteratorLineBox.h"
#include "LayoutIntegrationLineLayout.h"
#include "LegacyInlineFlowBox.h"
#include "LegacyInlineTextBox.h"
#include "LegacyRootInlineBox.h"
#include "OutlinePainter.h"
#include "RenderBlock.h"
#include "RenderBoxInlines.h"
#include "RenderChildIterator.h"
#include "RenderElementStyleInlines.h"
#include "RenderElementInlines.h"
#include "RenderFragmentedFlow.h"
#include "RenderGeometryMap.h"
#include "RenderIterator.h"
#include "RenderLayer.h"
#include "RenderLayoutState.h"
#include "RenderLineBreak.h"
#include "RenderListOutsideMarker.h"
#include "RenderObjectInlines.h"
#include "RenderSVGInline.h"
#include "RenderTable.h"
#include "RenderTheme.h"
#include "RenderTreeBuilder.h"
#include "RenderView.h"
#include "Settings.h"
#include "TransformState.h"
#include "VisiblePosition.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RenderInline);

RenderInline::RenderInline(Type type, Element& element, Style::ComputedStyle&& style)
    : RenderBoxModelObject(type, element, WTF::move(style), TypeFlag::IsInlineBox, { })
{
    setChildrenInline(true);
    ASSERT(isInlineBox());
}

RenderInline::RenderInline(Type type, Document& document, Style::ComputedStyle&& style)
    : RenderBoxModelObject(type, document, WTF::move(style), TypeFlag::IsInlineBox, { })
{
    setChildrenInline(true);
    ASSERT(isInlineBox());
}

RenderInline::~RenderInline() = default;

// Only SVG inlines have legacy line boxes, and they always do: SVG text is always laid out by
// LegacyLineLayout (Settings::useIFCForSVGText, the in-progress migration off it, is never enabled).
// Every legacy arm below is therefore reachable from RenderSVGInline only.
static LegacyInlineFlowBox* firstLegacyInlineBoxFor(const RenderInline& renderer)
{
    auto* svgInline = dynamicDowncast<RenderSVGInline>(renderer);
    return svgInline ? svgInline->firstLegacyInlineBox() : nullptr;
}

static LegacyInlineFlowBox* lastLegacyInlineBoxFor(const RenderInline& renderer)
{
    auto* svgInline = dynamicDowncast<RenderSVGInline>(renderer);
    return svgInline ? svgInline->lastLegacyInlineBox() : nullptr;
}

void RenderInline::updateFromStyle()
{
    RenderBoxModelObject::updateFromStyle();

    // FIXME: Support transforms and reflections on inline flows someday.
    setHasTransformRelatedProperty(false);
    setHasReflection(false);    
}

void RenderInline::styleWillChange(Style::Difference diff, const Style::ComputedStyle& newStyle)
{
    RenderBoxModelObject::styleWillChange(diff, newStyle);

    // RenderInlines forward their absolute positioned descendants to their (non-anonymous) containing block.
    // Check if this non-anonymous containing block can hold the absolute positioned elements when the inline is no longer positioned.
    CheckedPtr container = containingBlock();
    if (!container)
        return;

    const Style::ComputedStyle* oldStyle = hasInitializedStyle() ? &style() : nullptr;
    if (oldStyle)
        removeOutOfFlowBoxesIfNeededOnStyleChange(*container, *oldStyle, newStyle);
}

void RenderInline::styleDidChange(Style::Difference diff, const Style::ComputedStyle* oldStyle)
{
    RenderBoxModelObject::styleDidChange(diff, oldStyle);

    propagateStyleToAnonymousChildren(StylePropagationType::AllChildren);
}

bool RenderInline::mayAffectLayout() const
{
    auto* parentStyle = &parent()->style();
    auto* parentRenderInline = dynamicDowncast<RenderInline>(*parent());
    auto hasHardLineBreakChildOnly = firstChild() && firstChild() == lastChild() && firstChild()->isBR();
    bool checkFonts = document().inNoQuirksMode();
    auto mayAffectLayout = (parentRenderInline && parentRenderInline->mayAffectLayout())
        || (parentRenderInline && !WTF::holdsAlternative<CSS::Keyword::Baseline>(parentStyle->verticalAlign()))
        || !WTF::holdsAlternative<CSS::Keyword::Baseline>(style().verticalAlign())
        || !style().textEmphasisStyle().isNone()
        || (checkFonts && (!parentStyle->fontCascade().metricsOfPrimaryFont().hasIdenticalAscentDescentAndLineGap(style().fontCascade().metricsOfPrimaryFont())
        || parentStyle->textAutosizingAdjustedLineHeight() != style().textAutosizingAdjustedLineHeight()))
        || hasHardLineBreakChildOnly;

    if (!mayAffectLayout && checkFonts) {
        // Have to check the first line style as well.
        parentStyle = &parent()->firstLineStyle();
        auto& childStyle = firstLineStyle();
        mayAffectLayout = !parentStyle->fontCascade().metricsOfPrimaryFont().hasIdenticalAscentDescentAndLineGap(childStyle.fontCascade().metricsOfPrimaryFont())
            || !WTF::holdsAlternative<CSS::Keyword::Baseline>(childStyle.verticalAlign())
            || parentStyle->textAutosizingAdjustedLineHeight() != childStyle.textAutosizingAdjustedLineHeight();
    }
    return mayAffectLayout;
}

void RenderInline::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (auto* lineLayout = LayoutIntegration::LineLayout::containing(*this))
        lineLayout->paint(paintInfo, paintOffset, this);
}

template<typename GeneratorContext>
void RenderInline::generateLineBoxRects(GeneratorContext& context) const
{
    if (auto* lineLayout = LayoutIntegration::LineLayout::containing(*this)) {
        auto inlineBoxRects = lineLayout->collectInlineBoxRects(*this);
        if (inlineBoxRects.isEmpty()) {
            context.addRect({ });
            return;
        }
        for (auto inlineRect : inlineBoxRects)
            context.addRect(inlineRect);
        return;
    }
    if (auto* curr = firstLegacyInlineBoxFor(*this)) {
        for (; curr; curr = curr->nextLineBox())
            context.addRect(FloatRect(curr->topLeft(), curr->size()));
    } else
        context.addRect(FloatRect());
}

class AbsoluteRectsGeneratorContext {
public:
    AbsoluteRectsGeneratorContext(Vector<LayoutRect>& rects, const LayoutPoint& accumulatedOffset)
        : m_rects(rects)
        , m_accumulatedOffset(accumulatedOffset) { }

    void addRect(const FloatRect& rect)
    {
        LayoutRect adjustedRect = LayoutRect(rect);
        adjustedRect.moveBy(m_accumulatedOffset);
        m_rects.append(adjustedRect);
    }
private:
    Vector<LayoutRect>& m_rects;
    const LayoutPoint& m_accumulatedOffset;
};

void RenderInline::boundingRects(Vector<LayoutRect>& rects, const LayoutPoint& accumulatedOffset) const
{
    AbsoluteRectsGeneratorContext context(rects, accumulatedOffset);
    generateLineBoxRects(context);
}

namespace {

class AbsoluteQuadsGeneratorContext {
public:
    AbsoluteQuadsGeneratorContext(const RenderInline* renderer, Vector<FloatQuad>& quads)
        : m_quads(quads)
        , m_geometryMap()
    {
        m_geometryMap.pushMappingsToAncestor(renderer, nullptr);
    }

    void addRect(const FloatRect& rect)
    {
        m_quads.append(m_geometryMap.absoluteRect(rect));
    }
private:
    Vector<FloatQuad>& m_quads;
    RenderGeometryMap m_geometryMap;
};

} // unnamed namespace

void RenderInline::absoluteQuads(Vector<FloatQuad>& quads, bool*) const
{
    AbsoluteQuadsGeneratorContext context(this, quads);
    generateLineBoxRects(context);
}

LayoutUnit RenderInline::offsetLeft() const
{
    return adjustedPositionRelativeToOffsetParent(firstInlineBoxTopLeft()).x();
}

LayoutUnit RenderInline::offsetTop() const
{
    return adjustedPositionRelativeToOffsetParent(firstInlineBoxTopLeft()).y();
}

LayoutPoint RenderInline::firstInlineBoxTopLeft() const
{
    if (auto* lineLayout = LayoutIntegration::LineLayout::containing(*this))
        return lineLayout->firstInlineBoxRect(*this).location();
    if (auto* inlineBox = firstLegacyInlineBoxFor(*this))
        return flooredLayoutPoint(inlineBox->locationIncludingFlipping());
    return { };
}

static LayoutUnit computeMargin(const RenderInline* renderer, const Style::MarginEdge& margin, const Style::ZoomFactor& zoomFactor)
{
    return Style::evaluateMinimum<LayoutUnit>(margin, [&] ALWAYS_INLINE_LAMBDA {
        return std::max<LayoutUnit>(0, renderer->containingBlock()->contentBoxLogicalWidth());
    }, zoomFactor);
}

LayoutUnit RenderInline::marginLeft() const
{
    return computeMargin(this, style().marginLeft(), style().usedZoomForLength());
}

LayoutUnit RenderInline::marginRight() const
{
    return computeMargin(this, style().marginRight(), style().usedZoomForLength());
}

LayoutUnit RenderInline::marginTop() const
{
    return computeMargin(this, style().marginTop(), style().usedZoomForLength());
}

LayoutUnit RenderInline::marginBottom() const
{
    return computeMargin(this, style().marginBottom(), style().usedZoomForLength());
}

LayoutUnit RenderInline::marginStart(const WritingMode writingMode) const
{
    return computeMargin(this, style().marginStart(writingMode), style().usedZoomForLength());
}

LayoutUnit RenderInline::marginEnd(const WritingMode writingMode) const
{
    return computeMargin(this, style().marginEnd(writingMode), style().usedZoomForLength());
}

LayoutUnit RenderInline::marginBefore(const WritingMode writingMode) const
{
    return computeMargin(this, style().marginBefore(writingMode), style().usedZoomForLength());
}

LayoutUnit RenderInline::marginAfter(const WritingMode writingMode) const
{
    return computeMargin(this, style().marginAfter(writingMode), style().usedZoomForLength());
}

ASCIILiteral RenderInline::renderName() const
{
    if (isRelativelyPositioned())
        return "RenderInline (relative positioned)"_s;
    if (isStickilyPositioned())
        return "RenderInline (sticky positioned)"_s;
    // FIXME: Temporary hack while the new generated content system is being implemented.
    if (isPseudoElement())
        return "RenderInline (generated)"_s;
    if (isAnonymous())
        return "RenderInline (generated)"_s;
    return "RenderInline"_s;
}

bool RenderInline::nodeAtPoint(const HitTestRequest& request, HitTestResult& result,
                                const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction hitTestAction)
{
    ASSERT(layer());
    if (auto* lineLayout = LayoutIntegration::LineLayout::containing(*this))
        return lineLayout->hitTest(request, result, locationInContainer, accumulatedOffset, hitTestAction, this);
    return false;
}

PositionWithAffinity RenderInline::positionForPoint(const LayoutPoint& point, HitTestSource source, const RenderFragmentContainer* fragment)
{
    auto& containingBlock = *this->containingBlock();
    return containingBlock.positionForPoint(point, source, fragment);
}

LayoutUnit RenderInline::innerPaddingBoxWidth() const
{
    auto firstInlineBoxPaddingBoxLeft = LayoutUnit { };
    auto lastInlineBoxPaddingBoxRight = LayoutUnit { };

    if (LayoutIntegration::LineLayout::containing(*this)) {
        if (auto inlineBox = InlineIterator::lineLeftmostInlineBoxFor(*this)) {
            if (writingMode().isBidiLTR()) {
                firstInlineBoxPaddingBoxLeft = inlineBox->logicalLeftIgnoringInlineDirection() + borderStart();
                for (; inlineBox->nextInlineBoxLineRightward(); inlineBox.traverseInlineBoxLineRightward()) { }
                ASSERT(inlineBox);
                lastInlineBoxPaddingBoxRight = inlineBox->logicalRightIgnoringInlineDirection() - borderEnd();
            } else {
                lastInlineBoxPaddingBoxRight = inlineBox->logicalRightIgnoringInlineDirection() - borderStart();
                for (; inlineBox->nextInlineBoxLineRightward(); inlineBox.traverseInlineBoxLineRightward()) { }
                ASSERT(inlineBox);
                firstInlineBoxPaddingBoxLeft = inlineBox->logicalLeftIgnoringInlineDirection() + borderEnd();
            }
            return std::max(0_lu, lastInlineBoxPaddingBoxRight - firstInlineBoxPaddingBoxLeft);
        }
        return { };
    }

    auto* firstInlineBox = firstLegacyInlineBoxFor(*this);
    auto* lastInlineBox = lastLegacyInlineBoxFor(*this);
    if (!firstInlineBox || !lastInlineBox)
        return { };

    if (writingMode().isBidiLTR()) {
        firstInlineBoxPaddingBoxLeft = firstInlineBox->logicalLeft();
        lastInlineBoxPaddingBoxRight = lastInlineBox->logicalRight();
    } else {
        lastInlineBoxPaddingBoxRight = firstInlineBox->logicalRight();
        firstInlineBoxPaddingBoxLeft = lastInlineBox->logicalLeft();
    }
    return std::max(0_lu, lastInlineBoxPaddingBoxRight - firstInlineBoxPaddingBoxLeft);
}

LayoutUnit RenderInline::innerPaddingBoxHeight() const
{
    auto innerPaddingBoxLogicalHeight = LayoutUnit { isHorizontalWritingMode() ? linesBoundingBox().height() : linesBoundingBox().width() };
    innerPaddingBoxLogicalHeight -= (borderBefore() + borderAfter());
    return innerPaddingBoxLogicalHeight;
}

IntRect RenderInline::linesBoundingBox() const
{
    if (auto* layout = LayoutIntegration::LineLayout::containing(*this)) {
        if (!layoutBox() || !layout->contains(*this)) {
            // Repaint may be issued on subtrees during content mutation with newly inserted renderers
            // (or we just forgot to initiate layout before querying geometry on stale content after moving inline boxes between blocks).
            ASSERT(needsLayout());
            return { };
        }
        if (isRenderSVGInline()) {
            // FIXME: Always build the bounding box like this. LineLayouyt::enclosingBorderBoxRectFor does not include
            // any post-layout box adjustments.
            FloatRect result;
            for (auto box = InlineIterator::lineLeftmostInlineBoxFor(*this); box; box.traverseInlineBoxLineRightward()) {
                auto rect = box->visualRectIgnoringBlockDirection();
                result.unite(rect);
            }
            return enclosingIntRect(result);
        }
        return enclosingIntRect(layout->enclosingBorderBoxRectFor(*this));
    }

    auto* firstInlineBox = firstLegacyInlineBoxFor(*this);
    auto* lastInlineBox = lastLegacyInlineBoxFor(*this);

    // See <rdar://problem/5289721>, for an unknown reason the linked list here is sometimes inconsistent, first is non-zero and last is zero.  We have been
    // unable to reproduce this at all (and consequently unable to figure ot why this is happening).  The assert will hopefully catch the problem in debug
    // builds and help us someday figure out why.  We also put in a redundant check of lastLineBox() to avoid the crash for now.
    ASSERT(!firstInlineBox == !lastInlineBox); // Either both are null or both exist.
    IntRect result;
    if (firstInlineBox && lastInlineBox) {
        // Return the width of the minimal left side and the maximal right side.
        float logicalLeftSide = 0;
        float logicalRightSide = 0;
        for (auto* curr = firstInlineBox; curr; curr = curr->nextLineBox()) {
            if (curr == firstInlineBox || curr->logicalLeft() < logicalLeftSide)
                logicalLeftSide = curr->logicalLeft();
            if (curr == firstInlineBox || curr->logicalRight() > logicalRightSide)
                logicalRightSide = curr->logicalRight();
        }

        bool isHorizontal = writingMode().isHorizontal();

        float x = isHorizontal ? logicalLeftSide : firstInlineBox->x();
        float y = isHorizontal ? firstInlineBox->y() : logicalLeftSide;
        float width = isHorizontal ? logicalRightSide - logicalLeftSide : lastInlineBox->logicalBottom() - x;
        float height = isHorizontal ? lastInlineBox->logicalBottom() - y : logicalRightSide - logicalLeftSide;
        result = enclosingIntRect(FloatRect(x, y, width, height));
    }

    return result;
}

LayoutRect RenderInline::linesVisualOverflowBoundingBox() const
{
    if (auto* layout = LayoutIntegration::LineLayout::containing(*this)) {
        if (!layoutBox()) {
            // Repaint may be issued on subtrees during content mutation with newly inserted renderers. 
            ASSERT(needsLayout());
            return { };
        }
        return layout->inkOverflowBoundingBoxRectFor(*this);
    }

    auto* firstInlineBox = firstLegacyInlineBoxFor(*this);
    auto* lastInlineBox = lastLegacyInlineBoxFor(*this);
    if (!firstInlineBox || !lastInlineBox)
        return { };

    // Return the width of the minimal left side and the maximal right side.
    LayoutUnit logicalLeftSide = LayoutUnit::max();
    LayoutUnit logicalRightSide = LayoutUnit::min();
    for (auto* curr = firstInlineBox; curr; curr = curr->nextLineBox()) {
        logicalLeftSide = std::min(logicalLeftSide, curr->logicalLeftVisualOverflow());
        logicalRightSide = std::max(logicalRightSide, curr->logicalRightVisualOverflow());
    }

    const LegacyRootInlineBox& firstRootBox = firstInlineBox->root();
    const LegacyRootInlineBox& lastRootBox = lastInlineBox->root();

    LayoutUnit logicalTop = firstInlineBox->logicalTopVisualOverflow(firstRootBox.lineTop());
    LayoutUnit logicalWidth = logicalRightSide - logicalLeftSide;
    LayoutUnit logicalHeight = lastInlineBox->logicalBottomVisualOverflow(lastRootBox.lineBottom()) - logicalTop;

    LayoutRect rect(logicalLeftSide, logicalTop, logicalWidth, logicalHeight);
    if (!writingMode().isHorizontal())
        rect = rect.transposedRect();
    return rect;
}

auto RenderInline::localRectsForRepaint(RepaintOutlineBounds) const -> RepaintRects
{
    // RepaintOutlineBounds is unused for inlines.

    // Only first-letter renderers are allowed in here during layout. They mutate the tree triggering repaints.
#ifndef NDEBUG
    auto insideSelfPaintingInlineBox = [&] {
        if (hasSelfPaintingLayer())
            return true;
        auto* containingBlock = this->containingBlock();
        for (auto* ancestor = this->parent(); ancestor && ancestor != containingBlock; ancestor = ancestor->parent()) {
            if (ancestor->hasSelfPaintingLayer())
                return true;
        }
        return false;
    };
    ASSERT_UNUSED(insideSelfPaintingInlineBox, !view().frameView().layoutContext().isPaintOffsetCacheEnabled() || style().pseudoElementType() == PseudoElementType::FirstLetter || insideSelfPaintingInlineBox());
#endif

    if (!firstLegacyInlineBoxFor(*this) && !LayoutIntegration::LineLayout::containing(*this))
        return { };

    auto repaintRect = linesVisualOverflowBoundingBox();
    repaintRect.inflate(LayoutUnit { style().usedOutlineSize(style().usedZoomForLength(), style().deviceScaleFactor()) });
    return { repaintRect };
}

LayoutRect RenderInline::rectWithOutlineForRepaint(const RenderLayerModelObject* repaintContainer, LayoutUnit outlineWidth) const
{
    LayoutRect r(RenderBoxModelObject::rectWithOutlineForRepaint(repaintContainer, outlineWidth));
    for (auto& child : childrenOfType<RenderElement>(*this))
        r.unite(child.rectWithOutlineForRepaint(repaintContainer, outlineWidth));
    return r;
}

auto RenderInline::computeVisibleRectsInContainer(const RepaintRects& rects, const RenderLayerModelObject* container, const VisibleRectContext& context, VisibleRectState state) const -> std::optional<RepaintRects>
{
    // Repaint offset cache is only valid for root-relative repainting
    if (view().frameView().layoutContext().isPaintOffsetCacheEnabled() && !container && !context.options.contains(VisibleRectContext::Option::UseEdgeInclusiveIntersection))
        return computeVisibleRectsUsingPaintOffset(rects);

    if (container == this)
        return rects;

    bool containerSkipped;
    RenderElement* localContainer = this->container(container, containerSkipped);
    if (!localContainer)
        return rects;

    auto adjustedRects = rects;
    if (style().hasInFlowPosition() && layer()) {
        // Apply the in-flow position offset when invalidating a rectangle. The layer
        // is translated, but the render box isn't, so we need to do this to get the
        // right dirty rect. Since this is called from RenderObject::setStyle, the relative or sticky position
        // flag on the RenderObject has been cleared, so use the one on the style().
        auto offsetForInFlowPosition = layer()->offsetForInFlowPosition();
        adjustedRects.move(offsetForInFlowPosition);
    }

    if (localContainer->hasNonVisibleOverflow()) {
        // FIXME: Respect the value of context.options.
        auto containerContext = context;
        containerContext.options.add(VisibleRectContext::Option::ApplyCompositedContainerScrolls);
        bool isEmpty = !downcast<RenderLayerModelObject>(*localContainer).applyCachedClipAndScrollPosition(adjustedRects, container, containerContext);
        if (isEmpty) {
            if (context.options.contains(VisibleRectContext::Option::UseEdgeInclusiveIntersection))
                return std::nullopt;
            return adjustedRects;
        }
    }

    if (containerSkipped) {
        // If the repaintContainer is below o, then we need to map the rect into repaintContainer's coordinates.
        auto containerOffset = container->offsetFromAncestorContainer(*localContainer);
        adjustedRects.move(-containerOffset);
        return adjustedRects;
    }

    return localContainer->computeVisibleRectsInContainer(adjustedRects, container, context, state);
}

LayoutSize RenderInline::offsetFromContainer(const RenderElement& container, const LayoutPoint&, bool* offsetDependsOnPoint) const
{
    ASSERT(&container == this->container());
    
    LayoutSize offset;    
    if (isInFlowPositioned())
        offset += offsetForInFlowPosition();

    if (auto* box = dynamicDowncast<RenderBox>(container))
        offset -= toLayoutSize(box->scrollPosition());

    if (offsetDependsOnPoint)
        *offsetDependsOnPoint = (is<RenderBox>(container) && container.writingMode().isBlockFlipped()) || is<RenderFragmentedFlow>(container);

    return offset;
}

void RenderInline::mapLocalToContainer(const RenderLayerModelObject* ancestorContainer, TransformState& transformState, OptionSet<MapCoordinatesMode> mode, bool* wasFixed) const
{
    if (ancestorContainer == this)
        return;

    if (view().frameView().layoutContext().isPaintOffsetCacheEnabled() && !ancestorContainer) {
        auto* layoutState = view().frameView().layoutContext().layoutState();
        LayoutSize offset = layoutState->paintOffset();
        if (style().hasInFlowPosition() && layer())
            offset += layer()->offsetForInFlowPosition();
        transformState.move(offset);
        return;
    }

    bool containerSkipped;
    RenderElement* container = this->container(ancestorContainer, containerSkipped);
    if (!container)
        return;

    if (mode.contains(MapCoordinatesMode::ApplyContainerFlip)) {
        if (CheckedPtr box = dynamicDowncast<RenderBox>(*container)) {
            if (container->writingMode().isBlockFlipped()) {
                LayoutPoint centerPoint(transformState.mappedPoint());
                transformState.move(box->flipForWritingMode(centerPoint) - centerPoint);
            }
            mode.remove(MapCoordinatesMode::ApplyContainerFlip);
        }
    }

    LayoutSize containerOffset = offsetFromContainer(*container, LayoutPoint(transformState.mappedPoint()));

    pushOntoTransformState(transformState, mode, ancestorContainer, container, containerOffset, containerSkipped);
    if (containerSkipped)
        return;

    container->mapLocalToContainer(ancestorContainer, transformState, mode, wasFixed);
}

LayoutSize RenderInline::offsetForInFlowPositionedInline(const RenderBox* child) const
{
    // FIXME: This function isn't right with mixed writing modes.
    // An inline box is the containing block for an out-of-flow child when it is in-flow positioned, and also when
    // something else about it makes it one, e.g. a filter. Either way the child's static position is relative to the
    // inline's own content, so it needs the offset of the line the inline starts on.
    if (!canContainAbsolutelyPositionedObjects()) {
        ASSERT_NOT_REACHED();
        return { };
    }

    if (!hasLayer()) {
        // It looks like we are a containing block but no layer created yet. It essentially means we don't have a position offset yet.
        return { };
    }

    // When we have an enclosing relpositioned inline, we need to add in the offset of the first line
    // box from the rest of the content, but only in the cases where we know we're positioned
    // relative to the inline itself.
    auto inlinePosition = layer()->staticInlinePosition();
    auto blockPosition = layer()->staticBlockPosition();
    if (auto* inlineBox = firstLegacyInlineBoxFor(*this)) {
        inlinePosition = LayoutUnit::fromFloatRound(inlineBox->logicalLeft());
        blockPosition = inlineBox->logicalTop();
    } else if (LayoutIntegration::LineLayout::containing(*this)) {
        if (!layoutBox()) {
            // Repaint may be issued on subtrees during content mutation with newly inserted renderers.
            ASSERT(needsLayout());
            return { };
        }
        if (auto inlineBox = InlineIterator::lineLeftmostInlineBoxFor(*this)) {
            inlinePosition = LayoutUnit::fromFloatRound(inlineBox->logicalLeftIgnoringInlineDirection());
            blockPosition = inlineBox->logicalTop();
        } else if (auto* blockContainer = containingBlock()) {
            // This must be a block with no in-flow content e.g. <div><span><abs pos box></span></div> where we don't construct any display box at all.
            auto contentBoxLocation = blockContainer->contentBoxLocation();
            inlinePosition = contentBoxLocation.x();
            blockPosition = contentBoxLocation.y();
        }
    }

    // Per http://www.w3.org/TR/CSS2/visudet.html#abs-non-replaced-width an absolute positioned box with a static position
    // should locate itself as though it is a normal flow box in relation to its containing block.
    LayoutSize logicalOffset;
    if (!child->style().hasStaticInlinePosition(writingMode().isHorizontal())
        || !child->style().positionArea().isNone() || child->style().justifySelf().isAnchorCenter())
        logicalOffset.setWidth(inlinePosition);

    if (!child->style().hasStaticBlockPosition(writingMode().isHorizontal())
        || !child->style().positionArea().isNone() || child->style().alignSelf().isAnchorCenter())
        logicalOffset.setHeight(blockPosition);

    return writingMode().isHorizontal() ? logicalOffset : logicalOffset.transposedSize();
}

void RenderInline::imageChanged(WrappedImagePtr image, const IntRect*)
{
    if (!parent())
        return;

    bool isNonEmpty;
    RefPtr styleImage = Style::findLayerUsedImage(style().backgroundLayers(), image, isNonEmpty);
    if (styleImage && isNonEmpty) {
        if (auto styleable = Styleable::fromRenderer(*this))
            protect(document())->didLoadImage(protect(styleable->element).get(), protect(styleImage->cachedImage()));
    }

    // FIXME: We can do better.
    repaint();
}

namespace {
    class AbsoluteRectsIgnoringEmptyGeneratorContext : public AbsoluteRectsGeneratorContext {
        public:
            AbsoluteRectsIgnoringEmptyGeneratorContext(Vector<LayoutRect>& rects, const LayoutPoint& accumulatedOffset)
                : AbsoluteRectsGeneratorContext(rects, accumulatedOffset) { }

                void addRect(const FloatRect& rect)
                {
                    if (!rect.isEmpty())
                        AbsoluteRectsGeneratorContext::addRect(rect);
                }
    };
} // unnamed namespace

void RenderInline::collectLineBoxRects(Vector<LayoutRect>& rects, const LayoutPoint& additionalOffset) const
{
    AbsoluteRectsIgnoringEmptyGeneratorContext context(rects, additionalOffset);
    generateLineBoxRects(context);
}

static RenderObject* firstContentfulChild(const RenderInline& renderer)
{
    for (auto& current : childrenOfType<RenderObject>(renderer)) {
        if (current.isFloatingOrOutOfFlowPositioned())
            continue;
        if (auto* text = dynamicDowncast<RenderText>(current); text && text->containsOnlyCollapsibleWhitespace())
            continue;
        if (auto* renderInline = dynamicDowncast<RenderInline>(current)) {
            if (auto* nested = firstContentfulChild(*renderInline))
                return nested;
            continue;
        }
        return const_cast<RenderObject*>(&current);
    }
    return { };
}

bool isEmptyInline(const RenderInline& renderer)
{
    return !firstContentfulChild(renderer);
}

RenderObject* firstContentfulChild(RenderInline& renderer)
{
    return firstContentfulChild(const_cast<const RenderInline&>(renderer));
}

bool RenderInline::requiresLayer() const
{
    return isInFlowPositioned()
        || createsGroup()
        || hasClipPath()
        || style().willChange().canCreateStackingContext()
        || hasRunningAcceleratedAnimations()
        || requiresRenderingConsolidationForViewTransition();
}

} // namespace WebCore
