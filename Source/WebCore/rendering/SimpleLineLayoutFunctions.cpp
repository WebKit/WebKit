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

static void paintDebugBorders(GraphicsContext& context, LayoutRect borderRect, const LayoutPoint& paintOffset)
{
    borderRect.moveBy(paintOffset);
    IntRect snappedRect = snappedIntRect(borderRect);
    if (snappedRect.isEmpty())
        return;
    GraphicsContextStateSaver stateSaver(context);
    context.setStrokeColor(Color(0, 255, 0), ColorSpaceDeviceRGB);
    context.setFillColor(Color::transparent, ColorSpaceDeviceRGB);
    context.drawRect(snappedRect);
}

void paintFlow(const RenderBlockFlow& flow, const Layout& layout, PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (paintInfo.phase != PaintPhaseForeground)
        return;

    RenderStyle& style = flow.style();
    if (style.visibility() != VISIBLE)
        return;

    bool debugBordersEnabled = flow.frame().settings().simpleLineLayoutDebugBordersEnabled();

    GraphicsContext& context = *paintInfo.context;
    const FontCascade& font = style.fontCascade();
    TextPaintStyle textPaintStyle = computeTextPaintStyle(flow.frame(), style, paintInfo);
    GraphicsContextStateSaver stateSaver(context, textPaintStyle.strokeWidth > 0);

    updateGraphicsContext(context, textPaintStyle);
    LayoutRect paintRect = paintInfo.rect;
    paintRect.moveBy(-paintOffset);

    auto resolver = runResolver(flow, layout);
    float strokeOverflow = ceilf(flow.style().textStrokeWidth());
    for (const auto& run : resolver.rangeForRect(paintRect)) {
        FloatRect rect = run.rect();
        rect.inflate(strokeOverflow);
        if (!rect.intersects(paintRect) || run.start() == run.end())
            continue;
        TextRun textRun(run.text());
        textRun.setTabSize(!style.collapseWhiteSpace(), style.tabSize());
        textRun.setXPos(run.rect().x());
        FloatPoint textOrigin = run.baseline() + paintOffset;
        textOrigin.setY(roundToDevicePixel(LayoutUnit(textOrigin.y()), flow.document().deviceScaleFactor()));
        context.drawText(font, textRun, textOrigin);
        if (debugBordersEnabled)
            paintDebugBorders(context, run.rect(), paintOffset);
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

    RenderObject& renderer = *flow.firstChild();
    LayoutRect rangeRect = locationInContainer.boundingBox();
    rangeRect.moveBy(-accumulatedOffset);

    auto resolver = lineResolver(flow, layout);
    auto range = resolver.rangeForRect(rangeRect);
    for (auto it = range.begin(), end = range.end(); it != end; ++it) {
        auto lineRect = *it;
        lineRect.moveBy(accumulatedOffset);
        if (!locationInContainer.intersects(lineRect))
            continue;
        renderer.updateHitTestResult(result, locationInContainer.point() - toLayoutSize(accumulatedOffset));
        if (!result.addNodeToRectBasedTestResult(renderer.node(), request, locationInContainer, lineRect))
            return true;
    }

    return false;
}

void collectFlowOverflow(RenderBlockFlow& flow, const Layout& layout)
{
    auto resolver = lineResolver(flow, layout);
    float strokeOverflow = ceilf(flow.style().textStrokeWidth());
    for (auto it = resolver.begin(), end = resolver.end(); it != end; ++it) {
        auto rect = *it;
        rect.inflate(strokeOverflow);
        flow.addLayoutOverflow(rect);
        flow.addVisualOverflow(rect);
    }
}

IntRect computeBoundingBox(const RenderObject& renderer, const Layout& layout)
{
    auto resolver = runResolver(downcast<RenderBlockFlow>(*renderer.parent()), layout);
    FloatRect boundingBoxRect;
    for (const auto& run : resolver.rangeForRenderer(renderer)) {
        FloatRect rect = run.rect();
        if (boundingBoxRect == FloatRect())
            boundingBoxRect = rect;
        else
            boundingBoxRect.uniteEvenIfEmpty(rect);
    }
    return enclosingIntRect(boundingBoxRect);
}

IntPoint computeFirstRunLocation(const RenderObject& renderer, const Layout& layout)
{
    auto resolver = runResolver(downcast<RenderBlockFlow>(*renderer.parent()), layout);
    const auto& it = resolver.rangeForRenderer(renderer);
    auto begin = it.begin();
    if (begin == it.end())
        return IntPoint(0, 0);

    return flooredIntPoint((*begin).rect().location());
}

Vector<IntRect> collectAbsoluteRects(const RenderObject& renderer, const Layout& layout, const LayoutPoint& accumulatedOffset)
{
    Vector<IntRect> rects;
    auto resolver = runResolver(downcast<RenderBlockFlow>(*renderer.parent()), layout);
    for (const auto& run : resolver.rangeForRenderer(renderer)) {
        LayoutRect rect = run.rect();
        rects.append(enclosingIntRect(FloatRect(accumulatedOffset + rect.location(), rect.size())));
    }
    return rects;
}

Vector<FloatQuad> collectAbsoluteQuads(const RenderObject& renderer, const Layout& layout, bool* wasFixed)
{
    Vector<FloatQuad> quads;
    auto resolver = runResolver(downcast<RenderBlockFlow>(*renderer.parent()), layout);
    for (const auto& run : resolver.rangeForRenderer(renderer))
        quads.append(renderer.localToAbsoluteQuad(FloatQuad(run.rect()), UseTransforms, wasFixed));
    return quads;
}

#if ENABLE(TREE_DEBUGGING)
static void printPrefix(int& printedCharacters, int depth)
{
    fprintf(stderr, "------- --");
    printedCharacters = 0;
    while (++printedCharacters <= depth * 2)
        fputc(' ', stderr);
}

void showLineLayoutForFlow(const RenderBlockFlow& flow, const Layout& layout, int depth)
{
    int printedCharacters = 0;
    printPrefix(printedCharacters, depth);

    fprintf(stderr, "SimpleLineLayout (%u lines, %u runs) (%p)\n", layout.lineCount(), layout.runCount(), &layout);
    ++depth;

    auto resolver = runResolver(flow, layout);
    for (auto it = resolver.begin(), end = resolver.end(); it != end; ++it) {
        const auto& run = *it;
        LayoutRect r = run.rect();
        printPrefix(printedCharacters, depth);
        if (run.start() < run.end()) {
            fprintf(stderr, "line %u run(%u, %u) (%.2f, %.2f) (%.2f, %.2f) \"%s\"\n", run.lineIndex(), run.start(), run.end(),
                r.x().toFloat(), r.y().toFloat(), r.width().toFloat(), r.height().toFloat(), run.text().toStringWithoutCopying().utf8().data());
        } else {
            ASSERT(run.start() == run.end());
            fprintf(stderr, "line break %u run(%u, %u) (%.2f, %.2f) (%.2f, %.2f)\n", run.lineIndex(), run.start(), run.end(),
                r.x().toFloat(), r.y().toFloat(), r.width().toFloat(), r.height().toFloat());
        }
    }
}
#endif

}
}
