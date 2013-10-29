/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "SimpleLineLayoutFunctions.h"

#include "FontCache.h"
#include "GraphicsContext.h"
#include "HitTestLocation.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "InlineTextBox.h"
#include "LineWidth.h"
#include "PaintInfo.h"
#include "RenderBlockFlow.h"
#include "RenderStyle.h"
#include "RenderText.h"
#include "SimpleLineLayoutResolver.h"
#include "Text.h"
#include "TextPaintStyle.h"
#include <wtf/unicode/Unicode.h>

namespace WebCore {
namespace SimpleLineLayout {

void paintFlow(const RenderBlockFlow& flow, const Layout& layout, PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (paintInfo.phase != PaintPhaseForeground)
        return;
    RenderText& textRenderer = toRenderText(*flow.firstChild());
    ASSERT(!textRenderer.firstTextBox());

    RenderStyle& style = flow.style();
    const Font& font = style.font();

    GraphicsContext& context = *paintInfo.context;

    TextPaintStyle textPaintStyle = computeTextPaintStyle(textRenderer, style, paintInfo);
    GraphicsContextStateSaver stateSaver(context, textPaintStyle.strokeWidth > 0);

    updateGraphicsContext(context, textPaintStyle);

    auto resolver = runResolver(flow, layout);
    for (auto it = resolver.begin(), end = resolver.end(); it != end; ++it) {
        auto run = *it;
        context.drawText(font, TextRun(run.text()), run.baseline() + paintOffset);
    }
}

bool hitTestFlow(const RenderBlockFlow& flow, const Layout& layout, const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction hitTestAction)
{
    if (hitTestAction != HitTestForeground)
        return false;

    if (layout.runs.isEmpty())
        return false;

    RenderStyle& style = flow.style();
    if (style.visibility() != VISIBLE || style.pointerEvents() == PE_NONE)
        return false;

    RenderText& textRenderer = toRenderText(*flow.firstChild());

    auto resolver = lineResolver(flow, layout);
    for (auto it = resolver.begin(), end = resolver.end(); it != end; ++it) {
        auto line = *it;
        auto lineRect = line.rect();
        lineRect.moveBy(accumulatedOffset);
        if (!locationInContainer.intersects(lineRect))
            continue;
        textRenderer.updateHitTestResult(result, locationInContainer.point() - toLayoutSize(accumulatedOffset));
        if (!result.addNodeToRectBasedTestResult(textRenderer.textNode(), request, locationInContainer, lineRect))
            return true;
    }

    return false;
}

void collectFlowOverflow(RenderBlockFlow& flow, const Layout& layout)
{
    auto resolver = lineResolver(flow, layout);
    for (auto it = resolver.begin(), end = resolver.end(); it != end; ++it) {
        auto line = *it;
        auto rect = line.rect();
        flow.addLayoutOverflow(rect);
        flow.addVisualOverflow(rect);
    }
}

IntRect computeTextBoundingBox(const RenderText& textRenderer, const Layout& layout)
{
    auto resolver = lineResolver(toRenderBlockFlow(*textRenderer.parent()), layout);
    auto it = resolver.begin();
    auto end = resolver.end();
    if (it == end)
        return IntRect();
    auto firstLineRect = (*it).rect();
    float left = firstLineRect.x();
    float right = firstLineRect.maxX();
    for (++it; it != end; ++it) {
        auto line = *it;
        auto rect = line.rect();
        if (rect.x() < left)
            left = rect.x();
        if (rect.maxX() > right)
            right = rect.maxX();
    }
    float x = firstLineRect.x();
    float y = firstLineRect.y();
    float width = right - left;
    float height = (*resolver.last()).rect().maxY() - y;
    return enclosingIntRect(FloatRect(x, y, width, height));
}

}
}
