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
#include "LayoutContainer.h"
#include "LayoutDescendantIterator.h"
#include "LayoutState.h"
#include "RenderStyle.h"
#include "TextRun.h"

namespace WebCore {
namespace Display {

static void paintBlockLevelBoxDecoration(GraphicsContext& context, const Box& absoluteDisplayBox, const RenderStyle& style)
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

static void paintInlineContent(GraphicsContext& context, const Box& absoluteDisplayBox, const RenderStyle& style, const String& content, const Layout::InlineFormattingState& formattingState)
{
    // FIXME: Only very simple text painting for now.
    auto& lineBox = formattingState.lineBoxes()[0];
    for (auto& run : formattingState.inlineRuns()) {
        if (!run.textContext())
            continue;
        context.setStrokeColor(style.color());
        context.setFillColor(style.color());
        auto logicalTopLeft = absoluteDisplayBox.topLeft() + run.logicalTopLeft();
        auto textContext = run.textContext().value();
        auto runContent = content.substring(textContext.start(), textContext.length());
        context.drawText(style.fontCascade(), TextRun { runContent }, { logicalTopLeft.x(), logicalTopLeft.y() + lineBox.baselineOffset() });
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

    for (auto& layoutBox : Layout::descendantsOfType<Layout::Box>(layoutRoot)) {
        if (layoutBox.isBlockLevelBox()) {
            paintBlockLevelBoxDecoration(context, absoluteDisplayBox(layoutBox), layoutBox.style());
            continue;
        }
        // FIXME: This only covers the most simple cases like <div>inline content</div>
        // Find a way to conect inline runs and the inline content.
        if (layoutBox.isInlineLevelBox() && layoutBox.isAnonymous()) {
            ASSERT(layoutBox.hasTextContent());
            auto& containingBlock = *layoutBox.containingBlock();
            auto& inlineFormattingState = downcast<Layout::InlineFormattingState>(layoutState.establishedFormattingState(containingBlock));
            paintInlineContent(context, absoluteDisplayBox(containingBlock), layoutBox.style(), layoutBox.textContent(), inlineFormattingState);

        }
    }
}

}
}

#endif
