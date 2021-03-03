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
#include "LayoutIntegrationInlineContentBuilder.h"
#include "LayoutIntegrationPagination.h"
#include "LayoutReplacedBox.h"
#include "LayoutTreeBuilder.h"
#include "PaintInfo.h"
#include "RenderBlockFlow.h"
#include "RenderChildIterator.h"
#include "RenderDescendantIterator.h"
#include "RenderImage.h"
#include "RenderInline.h"
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

RenderBlockFlow* LineLayout::blockContainer(RenderObject& renderer)
{
    // FIXME: These fake renderers have their parent set but are not actually in the tree.
    if (renderer.isReplica() || renderer.isRenderScrollbarPart())
        return nullptr;
    
    for (auto* parent = renderer.parent(); parent; parent = parent->parent()) {
        if (!parent->childrenInline())
            return nullptr;
        if (is<RenderBlockFlow>(*parent))
            return downcast<RenderBlockFlow>(parent);
    }

    return nullptr;
}

LineLayout* LineLayout::containing(RenderObject& renderer)
{
    if (!renderer.isInline())
        return nullptr;

    if (auto* container = blockContainer(renderer))
        return container->modernLineLayout();

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

bool LineLayout::shouldSwitchToLegacyOnInvalidation() const
{
    // FIXME: Support partial invalidation in LFC.
    // This avoids O(n^2) when lots of boxes are being added dynamically while forcing layouts between.
    constexpr size_t maximimumBoxTreeSizeForInvalidation = 128;
    return m_boxTree.boxCount() > maximimumBoxTreeSizeForInvalidation;
}

void LineLayout::updateReplacedDimensions(const RenderBox& replaced)
{
    updateLayoutBoxDimensions(replaced);
}

void LineLayout::updateInlineBlockDimensions(const RenderBlock& inlineBlock)
{
    updateLayoutBoxDimensions(inlineBlock);
}

void LineLayout::updateLayoutBoxDimensions(const RenderBox& replacedOrInlineBlock)
{
    auto& layoutBox = m_boxTree.layoutBoxForRenderer(replacedOrInlineBlock);
    // Internally both replaced and inline-box content use replaced boxes.
    auto& replacedBox = downcast<Layout::ReplacedBox>(layoutBox);

    // Always use the physical size here for inline level boxes (this is where the logical vs. physical coords flip happens).
    auto& replacedBoxGeometry = m_layoutState.ensureGeometryForBox(replacedBox);
    // Scrollbars are placed "between" the border and the padding box and they never stretch the border box. They may shrink the padding box though.
    auto horizontalSpaceReservedForScrollbar = std::min(replacedOrInlineBlock.width() - replacedOrInlineBlock.paddingBoxWidth(), LayoutUnit(replacedOrInlineBlock.verticalScrollbarWidth()));
    replacedBoxGeometry.setHorizontalSpaceForScrollbar(horizontalSpaceReservedForScrollbar);

    auto verticalSpaceReservedForScrollbar = std::min(replacedOrInlineBlock.height() - replacedOrInlineBlock.paddingBoxHeight(), LayoutUnit(replacedOrInlineBlock.horizontalScrollbarHeight()));
    replacedBoxGeometry.setVerticalSpaceForScrollbar(verticalSpaceReservedForScrollbar);

    replacedBoxGeometry.setContentBoxWidth(replacedOrInlineBlock.contentWidth());
    replacedBoxGeometry.setContentBoxHeight(replacedOrInlineBlock.contentHeight());

    replacedBoxGeometry.setBorder({ { replacedOrInlineBlock.borderLeft(), replacedOrInlineBlock.borderRight() }, { replacedOrInlineBlock.borderTop(), replacedOrInlineBlock.borderBottom() } });
    replacedBoxGeometry.setPadding(Layout::Edges { { replacedOrInlineBlock.paddingLeft(), replacedOrInlineBlock.paddingRight() }, { replacedOrInlineBlock.paddingTop(), replacedOrInlineBlock.paddingBottom() } });

    replacedBoxGeometry.setHorizontalMargin({ replacedOrInlineBlock.marginLeft(), replacedOrInlineBlock.marginRight() });
    replacedBoxGeometry.setVerticalMargin({ replacedOrInlineBlock.marginTop(), replacedOrInlineBlock.marginBottom() });

    auto baseline = replacedOrInlineBlock.baselinePosition(AlphabeticBaseline, false /* firstLine */, HorizontalLine, PositionOnContainingLine);
    replacedBox.setBaseline(roundToInt(baseline));
}

void LineLayout::updateLineBreakBoxDimensions(const RenderLineBreak& lineBreakBox)
{
    // This is just a box geometry reset (see InlineFormattingContext::layoutInFlowContent).
    auto& boxGeometry = m_layoutState.ensureGeometryForBox(m_boxTree.layoutBoxForRenderer(lineBreakBox));

    boxGeometry.setHorizontalMargin({ });
    boxGeometry.setBorder({ });
    boxGeometry.setPadding({ });
    boxGeometry.setContentBoxWidth({ });
    boxGeometry.setVerticalMargin({ });
}

void LineLayout::updateInlineBoxDimensions(const RenderInline& renderInline)
{
    auto& boxGeometry = m_layoutState.ensureGeometryForBox(m_boxTree.layoutBoxForRenderer(renderInline));

    // Check if this renderer is part of a continuation and adjust horizontal margin/border/padding accordingly.
    auto shouldNotRetainBorderPaddingAndMarginStart = renderInline.parent()->isAnonymousBlock() && renderInline.isContinuation();
    auto shouldNotRetainBorderPaddingAndMarginEnd = renderInline.parent()->isAnonymousBlock() && !renderInline.isContinuation() && renderInline.inlineContinuation();
    
    auto horizontalMargin = Layout::BoxGeometry::HorizontalMargin { shouldNotRetainBorderPaddingAndMarginStart ? 0_lu : renderInline.marginLeft(), shouldNotRetainBorderPaddingAndMarginEnd ? 0_lu : renderInline.marginRight() };
    auto horizontalBorder = Layout::HorizontalEdges { shouldNotRetainBorderPaddingAndMarginStart ? 0_lu : renderInline.borderLeft(), shouldNotRetainBorderPaddingAndMarginEnd ? 0_lu : renderInline.borderRight() };
    auto horizontalPadding = Layout::HorizontalEdges { shouldNotRetainBorderPaddingAndMarginStart ? 0_lu : renderInline.paddingLeft(), shouldNotRetainBorderPaddingAndMarginEnd ? 0_lu : renderInline.paddingRight() };
    
    boxGeometry.setPadding(Layout::Edges { horizontalPadding, { renderInline.paddingTop(), renderInline.paddingBottom() } });
    boxGeometry.setBorder({ horizontalBorder, { renderInline.borderTop(), renderInline.borderBottom() } });
    boxGeometry.setHorizontalMargin(horizontalMargin);
    boxGeometry.setVerticalMargin({ });
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

    inlineFormattingContext.lineLayoutForIntergration(invalidationState, { horizontalConstraints, verticalConstraints });

    constructContent();
}

void LineLayout::constructContent()
{
    auto inlineFormattingContext = Layout::InlineFormattingContext { rootLayoutBox(), m_inlineFormattingState };

    auto inlineContentBuilder = InlineContentBuilder { m_layoutState, flow(), m_boxTree };
    inlineContentBuilder.build(inlineFormattingContext, ensureInlineContent());
    ASSERT(m_inlineContent);

    for (auto& run : m_inlineContent->runs) {
        auto& layoutBox = run.layoutBox();
        if (!layoutBox.isReplacedBox())
            continue;

        auto& renderer = downcast<RenderBox>(m_boxTree.rendererForLayoutBox(layoutBox));
        renderer.setLocation(flooredLayoutPoint(run.rect().location()));
    }

    m_inlineContent->clearGapAfterLastLine = m_inlineFormattingState.clearGapAfterLastLine();
    m_inlineContent->shrinkToFit();
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
    return LayoutUnit { lines.last().lineBoxBottom() - lines.first().lineBoxTop() + m_inlineContent->clearGapAfterLastLine };
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
    return LayoutUnit { firstLine.lineBoxTop() + firstLine.baseline() };
}

LayoutUnit LineLayout::lastLineBaseline() const
{
    if (!m_inlineContent || m_inlineContent->lines.isEmpty()) {
        ASSERT_NOT_REACHED();
        return { };
    }

    auto& lastLine = m_inlineContent->lines.last();
    return LayoutUnit { lastLine.lineBoxTop() + lastLine.baseline() };
}

void LineLayout::adjustForPagination()
{
    auto paginedInlineContent = adjustLinePositionsForPagination(*m_inlineContent, flow());
    if (paginedInlineContent.ptr() == m_inlineContent) {
        m_paginatedHeight = { };
        return;
    }

    auto& lines = paginedInlineContent->lines;
    m_paginatedHeight = LayoutUnit { lines.last().lineBoxBottom() - lines.first().lineBoxTop() };

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

LayoutRect LineLayout::enclosingBorderBoxRectFor(const RenderInline& renderInline) const
{
    if (!m_inlineContent)
        return { };

    if (m_inlineContent->runs.isEmpty())
        return { };

    return Layout::BoxGeometry::borderBoxRect(m_inlineFormattingState.boxGeometry(m_boxTree.layoutBoxForRenderer(renderInline)));
}

LayoutRect LineLayout::visualOverflowBoundingBoxRectFor(const RenderInline& renderInline) const
{
    // FIXME: This doesn't contain overflow.
    return enclosingBorderBoxRectFor(renderInline);
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
            if (style.pointerEvents() != PointerEvents::None) {
                visualOverflowRect.moveBy(paintOffset);
                paintInfo.eventRegionContext->unite(enclosingIntRect(visualOverflowRect), style);
            }
            continue;
        }

        auto& line = inlineContent.lineForRun(run);
        auto expansion = run.expansion();
        // TextRun expects the xPos to be adjusted with the aligment offset (e.g. when the line is center aligned
        // and the run starts at 100px, due to the horizontal aligment, the xpos is supposed to be at 0px).
        auto& fontCascade = style.fontCascade();
        auto xPos = rect.x() - (line.lineBoxLeft() + line.contentLeftOffset());
        WebCore::TextRun textRun { textContent.renderedContent(), xPos, expansion.horizontalExpansion, expansion.behavior };
        textRun.setTabSize(!style.collapseWhiteSpace(), style.tabSize());

        TextPainter textPainter(paintInfo.context());
        textPainter.setFont(fontCascade);
        textPainter.setStyle(computeTextPaintStyle(flow().frame(), style, paintInfo));
        textPainter.setGlyphDisplayListIfNeeded(run, paintInfo, fontCascade, paintInfo.context(), textRun);

        auto textOrigin = FloatPoint { paintOffset.x() + rect.x(), roundToDevicePixel(paintOffset.y() + rect.y() + fontCascade.fontMetrics().ascent(), deviceScaleFactor) };
        textPainter.paint(textRun, rect, textOrigin);

        if (!style.textDecorationsInEffect().isEmpty()) {
            auto& textRenderer = downcast<RenderText>(m_boxTree.rendererForLayoutBox(run.layoutBox()));
            auto painter = TextDecorationPainter { paintInfo.context(), style.textDecorationsInEffect(), textRenderer, false, fontCascade };
            painter.setWidth(rect.width());
            painter.paintTextDecoration(textRun, textOrigin, rect.location() + paintOffset);
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
    for (auto& run : WTF::makeReversedRange(inlineContent.runs)) {
        auto runRect = Layout::toLayoutRect(run.rect());
        runRect.moveBy(accumulatedOffset);

        if (!locationInContainer.intersects(runRect))
            continue;

        auto& renderer = m_boxTree.rendererForLayoutBox(run.layoutBox());

        if (is<RenderText>(renderer)) {
            auto& style = run.style();
            if (style.visibility() != Visibility::Visible || style.pointerEvents() == PointerEvents::None)
                continue;

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

    for (auto& inlineBox : WTF::makeReversedRange(inlineContent.nonRootInlineBoxes)) {
        auto inlineBoxRect = Layout::toLayoutRect(inlineBox.rect());
        inlineBoxRect.moveBy(accumulatedOffset);

        if (!locationInContainer.intersects(inlineBoxRect))
            continue;

        auto& style = inlineBox.style();
        if (style.visibility() != Visibility::Visible || style.pointerEvents() == PointerEvents::None)
            continue;

        auto& renderer = m_boxTree.rendererForLayoutBox(inlineBox.layoutBox());

        renderer.updateHitTestResult(result, locationInContainer.point() - toLayoutSize(accumulatedOffset));
        if (result.addNodeToListBasedTestResult(renderer.nodeForHitTest(), request, locationInContainer, inlineBoxRect) == HitTestProgress::Stop)
            return true;
    }

    return false;
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
