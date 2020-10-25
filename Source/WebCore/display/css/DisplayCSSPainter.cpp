/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "DisplayCSSPainter.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "CachedImage.h"
#include "Color.h"
#include "DisplayBoxDecorationData.h"
#include "DisplayContainerBox.h"
#include "DisplayFillLayerImageGeometry.h"
#include "DisplayImageBox.h"
#include "DisplayStyle.h"
#include "DisplayTextBox.h"
#include "DisplayTree.h"
#include "FillLayer.h"
#include "GraphicsContext.h"
#include "IntRect.h"
#include "LayoutPoint.h"
#include "LayoutState.h"
#include "TextRun.h"

namespace WebCore {
namespace Display {

void CSSPainter::paintFillLayer(const BoxModelBox& box, const FillLayer& layer, const FillLayerImageGeometry& geometry, GraphicsContext& context)
{
    GraphicsContextStateSaver stateSaver(context, false);

    auto clipRectForLayer = [](const BoxModelBox& box, const FillLayer& layer) {
        switch (layer.clip()) {
        case FillBox::Border:
            return box.absoluteBorderBoxRect();
        case FillBox::Padding:
            return box.absolutePaddingBoxRect();
        case FillBox::Content:
            return box.absoluteContentBoxRect();
        case FillBox::Text:
            break;
        }
        return AbsoluteFloatRect();
    };

    switch (layer.clip()) {
    case FillBox::Border:
    case FillBox::Padding:
    case FillBox::Content: {
        stateSaver.save();
        context.clip(clipRectForLayer(box, layer));
        break;
    }
    case FillBox::Text:
        break;
    }

    // FIXME: Handle background compositing modes.

    auto* backgroundImage = layer.image();
    CompositeOperator op = CompositeOperator::SourceOver;

    if (geometry.destRect().isEmpty())
        return;

    auto image = backgroundImage->image(nullptr, geometry.tileSize());
    if (!image)
        return;

    // FIXME: call image->updateFromSettings().

    ImagePaintingOptions options = {
        op == CompositeOperator::SourceOver ? layer.composite() : op,
        layer.blendMode(),
        DecodingMode::Synchronous,
        ImageOrientation::FromImage,
        InterpolationQuality::Default
    };

    context.drawTiledImage(*image, geometry.destRect(), toFloatPoint(geometry.relativePhase()), geometry.tileSize(), geometry.spaceSize(), options);
}

void CSSPainter::paintBackgroundImages(const BoxModelBox& box, GraphicsContext& context)
{
    const auto& style = box.style();

    Vector<const FillLayer*, 8> layers;

    for (auto* layer = style.backgroundLayers(); layer; layer = layer->next())
        layers.append(layer);

    auto* boxDecorationData = box.boxDecorationData();
    ASSERT(boxDecorationData);

    auto& layerGeometryList = boxDecorationData->backgroundImageGeometry();

    for (int i = layers.size() - 1; i >=0; --i) {
        const auto* layer = layers[i];
        const auto& geometry = layerGeometryList[i];
        paintFillLayer(box, *layer, geometry, context);
    }
}

void CSSPainter::paintBoxDecorations(const BoxModelBox& box, GraphicsContext& context)
{
    // FIXME: Table decoration painting is special.

    auto borderBoxRect = box.absoluteBorderBoxRect();

    const auto& style = box.style();

    // Background color
    if (style.hasBackground()) {
        context.fillRect(borderBoxRect, style.backgroundColor());
        if (style.hasBackgroundImage())
            paintBackgroundImages(box, context);
    }

    // Border
    if (style.hasVisibleBorder()) {
        auto drawBorderSide = [&](auto start, auto end, const auto& borderStyle) {
            if (!borderStyle.width())
                return;
            if (borderStyle.style() == BorderStyle::None || borderStyle.style() == BorderStyle::Hidden)
                return;
            context.setStrokeColor(borderStyle.color());
            context.setStrokeThickness(borderStyle.width());
            context.drawLine(start, end);
        };

        context.setFillColor(Color::transparentBlack);

        // Top
        {
            auto borderWidth = style.borderTop().width();
            auto start = borderBoxRect.minXMinYCorner();
            auto end = FloatPoint { borderBoxRect.maxX(), start.y() + borderWidth };
            drawBorderSide(start, end, style.borderTop());
        }

        // Right
        {
            auto borderWidth = style.borderRight().width();
            auto start = FloatPoint { borderBoxRect.maxX() - borderWidth, borderBoxRect.y() };
            auto end = FloatPoint { start.x() + borderWidth, borderBoxRect.maxY() };
            drawBorderSide(start, end, style.borderRight());
        }

        // Bottom
        {
            auto borderWidth = style.borderBottom().width();
            auto start = FloatPoint { borderBoxRect.x(), borderBoxRect.maxY() - borderWidth };
            auto end = FloatPoint { borderBoxRect.maxX(), start.y() + borderWidth };
            drawBorderSide(start, end, style.borderBottom());
        }

        // Left
        {
            auto borderWidth = style.borderLeft().width();
            auto start = borderBoxRect.minXMinYCorner();
            auto end = FloatPoint { start.x() + borderWidth, borderBoxRect.maxY() };
            drawBorderSide(start, end, style.borderLeft());
        }
    }
}

void CSSPainter::paintBoxContent(const Box& box, GraphicsContext& context)
{
    if (is<ImageBox>(box)) {
        auto& imageBox = downcast<ImageBox>(box);
        
        auto* image = imageBox.image();
        auto imageRect = imageBox.replacedContentRect();

        if (image)
            context.drawImage(*image, imageRect);

        return;
    }
    
    if (is<TextBox>(box)) {
        auto& textBox = downcast<TextBox>(box);

        auto& style = box.style();
        auto textRect = box.absoluteBoxRect();

        context.setStrokeColor(style.color());
        context.setFillColor(style.color());

        // FIXME: Add non-baseline align painting
        auto baseline = textRect.y() + style.fontMetrics().ascent();
        auto expansion = textBox.expansion();

        auto textRun = TextRun { textBox.text().content().substring(textBox.text().start(), textBox.text().length()), textRect.x(), expansion.horizontalExpansion, expansion.behavior };
        textRun.setTabSize(!style.collapseWhiteSpace(), style.tabSize());
        context.drawText(style.fontCascade(), textRun, { textRect.x(), baseline });
        
        return;
    }
}

void CSSPainter::paintBox(const Box& box, GraphicsContext& context, const IntRect& dirtyRect)
{
    auto absoluteRect = box.absoluteBoxRect();
    // FIXME: Need to account for visual overflow.
    if (!dirtyRect.intersects(enclosingIntRect(absoluteRect)))
        return;

    if (is<ImageBox>(box)) {
        auto& imageBox = downcast<ImageBox>(box);
        
        auto* image = imageBox.image();
        auto imageRect = imageBox.replacedContentRect();

        if (image)
            context.drawImage(*image, imageRect);
    }

    if (is<BoxModelBox>(box))
        paintBoxDecorations(downcast<BoxModelBox>(box), context);

    paintBoxContent(box, context);
}

// FIXME: Make this an iterator.
void CSSPainter::recursivePaintDescendants(const ContainerBox& containerBox, GraphicsContext& context, PaintPhase paintPhase)
{
    for (const auto* child = containerBox.firstChild(); child; child = child->nextSibling()) {
        auto& box = *child;
        if (isStackingContextPaintingBoundary(box))
            continue;

        switch (paintPhase) {
        case PaintPhase::BlockBackgrounds:
            if (!box.style().isFloating() && !box.style().isPositioned() && is<BoxModelBox>(box))
                paintBoxDecorations(downcast<BoxModelBox>(box), context);
            break;
        case PaintPhase::Floats:
            if (box.style().isFloating() && !box.style().isPositioned() && is<BoxModelBox>(box))
                paintBoxDecorations(downcast<BoxModelBox>(box), context);
            break;
        case PaintPhase::BlockForegrounds:
            if (!box.style().isFloating() && !box.style().isPositioned())
                paintBoxContent(box, context);
        };
        if (is<ContainerBox>(box))
            recursivePaintDescendants(downcast<ContainerBox>(box), context, paintPhase);
    }
}

void CSSPainter::paintStackingContext(const BoxModelBox& contextRoot, GraphicsContext& context, const IntRect& dirtyRect)
{
    UNUSED_PARAM(dirtyRect);
    
    paintBoxDecorations(contextRoot, context);

    auto paintDescendants = [&](const ContainerBox& containerBox) {
        // For all its in-flow, non-positioned, block-level descendants in tree order: If the element is a block, list-item, or other block equivalent:
        // Box decorations.
        // Table decorations.
        recursivePaintDescendants(containerBox, context, PaintPhase::BlockBackgrounds);

        // All non-positioned floating descendants, in tree order. For each one of these, treat the element as if it created a new stacking context,
        // but any positioned descendants and descendants which actually create a new stacking context should be considered part of the parent
        // stacking context, not this new one.
        recursivePaintDescendants(containerBox, context, PaintPhase::Floats);

        // If the element is an inline element that generates a stacking context, then:
        // FIXME: Handle this case.
        
        // Otherwise: first for the element, then for all its in-flow, non-positioned, block-level descendants in tree order:
        // 1. If the element is a block-level replaced element, then: the replaced content, atomically.
        // 2. Otherwise, for each line box of that element...
        recursivePaintDescendants(containerBox, context, PaintPhase::BlockForegrounds);
    };

    if (is<ContainerBox>(contextRoot)) {
        auto& containerBox = downcast<ContainerBox>(contextRoot);

        Vector<const BoxModelBox*> negativeZOrderList;
        Vector<const BoxModelBox*> positiveZOrderList;
    
        recursiveCollectLayers(containerBox, negativeZOrderList, positiveZOrderList);

        auto compareZIndex = [] (const BoxModelBox* a, const BoxModelBox* b) {
            return a->style().zIndex().valueOr(0) < b->style().zIndex().valueOr(0);
        };

        std::stable_sort(positiveZOrderList.begin(), positiveZOrderList.end(), compareZIndex);
        std::stable_sort(negativeZOrderList.begin(), negativeZOrderList.end(), compareZIndex);

        // Stacking contexts formed by positioned descendants with negative z-indices (excluding 0) in z-index order (most negative first) then tree order.
        for (auto* box : negativeZOrderList)
            paintStackingContext(*box, context, dirtyRect);

        paintDescendants(containerBox);

        // All positioned descendants with 'z-index: auto' or 'z-index: 0', in tree order. For those with 'z-index: auto', treat the element
        // as if it created a new stacking context, but any positioned descendants and descendants which actually create a new stacking context
        // should be considered part of the parent stacking context, not this new one. For those with 'z-index: 0', treat the stacking context
        // generated atomically.
        for (auto* box : positiveZOrderList) {
            if (box->style().isStackingContext())
                paintStackingContext(*box, context, dirtyRect);
            else if (is<ContainerBox>(*box)) {
                paintBoxDecorations(*box, context);
                paintDescendants(downcast<ContainerBox>(*box));
            } else
                paintBox(*box, context, dirtyRect);
        }
    }
}

bool CSSPainter::isStackingContextPaintingBoundary(const Box& box)
{
    return box.style().isStackingContext();
}

void CSSPainter::recursiveCollectLayers(const ContainerBox& containerBox, Vector<const BoxModelBox*>& negativeZOrderList, Vector<const BoxModelBox*>& positiveZOrderList)
{
    for (const auto* child = containerBox.firstChild(); child; child = child->nextSibling()) {
        if (child->style().participatesInZOrderSorting() && is<BoxModelBox>(*child)) {
            auto& childBox = downcast<BoxModelBox>(*child);

            auto zIndex = childBox.style().zIndex().valueOr(0);

            if (zIndex < 0)
                negativeZOrderList.append(&childBox);
            else
                positiveZOrderList.append(&childBox);
        }

        if (isStackingContextPaintingBoundary(*child))
            continue;

        if (is<ContainerBox>(*child))
            recursiveCollectLayers(downcast<ContainerBox>(*child), negativeZOrderList, positiveZOrderList);
    }
}

void CSSPainter::paintTree(const Tree& displayTree, GraphicsContext& context, const IntRect& dirtyRect)
{
    paintStackingContext(displayTree.rootBox(), context, dirtyRect);
}

} // namespace Display
} // namespace WebCore

#endif // ENABLE(LAYOUT_FORMATTING_CONTEXT)
