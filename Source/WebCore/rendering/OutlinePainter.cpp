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
#include "RenderInline.h"
#include "RenderListBox.h"
#include "RenderObjectDocument.h"
#include "RenderStyleInlines.h"
#include "RenderSVGModelObject.h"
#include "RenderTheme.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"

namespace WebCore {

OutlinePainter::OutlinePainter(const PaintInfo& paintInfo)
    : m_paintInfo(paintInfo)
{
}

static float deviceScaleFactor(const RenderElement& renderer)
{
    return renderer.protectedDocument()->deviceScaleFactor();
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

    auto outlineWidth = Style::evaluate<LayoutUnit>(styleToUse->outlineWidth(), styleToUse->usedZoomForLength());
    auto outlineOffset = Style::evaluate<LayoutUnit>(styleToUse->outlineOffset(), Style::ZoomNeeded { });

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

    auto outlineOffset = Style::evaluate<float>(styleToUse->outlineOffset(), Style::ZoomNeeded { });
    auto outlineWidth = Style::evaluate<float>(styleToUse->outlineWidth(), styleToUse->usedZoomForLength());

    auto deviceScaleFactor = WebCore::deviceScaleFactor(renderer);

    Vector<FloatRect> pixelSnappedRects;
    for (size_t index = 0; index < lineRects.size(); ++index) {
        auto rect = lineRects[index];

        rect.moveBy(paintOffset);
        rect.inflate(outlineOffset + outlineWidth / 2);
        pixelSnappedRects.append(snapRectToDevicePixels(rect, deviceScaleFactor));
    }
    auto path = PathUtilities::pathWithShrinkWrappedRectsForOutline(pixelSnappedRects, styleToUse->border().radii(), outlineOffset, styleToUse->writingMode(), deviceScaleFactor);
    if (path.isEmpty()) {
        // Disjoint line spanning inline boxes.
        for (auto rect : lineRects) {
            rect.moveBy(paintOffset);
            paintOutline(renderer, rect);
        }
        return;
    }

    auto& graphicsContext = m_paintInfo.context();
    auto outlineColor = styleToUse->visitedDependentColorWithColorFilter(CSSPropertyOutlineColor);
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
    context.drawFocusRing(path, Style::evaluate<float>(style.outlineWidth(), style.usedZoomForLength()), color);
}

static void drawFocusRing(GraphicsContext& context, Vector<FloatRect> rects, const RenderStyle& style, const Color& color)
{
#if PLATFORM(MAC)
    context.drawFocusRing(rects, 0, Style::evaluate<float>(style.outlineWidth(), style.usedZoomForLength()), color);
#else
    context.drawFocusRing(rects, Style::evaluate<float>(style.outlineOffset(), Style::ZoomNeeded { }), Style::evaluate<float>(style.outlineWidth(), style.usedZoomForLength()), color);
#endif
}

void OutlinePainter::paintFocusRing(const RenderElement& renderer, const Vector<LayoutRect>& focusRingRects) const
{
    CheckedRef style = renderer.style();

    ASSERT(style->outlineStyle() == OutlineStyle::Auto);

    auto deviceScaleFactor = WebCore::deviceScaleFactor(renderer);
    auto outlineOffset = Style::evaluate<float>(style->outlineOffset(), Style::ZoomNeeded { });

    Vector<FloatRect> pixelSnappedFocusRingRects;
    for (auto rect : focusRingRects) {
        rect.inflate(outlineOffset);
        pixelSnappedFocusRingRects.append(snapRectToDevicePixels(rect, deviceScaleFactor));
    }
    auto styleOptions = renderer.styleColorOptions();
    styleOptions.add(StyleColorOptions::UseSystemAppearance);
    auto focusRingColor = usePlatformFocusRingColorForOutlineStyleAuto() ? RenderTheme::singleton().focusRingColor(styleOptions) : style->visitedDependentColorWithColorFilter(CSSPropertyOutlineColor);
    if (useShrinkWrappedFocusRingForOutlineStyleAuto() && style->hasBorderRadius()) {
        Path path = PathUtilities::pathWithShrinkWrappedRectsForOutline(pixelSnappedFocusRingRects, style->border().radii(), outlineOffset, style->writingMode(), deviceScaleFactor);
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
        appendIfNotEmpty(rects, WTFMove(rect));
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
        appendIfNotEmpty(rects, WTFMove(rect));
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
    m_paintInfo.context().setURLForRect(element->protectedDocument()->completeURL(href), urlRect);
}

}
