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
#include "LayoutIntegrationCoverage.h"
#include "LayoutIntegrationPagination.h"
#include "LayoutReplacedBox.h"
#include "LayoutTreeBuilder.h"
#include "PaintInfo.h"
#include "RenderBlockFlow.h"
#include "RenderChildIterator.h"
#include "RenderDescendantIterator.h"
#include "RenderImage.h"
#include "RenderLineBreak.h"
#include "RenderView.h"
#include "RuntimeEnabledFeatures.h"
#include "Settings.h"
#include "TextDecorationPainter.h"
#include "TextPainter.h"

namespace WebCore {
namespace LayoutIntegration {

LineLayout::LineLayout(RenderBlockFlow& flow)
    : m_boxTree(flow)
    , m_layoutState(flow.document(), rootLayoutBox())
    , m_inlineFormattingState(m_layoutState.ensureInlineFormattingState(rootLayoutBox()))
{
    m_layoutState.setIsIntegratedRootBoxFirstChild(flow.parent()->firstChild() == &flow);
}

LineLayout::~LineLayout() = default;

LineLayout* LineLayout::containing(RenderObject& renderer)
{
    if (!renderer.isInline())
        return nullptr;

    // FIXME: These fake renderers have their parent set but are not actually in the tree.
    if (renderer.isReplica() || renderer.isRenderScrollbarPart())
        return nullptr;
    
    if (auto* parent = renderer.parent()) {
        if (is<RenderBlockFlow>(*parent))
            return downcast<RenderBlockFlow>(*parent).modernLineLayout();
    }

    return nullptr;
}

const LineLayout* LineLayout::containing(const RenderObject& renderer)
{
    return containing(const_cast<RenderObject&>(renderer));
}

bool LineLayout::isEnabled()
{
    return RuntimeEnabledFeatures::sharedFeatures().layoutFormattingContextIntegrationEnabled();
}

bool LineLayout::canUseFor(const RenderBlockFlow& flow)
{
    if (!isEnabled())
        return false;

    return canUseForLineLayout(flow);
}

bool LineLayout::canUseForAfterStyleChange(const RenderBlockFlow& flow, StyleDifference diff)
{
    ASSERT(isEnabled());
    return canUseForLineLayoutAfterStyleChange(flow, diff);
}

void LineLayout::updateReplacedDimensions(const RenderBox& replaced)
{
    auto& layoutBox = m_boxTree.layoutBoxForRenderer(replaced);
    auto& replacedBox = downcast<Layout::ReplacedBox>(layoutBox);

    replacedBox.setContentSizeForIntegration({ replaced.contentLogicalWidth(), replaced.contentLogicalHeight() });
}

void LineLayout::updateStyle(const RenderBoxModelObject& renderer)
{
    m_boxTree.updateStyle(renderer);
}

void LineLayout::layout()
{
    if (!rootLayoutBox().hasInFlowOrFloatingChild())
        return;

    prepareLayoutState();
    prepareFloatingState();

    m_inlineContent = nullptr;
    auto inlineFormattingContext = Layout::InlineFormattingContext { rootLayoutBox(), m_inlineFormattingState };

    auto invalidationState = Layout::InvalidationState { };
    auto horizontalConstraints = Layout::HorizontalConstraints { flow().borderAndPaddingStart(), flow().contentSize().width() };
    auto verticalConstraints = Layout::VerticalConstraints { flow().borderAndPaddingBefore(), { } };

    inlineFormattingContext.layoutInFlowContent(invalidationState, { horizontalConstraints, verticalConstraints });
    constructContent();
}

inline static float lineOverflowWidth(const RenderBlockFlow& flow, InlineLayoutUnit lineLogicalWidth, InlineLayoutUnit lineBoxLogicalWidth)
{
    // FIXME: It's the copy of the lets-adjust-overflow-for-the-caret behavior from ComplexLineLayout::addOverflowFromInlineChildren.
    auto endPadding = flow.hasOverflowClip() ? flow.paddingEnd() : 0_lu;
    if (!endPadding)
        endPadding = flow.endPaddingWidthForCaret();
    if (flow.hasOverflowClip() && !endPadding && flow.element() && flow.element()->isRootEditableElement())
        endPadding = 1;
    lineBoxLogicalWidth += endPadding;
    return std::max(lineLogicalWidth, lineBoxLogicalWidth);
}

void LineLayout::constructContent()
{
    auto& displayInlineContent = ensureInlineContent();
    auto& lines = m_inlineFormattingState.lines();

    struct LineLevelVisualAdjustmentsForRuns {
        bool needsIntegralPosition { false };
        // It's only 'text-overflow: ellipsis' for now.
        bool needsTrailingContentReplacement { false };
    };
    Vector<LineLevelVisualAdjustmentsForRuns> lineLevelVisualAdjustmentsForRuns(lines.size());

    auto computeLineLevelVisualAdjustmentsForRuns = [&] {
        auto& rootStyle = rootLayoutBox().style();
        auto shouldCheckHorizontalOverflowForContentReplacement = rootStyle.overflowX() == Overflow::Hidden && rootStyle.textOverflow() != TextOverflow::Clip;

        for (size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
            auto lineNeedsLegacyIntegralVerticalPosition = [&] {
                // InlineTree rounds y position to integral value for certain content (see InlineFlowBox::placeBoxesInBlockDirection).
                auto inlineLevelBoxList = m_inlineFormattingState.lineBoxes()[lineIndex].inlineLevelBoxList();
                if (inlineLevelBoxList.size() == 1) {
                    // This is text content only with root inline box.
                    return true;
                }
                // Text + <br> (or just <br> or text<span></span><br>) behaves like text.
                for (auto& inlineLevelBox : inlineLevelBoxList) {
                    if (inlineLevelBox->isAtomicInlineLevelBox()) {
                        // Content like text<img> prevents legacy snapping.
                        return false;
                    }
                }
                return true;
            };
            lineLevelVisualAdjustmentsForRuns[lineIndex].needsIntegralPosition = lineNeedsLegacyIntegralVerticalPosition();
            if (shouldCheckHorizontalOverflowForContentReplacement) {
                auto& line = lines[lineIndex];
                auto overflowWidth = lineOverflowWidth(flow(), line.logicalWidth(), line.lineBoxLogicalRect().width());
                lineLevelVisualAdjustmentsForRuns[lineIndex].needsTrailingContentReplacement = overflowWidth > line.logicalWidth();
            }
        }
    };
    computeLineLevelVisualAdjustmentsForRuns();

    auto constructDisplayLineRuns = [&] {
        auto initialContaingBlockSize = m_layoutState.viewportSize();
        for (auto& lineRun : m_inlineFormattingState.lineRuns()) {
            auto& layoutBox = lineRun.layoutBox();
            auto& style = layoutBox.style();
            auto computedInkOverflow = [&] (auto runRect) {
                // FIXME: Add support for non-text ink overflow.
                if (!lineRun.text())
                    return runRect;
                auto inkOverflow = runRect;
                auto strokeOverflow = std::ceil(style.computedStrokeWidth(ceiledIntSize(initialContaingBlockSize)));
                inkOverflow.inflate(strokeOverflow);

                auto letterSpacing = style.fontCascade().letterSpacing();
                if (letterSpacing < 0) {
                    // Last letter's negative spacing shrinks logical rect. Push it to ink overflow.
                    inkOverflow.expand(-letterSpacing, { });
                }
                return inkOverflow;
            };
            auto runRect = FloatRect { lineRun.logicalRect() };
            // Inline boxes are relative to the line box while final Runs need to be relative to the parent Box
            // FIXME: Shouldn't we just leave them be relative to the line box?
            auto lineIndex = lineRun.lineIndex();
            auto lineBoxLogicalRect = lines[lineIndex].lineBoxLogicalRect();
            runRect.moveBy({ lineBoxLogicalRect.left(), lineBoxLogicalRect.top() });
            if (lineLevelVisualAdjustmentsForRuns[lineIndex].needsIntegralPosition)
                runRect.setY(roundToInt(runRect.y()));

            WTF::Optional<Run::TextContent> textContent;
            if (auto text = lineRun.text()) {
                auto adjustedContentToRenderer = [&] {
                    // FIXME: This is where we create strings with trailing hyphens and truncate/replace content with ellipsis.
                    if (text->needsHyphen())
                        return makeString(StringView(text->content()).substring(text->start(), text->length()), style.hyphenString());
                    return String();
                };
                textContent = Run::TextContent { text->start(), text->length(), text->content(), adjustedContentToRenderer(), text->needsHyphen() };
            }
            auto expansion = Run::Expansion { lineRun.expansion().behavior, lineRun.expansion().horizontalExpansion };
            auto displayRun = Run { lineIndex, layoutBox, runRect, computedInkOverflow(runRect), expansion, textContent };
            displayInlineContent.runs.append(displayRun);

            if (layoutBox.isReplacedBox()) {
                auto& renderer = downcast<RenderBox>(m_boxTree.rendererForLayoutBox(layoutBox));
                auto& boxGeometry = m_layoutState.geometryForBox(layoutBox);
                auto borderBoxLocation = FloatPoint { runRect.x() + std::max(boxGeometry.marginStart(), 0_lu), runRect.y() + boxGeometry.marginBefore() };
                renderer.setLocation(flooredLayoutPoint(borderBoxLocation));
            }
        }
    };
    constructDisplayLineRuns();

    auto constructDisplayLines = [&] {
        auto& lineBoxes = m_inlineFormattingState.lineBoxes();
        auto& runs = displayInlineContent.runs;
        size_t runIndex = 0;
        for (size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
            auto& line = lines[lineIndex];
            auto lineBoxLogicalRect = line.lineBoxLogicalRect();
            // FIXME: This is where the logical to physical translate should happen.
            auto lineBoxLogicalBottom = (lineBoxLogicalRect.top() - line.logicalTop()) + lineBoxLogicalRect.height();
            auto overflowHeight = std::max(line.logicalHeight(), lineBoxLogicalBottom);
            auto scrollableOverflowRect = FloatRect { line.logicalLeft(), line.logicalTop(), lineOverflowWidth(flow(), line.logicalWidth(), lineBoxLogicalRect.width()), overflowHeight };

            auto firstRunIndex = runIndex;
            auto lineInkOverflowRect = scrollableOverflowRect;
            while (runIndex < runs.size() && runs[runIndex].lineIndex() == lineIndex)
                lineInkOverflowRect.unite(runs[runIndex++].inkOverflow());
            auto runCount = runIndex - firstRunIndex;
            auto lineRect = FloatRect { line.logicalRect() };
            // Painting code (specifically TextRun's xPos) needs the line box offset to be able to compute tab positions.
            lineRect.setX(lineBoxLogicalRect.left());
            auto enclosingLineRect = [&] {
                // Let's (vertically)enclose all the inline level boxes.
                // This mostly matches 'lineRect', unless line-height triggers line box overflow (not to be confused with ink or scroll overflow).
                auto enclosingRect = lineRect;
                auto& lineBox = lineBoxes[lineIndex];
                for (auto& inlineLevelBox : lineBox.inlineLevelBoxList()) {
                    auto inlineLevelBoxLogicalRect = lineBox.logicalRectForInlineLevelBox(inlineLevelBox->layoutBox());
                    // inlineLevelBoxLogicalRect is relative to the line.
                    inlineLevelBoxLogicalRect.moveBy(lineRect.location());
                    enclosingRect.setY(std::min(enclosingRect.y(), inlineLevelBoxLogicalRect.top()));
                    enclosingRect.shiftMaxYEdgeTo(std::max(enclosingRect.maxY(), inlineLevelBoxLogicalRect.bottom()));
                }
                return enclosingRect;
            }();
            if (lineLevelVisualAdjustmentsForRuns[lineIndex].needsIntegralPosition) {
                lineRect.setY(roundToInt(lineRect.y()));
                enclosingLineRect.setY(roundToInt(enclosingLineRect.y()));
            }
            displayInlineContent.lines.append({ firstRunIndex, runCount, lineRect, enclosingLineRect, scrollableOverflowRect, lineInkOverflowRect, line.baseline(), line.horizontalAlignmentOffset() });
        }
    };
    constructDisplayLines();

    displayInlineContent.clearGapAfterLastLine = m_inlineFormattingState.clearGapAfterLastLine();
    displayInlineContent.shrinkToFit();
    m_inlineFormattingState.shrinkToFit();
}

void LineLayout::prepareLayoutState()
{
    m_layoutState.setViewportSize(flow().frame().view()->size());

    auto& rootGeometry = m_layoutState.ensureGeometryForBox(rootLayoutBox());
    rootGeometry.setContentBoxWidth(flow().contentSize().width());
    rootGeometry.setPadding({ { } });
    rootGeometry.setBorder({ });
    rootGeometry.setHorizontalMargin({ });
    rootGeometry.setVerticalMargin({ });
}

void LineLayout::prepareFloatingState()
{
    auto& floatingState = m_inlineFormattingState.floatingState();
    floatingState.clear();

    if (!flow().containsFloats())
        return;

    for (auto& floatingObject : *flow().floatingObjectSet()) {
        auto& rect = floatingObject->frameRect();
        auto position = floatingObject->type() == FloatingObject::FloatRight
            ? Layout::FloatingState::FloatItem::Position::Right
            : Layout::FloatingState::FloatItem::Position::Left;
        auto boxGeometry = Layout::BoxGeometry { };
        // FIXME: We are flooring here for legacy compatibility.
        //        See FloatingObjects::intervalForFloatingObject and RenderBlockFlow::clearFloats.
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
    if (m_paginatedHeight)
        return *m_paginatedHeight;
    if (!m_inlineContent)
        return { };

    auto& lines = m_inlineContent->lines;
    return LayoutUnit { lines.last().rect().maxY() - lines.first().rect().y() + m_inlineContent->clearGapAfterLastLine };
}

size_t LineLayout::lineCount() const
{
    if (!m_inlineContent)
        return 0;
    if (m_inlineContent->runs.isEmpty())
        return 0;

    return m_inlineContent->lines.size();
}

LayoutUnit LineLayout::firstLineBaseline() const
{
    if (!m_inlineContent || m_inlineContent->lines.isEmpty()) {
        ASSERT_NOT_REACHED();
        return { };
    }

    auto& firstLine = m_inlineContent->lines.first();
    return LayoutUnit { firstLine.rect().y() + firstLine.baseline() };
}

LayoutUnit LineLayout::lastLineBaseline() const
{
    if (!m_inlineContent || m_inlineContent->lines.isEmpty()) {
        ASSERT_NOT_REACHED();
        return { };
    }

    auto& lastLine = m_inlineContent->lines.last();
    return LayoutUnit { lastLine.rect().y() + lastLine.baseline() };
}

void LineLayout::adjustForPagination()
{
    auto paginedInlineContent = adjustLinePositionsForPagination(*m_inlineContent, flow());
    if (paginedInlineContent.ptr() == m_inlineContent) {
        m_paginatedHeight = { };
        return;
    }

    auto& lines = paginedInlineContent->lines;
    m_paginatedHeight = LayoutUnit { lines.last().rect().maxY() - lines.first().rect().y() };

    m_inlineContent = WTFMove(paginedInlineContent);
}

void LineLayout::collectOverflow()
{
    for (auto& line : inlineContent()->lines) {
        flow().addLayoutOverflow(Layout::toLayoutRect(line.scrollableOverflow()));
        if (!flow().hasOverflowClip())
            flow().addVisualOverflow(Layout::toLayoutRect(line.inkOverflow()));
    }
}

InlineContent& LineLayout::ensureInlineContent()
{
    if (!m_inlineContent)
        m_inlineContent = InlineContent::create(*this);
    return *m_inlineContent;
}

TextRunIterator LineLayout::textRunsFor(const RenderText& renderText) const
{
    if (!m_inlineContent)
        return { };
    auto& layoutBox = m_boxTree.layoutBoxForRenderer(renderText);

    auto firstIndex = [&]() -> Optional<size_t> {
        for (size_t i = 0; i < m_inlineContent->runs.size(); ++i) {
            if (&m_inlineContent->runs[i].layoutBox() == &layoutBox)
                return i;
        }
        return { };
    }();

    if (!firstIndex)
        return { };

    return { RunIteratorModernPath(*m_inlineContent, *firstIndex) };
}

RunIterator LineLayout::runFor(const RenderElement& renderElement) const
{
    if (!m_inlineContent)
        return { };
    auto& layoutBox = m_boxTree.layoutBoxForRenderer(renderElement);

    for (size_t i = 0; i < m_inlineContent->runs.size(); ++i) {
        auto& run =  m_inlineContent->runs[i];
        if (&run.layoutBox() == &layoutBox)
            return { RunIteratorModernPath(*m_inlineContent, i) };
    }

    return { };
}

LineIterator LineLayout::firstLine() const
{
    if (!m_inlineContent)
        return { };

    return { LineIteratorModernPath(*m_inlineContent, 0) };
}

LineIterator LineLayout::lastLine() const
{
    if (!m_inlineContent)
        return { };

    return { LineIteratorModernPath(*m_inlineContent, m_inlineContent->lines.isEmpty() ? 0 : m_inlineContent->lines.size() - 1) };
}

const RenderObject& LineLayout::rendererForLayoutBox(const Layout::Box& layoutBox) const
{
    return m_boxTree.rendererForLayoutBox(layoutBox);
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
    if (!m_inlineContent)
        return;

    if (paintInfo.phase != PaintPhase::Foreground && paintInfo.phase != PaintPhase::EventRegion)
        return;

    auto& inlineContent = *m_inlineContent;
    float deviceScaleFactor = flow().document().deviceScaleFactor();

    auto paintRect = paintInfo.rect;
    paintRect.moveBy(-paintOffset);

    for (auto& run : inlineContent.runsForRect(paintRect)) {
        if (!run.textContent()) {
            auto& renderer = m_boxTree.rendererForLayoutBox(run.layoutBox());
            if (renderer.isReplaced() && is<RenderBox>(renderer)) {
                auto& renderBox = downcast<RenderBox>(renderer);
                if (renderBox.hasSelfPaintingLayer())
                    continue;
                if (!paintInfo.shouldPaintWithinRoot(renderBox))
                    continue;
                renderBox.paintAsInlineBlock(paintInfo, paintOffset);
            }
            continue;
        }

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
        // TextRun expects the xPos to be adjusted with the aligment offset (e.g. when the line is center aligned
        // and the run starts at 100px, due to the horizontal aligment, the xpos is supposed to be at 0px).
        auto xPos = rect.x() - (line.rect().x() + line.horizontalAlignmentOffset());
        WebCore::TextRun textRun { textContent.renderedContent(), xPos, expansion.horizontalExpansion, expansion.behavior };
        textRun.setTabSize(!style.collapseWhiteSpace(), style.tabSize());
        FloatPoint textOrigin { rect.x() + paintOffset.x(), roundToDevicePixel(baseline, deviceScaleFactor) };

        TextPainter textPainter(paintInfo.context());
        textPainter.setFont(style.fontCascade());
        textPainter.setStyle(computeTextPaintStyle(flow().frame(), style, paintInfo));
        if (auto* debugShadow = debugTextShadow())
            textPainter.setShadow(debugShadow);

        textPainter.setGlyphDisplayListIfNeeded(run, paintInfo, style.fontCascade(), paintInfo.context(), textRun);
        textPainter.paint(textRun, rect, textOrigin);

        if (!style.textDecorationsInEffect().isEmpty()) {
            // FIXME: Use correct RenderText.
            if (auto* textRenderer = childrenOfType<RenderText>(flow()).first()) {
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

    if (!m_inlineContent)
        return false;

    auto& inlineContent = *m_inlineContent;

    // FIXME: This should do something efficient to find the run range.
    for (auto& run : inlineContent.runs) {
        auto runRect = Layout::toLayoutRect(run.rect());
        runRect.moveBy(accumulatedOffset);

        if (!locationInContainer.intersects(runRect))
            continue;

        auto& style = run.style();
        if (style.visibility() != Visibility::Visible || style.pointerEvents() == PointerEvents::None)
            continue;

        auto& renderer = m_boxTree.rendererForLayoutBox(run.layoutBox());

        if (is<RenderText>(renderer)) {
            renderer.updateHitTestResult(result, locationInContainer.point() - toLayoutSize(accumulatedOffset));
            if (result.addNodeToListBasedTestResult(renderer.nodeForHitTest(), request, locationInContainer, runRect) == HitTestProgress::Stop)
                return true;
            continue;
        }

        if (is<RenderBox>(renderer)) {
            auto& renderBox = downcast<RenderBox>(renderer);
            if (renderBox.hasSelfPaintingLayer())
                continue;
            
            if (renderBox.hitTest(request, result, locationInContainer, accumulatedOffset)) {
                renderBox.updateHitTestResult(result, locationInContainer.point() - toLayoutSize(accumulatedOffset));
                return true;
            }
        }
    }

    return false;
}

ShadowData* LineLayout::debugTextShadow()
{
    if (!flow().settings().simpleLineLayoutDebugBordersEnabled())
        return nullptr;

    static NeverDestroyed<ShadowData> debugTextShadow(IntPoint(0, 0), 10, 20, ShadowStyle::Normal, true, SRGBA<uint8_t> { 0, 0, 150, 150 });
    return &debugTextShadow.get();
}

void LineLayout::releaseCaches(RenderView& view)
{
    if (!RuntimeEnabledFeatures::sharedFeatures().layoutFormattingContextIntegrationEnabled())
        return;

    for (auto& renderer : descendantsOfType<RenderBlockFlow>(view)) {
        if (auto* lineLayout = renderer.modernLineLayout())
            lineLayout->releaseInlineItemCache();
    }
}

void LineLayout::releaseInlineItemCache()
{
    m_inlineFormattingState.inlineItems().clear();
}

#if ENABLE(TREE_DEBUGGING)
void LineLayout::outputLineTree(WTF::TextStream& stream, size_t depth) const
{
    showInlineTreeAndRuns(stream, m_layoutState, rootLayoutBox(), depth);
}
#endif

}
}

#endif
