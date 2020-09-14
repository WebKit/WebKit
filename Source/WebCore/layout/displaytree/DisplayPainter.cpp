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
#include "DisplayPainter.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "CachedImage.h"
#include "Color.h"
#include "DisplayBox.h"
#include "GraphicsContext.h"
#include "InlineFormattingState.h"
#include "InlineTextItem.h"
#include "IntRect.h"
#include "LayoutContainerBox.h"
#include "LayoutInitialContainingBlock.h"
#include "LayoutState.h"
#include "RenderStyle.h"
#include "TextRun.h"

namespace WebCore {
namespace Display {

static void paintBoxDecoration(GraphicsContext& context, const Box& absoluteDisplayBox, const RenderStyle& style, bool needsMarginPainting)
{
    auto decorationBoxTopLeft = needsMarginPainting ? absoluteDisplayBox.rectWithMargin().topLeft() : absoluteDisplayBox.topLeft();
    auto decorationBoxSize = needsMarginPainting ? absoluteDisplayBox.rectWithMargin().size() : LayoutSize(absoluteDisplayBox.borderBoxWidth(), absoluteDisplayBox.borderBoxHeight());
    // Background color
    if (style.hasBackground()) {
        context.fillRect({ decorationBoxTopLeft, decorationBoxSize }, style.backgroundColor());
        if (style.hasBackgroundImage()) {
            auto& backgroundLayer = style.backgroundLayers();
            if (backgroundLayer.image() && backgroundLayer.image()->cachedImage() && backgroundLayer.image()->cachedImage()->image())
                context.drawImage(*backgroundLayer.image()->cachedImage()->image(), { decorationBoxTopLeft, backgroundLayer.image()->cachedImage()->image()->size() });
        }
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

        auto decorationBoxWidth = decorationBoxSize.width();
        auto decorationBoxHeight = decorationBoxSize.height();

        // Top
        {
            auto borderWidth = style.borderTop().width();
            auto start = LayoutPoint { decorationBoxTopLeft };
            auto end = LayoutPoint { start.x() + decorationBoxWidth, start.y() + borderWidth };
            drawBorderSide(start, end, style.borderTop());
        }

        // Right
        {
            auto borderWidth = style.borderRight().width();
            auto start = LayoutPoint { decorationBoxTopLeft.x() + decorationBoxWidth - borderWidth, decorationBoxTopLeft.y() };
            auto end = LayoutPoint { start.x() + borderWidth, decorationBoxTopLeft.y() + decorationBoxHeight };
            drawBorderSide(start, end, style.borderRight());
        }

        // Bottom
        {
            auto borderWidth = style.borderBottom().width();
            auto start = LayoutPoint { decorationBoxTopLeft.x(), decorationBoxTopLeft.y() + decorationBoxHeight - borderWidth };
            auto end = LayoutPoint { start.x() + decorationBoxWidth, start.y() + borderWidth };
            drawBorderSide(start, end, style.borderBottom());
        }

        // Left
        {
            auto borderWidth = style.borderLeft().width();
            auto start = decorationBoxTopLeft;
            auto end = LayoutPoint { start.x() + borderWidth, decorationBoxTopLeft.y() + decorationBoxHeight };
            drawBorderSide(start, end, style.borderLeft());
        }
    }
}

static void paintInlineContent(GraphicsContext& context, LayoutPoint absoluteOffset, const Layout::InlineFormattingState& formattingState)
{
    auto* displayInlineContent = formattingState.displayInlineContent();
    if (!displayInlineContent)
        return;

    auto& displayRuns = displayInlineContent->runs;
    if (displayRuns.isEmpty())
        return;

    for (auto& run : displayRuns) {
        if (auto& textContent = run.textContent()) {
            auto& style = run.style();
            context.setStrokeColor(style.color());
            context.setFillColor(style.color());

            auto absoluteLeft = absoluteOffset.x() + run.left();
            // FIXME: Add non-baseline align painting
            auto& line = displayInlineContent->lineForRun(run);
            auto baseline = absoluteOffset.y() + run.top() + style.fontMetrics().ascent();
            auto expansion = run.expansion();
            auto textRun = TextRun { textContent->content(), run.left() - line.left(), expansion.horizontalExpansion, expansion.behavior };
            textRun.setTabSize(!style.collapseWhiteSpace(), style.tabSize());
            context.drawText(style.fontCascade(), textRun, { absoluteLeft, baseline });
        } else if (auto* cachedImage = run.image()) {
            auto runAbsoluteRect = FloatRect { absoluteOffset.x() + run.left(), absoluteOffset.y() + run.top(), run.width(), run.height() };
            context.drawImage(*cachedImage->image(), runAbsoluteRect);
        }
    }
}

Box Painter::absoluteDisplayBox(const Layout::LayoutState& layoutState, const Layout::Box& layoutBoxToPaint)
{
    // Should never really happen but table code is way too incomplete.
    if (!layoutState.hasDisplayBox(layoutBoxToPaint))
        return { };
    if (is<Layout::InitialContainingBlock>(layoutBoxToPaint))
        return layoutState.displayBoxForLayoutBox(layoutBoxToPaint);

    auto paintContainer = [&] (const auto& layoutBox) {
        if (layoutBox.isTableCell()) {
            // The table cell's containing block is the table box (skipping both the row and the section), but it's positioned relative to the table section.
            // Let's skip the row and go right to the section box.
            return &layoutBox.parent().parent();
        }
        return &layoutBox.containingBlock();
    };
    auto absoluteBox = Box { layoutState.displayBoxForLayoutBox(layoutBoxToPaint) };
    for (auto* container = paintContainer(layoutBoxToPaint); !is<Layout::InitialContainingBlock>(container); container = paintContainer(*container))
        absoluteBox.moveBy(layoutState.displayBoxForLayoutBox(*container).topLeft());
    return absoluteBox;
}

static bool isPaintRootCandidate(const Layout::Box& layoutBox)
{
    return layoutBox.isPositioned();
}

using LayoutBoxList = Vector<const Layout::Box*>;

enum PaintPhase { Decoration, Content };
static void paintSubtree(GraphicsContext& context, const Layout::LayoutState& layoutState, const Layout::Box& paintRootBox, const IntRect& dirtyRect, PaintPhase paintPhase)
{
    auto paint = [&] (auto& layoutBox) {
        if (layoutBox.style().visibility() != Visibility::Visible)
            return;
        if (!layoutState.hasDisplayBox(layoutBox))
            return;
        auto absoluteDisplayBox = Painter::absoluteDisplayBox(layoutState, layoutBox);
        if (!dirtyRect.intersects(snappedIntRect(absoluteDisplayBox.rect())))
            return;

        if (paintPhase == PaintPhase::Decoration) {
            if (layoutBox.isAnonymous())
                return;
            paintBoxDecoration(context, absoluteDisplayBox, layoutBox.style(), layoutBox.isBodyBox());
            return;
        }
        // Only inline content for now.
        if (layoutBox.establishesInlineFormattingContext()) {
            auto& containerBox = downcast<Layout::ContainerBox>(layoutBox);
            paintInlineContent(context, absoluteDisplayBox.topLeft(), layoutState.establishedInlineFormattingState(containerBox));
        }
    };

    paint(paintRootBox);
    if (!is<Layout::ContainerBox>(paintRootBox) || !downcast<Layout::ContainerBox>(paintRootBox).hasChild())
        return;

    LayoutBoxList layoutBoxList;
    layoutBoxList.append(downcast<Layout::ContainerBox>(paintRootBox).firstChild());
    while (!layoutBoxList.isEmpty()) {
        while (true) {
            auto& layoutBox = *layoutBoxList.last();
            if (isPaintRootCandidate(layoutBox))
                break;
            paint(layoutBox);
            if (!is<Layout::ContainerBox>(layoutBox) || !downcast<Layout::ContainerBox>(layoutBox).hasChild())
                break;
            layoutBoxList.append(downcast<Layout::ContainerBox>(layoutBox).firstChild());
        }
        while (!layoutBoxList.isEmpty()) {
            auto& layoutBox = *layoutBoxList.takeLast();
            // Stay within.
            if (&layoutBox == &paintRootBox)
                return;
            if (auto* nextSibling = layoutBox.nextSibling()) {
                layoutBoxList.append(nextSibling);
                break;
            }
        }
    }
}

static LayoutRect collectPaintRootsAndContentRect(const Layout::LayoutState& layoutState, const Layout::Box& rootLayoutBox, LayoutBoxList& positiveZOrderList, LayoutBoxList& negativeZOrderList)
{
    auto appendPaintRoot = [&] (const auto& layoutBox) {
        if (layoutBox.style().usedZIndex() < 0) {
            negativeZOrderList.append(&layoutBox);
            return;
        }
        positiveZOrderList.append(&layoutBox);
    };

    auto contentRect = LayoutRect { layoutState.displayBoxForLayoutBox(rootLayoutBox).rect() };

    // Initial BFC is always a paint root.
    appendPaintRoot(rootLayoutBox);
    LayoutBoxList layoutBoxList;
    layoutBoxList.append(&rootLayoutBox);
    while (!layoutBoxList.isEmpty()) {
        while (true) {
            auto& layoutBox = *layoutBoxList.last();
            if (layoutBox.style().visibility() != Visibility::Visible)
                break;
            if (isPaintRootCandidate(layoutBox))
                appendPaintRoot(layoutBox);
            if (layoutState.hasDisplayBox(layoutBox))
                contentRect.uniteIfNonZero(Painter::absoluteDisplayBox(layoutState, layoutBox).rect());
            if (!is<Layout::ContainerBox>(layoutBox) || !downcast<Layout::ContainerBox>(layoutBox).hasChild())
                break;
            layoutBoxList.append(downcast<Layout::ContainerBox>(layoutBox).firstChild());
        }
        while (!layoutBoxList.isEmpty()) {
            auto& layoutBox = *layoutBoxList.takeLast();
            if (auto* nextSibling = layoutBox.nextSibling()) {
                layoutBoxList.append(nextSibling);
                break;
            }
        }
    }

    auto compareZIndex = [] (const Layout::Box* a, const Layout::Box* b) {
        return a->style().usedZIndex() < b->style().usedZIndex();
    };

    std::stable_sort(positiveZOrderList.begin(), positiveZOrderList.end(), compareZIndex);
    std::stable_sort(negativeZOrderList.begin(), negativeZOrderList.end(), compareZIndex);
    return contentRect;
}

void Painter::paint(const Layout::LayoutState& layoutState, GraphicsContext& context, const IntRect& dirtyRect)
{
    auto& rootLayoutBox = layoutState.root();
    if (!rootLayoutBox.firstChild())
        return;

    Vector<const Layout::Box*> negativeZOrderList;
    Vector<const Layout::Box*> positiveZOrderList;
    auto contentRect = collectPaintRootsAndContentRect(layoutState, rootLayoutBox, positiveZOrderList, negativeZOrderList);

    // Fill the entire content area.
    context.fillRect(contentRect, Color::white);

    for (auto& paintRootBox : negativeZOrderList) {
        paintSubtree(context, layoutState, *paintRootBox, dirtyRect, PaintPhase::Decoration);
        paintSubtree(context, layoutState, *paintRootBox, dirtyRect, PaintPhase::Content);
    }

    for (auto& paintRootBox : positiveZOrderList) {
        paintSubtree(context, layoutState, *paintRootBox, dirtyRect, PaintPhase::Decoration);
        paintSubtree(context, layoutState, *paintRootBox, dirtyRect, PaintPhase::Content);
    }
}

void Painter::paintInlineFlow(const Layout::LayoutState& layoutState, GraphicsContext& context)
{
    auto& layoutRoot = layoutState.root();

    ASSERT(layoutRoot.establishesInlineFormattingContext());

    paintInlineContent(context, { }, layoutState.establishedInlineFormattingState(layoutRoot));
}

}
}

#endif
