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
#include "Frame.h"
#include "GraphicsContext.h"
#include "HitTestLocation.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "InlineTextBox.h"
#include "PaintInfo.h"
#include "RenderBlockFlow.h"
#include "RenderStyle.h"
#include "RenderText.h"
#include "RenderView.h"
#include "Settings.h"
#include "SimpleLineLayoutResolver.h"
#include "Text.h"
#include "TextPaintStyle.h"

namespace WebCore {
namespace SimpleLineLayout {

static void paintDebugBorders(GraphicsContext& context, const LayoutRect& borderRect, const LayoutPoint& paintOffset)
{
    if (borderRect.isEmpty())
        return;
    GraphicsContextStateSaver stateSaver(context);
    context.setStrokeColor(Color(0, 255, 0), ColorSpaceDeviceRGB);
    context.setFillColor(Color::transparent, ColorSpaceDeviceRGB);
    LayoutRect rect(borderRect);
    rect.moveBy(paintOffset);
    context.drawRect(pixelSnappedIntRect(rect));
}

void paintFlow(const RenderBlockFlow& flow, const Layout& layout, PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (paintInfo.phase != PaintPhaseForeground)
        return;

    RenderStyle& style = flow.style();
    if (style.visibility() != VISIBLE)
        return;

    RenderText& textRenderer = toRenderText(*flow.firstChild());
    ASSERT(!textRenderer.firstTextBox());

    bool debugBordersEnabled = flow.frame().settings().simpleLineLayoutDebugBordersEnabled();

    GraphicsContext& context = *paintInfo.context;

    const Font& font = style.font();
    TextPaintStyle textPaintStyle = computeTextPaintStyle(textRenderer, style, paintInfo);
    GraphicsContextStateSaver stateSaver(context, textPaintStyle.strokeWidth > 0);

    updateGraphicsContext(context, textPaintStyle);
    LayoutPoint adjustedPaintOffset = LayoutPoint(roundedForPainting(paintOffset, flow.document().deviceScaleFactor()));

    LayoutRect paintRect = paintInfo.rect;
    paintRect.moveBy(-adjustedPaintOffset);

    auto resolver = runResolver(flow, layout);
    auto range = resolver.rangeForRect(paintRect);
    for (auto it = range.begin(), end = range.end(); it != end; ++it) {
        const auto& run = *it;
        if (!run.rect().intersects(paintRect))
            continue;
        TextRun textRun(run.text());
        textRun.setTabSize(!style.collapseWhiteSpace(), style.tabSize());
        context.drawText(font, textRun, run.baseline() + adjustedPaintOffset);
        if (debugBordersEnabled)
            paintDebugBorders(context, run.rect(), adjustedPaintOffset);
    }
}

bool hitTestFlow(const RenderBlockFlow& flow, const Layout& layout, const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction hitTestAction)
{
    if (hitTestAction != HitTestForeground)
        return false;

    if (!layout.runCount())
        return false;

    RenderStyle& style = flow.style();
    if (style.visibility() != VISIBLE || style.pointerEvents() == PE_NONE)
        return false;

    RenderText& textRenderer = toRenderText(*flow.firstChild());

    LayoutRect rangeRect = locationInContainer.boundingBox();
    rangeRect.moveBy(-accumulatedOffset);

    auto resolver = lineResolver(flow, layout);
    auto range = resolver.rangeForRect(rangeRect);
    for (auto it = range.begin(), end = range.end(); it != end; ++it) {
        auto lineRect = *it;
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
        auto rect = *it;
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
    auto firstLineRect = *it;
    float left = firstLineRect.x();
    float right = firstLineRect.maxX();
    float bottom = firstLineRect.maxY();
    for (++it; it != end; ++it) {
        auto rect = *it;
        if (rect.x() < left)
            left = rect.x();
        if (rect.maxX() > right)
            right = rect.maxX();
        if (rect.maxY() > bottom)
            bottom = rect.maxY();
    }
    float x = left;
    float y = firstLineRect.y();
    float width = right - left;
    float height = bottom - y;
    return enclosingIntRect(FloatRect(x, y, width, height));
}

IntPoint computeTextFirstRunLocation(const RenderText& textRenderer, const Layout& layout)
{
    auto resolver = runResolver(toRenderBlockFlow(*textRenderer.parent()), layout);
    auto begin = resolver.begin();
    if (begin == resolver.end())
        return IntPoint();
    return flooredIntPoint((*begin).rect().location());
}

Vector<IntRect> collectTextAbsoluteRects(const RenderText& textRenderer, const Layout& layout, const LayoutPoint& accumulatedOffset)
{
    Vector<IntRect> rects;
    auto resolver = runResolver(toRenderBlockFlow(*textRenderer.parent()), layout);
    for (auto it = resolver.begin(), end = resolver.end(); it != end; ++it) {
        const auto& run = *it;
        auto rect = run.rect();
        rects.append(enclosingIntRect(FloatRect(accumulatedOffset + rect.location(), rect.size())));
    }
    return rects;
}

Vector<FloatQuad> collectTextAbsoluteQuads(const RenderText& textRenderer, const Layout& layout, bool* wasFixed)
{
    Vector<FloatQuad> quads;
    auto resolver = runResolver(toRenderBlockFlow(*textRenderer.parent()), layout);
    for (auto it = resolver.begin(), end = resolver.end(); it != end; ++it) {
        const auto& run = *it;
        auto rect = run.rect();
        quads.append(textRenderer.localToAbsoluteQuad(FloatQuad(rect), 0, wasFixed));
    }
    return quads;
}

}
}
