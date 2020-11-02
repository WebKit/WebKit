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
#include "DisplayBoxPainter.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "CachedImage.h"
#include "Color.h"
#include "DisplayBoxDecorationData.h"
#include "DisplayContainerBox.h"
#include "DisplayFillLayerImageGeometry.h"
#include "DisplayImageBox.h"
#include "DisplayPaintingContext.h"
#include "DisplayStyle.h"
#include "DisplayTextBox.h"
#include "DisplayTree.h"
#include "FillLayer.h"
#include "GraphicsContext.h"
#include "IntRect.h"
#include "LayoutPoint.h"
#include "TextRun.h"

namespace WebCore {
namespace Display {

void BoxPainter::paintFillLayer(const BoxModelBox& box, const FillLayer& layer, const FillLayerImageGeometry& geometry, PaintingContext& paintingContext)
{
    GraphicsContextStateSaver stateSaver(paintingContext.context, false);

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
        paintingContext.context.clip(clipRectForLayer(box, layer));
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

    paintingContext.context.drawTiledImage(*image, geometry.destRect(), toFloatPoint(geometry.relativePhase()), geometry.tileSize(), geometry.spaceSize(), options);
}

void BoxPainter::paintBackgroundImages(const BoxModelBox& box, PaintingContext& paintingContext)
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
        paintFillLayer(box, *layer, geometry, paintingContext);
    }
}

void BoxPainter::paintBoxDecorations(const BoxModelBox& box, PaintingContext& paintingContext)
{
    // FIXME: Table decoration painting is special.

    auto borderBoxRect = box.absoluteBorderBoxRect();

    const auto& style = box.style();

    // Background color
    if (style.hasBackground()) {
        paintingContext.context.fillRect(borderBoxRect, style.backgroundColor());
        if (style.hasBackgroundImage())
            paintBackgroundImages(box, paintingContext);
    }

    // Border
    if (style.hasVisibleBorder()) {
        auto drawBorderSide = [&](auto start, auto end, const auto& borderStyle) {
            if (!borderStyle.width())
                return;
            if (borderStyle.style() == BorderStyle::None || borderStyle.style() == BorderStyle::Hidden)
                return;
            paintingContext.context.setStrokeColor(borderStyle.color());
            paintingContext.context.setStrokeThickness(borderStyle.width());
            paintingContext.context.drawLine(start, end);
        };

        paintingContext.context.setFillColor(Color::transparentBlack);

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

void BoxPainter::paintBoxContent(const Box& box, PaintingContext& paintingContext)
{
    if (is<ImageBox>(box)) {
        auto& imageBox = downcast<ImageBox>(box);
        
        auto* image = imageBox.image();
        auto imageRect = imageBox.replacedContentRect();

        if (image)
            paintingContext.context.drawImage(*image, imageRect);

        return;
    }
    
    if (is<TextBox>(box)) {
        auto& textBox = downcast<TextBox>(box);

        auto& style = box.style();
        auto textRect = box.absoluteBoxRect();

        paintingContext.context.setStrokeColor(style.color());
        paintingContext.context.setFillColor(style.color());

        // FIXME: Add non-baseline align painting
        auto baseline = textRect.y() + style.fontMetrics().ascent();
        auto expansion = textBox.expansion();

        auto textRun = TextRun { textBox.text().content().substring(textBox.text().start(), textBox.text().length()), textRect.x(), expansion.horizontalExpansion, expansion.behavior };
        textRun.setTabSize(!style.collapseWhiteSpace(), style.tabSize());
        paintingContext.context.drawText(style.fontCascade(), textRun, { textRect.x(), baseline });
        return;
    }
}

void BoxPainter::paintBox(const Box& box, PaintingContext& paintingContext, const IntRect& dirtyRect)
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
            paintingContext.context.drawImage(*image, imageRect);
    }

    if (is<BoxModelBox>(box))
        paintBoxDecorations(downcast<BoxModelBox>(box), paintingContext);

    paintBoxContent(box, paintingContext);
}

} // namespace Display
} // namespace WebCore

#endif // ENABLE(LAYOUT_FORMATTING_CONTEXT)
