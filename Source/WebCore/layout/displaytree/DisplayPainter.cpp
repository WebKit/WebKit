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
#include "LayoutContainer.h"
#include "LayoutDescendantIterator.h"
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

        context.setFillColor(Color::transparent);

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

static void paintInlineContent(GraphicsContext& context, const Box& rootAbsoluteDisplayBox, const Layout::InlineFormattingState& formattingState)
{
    auto& inlineRuns = formattingState.inlineRuns();
    if (inlineRuns.isEmpty())
        return;

    for (auto& run : inlineRuns) {
        if (run->textContext()) {
            auto& style = run->style();
            context.setStrokeColor(style.color());
            context.setFillColor(style.color());

            auto logicalLeft = rootAbsoluteDisplayBox.left() + run->logicalLeft();
            // FIXME: Add non-baseline align painting
            auto& lineBox = formattingState.lineBoxForRun(*run);
            auto baselineOffset = rootAbsoluteDisplayBox.top() + lineBox.logicalTop() + lineBox.baselineOffset(); 
            context.drawText(style.fontCascade(), TextRun { run->textContext()->content() }, { logicalLeft, baselineOffset });
        } else if (auto* cachedImage = run->image()) {
            auto runAbsoluteRect = FloatRect { rootAbsoluteDisplayBox.left() + run->logicalLeft(), rootAbsoluteDisplayBox.top() + run->logicalTop(), run->logicalWidth(), run->logicalHeight() };
            context.drawImage(*cachedImage->image(), runAbsoluteRect);
        }
    }
}

static Box absoluteDisplayBox(const Layout::LayoutState& layoutState, const Layout::Box& layoutBox)
{
    // Should never really happen but table code is way too incomplete.
    if (!layoutState.hasDisplayBox(layoutBox))
        return { };

    auto absoluteBox = Box { layoutState.displayBoxForLayoutBox(layoutBox) };
    for (auto* container = layoutBox.containingBlock(); container != &layoutBox.initialContainingBlock(); container = container->containingBlock())
        absoluteBox.moveBy(layoutState.displayBoxForLayoutBox(*container).topLeft());
    return absoluteBox;
}

static void paintBoxDecorationAndChildren(GraphicsContext& context, const Layout::LayoutState& layoutState, const Layout::Box& layoutBox)
{
    if (!layoutBox.isAnonymous())
        paintBoxDecoration(context, absoluteDisplayBox(layoutState, layoutBox), layoutBox.style(), layoutBox.isBodyBox());

    if (!is<Layout::Container>(layoutBox))
        return;
    for (auto& childLayoutBox : Layout::childrenOfType<Layout::Box>(downcast<Layout::Container>(layoutBox))) {
        if (childLayoutBox.style().visibility() != Visibility::Visible)
            continue;
        paintBoxDecorationAndChildren(context, layoutState, childLayoutBox);
    }
}

void Painter::paint(const Layout::LayoutState& layoutState, GraphicsContext& context)
{
    auto& layoutRoot = layoutState.root();
    auto& rootDisplayBox = layoutState.displayBoxForLayoutBox(layoutRoot);
    context.fillRect({ FloatPoint { }, FloatSize { rootDisplayBox.borderBoxWidth(), rootDisplayBox.borderBoxHeight() } }, Color::white);
    if (!layoutRoot.firstChild())
        return;

    // 1. Paint box decoration (both block and inline).
    paintBoxDecorationAndChildren(context, layoutState, *layoutRoot.firstChild());

    // 2. Paint content
    for (auto& layoutBox : Layout::descendantsOfType<Layout::Box>(layoutRoot)) {
        if (layoutBox.style().visibility() != Visibility::Visible)
            continue;
        if (layoutBox.establishesInlineFormattingContext()) {
            auto& container = downcast<Layout::Container>(layoutBox);
            paintInlineContent(context, absoluteDisplayBox(layoutState, container), downcast<Layout::InlineFormattingState>(layoutState.establishedFormattingState(container)));
            continue;
        }
    }
}

}
}

#endif
