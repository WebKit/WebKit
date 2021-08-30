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
#include "DisplayBoxDecorationPainter.h"
#include "DisplayContainerBox.h"
#include "DisplayFillLayerImageGeometry.h"
#include "DisplayImageBox.h"
#include "DisplayPaintingContext.h"
#include "DisplayStyle.h"
#include "DisplayTextBox.h"
#include "DisplayTree.h"
#include "GraphicsContext.h"
#include "IntRect.h"
#include "LayoutPoint.h"
#include "TextRun.h"

namespace WebCore {
namespace Display {

void BoxPainter::paintBoxDecorations(const BoxModelBox& box, PaintingContext& paintingContext)
{
    BoxDecorationPainter painter(box, paintingContext);
    painter.paintBackgroundAndBorders(paintingContext);
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

        auto textRun = TextRun { textBox.text().originalContent().substring(textBox.text().start(), textBox.text().length()), textRect.x(), expansion.horizontalExpansion, expansion.behavior };
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

    if (is<BoxModelBox>(box))
        paintBoxDecorations(downcast<BoxModelBox>(box), paintingContext);

    paintBoxContent(box, paintingContext);
}

} // namespace Display
} // namespace WebCore

#endif // ENABLE(LAYOUT_FORMATTING_CONTEXT)
