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

    // Scrollbars eat into the padding box area. They never stretch the border box but they may shrink the padding box.
    // In legacy render tree, RenderBox::contentWidth/contentHeight values are adjusted to accomodate the scrollbar width/height.
    // e.g. <div style="width: 10px; overflow: scroll;">content</div>, RenderBox::contentWidth() won't be returning the value of 10px but instead 0px (10px - 15px).
    auto horizontalSpaceReservedForScrollbar = replacedOrInlineBlock.paddingBoxRectIncludingScrollbar().width() - replacedOrInlineBlock.paddingBoxWidth();
    replacedBoxGeometry.setHorizontalSpaceForScrollbar(horizontalSpaceReservedForScrollbar);

    auto verticalSpaceReservedForScrollbar = replacedOrInlineBlock.paddingBoxRectIncludingScrollbar().height() - replacedOrInlineBlock.paddingBoxHeight();
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
    auto& rootLayoutBox = this->rootLayoutBox();
    if (!rootLayoutBox.hasInFlowOrFloatingChild())
        return;

    prepareLayoutState();
    prepareFloatingState();

    m_inlineContent = nullptr;
    auto& rootGeometry = m_layoutState.geometryForBox(rootLayoutBox);
    auto inlineFormattingContext = Layout::InlineFormattingContext { rootLayoutBox, m_inlineFormattingState };

    auto invalidationState = Layout::InvalidationState { };
    auto horizontalConstraints = Layout::HorizontalConstraints { rootGeometry.contentBoxLeft(), rootGeometry.contentBoxWidth() };

    inlineFormattingContext.lineLayoutForIntergration(invalidationState, { horizontalConstraints, rootGeometry.contentBoxTop() });

    constructContent();
}

void LineLayout::constructContent()
{
    auto inlineContentBuilder = InlineContentBuilder { m_layoutState, flow(), m_boxTree };
    inlineContentBuilder.build(m_inlineFormattingState, ensureInlineContent());
    ASSERT(m_inlineContent);

    auto& boxAndRendererList = m_boxTree.boxAndRendererList();
    for (auto& boxAndRenderer : boxAndRendererList) {
        auto& layoutBox = *boxAndRenderer.box;
        if (!layoutBox.isReplacedBox())
            continue;

        auto& renderer = downcast<RenderBox>(*boxAndRenderer.renderer);
        renderer.setLocation(Layout::BoxGeometry::borderBoxTopLeft(m_inlineFormattingState.boxGeometry(layoutBox)));
    }

    m_inlineContent->clearGapAfterLastLine = m_inlineFormattingState.clearGapAfterLastLine();
    m_inlineContent->shrinkToFit();
    m_inlineFormattingState.shrinkToFit();
}

void LineLayout::prepareLayoutState()
{
    auto& flow = this->flow();
    m_layoutState.setViewportSize(flow.frame().view()->size());

    auto& rootGeometry = m_layoutState.ensureGeometryForBox(rootLayoutBox());
    rootGeometry.setContentBoxWidth(flow.contentSize().width());
    rootGeometry.setPadding(Layout::Edges { { flow.paddingStart(), flow.paddingEnd() }, { flow.paddingBefore(), flow.paddingAfter() } });
    rootGeometry.setBorder(Layout::Edges { { flow.borderStart(), flow.borderEnd() }, { flow.borderBefore(), flow.borderAfter() } });
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
        if (!flow().hasNonVisibleOverflow())
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

    auto firstIndex = [&]() -> std::optional<size_t> {
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

    auto paintRect = paintInfo.rect;
    paintRect.moveBy(-paintOffset);

    for (auto& run : m_inlineContent->runsForRect(paintRect)) {
        if (run.text())
            paintTextRunUsingPhysicalCoordinates(paintInfo, paintOffset, m_inlineContent->lineForRun(run), run);
        else if (auto& renderer = m_boxTree.rendererForLayoutBox(run.layoutBox()); is<RenderBox>(renderer) && renderer.isReplaced()) {
            auto& renderBox = downcast<RenderBox>(renderer);
            if (!renderBox.hasSelfPaintingLayer() && paintInfo.shouldPaintWithinRoot(renderBox))
                renderBox.paintAsInlineBlock(paintInfo, paintOffset);
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
        auto& renderer = m_boxTree.rendererForLayoutBox(run.layoutBox());

        if (is<RenderText>(renderer)) {
            auto runRect = Layout::toLayoutRect(run.logicalRect());
            runRect.moveBy(accumulatedOffset);

            if (!locationInContainer.intersects(runRect))
                continue;
            
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

void LineLayout::paintTextRunUsingPhysicalCoordinates(PaintInfo& paintInfo, const LayoutPoint& paintOffset, const Line& line, const Run& run)
{
    auto& style = run.style();
    if (run.style().visibility() != Visibility::Visible)
        return;

    auto& textContent = *run.text();
    if (!textContent.length())
        return;

    auto& formattingContextRoot = flow();
    auto blockIsHorizontalWriting = formattingContextRoot.style().isHorizontalWritingMode();
    auto physicalPaintOffset = paintOffset;
    if (!blockIsHorizontalWriting) {
        // FIXME: Figure out why this translate is required.
        physicalPaintOffset.move({ 0, -run.logicalRect().height() });
    }

    auto physicalRect = [&](const auto& rect) {
        if (!style.isFlippedBlocksWritingMode())
            return rect;
        if (!blockIsHorizontalWriting)
            return FloatRect { formattingContextRoot.width() - rect.maxY(), rect.x() , rect.width(), rect.height() };
        ASSERT_NOT_IMPLEMENTED_YET();
        return rect;
    };

    auto visualOverflowRect = physicalRect(run.inkOverflow());

    auto damagedRect = paintInfo.rect;
    damagedRect.moveBy(-physicalPaintOffset);
    if (damagedRect.y() > visualOverflowRect.maxY() || damagedRect.maxY() < visualOverflowRect.y())
        return;

    if (paintInfo.eventRegionContext) {
        if (style.pointerEvents() != PointerEvents::None) {
            visualOverflowRect.moveBy(physicalPaintOffset);
            paintInfo.eventRegionContext->unite(enclosingIntRect(visualOverflowRect), style);
        }
        return;
    }

    auto& paintContext = paintInfo.context();
    auto expansion = run.expansion();
    // TextRun expects the xPos to be adjusted with the aligment offset (e.g. when the line is center aligned
    // and the run starts at 100px, due to the horizontal aligment, the xpos is supposed to be at 0px).
    auto& fontCascade = style.fontCascade();
    auto xPos = run.logicalRect().x() - (line.lineBoxLeft() + line.contentLeft());
    auto textRun = WebCore::TextRun { textContent.renderedContent(), xPos, expansion.horizontalExpansion, expansion.behavior };
    textRun.setTabSize(!style.collapseWhiteSpace(), style.tabSize());

    auto textPainter = TextPainter { paintContext };
    textPainter.setFont(fontCascade);
    textPainter.setStyle(computeTextPaintStyle(formattingContextRoot.frame(), style, paintInfo));
    textPainter.setGlyphDisplayListIfNeeded(run, paintInfo, fontCascade, paintContext, textRun);

    // Painting uses only physical coordinates.
    {
        auto runRect = physicalRect(run.logicalRect());
        auto boxRect = FloatRect { FloatPoint { physicalPaintOffset.x() + runRect.x(), physicalPaintOffset.y() + runRect.y() }, runRect.size() };
        auto textOrigin = FloatPoint { boxRect.x(), roundToDevicePixel(boxRect.y() + fontCascade.fontMetrics().ascent(), formattingContextRoot.document().deviceScaleFactor()) };

        auto shouldRotate = !blockIsHorizontalWriting;
        if (shouldRotate)
            paintContext.concatCTM(rotation(boxRect, Clockwise));
        textPainter.paint(textRun, runRect, textOrigin);

        if (!style.textDecorationsInEffect().isEmpty()) {
            auto& textRenderer = downcast<RenderText>(m_boxTree.rendererForLayoutBox(run.layoutBox()));
            auto decorationPainter = TextDecorationPainter { paintContext, style.textDecorationsInEffect(), textRenderer, false, fontCascade };
            decorationPainter.setTextRunIterator(m_inlineContent->iteratorForTextRun(run));
            decorationPainter.setWidth(runRect.width());
            decorationPainter.paintTextDecoration(textRun, textOrigin, runRect.location() + physicalPaintOffset);
        }
        if (shouldRotate)
            paintInfo.context().concatCTM(rotation(boxRect, Counterclockwise));
    }
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
