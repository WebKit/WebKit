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

static void paintBoxDecoration(GraphicsContext& context, const Box& absoluteDisplayBox, const RenderStyle& style)
{
    auto borderBoxAbsoluteTopLeft = absoluteDisplayBox.topLeft();
    // Background color
    if (style.hasBackground())
        context.fillRect({ borderBoxAbsoluteTopLeft, FloatSize { absoluteDisplayBox.borderBoxWidth(), absoluteDisplayBox.borderBoxHeight() } }, style.backgroundColor());

    // Border
    if (style.hasBorder()) {

        auto drawBorderSide = [&](auto start, auto end, const auto& borderStyle) {
            context.setStrokeColor(borderStyle.color());
            context.setStrokeThickness(borderStyle.width());
            context.drawLine(start, end);
        };

        context.setFillColor(Color::transparent);

        auto borderBoxWidth = absoluteDisplayBox.borderBoxWidth();
        auto borderBoxHeight = absoluteDisplayBox.borderBoxHeight();

        // Top
        {
            auto borderWidth = style.borderTop().width();
            auto start = LayoutPoint { borderBoxAbsoluteTopLeft };
            auto end = LayoutPoint { start.x() + borderBoxWidth, start.y() + borderWidth };
            drawBorderSide(start, end, style.borderTop());
        }

        // Right
        {
            auto borderWidth = style.borderRight().width();
            auto start = LayoutPoint { borderBoxAbsoluteTopLeft.x() + borderBoxWidth - borderWidth, borderBoxAbsoluteTopLeft.y() };
            auto end = LayoutPoint { start.x() + borderWidth, borderBoxAbsoluteTopLeft.y() + borderBoxHeight };
            drawBorderSide(start, end, style.borderRight());
        }

        // Bottom
        {
            auto borderWidth = style.borderBottom().width();
            auto start = LayoutPoint { borderBoxAbsoluteTopLeft.x(), borderBoxAbsoluteTopLeft.y() + borderBoxHeight - borderWidth };
            auto end = LayoutPoint { start.x() + borderBoxWidth, start.y() + borderWidth };
            drawBorderSide(start, end, style.borderBottom());
        }

        // Left
        {
            auto borderWidth = style.borderLeft().width();
            auto start = borderBoxAbsoluteTopLeft;
            auto end = LayoutPoint { start.x() + borderWidth, borderBoxAbsoluteTopLeft.y() + borderBoxHeight };
            drawBorderSide(start, end, style.borderLeft());
        }
    }
}

static void paintInlineContent(GraphicsContext& context, const Box& rootAbsoluteDisplayBox, const Layout::InlineFormattingState& formattingState)
{
    auto& inlineRuns = formattingState.inlineRuns();
    if (inlineRuns.isEmpty())
        return;
    // FIXME: We should be able to paint runs independently from inline items.
    unsigned runIndex = 0;
    for (auto& inlineItem : formattingState.inlineItems()) {
        auto& style = inlineItem->style();
        context.setStrokeColor(style.color());
        context.setFillColor(style.color());

        if (inlineItem->isText()) {
            auto& inlineTextItem = downcast<Layout::InlineTextItem>(*inlineItem);
            auto inlineContent = inlineTextItem.layoutBox().textContent();
            while (true) {
                auto& run = inlineRuns[runIndex++];
                auto textContext = run.textContext().value();
                auto runContent = inlineContent.substring(textContext.start(), textContext.length());
                auto logicalTopLeft = rootAbsoluteDisplayBox.topLeft() + run.logicalTopLeft();
                context.drawText(style.fontCascade(), TextRun { runContent }, { logicalTopLeft.x(), logicalTopLeft.y() + formattingState.lineBoxes()[0].baselineOffset() });
                if (inlineTextItem.end() == textContext.end())
                    break;
                if (runIndex == inlineRuns.size())
                    return;
            }
            continue;
        }

        if (inlineItem->isBox()) {
            auto& run = inlineRuns[runIndex++];
            auto logicalTopLeft = rootAbsoluteDisplayBox.topLeft() + run.logicalTopLeft();
            context.fillRect({ logicalTopLeft, FloatSize { run.logicalWidth(), run.logicalHeight() } });
            continue;
        }
    }
}

void Painter::paint(const Layout::LayoutState& layoutState, GraphicsContext& context)
{
    auto absoluteDisplayBox = [&](const auto& layoutBox) {
        auto absoluteBox = Box { layoutState.displayBoxForLayoutBox(layoutBox) };
        for (auto* container = layoutBox.containingBlock(); container != &layoutBox.initialContainingBlock(); container = container->containingBlock())
            absoluteBox.moveBy(layoutState.displayBoxForLayoutBox(*container).topLeft());
        return absoluteBox;
    };

    auto& layoutRoot = layoutState.root();
    auto& rootDisplayBox = layoutState.displayBoxForLayoutBox(layoutRoot);
    context.fillRect({ FloatPoint { }, FloatSize { rootDisplayBox.borderBoxWidth(), rootDisplayBox.borderBoxHeight() } }, Color::white);

    // 1. Paint box decoration (both block and inline).
    for (auto& layoutBox : Layout::descendantsOfType<Layout::Box>(layoutRoot)) {
        if (layoutBox.isAnonymous())
            continue;
        paintBoxDecoration(context, absoluteDisplayBox(layoutBox), layoutBox.style());
    }

    // 2. Paint content
    for (auto& layoutBox : Layout::descendantsOfType<Layout::Box>(layoutRoot)) {
        if (layoutBox.establishesInlineFormattingContext()) {
            auto& container = downcast<Layout::Container>(layoutBox);
            paintInlineContent(context, absoluteDisplayBox(container), downcast<Layout::InlineFormattingState>(layoutState.establishedFormattingState(container)));
            continue;
        }
    }
}

}
}

#endif
