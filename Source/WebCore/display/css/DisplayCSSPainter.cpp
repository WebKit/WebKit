/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "DisplayContainerBox.h"
#include "DisplayImageBox.h"
#include "DisplayStyle.h"
#include "DisplayTextBox.h"
#include "DisplayTree.h"
#include "GraphicsContext.h"
#include "IntRect.h"
#include "LayoutState.h"
#include "TextRun.h"

namespace WebCore {
namespace Display {

void CSSPainter::paintBoxDecorations(const Box& displayBox, GraphicsContext& context)
{
    // FIXME: Table decoration painting is special.

    auto borderBoxRect = displayBox.borderBoxFrame();
    
    const auto& style = displayBox.style();

    // Background color
    if (style.hasBackground()) {
        context.fillRect(borderBoxRect, style.backgroundColor());
        // FIXME: Paint background image.
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
        if (!textBox.text())
            return;

        auto& style = box.style();
        auto textRect = box.borderBoxFrame();

        context.setStrokeColor(style.color());
        context.setFillColor(style.color());

        // FIXME: Add non-baseline align painting
        auto baseline = textRect.y() + style.fontMetrics().ascent();
        auto expansion = textBox.expansion();

        auto textRun = TextRun { textBox.text()->content().substring(textBox.text()->start(), textBox.text()->length()), textRect.x(), expansion.horizontalExpansion, expansion.behavior };
        textRun.setTabSize(!style.collapseWhiteSpace(), style.tabSize());
        context.drawText(style.fontCascade(), textRun, { textRect.x(), baseline });
        
        return;
    }
}

void CSSPainter::paintBox(const Box& box, GraphicsContext& context, const IntRect& dirtyRect)
{
    auto relativeRect = box.borderBoxFrame();
    if (!dirtyRect.intersects(enclosingIntRect(relativeRect)))
        return;

    if (is<ImageBox>(box)) {
        auto& imageBox = downcast<ImageBox>(box);
        
        auto* image = imageBox.image();
        auto imageRect = imageBox.replacedContentRect();

        if (image)
            context.drawImage(*image, imageRect);
    }

    paintBoxDecorations(box, context);
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
            if (!box.style().isFloating() && !box.style().isPositioned())
                paintBoxDecorations(box, context);
            break;
        case PaintPhase::Floats:
            if (box.style().isFloating() && !box.style().isPositioned())
                paintBoxDecorations(box, context);
            break;
        case PaintPhase::BlockForegrounds:
            if (!box.style().isFloating() && !box.style().isPositioned())
                paintBoxContent(box, context);
        };
        if (is<ContainerBox>(box))
            recursivePaintDescendants(downcast<ContainerBox>(box), context, paintPhase);
    }
}

void CSSPainter::paintStackingContext(const Box& contextRoot, GraphicsContext& context, const IntRect& dirtyRect)
{
    UNUSED_PARAM(dirtyRect);
    
    // Here the paintOffset represents the offset of the top left of contextRoot's borderBoxFrame relative to the root.
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

        Vector<const Box*> negativeZOrderList;
        Vector<const Box*> positiveZOrderList;
    
        recursiveCollectLayers(containerBox, negativeZOrderList, positiveZOrderList);

        auto compareZIndex = [] (const Box* a, const Box* b) {
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

void CSSPainter::recursiveCollectLayers(const ContainerBox& containerBox, Vector<const Box*>& negativeZOrderList, Vector<const Box*>& positiveZOrderList)
{
    for (const auto* child = containerBox.firstChild(); child; child = child->nextSibling()) {
        if (child->style().participatesInZOrderSorting()) {
            auto zIndex = child->style().zIndex().valueOr(0);

            if (zIndex < 0)
                negativeZOrderList.append(child);
            else
                positiveZOrderList.append(child);
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
