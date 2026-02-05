/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2005 Allan Sandfeld Jensen (kde@carewolf.com)
 *           (C) 2005, 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2005-2025 Apple Inc. All rights reserved.
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
#include "OutlinePainter.h"

#include "BorderEdge.h"
#include "BorderPainter.h"
#include "BorderShape.h"
#include "ContainerNodeInlines.h"
#include "FloatPointGraph.h"
#include "FloatRoundedRect.h"
#include "GeometryUtilities.h"
#include "GraphicsContext.h"
#include "HTMLNames.h"
#include "HTMLSelectElement.h"
#include "InlineIteratorInlineBox.h"
#include "InlineIteratorLineBox.h"
#include "LegacyRenderSVGModelObject.h"
#include "PaintInfo.h"
#include "PathUtilities.h"
#include "RenderBlockFlow.h"
#include "RenderChildIterator.h"
#include "RenderElementStyleInlines.h"
#include "RenderInline.h"
#include "RenderListBox.h"
#include "RenderObjectDocument.h"
#include "RenderStyle+GettersInlines.h"
#include "RenderSVGModelObject.h"
#include "RenderTheme.h"
#include "StyleBorderRadius.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"

namespace WebCore {

OutlinePainter::OutlinePainter(const PaintInfo& paintInfo)
    : m_paintInfo(paintInfo)
{
}

static float deviceScaleFactor(const RenderElement& renderer)
{
    return protect(renderer.document())->deviceScaleFactor();
}

void OutlinePainter::paintOutline(const RenderElement& renderer, const LayoutRect& paintRect) const
{
    ASSERT(renderer.hasOutline());

    CheckedRef styleToUse = renderer.style();
    auto hasThemedFocusRing = renderer.theme().supportsFocusRing(renderer, styleToUse.get());

    // Only paint the focus ring by hand if the theme isn't able to draw it.
    if (styleToUse->outlineStyle() == OutlineStyle::Auto && !hasThemedFocusRing) {
        LayoutRect paintRectToUse { paintRect };
        if (CheckedPtr box = dynamicDowncast<RenderBox>(renderer))
            paintRectToUse = renderer.theme().adjustedPaintRect(*box, paintRectToUse);
        CheckedPtr paintContainer = m_paintInfo.paintContainer;
        auto focusRingRects = collectFocusRingRects(renderer, paintRectToUse.location(), paintContainer.get());

        paintFocusRing(renderer, focusRingRects);
        return;
    }

    if (renderer.hasOutlineAnnotation() && !hasThemedFocusRing)
        addPDFURLAnnotationForLink(renderer, paintRect.location());

    auto borderStyle = toBorderStyle(styleToUse->outlineStyle());
    if (!borderStyle || *borderStyle == BorderStyle::None)
        return;

    auto outlineWidth = Style::evaluate<LayoutUnit>(styleToUse->usedOutlineWidth(), Style::ZoomNeeded { });
    auto outlineOffset = Style::evaluate<LayoutUnit>(styleToUse->usedOutlineOffset(), Style::ZoomNeeded { });

    auto outerRect = paintRect;
    outerRect.inflate(outlineOffset + outlineWidth);
    // FIXME: This prevents outlines from painting inside the object http://webkit.org/b/12042.
    if (outerRect.isEmpty())
        return;

    auto hasBorderRadius = styleToUse->hasBorderRadius();
    auto closedEdges = RectEdges<bool> { true };

    auto outlineEdgeWidths = RectEdges<LayoutUnit> { outlineWidth };
    auto outlineShape = BorderShape::shapeForOutsetRect(styleToUse.get(), paintRect, outerRect, outlineEdgeWidths, closedEdges);

    auto bleedAvoidance = BleedAvoidance::ShrinkBackground;
    auto appliedClipAlready = false;
    auto edges = borderEdgesForOutline(styleToUse, *borderStyle, deviceScaleFactor(renderer));
    auto haveAllSolidEdges = BorderPainter::decorationHasAllSolidEdges(edges);

    BorderPainter { renderer, m_paintInfo }.paintSides(outlineShape, {
        hasBorderRadius ? std::make_optional(styleToUse->borderRadii()) : std::nullopt,
        edges,
        haveAllSolidEdges,
        outlineShape.outerShapeIsRectangular(),
        outlineShape.innerShapeIsRectangular(),
        bleedAvoidance,
        closedEdges,
        appliedClipAlready,
    });
}

void OutlinePainter::paintOutline(const RenderInline& renderer, const LayoutPoint& paintOffset) const
{
    ASSERT(renderer.hasOutline());

    CheckedRef styleToUse = renderer.style();

    if (styleToUse->outlineStyle() == OutlineStyle::Auto) {
        CheckedPtr paintContainer = m_paintInfo.paintContainer;
        auto focusRingRects = collectFocusRingRects(renderer, paintOffset, paintContainer.get());

        paintFocusRing(renderer, focusRingRects);
        return;
    }

    if (renderer.hasOutlineAnnotation())
        addPDFURLAnnotationForLink(renderer, paintOffset);

    if (m_paintInfo.context().paintingDisabled() || !styleToUse->hasOutline())
        return;

    if (!renderer.containingBlock()) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto isHorizontalWritingMode = renderer.isHorizontalWritingMode();
    CheckedRef containingBlock = *renderer.containingBlock();
    auto isFlipped = containingBlock->writingMode().isBlockFlipped();
    Vector<LayoutRect> rects;
    for (auto box = InlineIterator::lineLeftmostInlineBoxFor(renderer); box; box.traverseInlineBoxLineRightward()) {
        auto lineBox = box->lineBox();
        auto logicalTop = std::max(lineBox->contentLogicalTop(), box->logicalTop());
        auto logicalBottom = std::min(lineBox->contentLogicalBottom(), box->logicalBottom());
        auto enclosingVisualRect = FloatRect { box->logicalLeftIgnoringInlineDirection(), logicalTop, box->logicalWidth(), logicalBottom - logicalTop };

        if (!isHorizontalWritingMode)
            enclosingVisualRect = enclosingVisualRect.transposedRect();

        if (isFlipped)
            containingBlock->flipForWritingMode(enclosingVisualRect);

        rects.append(LayoutRect { enclosingVisualRect });
    }
    paintOutlineWithLineRects(renderer, paintOffset, rects);
}

void OutlinePainter::paintOutlineWithLineRects(const RenderInline& renderer, const LayoutPoint& paintOffset, const Vector<LayoutRect>& lineRects) const
{
    if (lineRects.size() == 1) {
        auto adjustedPaintRect = lineRects[0];
        adjustedPaintRect.moveBy(paintOffset);
        paintOutline(renderer, adjustedPaintRect);
        return;
    }

    auto styleToUse = CheckedRef { renderer.style() };

    auto outlineOffset = Style::evaluate<float>(styleToUse->usedOutlineOffset(), Style::ZoomNeeded { });
    auto outlineWidth = Style::evaluate<float>(styleToUse->usedOutlineWidth(), Style::ZoomNeeded { });

    auto deviceScaleFactor = WebCore::deviceScaleFactor(renderer);

    Vector<FloatRect> pixelSnappedRects;
    for (size_t index = 0; index < lineRects.size(); ++index) {
        auto rect = lineRects[index];

        rect.moveBy(paintOffset);
        rect.inflate(outlineOffset + outlineWidth / 2);
        pixelSnappedRects.append(snapRectToDevicePixels(rect, deviceScaleFactor));
    }
    auto path = pathWithShrinkWrappedRects(pixelSnappedRects, styleToUse->border().radii, outlineOffset, styleToUse->writingMode(), deviceScaleFactor);
    if (path.isEmpty()) {
        // Disjoint line spanning inline boxes.
        for (auto rect : lineRects) {
            rect.moveBy(paintOffset);
            paintOutline(renderer, rect);
        }
        return;
    }

    auto& graphicsContext = m_paintInfo.context();
    auto outlineColor = styleToUse->visitedDependentOutlineColorApplyingColorFilter();
    auto useTransparencyLayer = !outlineColor.isOpaque();
    if (useTransparencyLayer) {
        graphicsContext.beginTransparencyLayer(outlineColor.alphaAsFloat());
        outlineColor = outlineColor.opaqueColor();
    }

    graphicsContext.setStrokeColor(outlineColor);
    graphicsContext.setStrokeThickness(outlineWidth);
    graphicsContext.setStrokeStyle(StrokeStyle::SolidStroke);
    graphicsContext.strokePath(path);

    if (useTransparencyLayer)
        graphicsContext.endTransparencyLayer();
}

static bool usePlatformFocusRingColorForOutlineStyleAuto()
{
#if PLATFORM(COCOA) || PLATFORM(GTK) || PLATFORM(WPE)
    return true;
#else
    return false;
#endif
}

static bool useShrinkWrappedFocusRingForOutlineStyleAuto()
{
#if PLATFORM(COCOA) || PLATFORM(GTK) || PLATFORM(WPE)
    return true;
#else
    return false;
#endif
}

static void drawFocusRing(GraphicsContext& context, const Path& path, const RenderStyle& style, const Color& color)
{
    context.drawFocusRing(path, Style::evaluate<float>(style.usedOutlineWidth(), Style::ZoomNeeded { }), color);
}

static void drawFocusRing(GraphicsContext& context, Vector<FloatRect> rects, const RenderStyle& style, const Color& color)
{
#if PLATFORM(MAC)
    context.drawFocusRing(rects, 0, Style::evaluate<float>(style.usedOutlineWidth(), Style::ZoomNeeded { }), color);
#else
    context.drawFocusRing(rects, Style::evaluate<float>(style.usedOutlineOffset(), Style::ZoomNeeded { }), Style::evaluate<float>(style.usedOutlineWidth(), Style::ZoomNeeded { }), color);
#endif
}

void OutlinePainter::paintFocusRing(const RenderElement& renderer, const Vector<LayoutRect>& focusRingRects) const
{
    CheckedRef style = renderer.style();

    ASSERT(style->outlineStyle() == OutlineStyle::Auto);

    auto deviceScaleFactor = WebCore::deviceScaleFactor(renderer);
    auto outlineOffset = Style::evaluate<float>(style->usedOutlineOffset(), Style::ZoomNeeded { });

    Vector<FloatRect> pixelSnappedFocusRingRects;
    for (auto rect : focusRingRects) {
        rect.inflate(outlineOffset);
        pixelSnappedFocusRingRects.append(snapRectToDevicePixels(rect, deviceScaleFactor));
    }
    auto styleOptions = renderer.styleColorOptions();
    styleOptions.add(StyleColorOptions::UseSystemAppearance);
    auto focusRingColor = usePlatformFocusRingColorForOutlineStyleAuto() ? RenderTheme::singleton().focusRingColor(styleOptions) : style->visitedDependentOutlineColorApplyingColorFilter();
    if (useShrinkWrappedFocusRingForOutlineStyleAuto() && style->hasBorderRadius()) {
        auto path = pathWithShrinkWrappedRects(pixelSnappedFocusRingRects, style->border().radii, outlineOffset, style->writingMode(), deviceScaleFactor);
        if (path.isEmpty()) {
            for (auto rect : pixelSnappedFocusRingRects)
                path.addRect(rect);
        }
        drawFocusRing(m_paintInfo.context(), path, style.get(), focusRingColor);
    } else
        drawFocusRing(m_paintInfo.context(), pixelSnappedFocusRingRects, style.get(), focusRingColor);
}

Vector<LayoutRect> OutlinePainter::collectFocusRingRects(const RenderElement& renderer, const LayoutPoint& additionalOffset, const RenderLayerModelObject* paintContainer)
{
    Vector<LayoutRect> rects;
    collectFocusRingRects(renderer, rects, additionalOffset, paintContainer);
    return rects;
}

static void appendIfNotEmpty(Vector<LayoutRect>& rects, LayoutRect&& rect)
{
    if (rect.isEmpty())
        return;
    rects.append(rect);
}

void OutlinePainter::collectFocusRingRects(const RenderElement& renderer, Vector<LayoutRect>& rects, const LayoutPoint& additionalOffset, const RenderLayerModelObject* paintContainer)
{
    if (CheckedPtr svgRenderer = dynamicDowncast<RenderSVGModelObject>(renderer)) {
        svgRenderer->addFocusRingRects(rects, additionalOffset, paintContainer);
        return;
    }
    if (CheckedPtr svgRenderer = dynamicDowncast<LegacyRenderSVGModelObject>(renderer)) {
        svgRenderer->addFocusRingRects(rects, additionalOffset, paintContainer);
        return;
    }
    if (CheckedPtr renderInline = dynamicDowncast<RenderInline>(renderer)) {
        collectFocusRingRectsForInline(*renderInline, rects, additionalOffset, paintContainer);
        return;
    }
    if (CheckedPtr listBox = dynamicDowncast<RenderListBox>(renderer)) {
        if (collectFocusRingRectsForListBox(*listBox, rects, additionalOffset, paintContainer))
            return;
    }
    if (CheckedPtr block = dynamicDowncast<RenderBlock>(renderer)) {
        if (collectFocusRingRectsForBlock(*block, rects, additionalOffset, paintContainer))
            return;
    }
    if (CheckedPtr box = dynamicDowncast<RenderBox>(renderer))
        appendIfNotEmpty(rects, { additionalOffset, box->size() });
}

bool OutlinePainter::collectFocusRingRectsForListBox(const RenderListBox& renderer, Vector<LayoutRect>& rects, const LayoutPoint& additionalOffset, const RenderLayerModelObject*)
{
    if (!renderer.selectElement().allowsNonContiguousSelection())
        return false;

    CheckedRef selectElement = renderer.selectElement();

    // Focus the last selected item.
    int selectedItem = selectElement->activeSelectionEndListIndex();
    if (selectedItem >= 0) {
        rects.append(snappedIntRect(renderer.itemBoundingBoxRect(additionalOffset, selectedItem)));
        return true;
    }

    // No selected items, find the first non-disabled item.
    int indexOfFirstEnabledOption = 0;
    for (auto& item : selectElement->listItems()) {
        if (is<HTMLOptionElement>(item.get()) && !item->isDisabledFormControl()) {
            selectElement->setActiveSelectionEndIndex(indexOfFirstEnabledOption);
            rects.append(renderer.itemBoundingBoxRect(additionalOffset, indexOfFirstEnabledOption));
            return true;
        }
        indexOfFirstEnabledOption++;
    }
    return true;
}

void OutlinePainter::collectFocusRingRectsForInline(const RenderInline& renderer, Vector<LayoutRect>& rects, const LayoutPoint& additionalOffset, const RenderLayerModelObject* paintContainer)
{
    renderer.collectLineBoxRects(rects, additionalOffset);

    for (CheckedRef child : childrenOfType<RenderBoxModelObject>(renderer)) {
        if (child->isRenderListMarker())
            continue;
        FloatPoint pos(additionalOffset);
        // FIXME: This doesn't work correctly with transforms.
        if (child->hasLayer())
            pos = child->localToContainerPoint(FloatPoint(), paintContainer);
        else if (auto* box = dynamicDowncast<RenderBox>(child.get()))
            pos.move(box->locationOffset());
        collectFocusRingRects(child, rects, flooredIntPoint(pos), paintContainer);
    }

    if (CheckedPtr continuation = renderer.continuation()) {
        if (CheckedPtr inlineRenderer = dynamicDowncast<RenderInline>(*continuation))
            collectFocusRingRectsForInline(*inlineRenderer, rects, flooredLayoutPoint(LayoutPoint(additionalOffset + continuation->containingBlock()->location() - renderer.containingBlock()->location())), paintContainer);
        else
            collectFocusRingRects(*continuation, rects, flooredLayoutPoint(LayoutPoint(additionalOffset + downcast<RenderBox>(*continuation).location() - renderer.containingBlock()->location())), paintContainer);
    }
}

bool OutlinePainter::collectFocusRingRectsForBlock(const RenderBlock& renderer, Vector<LayoutRect>& rects, const LayoutPoint& additionalOffset, const RenderLayerModelObject* paintContainer)
{
    if (renderer.isRenderTextControl())
        return false;

    // For blocks inside inlines, we include margins so that we run right up to the inline boxes
    // above and below us (thus getting merged with them to form a single irregular shape).
    CheckedPtr inlineContinuation = renderer.inlineContinuation();
    if (inlineContinuation) {
        // FIXME: This check really isn't accurate.
        bool nextInlineHasLineBox = inlineContinuation->firstLegacyInlineBox();
        // FIXME: This is wrong. The principal renderer may not be the continuation preceding this block.
        // FIXME: This is wrong for block-flows that are horizontal.
        // https://bugs.webkit.org/show_bug.cgi?id=46781
        bool prevInlineHasLineBox = downcast<RenderInline>(*inlineContinuation->element()->renderer()).firstLegacyInlineBox();
        auto topMargin = prevInlineHasLineBox ? renderer.collapsedMarginBefore() : 0_lu;
        auto bottomMargin = nextInlineHasLineBox ? renderer.collapsedMarginAfter() : 0_lu;
        LayoutRect rect(additionalOffset.x(), additionalOffset.y() - topMargin, renderer.width(), renderer.height() + topMargin + bottomMargin);
        appendIfNotEmpty(rects, WTF::move(rect));
    } else if (renderer.width() && renderer.height())
        rects.append(LayoutRect(additionalOffset, renderer.size()));

    if (!renderer.hasNonVisibleOverflow() && !renderer.hasControlClip()) {
        if (renderer.childrenInline() && is<RenderBlockFlow>(renderer))
            collectFocusRingRectsForInlineChildren(downcast<RenderBlockFlow>(renderer), rects, additionalOffset, paintContainer);

        for (CheckedRef box : childrenOfType<RenderBox>(renderer))
            collectFocusRingRectsForChildBox(box, rects, additionalOffset, paintContainer);
    }

    if (inlineContinuation)
        collectFocusRingRects(*inlineContinuation, rects, flooredLayoutPoint(LayoutPoint(additionalOffset + inlineContinuation->containingBlock()->location() - renderer.location())), paintContainer);
    return true;
}

void OutlinePainter::collectFocusRingRectsForChildBox(const RenderBox& box, Vector<LayoutRect>& rects, const LayoutPoint& additionalOffset, const RenderLayerModelObject* paintContainer)
{
    if (box.isRenderListMarker() || box.isOutOfFlowPositioned())
        return;

    FloatPoint pos;
    // FIXME: This doesn't work correctly with transforms.
    if (box.layer())
        pos = box.localToContainerPoint(FloatPoint(), paintContainer);
    else
        pos = FloatPoint(additionalOffset.x() + box.x(), additionalOffset.y() + box.y());
    collectFocusRingRects(box, rects, flooredLayoutPoint(pos), paintContainer);
}

void OutlinePainter::collectFocusRingRectsForInlineChildren(const RenderBlockFlow& renderer, Vector<LayoutRect>& rects, const LayoutPoint& additionalOffset, const RenderLayerModelObject* paintContainer)
{
    ASSERT(renderer.childrenInline());

    bool hasBlockContent = false;

    for (auto box = InlineIterator::firstRootInlineBoxFor(renderer); box; box.traverseInlineBoxLineRightward()) {
        auto lineBox = box->lineBox();
        if (lineBox->hasBlockContent()) {
            hasBlockContent = true;
            continue;
        }
        // FIXME: This is mixing physical and logical coordinates.
        auto unflippedVisualRect = box->visualRectIgnoringBlockDirection();
        auto top = std::max(lineBox->contentLogicalTop(), unflippedVisualRect.y());
        auto bottom = std::min(lineBox->contentLogicalBottom(), unflippedVisualRect.maxY());
        auto rect = LayoutRect { LayoutUnit { additionalOffset.x() + unflippedVisualRect.x() }
            , additionalOffset.y() + top
            , LayoutUnit { unflippedVisualRect.width() }
            , bottom - top };
        appendIfNotEmpty(rects, WTF::move(rect));
    }

    if (hasBlockContent) {
        for (auto line = InlineIterator::firstLineBoxFor(renderer); line; line.traverseNext()) {
            auto blockLevelBox = line->blockLevelBox();
            if (!blockLevelBox)
                continue;
            CheckedRef renderBox = downcast<RenderBox>(blockLevelBox->renderer());
            collectFocusRingRectsForChildBox(renderBox, rects, additionalOffset, paintContainer);
        }
    }
}

static std::pair<FloatPoint, FloatPoint> startAndEndPointsForCorner(const FloatPointGraph::Edge& fromEdge, const FloatPointGraph::Edge& toEdge, const FloatSize& radius)
{
    FloatSize fromEdgeVector = *fromEdge.second - *fromEdge.first;
    FloatSize toEdgeVector = *toEdge.second - *toEdge.first;

    FloatPoint fromEdgeNorm = toFloatPoint(fromEdgeVector);
    fromEdgeNorm.normalize();
    FloatSize fromOffset = FloatSize(radius.width() * fromEdgeNorm.x(), radius.height() * fromEdgeNorm.y());
    FloatPoint startPoint = *fromEdge.second - fromOffset;

    FloatPoint toEdgeNorm = toFloatPoint(toEdgeVector);
    toEdgeNorm.normalize();
    FloatSize toOffset = FloatSize(radius.width() * toEdgeNorm.x(), radius.height() * toEdgeNorm.y());
    FloatPoint endPoint = *toEdge.first + toOffset;
    return std::make_pair(startPoint, endPoint);
}

enum class PainterCornerType : uint8_t { TopLeft, TopRight, BottomRight, BottomLeft, Other };

static PainterCornerType cornerType(const FloatPointGraph::Edge& fromEdge, const FloatPointGraph::Edge& toEdge)
{
    auto fromEdgeVector = *fromEdge.second - *fromEdge.first;
    auto toEdgeVector = *toEdge.second - *toEdge.first;

    if (fromEdgeVector.height() < 0 && toEdgeVector.width() > 0)
        return PainterCornerType::TopLeft;
    if (fromEdgeVector.width() > 0 && toEdgeVector.height() > 0)
        return PainterCornerType::TopRight;
    if (fromEdgeVector.height() > 0 && toEdgeVector.width() < 0)
        return PainterCornerType::BottomRight;
    if (fromEdgeVector.width() < 0 && toEdgeVector.height() < 0)
        return PainterCornerType::BottomLeft;
    return PainterCornerType::Other;
}

static PainterCornerType cornerTypeForMultiline(const FloatPointGraph::Edge& fromEdge, const FloatPointGraph::Edge& toEdge, const Vector<FloatPoint>& corners)
{
    auto corner = cornerType(fromEdge, toEdge);
    if (corner == PainterCornerType::TopLeft && corners.at(0) == *fromEdge.second)
        return corner;
    if (corner == PainterCornerType::TopRight && corners.at(1) == *fromEdge.second)
        return corner;
    if (corner == PainterCornerType::BottomRight && corners.at(2) == *fromEdge.second)
        return corner;
    if (corner == PainterCornerType::BottomLeft && corners.at(3) == *fromEdge.second)
        return corner;
    return PainterCornerType::Other;
}

static std::pair<FloatPoint, FloatPoint> controlPointsForBezierCurve(PainterCornerType cornerType, const FloatPointGraph::Edge& fromEdge, const FloatPointGraph::Edge& toEdge, const FloatSize& radius)
{
    FloatPoint cp1;
    FloatPoint cp2;
    switch (cornerType) {
    case PainterCornerType::TopLeft: {
        cp1 = FloatPoint(fromEdge.second->x(), fromEdge.second->y() + radius.height() * Path::circleControlPoint());
        cp2 = FloatPoint(toEdge.first->x() + radius.width() * Path::circleControlPoint(), toEdge.first->y());
        break;
    }
    case PainterCornerType::TopRight: {
        cp1 = FloatPoint(fromEdge.second->x() - radius.width() * Path::circleControlPoint(), fromEdge.second->y());
        cp2 = FloatPoint(toEdge.first->x(), toEdge.first->y() + radius.height() * Path::circleControlPoint());
        break;
    }
    case PainterCornerType::BottomRight: {
        cp1 = FloatPoint(fromEdge.second->x(), fromEdge.second->y() - radius.height() * Path::circleControlPoint());
        cp2 = FloatPoint(toEdge.first->x() - radius.width() * Path::circleControlPoint(), toEdge.first->y());
        break;
    }
    case PainterCornerType::BottomLeft: {
        cp1 = FloatPoint(fromEdge.second->x() + radius.width() * Path::circleControlPoint(), fromEdge.second->y());
        cp2 = FloatPoint(toEdge.first->x(), toEdge.first->y() - radius.height() * Path::circleControlPoint());
        break;
    }
    case PainterCornerType::Other: {
        ASSERT_NOT_REACHED();
        break;
    }
    }
    return std::make_pair(cp1, cp2);
}

static CornerRadii adjustedRadiiForHuggingCurve(const CornerRadii& inputRadii, float outlineOffset)
{
    // This adjusts the radius so that it follows the border curve even when offset is present.
    auto adjustedRadius = [outlineOffset](const FloatSize& radius) {
        FloatSize expandSize;
        if (radius.width() > outlineOffset)
            expandSize.setWidth(std::min(outlineOffset, radius.width() - outlineOffset));
        if (radius.height() > outlineOffset)
            expandSize.setHeight(std::min(outlineOffset, radius.height() - outlineOffset));
        FloatSize adjustedRadius = radius;
        adjustedRadius.expand(expandSize.width(), expandSize.height());
        // Do not go to negative radius.
        return adjustedRadius.expandedTo(FloatSize(0, 0));
    };

    return {
        adjustedRadius(inputRadii.topLeft()),
        adjustedRadius(inputRadii.topRight()),
        adjustedRadius(inputRadii.bottomLeft()),
        adjustedRadius(inputRadii.bottomRight()),
    };
}

static std::optional<FloatRect> rectFromPolygon(const FloatPointGraph::Polygon& poly)
{
    if (poly.size() != 4)
        return std::optional<FloatRect>();

    std::optional<FloatPoint> topLeft;
    std::optional<FloatPoint> bottomRight;
    for (unsigned i = 0; i < poly.size(); ++i) {
        const auto& toEdge = poly[i];
        const auto& fromEdge = (i > 0) ? poly[i - 1] : poly[poly.size() - 1];
        auto corner = cornerType(fromEdge, toEdge);
        if (corner == PainterCornerType::TopLeft) {
            ASSERT(!topLeft);
            topLeft = *fromEdge.second;
        } else if (corner == PainterCornerType::BottomRight) {
            ASSERT(!bottomRight);
            bottomRight = *fromEdge.second;
        }
    }
    if (!topLeft || !bottomRight)
        return std::optional<FloatRect>();
    return FloatRect(topLeft.value(), bottomRight.value());
}

Path OutlinePainter::pathWithShrinkWrappedRects(const Vector<FloatRect>& rects, const Style::BorderRadius& radii, float outlineOffset, WritingMode writingMode, float deviceScaleFactor)
{
    auto roundedRect = [radii, outlineOffset, deviceScaleFactor](const FloatRect& rect) {
        auto adjustedRadii = adjustedRadiiForHuggingCurve(Style::evaluate<CornerRadii>(radii, rect.size(), Style::ZoomNeeded { }), outlineOffset);
        adjustedRadii.scale(calcBorderRadiiConstraintScaleFor(rect, adjustedRadii));

        LayoutRoundedRect roundedRect(
            LayoutRect(rect),
            LayoutRoundedRect::Radii(
                LayoutSize(adjustedRadii.topLeft()),
                LayoutSize(adjustedRadii.topRight()),
                LayoutSize(adjustedRadii.bottomLeft()),
                LayoutSize(adjustedRadii.bottomRight())
            )
        );
        Path path;
        path.addRoundedRect(roundedRect.pixelSnappedRoundedRectForPainting(deviceScaleFactor));
        return path;
    };

    if (rects.size() == 1)
        return roundedRect(rects.at(0));

    auto [graph, polys] = FloatPointGraph::polygonsForRect(rects);

    // Fall back to corner painting with no radius for empty and disjoint rectangles.
    if (!polys.size() || polys.size() > 1)
        return Path();
    const auto& poly = polys.at(0);
    // Fast path when poly has one rect only.
    std::optional<FloatRect> rect = rectFromPolygon(poly);
    if (rect)
        return roundedRect(rect.value());

    Path path;
    // Multiline outline needs to match multiline border painting. Only first and last lines are getting rounded borders.
    auto isLeftToRight = writingMode.isBidiLTR();
    auto firstLineRect = isLeftToRight ? rects.at(0) : rects.at(rects.size() - 1);
    auto lastLineRect = isLeftToRight ? rects.at(rects.size() - 1) : rects.at(0);
    // Adjust radius so that it matches the box border.
    auto firstLineRadii = Style::evaluate<CornerRadii>(radii, firstLineRect.size(), Style::ZoomNeeded { });
    auto lastLineRadii = Style::evaluate<CornerRadii>(radii, lastLineRect.size(), Style::ZoomNeeded { });
    firstLineRadii.scale(calcBorderRadiiConstraintScaleFor(firstLineRect, firstLineRadii));
    lastLineRadii.scale(calcBorderRadiiConstraintScaleFor(lastLineRect, lastLineRadii));

    // physical topLeft/topRight/bottomRight/bottomLeft
    auto isHorizontal = writingMode.isHorizontal();
    auto corners = Vector<FloatPoint>::from(
        firstLineRect.minXMinYCorner(),
        isHorizontal ? lastLineRect.maxXMinYCorner() : firstLineRect.maxXMinYCorner(),
        lastLineRect.maxXMaxYCorner(),
        isHorizontal ? firstLineRect.minXMaxYCorner() : lastLineRect.minXMaxYCorner()
    );

    for (unsigned i = 0; i < poly.size(); ++i) {
        auto moveOrAddLineTo = [i, &path](const FloatPoint& startPoint) {
            if (!i)
                path.moveTo(startPoint);
            else
                path.addLineTo(startPoint);
        };
        const auto& toEdge = poly[i];
        const auto& fromEdge = (i > 0) ? poly[i - 1] : poly[poly.size() - 1];
        FloatSize radius;
        auto corner = cornerTypeForMultiline(fromEdge, toEdge, corners);
        switch (corner) {
        case PainterCornerType::TopLeft:
            radius = firstLineRadii.topLeft();
            break;
        case PainterCornerType::TopRight:
            radius = lastLineRadii.topRight();
            break;
        case PainterCornerType::BottomRight:
            radius = lastLineRadii.bottomRight();
            break;
        case PainterCornerType::BottomLeft:
            radius = firstLineRadii.bottomLeft();
            break;
        case PainterCornerType::Other:
            // Do not apply border radius on corners that normal border painting skips. (multiline content)
            moveOrAddLineTo(*fromEdge.second);
            continue;
        }
        auto [startPoint, endPoint] = startAndEndPointsForCorner(fromEdge, toEdge, radius);
        moveOrAddLineTo(startPoint);

        auto [cp1, cp2] = controlPointsForBezierCurve(corner, fromEdge, toEdge, radius);
        path.addBezierCurveTo(cp1, cp2, endPoint);
    }
    path.closeSubpath();
    return path;
}

void OutlinePainter::addPDFURLAnnotationForLink(const RenderElement& renderer, const LayoutPoint& paintOffset) const
{
    Ref element = *renderer.element();
    ASSERT(element->isLink());

    CheckedPtr paintContainer = m_paintInfo.paintContainer;
    auto focusRingRects = collectFocusRingRects(renderer, paintOffset, paintContainer.get());
    LayoutRect urlRect = unionRect(focusRingRects);

    if (urlRect.isEmpty())
        return;

    auto href = element->getAttribute(HTMLNames::hrefAttr);
    if (href.isNull())
        return;

    if (m_paintInfo.context().supportsInternalLinks()) {
        String outAnchorName;
        RefPtr linkTarget = element->findAnchorElementForLink(outAnchorName);
        if (linkTarget) {
            m_paintInfo.context().setDestinationForRect(outAnchorName, urlRect);
            return;
        }
    }
    m_paintInfo.context().setURLForRect(protect(element->document())->completeURL(href), urlRect);
}

} // namespace WebCore
