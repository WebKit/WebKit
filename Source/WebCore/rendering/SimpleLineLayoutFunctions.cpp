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

#include "BidiRun.h"
#include "BidiRunList.h"
#include "EventRegion.h"
#include "FontCache.h"
#include "Frame.h"
#include "GraphicsContext.h"
#include "HitTestLocation.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "InlineTextBox.h"
#include "LineInfo.h"
#include "PaintInfo.h"
#include "RenderBlockFlow.h"
#include "RenderChildIterator.h"
#include "RenderIterator.h"
#include "RenderStyle.h"
#include "RenderText.h"
#include "RenderView.h"
#include "Settings.h"
#include "SimpleLineLayoutFlowContents.h"
#include "SimpleLineLayoutResolver.h"
#include "Text.h"
#include "TextDecorationPainter.h"
#include "TextPaintStyle.h"
#include "TextPainter.h"
#include <wtf/text/TextStream.h>

#if ENABLE(TREE_DEBUGGING)
#include <stdio.h>
#endif

namespace WebCore {
namespace SimpleLineLayout {

FloatRect computeOverflow(const RenderBlockFlow& flow, const FloatRect& layoutRect)
{
    auto overflowRect = layoutRect;
    auto viewportSize = flow.frame().view() ? flow.frame().view()->size() : IntSize();
    auto strokeOverflow = std::ceil(flow.style().computedStrokeWidth(viewportSize));
    overflowRect.inflate(strokeOverflow);

    auto letterSpacing = flow.style().fontCascade().letterSpacing();
    if (letterSpacing >= 0)
        return overflowRect;
    // Last letter's negative spacing shrinks layout rect. Push it to visual overflow.
    overflowRect.expand(-letterSpacing, 0);
    return overflowRect;
}

void paintFlow(const RenderBlockFlow& flow, const Layout& layout, PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (paintInfo.phase == PaintPhase::EventRegion) {
        if (!flow.visibleToHitTesting())
            return;
        auto paintRect = paintInfo.rect;
        paintRect.moveBy(-paintOffset);
        for (auto run : layout.runResolver().rangeForRect(paintRect)) {
            FloatRect visualOverflowRect = computeOverflow(flow, run.rect());
            paintInfo.eventRegionContext->unite(enclosingIntRect(visualOverflowRect), flow.style());
        }
        return;
    }

    if (paintInfo.phase != PaintPhase::Foreground)
        return;

    auto& style = flow.style();
    if (style.visibility() != Visibility::Visible)
        return;

    TextPainter textPainter(paintInfo.context());
    textPainter.setFont(style.fontCascade());
    textPainter.setStyle(computeTextPaintStyle(flow.frame(), style, paintInfo));

    std::unique_ptr<ShadowData> debugShadow = nullptr;
    if (flow.settings().simpleLineLayoutDebugBordersEnabled()) {
        debugShadow = makeUnique<ShadowData>(IntPoint(0, 0), 10, 20, ShadowStyle::Normal, true, Color(0, 255, 0, 200));
        textPainter.setShadow(debugShadow.get());
    }

    Optional<TextDecorationPainter> textDecorationPainter;
    if (!style.textDecorationsInEffect().isEmpty()) {
        const RenderText* textRenderer = childrenOfType<RenderText>(flow).first();
        if (textRenderer) {
            textDecorationPainter.emplace(paintInfo.context(), style.textDecorationsInEffect(), *textRenderer, false, style.fontCascade());
        }
    }

    LayoutRect paintRect = paintInfo.rect;
    paintRect.moveBy(-paintOffset);

    auto& resolver = layout.runResolver();
    float deviceScaleFactor = flow.document().deviceScaleFactor();
    for (auto run : resolver.rangeForRect(paintRect)) {
        if (run.start() == run.end())
            continue;

        FloatRect rect = run.rect();
        FloatRect visualOverflowRect = computeOverflow(flow, rect);
        if (paintRect.y() > visualOverflowRect.maxY() || paintRect.maxY() < visualOverflowRect.y())
            continue;

        String textWithHyphen;
        if (run.hasHyphen())
            textWithHyphen = run.textWithHyphen();
        // x position indicates the line offset from the rootbox. It's always 0 in case of simple line layout.
        TextRun textRun { run.hasHyphen() ? textWithHyphen : run.text(), 0, run.expansion(), run.expansionBehavior() };
        textRun.setTabSize(!style.collapseWhiteSpace(), style.tabSize());
        FloatPoint textOrigin { rect.x() + paintOffset.x(), roundToDevicePixel(run.baselinePosition() + paintOffset.y(), deviceScaleFactor) };

        textPainter.setGlyphDisplayListIfNeeded(run.simpleRun(), paintInfo, style.fontCascade(), paintInfo.context(), textRun);
        textPainter.paint(textRun, rect, textOrigin);
        if (textDecorationPainter) {
            textDecorationPainter->setWidth(rect.width());
            textDecorationPainter->paintTextDecoration(textRun, textOrigin, rect.location() + paintOffset);
        }
    }
}

bool hitTestFlow(const RenderBlockFlow& flow, const Layout& layout, const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction hitTestAction)
{
    if (hitTestAction != HitTestForeground)
        return false;

    if (!layout.runCount())
        return false;

    auto& style = flow.style();
    if (style.visibility() != Visibility::Visible || style.pointerEvents() == PointerEvents::None)
        return false;

    LayoutRect rangeRect = locationInContainer.boundingBox();
    rangeRect.moveBy(-accumulatedOffset);
    auto resolver = lineResolver(layout.runResolver());
    auto range = resolver.rangeForRect(rangeRect);
    for (auto it = range.begin(), end = range.end(); it != end; ++it) {
        auto lineRect = *it;
        lineRect.moveBy(accumulatedOffset);
        auto& renderer = const_cast<RenderObject&>(it.renderer());
        if (!locationInContainer.intersects(lineRect))
            continue;
        renderer.updateHitTestResult(result, locationInContainer.point() - toLayoutSize(accumulatedOffset));
        if (result.addNodeToListBasedTestResult(renderer.node(), request, locationInContainer, lineRect) == HitTestProgress::Stop)
            return true;
    }
    return false;
}

void collectFlowOverflow(RenderBlockFlow& flow, const Layout& layout)
{
    for (auto lineRect : lineResolver(layout.runResolver())) {
        LayoutRect visualOverflowRect = LayoutRect(computeOverflow(flow, lineRect));
        flow.addLayoutOverflow(LayoutRect(lineRect));
        flow.addVisualOverflow(visualOverflowRect);
    }
}

IntRect computeBoundingBox(const RenderObject& renderer, const Layout& layout)
{
    auto& resolver = layout.runResolver();
    FloatRect boundingBoxRect;
    for (auto run : resolver.rangeForRenderer(renderer)) {
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
    auto& resolver = layout.runResolver();
    auto range = resolver.rangeForRenderer(renderer);
    auto begin = range.begin();
    if (begin == range.end())
        return IntPoint(0, 0);
    return flooredIntPoint((*begin).rect().location());
}

Vector<IntRect> collectAbsoluteRects(const RenderObject& renderer, const Layout& layout, const LayoutPoint& accumulatedOffset)
{
    Vector<IntRect> rects;
    auto& resolver = layout.runResolver();
    for (auto run : resolver.rangeForRenderer(renderer)) {
        FloatRect rect = run.rect();
        rects.append(enclosingIntRect(FloatRect(accumulatedOffset + rect.location(), rect.size())));
    }
    return rects;
}

Vector<FloatQuad> collectAbsoluteQuads(const RenderObject& renderer, const Layout& layout, bool* wasFixed)
{
    Vector<FloatQuad> quads;
    auto& resolver = layout.runResolver();
    for (auto run : resolver.rangeForRenderer(renderer))
        quads.append(renderer.localToAbsoluteQuad(FloatQuad(run.rect()), UseTransforms, wasFixed));
    return quads;
}

unsigned textOffsetForPoint(const LayoutPoint& point, const RenderText& renderer, const Layout& layout)
{
    auto& flow = downcast<RenderBlockFlow>(*renderer.parent());
    ASSERT(flow.firstChild() == flow.lastChild());
    auto& resolver = layout.runResolver();
    auto it = resolver.runForPoint(point);
    if (it == resolver.end())
        return renderer.text().length();
    auto run = *it;
    auto& style = flow.style();
    TextRun textRun(run.text(), run.logicalLeft(), run.expansion(), run.expansionBehavior());
    textRun.setTabSize(!style.collapseWhiteSpace(), style.tabSize());
    return run.start() + style.fontCascade().offsetForPosition(textRun, point.x() - run.logicalLeft(), true);
}

Vector<FloatQuad> collectAbsoluteQuadsForRange(const RenderObject& renderer, unsigned start, unsigned end, const Layout& layout, bool* wasFixed)
{
    auto& style = downcast<RenderBlockFlow>(*renderer.parent()).style();
    Vector<FloatQuad> quads;
    auto& resolver = layout.runResolver();
    for (auto run : resolver.rangeForRendererWithOffsets(renderer, start, end)) {
        // This run is fully contained.
        if (start <= run.start() && end >= run.end()) {
            quads.append(renderer.localToAbsoluteQuad(FloatQuad(run.rect()), UseTransforms, wasFixed));
            continue;
        }
        // Partially contained run.
        TextRun textRun(run.text(), run.logicalLeft(), run.expansion(), run.expansionBehavior());
        textRun.setTabSize(!style.collapseWhiteSpace(), style.tabSize());
        LayoutRect runRect(run.rect());
        // Special case empty ranges.
        if (start == end) {
            runRect.setWidth(0);
            quads.append(renderer.localToAbsoluteQuad(FloatQuad(runRect), UseTransforms, wasFixed));
            continue;
        }
        auto localStart = std::max(run.start(), start) - run.start();
        auto localEnd = std::min(run.end(), end) - run.start();
        ASSERT(localStart <= localEnd);
        style.fontCascade().adjustSelectionRectForText(textRun, runRect, localStart, localEnd);
        quads.append(renderer.localToAbsoluteQuad(FloatQuad(runRect), UseTransforms, wasFixed));
    }
    return quads;
}

const RenderObject& rendererForPosition(const FlowContents& flowContents, unsigned position)
{
    return flowContents.segmentForPosition(position).renderer;
}

void simpleLineLayoutWillBeDeleted(const Layout& layout)
{
    for (unsigned i = 0; i < layout.runCount(); ++i)
        TextPainter::removeGlyphDisplayList(layout.runAt(i));
}

bool canUseForLineBoxTree(RenderBlockFlow& flow, const Layout& layout)
{
    // Line breaking requires some context that SLL can't provide at the moment (see RootInlineBox::setLineBreakInfo).
    if (layout.lineCount() > 1)
        return false;

    if (layout.isPaginated())
        return false;
    
    if (flow.style().preserveNewline())
        return false;
    
    if (!flow.firstChild())
        return false;
    
    for (auto& child : childrenOfType<RenderObject>(flow)) {
        if (!is<RenderText>(child))
            return false;
        // Simple line layout iterator can't handle renderers with zero length properly.
        if (!downcast<RenderText>(child).length())
            return false;
    }
    return true;
}

static void initializeInlineTextBox(RenderBlockFlow& flow, InlineTextBox& inlineTextBox, const RunResolver::Run& run)
{
    inlineTextBox.setLogicalLeft(run.logicalLeft());
    inlineTextBox.setLogicalTop(run.rect().y());
    inlineTextBox.setLogicalWidth(run.logicalRight() - run.logicalLeft());
    auto overflowRect = computeOverflow(const_cast<RenderBlockFlow&>(flow), run.rect());
    if (overflowRect != run.rect())
        inlineTextBox.setLogicalOverflowRect(LayoutRect(overflowRect));

    inlineTextBox.setHasHyphen(run.hasHyphen());
    inlineTextBox.setExpansionWithoutGrowing(run.expansion());

    auto expansionBehavior = run.expansionBehavior();
    inlineTextBox.setCanHaveLeadingExpansion(expansionBehavior & AllowLeadingExpansion);
    inlineTextBox.setCanHaveTrailingExpansion(expansionBehavior & AllowTrailingExpansion);
    if (expansionBehavior & ForceTrailingExpansion)
        inlineTextBox.setForceTrailingExpansion();
    if (expansionBehavior & ForceLeadingExpansion)
        inlineTextBox.setForceLeadingExpansion();
}

void generateLineBoxTree(RenderBlockFlow& flow, const Layout& layout)
{
    ASSERT(!flow.complexLineLayout()->lineBoxes().firstLineBox());
    if (!layout.runCount())
        return;

    Ref<BidiContext> bidiContext = BidiContext::create(0, U_LEFT_TO_RIGHT);
    auto& resolver = layout.runResolver();
    unsigned lineIndex = 0;
    while (true) {
        auto range = resolver.rangeForLine(lineIndex++);
        if (range.begin() == range.end())
            break;

        // Generate bidi runs out of simple line layout runs.
        BidiRunList<BidiRun> bidiRuns;
        for (auto it = range.begin(); it != range.end(); ++it) {
            auto run = *it;
            bidiRuns.appendRun(makeUnique<BidiRun>(run.localStart(), run.localEnd(), const_cast<RenderObject&>(run.renderer()), bidiContext.ptr(), U_LEFT_TO_RIGHT));
        }

        LineInfo lineInfo;
        lineInfo.setFirstLine(!flow.complexLineLayout()->lineBoxes().firstLineBox());
        // FIXME: This is needed for flow boxes -but we don't have them yet.
        // lineInfo.setLastLine(lastLine);
        lineInfo.setEmpty(!bidiRuns.runCount());
        bidiRuns.setLogicallyLastRun(bidiRuns.lastRun());
        auto* root = flow.complexLineLayout()->constructLine(bidiRuns, lineInfo);
        bidiRuns.clear();
        if (!root)
            continue;

        auto& rootLineBox = *root;
        auto it = range.begin();
        float lineWidth = 0;
        // Set the geometry for the inlineboxes.
        for (auto* inlineBox = rootLineBox.firstChild(); inlineBox && it != range.end(); inlineBox = inlineBox->nextOnLine(), ++it) {
            auto run = *it;
            initializeInlineTextBox(flow, downcast<InlineTextBox>(*inlineBox), run);
            lineWidth += inlineBox->logicalWidth();
        }

        // Finish setting up the rootline.
        auto iter = range.begin();
        auto firstRun = *iter;
        rootLineBox.setLogicalLeft(firstRun.logicalLeft());
        rootLineBox.setLogicalWidth(lineWidth);
        auto lineTop = firstRun.rect().y();
        auto lineHeight = firstRun.rect().height();
        rootLineBox.setLogicalTop(lineTop);
        rootLineBox.setLineTopBottomPositions(LayoutUnit(lineTop), LayoutUnit(lineTop + lineHeight), LayoutUnit(lineTop), LayoutUnit(lineTop + lineHeight));
    }
}

#if ENABLE(TREE_DEBUGGING)
static void printPrefix(TextStream& stream, int& printedCharacters, int depth)
{
    stream << "-------- --";
    printedCharacters = 0;
    while (++printedCharacters <= depth * 2)
        stream << " ";
}

void outputLineLayoutForFlow(TextStream& stream, const RenderBlockFlow& flow, const Layout& layout, int depth)
{
    int printedCharacters = 0;
    printPrefix(stream, printedCharacters, depth);

    stream << "SimpleLineLayout (" << layout.lineCount() << " lines, " << layout.runCount() << " runs) (" << &layout << ")";
    stream.nextLine();
    ++depth;

    for (auto run : runResolver(flow, layout)) {
        FloatRect rect = run.rect();
        printPrefix(stream, printedCharacters, depth);
        if (run.start() < run.end()) {
            stream << "line " << run.lineIndex() << " run(" << run.start() << ", " << run.end() << ") " << rect << " \"" << run.text().toStringWithoutCopying().utf8().data() << "\"";
        } else {
            ASSERT(run.start() == run.end());
            stream << "line break " << run.lineIndex() << " run(" << run.start() << ", " << run.end() << ") " << rect;
        }
    }
    stream.nextLine();
}
#endif

}
}
