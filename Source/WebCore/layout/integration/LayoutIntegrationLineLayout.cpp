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
#include "LayoutIntegrationLineLayout.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "EventRegion.h"
#include "FloatingState.h"
#include "HitTestLocation.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "InlineFormattingContext.h"
#include "InlineFormattingState.h"
#include "InvalidationState.h"
#include "LayoutBoxGeometry.h"
#include "LayoutTreeBuilder.h"
#include "PaintInfo.h"
#include "RenderBlockFlow.h"
#include "RenderChildIterator.h"
#include "RenderDescendantIterator.h"
#include "RenderLineBreak.h"
#include "RenderView.h"
#include "RuntimeEnabledFeatures.h"
#include "Settings.h"
#include "SimpleLineLayout.h"
#include "TextDecorationPainter.h"
#include "TextPainter.h"

namespace WebCore {
namespace LayoutIntegration {

LineLayout::LineLayout(const RenderBlockFlow& flow)
    : m_flow(flow)
    , m_boxTree(flow)
    , m_layoutState(m_flow.document(), rootLayoutBox())
    , m_inlineFormattingState(m_layoutState.ensureInlineFormattingState(rootLayoutBox()))
{
    m_layoutState.setIsIntegratedRootBoxFirstChild(m_flow.parent()->firstChild() == &m_flow);
}

LineLayout::~LineLayout() = default;

bool LineLayout::canUseFor(const RenderBlockFlow& flow, Optional<bool> couldUseSimpleLineLayout)
{
    if (!RuntimeEnabledFeatures::sharedFeatures().layoutFormattingContextIntegrationEnabled())
        return false;

    // Initially only a subset of SLL features is supported.
    auto passesSimpleLineLayoutTest = valueOrCompute(couldUseSimpleLineLayout, [&] {
        return SimpleLineLayout::canUseFor(flow);
    });

    if (!passesSimpleLineLayoutTest)
        return false;

    if (flow.fragmentedFlowState() != RenderObject::NotInsideFragmentedFlow)
        return false;

    return true;
}

bool LineLayout::canUseForAfterStyleChange(const RenderBlockFlow& flow, StyleDifference diff)
{
    ASSERT(RuntimeEnabledFeatures::sharedFeatures().layoutFormattingContextIntegrationEnabled());
    return SimpleLineLayout::canUseForAfterStyleChange(flow, diff);
}

void LineLayout::updateStyle()
{
    auto& root = rootLayoutBox();

    // FIXME: Encapsulate style updates better.
    root.updateStyle(m_flow.style());

    for (auto* child = root.firstChild(); child; child = child->nextSibling()) {
        if (child->isAnonymous())
            child->updateStyle(RenderStyle::createAnonymousStyleWithDisplay(root.style(), DisplayType::Inline));
    }
}

void LineLayout::layout()
{
    if (!rootLayoutBox().hasInFlowOrFloatingChild())
        return;

    prepareLayoutState();
    prepareFloatingState();

    auto inlineFormattingContext = Layout::InlineFormattingContext { rootLayoutBox(), m_inlineFormattingState };

    auto invalidationState = Layout::InvalidationState { };
    auto horizontalConstraints = Layout::HorizontalConstraints { m_flow.borderAndPaddingStart(), m_flow.contentSize().width() };
    auto verticalConstraints = Layout::VerticalConstraints { m_flow.borderAndPaddingBefore(), { } };

    inlineFormattingContext.layoutInFlowContent(invalidationState, { horizontalConstraints, verticalConstraints });
    constructDisplayContent();
}

void LineLayout::constructDisplayContent()
{
    // FIXME: Move Display::Run construction over here.
    auto constructDisplayLine = [&] {
        auto& displayInlineContent = m_inlineFormattingState.ensureDisplayInlineContent();
        auto& lines = m_inlineFormattingState.lines();
        auto& runs = displayInlineContent.runs;
        size_t runIndex = 0;
        for (size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
            auto& line = lines[lineIndex];
            auto& lineBoxLogicalRect = line.lineBoxLogicalRect();
            // FIXME: This is where the logical to physical translate should happen.
            auto overflowWidth = std::max(line.logicalWidth(), lineBoxLogicalRect.width());
            auto lineBoxLogicalBottom = (lineBoxLogicalRect.top() - line.logicalTop()) + lineBoxLogicalRect.height();
            auto overflowHeight = std::max(line.logicalHeight(), lineBoxLogicalBottom);
            auto scrollableOverflowRect = FloatRect { line.logicalLeft(), line.logicalTop(), overflowWidth, overflowHeight };

            auto lineInkOverflowRect = scrollableOverflowRect;
            while (runIndex < runs.size() && runs[runIndex].lineIndex() == lineIndex)
                lineInkOverflowRect.unite(runs[runIndex++].inkOverflow());
            auto lineRect = FloatRect { line.logicalRect() };
            // Painting code (specifically TextRun's xPos) needs the line box offset to be able to compute tab positions.
            lineRect.setX(lineBoxLogicalRect.left());
            displayInlineContent.lines.append({ lineRect, scrollableOverflowRect, lineInkOverflowRect, line.baseline() });
        }
    };
    constructDisplayLine();
    m_inlineFormattingState.shrinkDisplayInlineContent();
}

void LineLayout::prepareLayoutState()
{
    m_layoutState.setViewportSize(m_flow.frame().view()->size());
}

void LineLayout::prepareFloatingState()
{
    auto& floatingState = m_inlineFormattingState.floatingState();
    floatingState.clear();

    if (!m_flow.containsFloats())
        return;

    for (auto& floatingObject : *m_flow.floatingObjectSet()) {
        auto& rect = floatingObject->frameRect();
        auto position = floatingObject->type() == FloatingObject::FloatRight
            ? Layout::FloatingState::FloatItem::Position::Right
            : Layout::FloatingState::FloatItem::Position::Left;
        auto boxGeometry = Layout::BoxGeometry { };
        // FIXME: We are flooring here for legacy compatibility.
        //        See FloatingObjects::intervalForFloatingObject.
        auto y = rect.y().floor();
        auto maxY = rect.maxY().floor();
        boxGeometry.setLogicalTopLeft({ rect.x(), y });
        boxGeometry.setContentBoxWidth(rect.width());
        boxGeometry.setContentBoxHeight(maxY - y);
        boxGeometry.setBorder({ });
        boxGeometry.setPadding({ });
        boxGeometry.setHorizontalMargin({ });
        boxGeometry.setVerticalMargin({ });
        floatingState.append({ position, boxGeometry });
    }
}

LayoutUnit LineLayout::contentLogicalHeight() const
{
    auto& lines = m_inlineFormattingState.lines();
    return LayoutUnit { lines.last().logicalBottom() - lines.first().logicalTop() };
}

size_t LineLayout::lineCount() const
{
    auto* inlineContent = displayInlineContent();
    if (!inlineContent)
        return 0;
    if (inlineContent->runs.isEmpty())
        return 0;
    return m_inlineFormattingState.lines().size();
}

LayoutUnit LineLayout::firstLineBaseline() const
{
    auto& lines = m_inlineFormattingState.lines();
    if (lines.isEmpty()) {
        ASSERT_NOT_REACHED();
        return 0_lu;
    }

    auto& firstLine = lines.first();
    return Layout::toLayoutUnit(firstLine.logicalTop() + firstLine.baseline());
}

LayoutUnit LineLayout::lastLineBaseline() const
{
    auto& lines = m_inlineFormattingState.lines();
    if (lines.isEmpty()) {
        ASSERT_NOT_REACHED();
        return 0_lu;
    }

    auto& lastLine = lines.last();
    return Layout::toLayoutUnit(lastLine.logicalTop() + lastLine.baseline());
}

void LineLayout::collectOverflow(RenderBlockFlow& flow)
{
    ASSERT(&flow == &m_flow);
    ASSERT(!flow.hasOverflowClip());

    for (auto& line : displayInlineContent()->lines) {
        flow.addLayoutOverflow(Layout::toLayoutRect(line.scrollableOverflow()));
        flow.addVisualOverflow(Layout::toLayoutRect(line.inkOverflow()));
    }
}

const Display::InlineContent* LineLayout::displayInlineContent() const
{
    return m_inlineFormattingState.displayInlineContent();
}

LineLayoutTraversal::TextBoxIterator LineLayout::textBoxesFor(const RenderText& renderText) const
{
    auto* inlineContent = displayInlineContent();
    if (!inlineContent)
        return { };
    auto* layoutBox = m_boxTree.layoutBoxForRenderer(renderText);
    ASSERT(layoutBox);

    Optional<size_t> firstIndex;
    size_t lastIndex = 0;
    for (size_t i = 0; i < inlineContent->runs.size(); ++i) {
        auto& run = inlineContent->runs[i];
        if (&run.layoutBox() == layoutBox) {
            if (!firstIndex)
                firstIndex = i;
            lastIndex = i;
        } else if (firstIndex)
            break;
    }
    if (!firstIndex)
        return { };

    return { LineLayoutTraversal::DisplayRunPath(*inlineContent, *firstIndex, lastIndex + 1) };
}

LineLayoutTraversal::ElementBoxIterator LineLayout::elementBoxFor(const RenderLineBreak& renderLineBreak) const
{
    auto* inlineContent = displayInlineContent();
    if (!inlineContent)
        return { };
    auto* layoutBox = m_boxTree.layoutBoxForRenderer(renderLineBreak);
    ASSERT(layoutBox);

    for (size_t i = 0; i < inlineContent->runs.size(); ++i) {
        auto& run =  inlineContent->runs[i];
        if (&run.layoutBox() == layoutBox)
            return { LineLayoutTraversal::DisplayRunPath(*inlineContent, i, i + 1) };
    }

    return { };
}

const Layout::ContainerBox& LineLayout::rootLayoutBox() const
{
    return m_boxTree.rootLayoutBox();
}

Layout::ContainerBox& LineLayout::rootLayoutBox()
{
    return m_boxTree.rootLayoutBox();
}

void LineLayout::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (!displayInlineContent())
        return;

    if (paintInfo.phase != PaintPhase::Foreground && paintInfo.phase != PaintPhase::EventRegion)
        return;

    auto& inlineContent = *displayInlineContent();
    float deviceScaleFactor = m_flow.document().deviceScaleFactor();

    auto paintRect = paintInfo.rect;
    paintRect.moveBy(-paintOffset);

    for (auto& run : inlineContent.runsForRect(paintRect)) {
        if (!run.textContent())
            continue;

        auto& textContent = *run.textContent();
        if (!textContent.length())
            continue;

        auto& style = run.style();
        if (style.visibility() != Visibility::Visible)
            continue;

        auto rect = FloatRect { run.rect() };
        auto visualOverflowRect = FloatRect { run.inkOverflow() };
        if (paintRect.y() > visualOverflowRect.maxY() || paintRect.maxY() < visualOverflowRect.y())
            continue;

        if (paintInfo.eventRegionContext) {
            if (style.pointerEvents() != PointerEvents::None)
                paintInfo.eventRegionContext->unite(enclosingIntRect(visualOverflowRect), style);
            continue;
        }

        auto& line = inlineContent.lineForRun(run);
        auto baseline = paintOffset.y() + line.rect().y() + line.baseline();
        auto expansion = run.expansion();

        String textWithHyphen;
        if (textContent.needsHyphen())
            textWithHyphen = makeString(textContent.content(), style.hyphenString());
        TextRun textRun { !textWithHyphen.isEmpty() ? textWithHyphen : textContent.content(), run.left() - line.rect().x(), expansion.horizontalExpansion, expansion.behavior };
        textRun.setTabSize(!style.collapseWhiteSpace(), style.tabSize());
        FloatPoint textOrigin { rect.x() + paintOffset.x(), roundToDevicePixel(baseline, deviceScaleFactor) };

        TextPainter textPainter(paintInfo.context());
        textPainter.setFont(style.fontCascade());
        textPainter.setStyle(computeTextPaintStyle(m_flow.frame(), style, paintInfo));
        if (auto* debugShadow = debugTextShadow())
            textPainter.setShadow(debugShadow);

        textPainter.setGlyphDisplayListIfNeeded(run, paintInfo, style.fontCascade(), paintInfo.context(), textRun);
        textPainter.paint(textRun, rect, textOrigin);

        if (!style.textDecorationsInEffect().isEmpty()) {
            // FIXME: Use correct RenderText.
            if (auto* textRenderer = childrenOfType<RenderText>(m_flow).first()) {
                auto painter = TextDecorationPainter { paintInfo.context(), style.textDecorationsInEffect(), *textRenderer, false, style.fontCascade() };
                painter.setWidth(rect.width());
                painter.paintTextDecoration(textRun, textOrigin, rect.location() + paintOffset);
            }
        }
    }
}

bool LineLayout::hitTest(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction hitTestAction)
{
    if (hitTestAction != HitTestForeground)
        return false;

    if (!displayInlineContent())
        return false;

    auto& inlineContent = *displayInlineContent();

    // FIXME: This should do something efficient to find the run range.
    for (auto& run : inlineContent.runs) {
        auto runRect = Layout::toLayoutRect(run.rect());
        runRect.moveBy(accumulatedOffset);

        if (!locationInContainer.intersects(runRect))
            continue;

        auto& style = run.style();
        if (style.visibility() != Visibility::Visible || style.pointerEvents() == PointerEvents::None)
            continue;

        auto& renderer = const_cast<RenderObject&>(*m_boxTree.rendererForLayoutBox(run.layoutBox()));

        renderer.updateHitTestResult(result, locationInContainer.point() - toLayoutSize(accumulatedOffset));
        if (result.addNodeToListBasedTestResult(renderer.nodeForHitTest(), request, locationInContainer, runRect) == HitTestProgress::Stop)
            return true;
    }

    return false;
}

ShadowData* LineLayout::debugTextShadow()
{
    if (!m_flow.settings().simpleLineLayoutDebugBordersEnabled())
        return nullptr;

    static NeverDestroyed<ShadowData> debugTextShadow(IntPoint(0, 0), 10, 20, ShadowStyle::Normal, true, SRGBA<uint8_t> { 0, 0, 150, 150 });
    return &debugTextShadow.get();
}

void LineLayout::releaseCaches(RenderView& view)
{
    if (!RuntimeEnabledFeatures::sharedFeatures().layoutFormattingContextIntegrationEnabled())
        return;

    for (auto& renderer : descendantsOfType<RenderBlockFlow>(view)) {
        if (auto* lineLayout = renderer.layoutFormattingContextLineLayout())
            lineLayout->releaseInlineItemCache();
    }
}

void LineLayout::releaseInlineItemCache()
{
    m_inlineFormattingState.inlineItems().clear();
}


}
}

#endif
