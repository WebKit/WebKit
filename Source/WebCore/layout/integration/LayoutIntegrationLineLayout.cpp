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
#include "InlineBoxPainter.h"
#include "InlineDamage.h"
#include "InlineFormattingContext.h"
#include "InlineFormattingState.h"
#include "InlineInvalidation.h"
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
#include "TextBoxPainter.h"
#include "TextDecorationPainter.h"

namespace WebCore {
namespace LayoutIntegration {

LineLayout::LineLayout(RenderBlockFlow& flow)
    : m_boxTree(flow)
    , m_layoutState(flow.document(), rootLayoutBox())
    , m_inlineFormattingState(m_layoutState.ensureInlineFormattingState(rootLayoutBox()))
{
    m_layoutState.setIsIntegratedRootBoxFirstChild(flow.parent()->firstChild() == &flow);
}

LineLayout::~LineLayout()
{
    clearInlineContent();
}

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

bool LineLayout::canUseForAfterInlineBoxStyleChange(const RenderInline& inlineBox, StyleDifference diff)
{
    ASSERT(isEnabled());
    return canUseForLineLayoutAfterInlineBoxStyleChange(inlineBox, diff);
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

static inline LayoutUnit contentLogicalWidthForRenderer(const RenderBox& renderer)
{
    return renderer.parent()->style().isHorizontalWritingMode() ? renderer.contentWidth() : renderer.contentHeight();
}

static inline LayoutUnit contentLogicalHeightForRenderer(const RenderBox& renderer)
{
    return renderer.parent()->style().isHorizontalWritingMode() ? renderer.contentHeight() : renderer.contentWidth();
}

static inline Layout::BoxGeometry::HorizontalMargin logicalMargin(const RenderBoxModelObject& renderer, bool isLeftToRightDirection, bool retainMarginStart = true, bool retainMarginEnd = true)
{
    auto marginStart = LayoutUnit { 0_lu };
    auto marginEnd = LayoutUnit { 0_lu };
    if (retainMarginStart)
        marginStart = isLeftToRightDirection ? renderer.marginLeft() : renderer.marginRight();
    if (retainMarginEnd)
        marginEnd = isLeftToRightDirection ? renderer.marginRight() : renderer.marginLeft();
    return { marginStart, marginEnd };
}

static inline Layout::Edges logicalBorder(const RenderBoxModelObject& renderer, bool isLeftToRightDirection, bool retainBorderStart = true, bool retainBorderEnd = true)
{
    auto borderStart = LayoutUnit { 0_lu };
    auto borderEnd = LayoutUnit { 0_lu };
    if (retainBorderStart)
        borderStart = isLeftToRightDirection ? renderer.borderLeft() : renderer.borderRight();
    if (retainBorderEnd)
        borderEnd = isLeftToRightDirection ? renderer.borderRight() : renderer.borderLeft();
    return { { borderStart, borderEnd }, { renderer.borderTop(), renderer.borderBottom() } };
}

static inline Layout::Edges logicalPadding(const RenderBoxModelObject& renderer, bool isLeftToRightDirection, bool retainPaddingStart = true, bool retainPaddingEnd = true)
{
    auto paddingStart = LayoutUnit { 0_lu };
    auto paddingEnd = LayoutUnit { 0_lu };
    if (retainPaddingStart)
        paddingStart = isLeftToRightDirection ? renderer.paddingLeft() : renderer.paddingRight();
    if (retainPaddingEnd)
        paddingEnd = isLeftToRightDirection ? renderer.paddingRight() : renderer.paddingLeft();
    return Layout::Edges { { paddingStart, paddingEnd }, { renderer.paddingTop(), renderer.paddingBottom() } };
}

void LineLayout::updateLayoutBoxDimensions(const RenderBox& replacedOrInlineBlock)
{
    auto& layoutBox = m_boxTree.layoutBoxForRenderer(replacedOrInlineBlock);
    // Internally both replaced and inline-box content use replaced boxes.
    auto& replacedBox = downcast<Layout::ReplacedBox>(layoutBox);

    auto& replacedBoxGeometry = m_layoutState.ensureGeometryForBox(replacedBox);
    // Scrollbars eat into the padding box area. They never stretch the border box but they may shrink the padding box.
    // In legacy render tree, RenderBox::contentWidth/contentHeight values are adjusted to accomodate the scrollbar width/height.
    // e.g. <div style="width: 10px; overflow: scroll;">content</div>, RenderBox::contentWidth() won't be returning the value of 10px but instead 0px (10px - 15px).
    auto horizontalSpaceReservedForScrollbar = replacedOrInlineBlock.paddingBoxRectIncludingScrollbar().width() - replacedOrInlineBlock.paddingBoxWidth();
    replacedBoxGeometry.setHorizontalSpaceForScrollbar(horizontalSpaceReservedForScrollbar);

    auto verticalSpaceReservedForScrollbar = replacedOrInlineBlock.paddingBoxRectIncludingScrollbar().height() - replacedOrInlineBlock.paddingBoxHeight();
    replacedBoxGeometry.setVerticalSpaceForScrollbar(verticalSpaceReservedForScrollbar);

    replacedBoxGeometry.setContentBoxWidth(contentLogicalWidthForRenderer(replacedOrInlineBlock));
    replacedBoxGeometry.setContentBoxHeight(contentLogicalHeightForRenderer(replacedOrInlineBlock));

    replacedBoxGeometry.setVerticalMargin({ replacedOrInlineBlock.marginTop(), replacedOrInlineBlock.marginBottom() });
    auto isLeftToRightDirection = replacedOrInlineBlock.parent()->style().isLeftToRightDirection();
    replacedBoxGeometry.setHorizontalMargin(logicalMargin(replacedOrInlineBlock, isLeftToRightDirection));
    replacedBoxGeometry.setBorder(logicalBorder(replacedOrInlineBlock, isLeftToRightDirection));
    replacedBoxGeometry.setPadding(logicalPadding(replacedOrInlineBlock, isLeftToRightDirection));

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
    auto shouldNotRetainBorderPaddingAndMarginStart = renderInline.isContinuation();
    auto shouldNotRetainBorderPaddingAndMarginEnd = !renderInline.isContinuation() && renderInline.inlineContinuation();

    boxGeometry.setVerticalMargin({ });
    auto isLeftToRightDirection = renderInline.style().isLeftToRightDirection();
    boxGeometry.setHorizontalMargin(logicalMargin(renderInline, isLeftToRightDirection, !shouldNotRetainBorderPaddingAndMarginStart, !shouldNotRetainBorderPaddingAndMarginEnd));
    boxGeometry.setBorder(logicalBorder(renderInline, isLeftToRightDirection, !shouldNotRetainBorderPaddingAndMarginStart, !shouldNotRetainBorderPaddingAndMarginEnd));
    boxGeometry.setPadding(logicalPadding(renderInline, isLeftToRightDirection, !shouldNotRetainBorderPaddingAndMarginStart, !shouldNotRetainBorderPaddingAndMarginEnd));
}

void LineLayout::updateStyle(const RenderBoxModelObject& renderer, const RenderStyle& oldStyle)
{
    auto invalidation = Layout::InlineInvalidation { ensureLineDamage() };
    invalidation.styleChanged(m_boxTree.layoutBoxForRenderer(renderer), oldStyle);

    m_boxTree.updateStyle(renderer);
}

std::pair<LayoutUnit, LayoutUnit> LineLayout::computeIntrinsicWidthConstraints()
{
    auto inlineFormattingContext = Layout::InlineFormattingContext { rootLayoutBox(), m_inlineFormattingState, nullptr };
    auto constraints = inlineFormattingContext.computedIntrinsicWidthConstraintsForIntegration();

    return { constraints.minimum, constraints.maximum };
}

void LineLayout::layout()
{
    auto& rootLayoutBox = this->rootLayoutBox();
    if (!rootLayoutBox.hasInFlowOrFloatingChild())
        return;

    prepareLayoutState();
    updateFormattingRootGeometryAndInvalidate();
    prepareFloatingState();

    // FIXME: Do not clear the lines and boxes here unconditionally, but consult with the damage object instead.
    clearInlineContent();

    auto& rootGeometry = m_layoutState.geometryForBox(rootLayoutBox);
    auto inlineFormattingContext = Layout::InlineFormattingContext { rootLayoutBox, m_inlineFormattingState, m_lineDamage.get() };

    auto horizontalConstraints = Layout::HorizontalConstraints { rootGeometry.contentBoxLeft(), rootGeometry.contentBoxWidth() };

    inlineFormattingContext.lineLayoutForIntergration({ horizontalConstraints, rootGeometry.contentBoxTop() });

    constructContent();

    m_lineDamage = { };
}

void LineLayout::constructContent()
{
    auto inlineContentBuilder = InlineContentBuilder { flow(), m_boxTree };
    inlineContentBuilder.build(m_inlineFormattingState, ensureInlineContent());
    ASSERT(m_inlineContent);

    auto& boxAndRendererList = m_boxTree.boxAndRendererList();
    for (auto& boxAndRenderer : boxAndRendererList) {
        auto& layoutBox = boxAndRenderer.box.get();
        if (!layoutBox.isReplacedBox())
            continue;

        auto& renderer = downcast<RenderBox>(*boxAndRenderer.renderer);
        renderer.setLocation(Layout::BoxGeometry::borderBoxTopLeft(m_inlineFormattingState.boxGeometry(layoutBox)));
    }

    m_inlineContent->clearGapAfterLastLine = m_inlineFormattingState.clearGapAfterLastLine();
    m_inlineContent->shrinkToFit();
    m_inlineFormattingState.shrinkToFit();
}

void LineLayout::updateFormattingRootGeometryAndInvalidate()
{
    auto& flow = this->flow();

    auto updateGeometry = [&](auto& root) {
        root.setContentBoxWidth(flow.contentLogicalWidth());
        root.setPadding(Layout::Edges { { flow.paddingStart(), flow.paddingEnd() }, { flow.paddingBefore(), flow.paddingAfter() } });
        root.setBorder(Layout::Edges { { flow.borderStart(), flow.borderEnd() }, { flow.borderBefore(), flow.borderAfter() } });
        root.setHorizontalMargin({ });
        root.setVerticalMargin({ });
    };
    auto& rootLayoutBox = this->rootLayoutBox();
    if (!m_layoutState.hasBoxGeometry(rootLayoutBox))
        return updateGeometry(m_layoutState.ensureGeometryForBox(rootLayoutBox));

    auto& rootGeometry = m_layoutState.geometryForRootBox();
    auto newLogicalWidth = flow.contentLogicalWidth();
    if (newLogicalWidth != rootGeometry.contentBoxWidth())
        Layout::InlineInvalidation(ensureLineDamage()).horizontalConstraintChanged();
    updateGeometry(rootGeometry);
}

void LineLayout::prepareLayoutState()
{
    m_layoutState.setViewportSize(flow().frame().view()->size());
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
    if (!m_inlineContent)
        return { };

    auto& lines = m_inlineContent->lines;
    auto flippedContentHeightForWritingMode = rootLayoutBox().style().isHorizontalWritingMode()
        ? lines.last().lineBoxBottom() - lines.first().lineBoxTop()
        : lines.last().lineBoxRight() - lines.first().lineBoxLeft();
    return LayoutUnit { flippedContentHeightForWritingMode + m_inlineContent->clearGapAfterLastLine };
}

size_t LineLayout::lineCount() const
{
    if (!m_inlineContent)
        return 0;
    if (!m_inlineContent->hasContent())
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
    if (!m_inlineContent)
        return;

    auto paginedInlineContent = adjustLinePositionsForPagination(*m_inlineContent, flow());
    if (paginedInlineContent.ptr() == m_inlineContent) {
        m_isPaginatedContent = false;
        return;
    }
    m_isPaginatedContent = true;
    m_inlineContent = WTFMove(paginedInlineContent);
}

void LineLayout::collectOverflow()
{
    if (!m_inlineContent)
        return;

    for (auto& line : m_inlineContent->lines) {
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

InlineIterator::TextBoxIterator LineLayout::textBoxesFor(const RenderText& renderText) const
{
    if (!m_inlineContent)
        return { };

    auto& layoutBox = m_boxTree.layoutBoxForRenderer(renderText);
    auto firstIndex = m_inlineContent->firstBoxIndexForLayoutBox(layoutBox);
    if (!firstIndex)
        return { };

    return InlineIterator::textBoxFor(*m_inlineContent, *firstIndex);
}

InlineIterator::LeafBoxIterator LineLayout::boxFor(const RenderElement& renderElement) const
{
    if (!m_inlineContent)
        return { };

    auto& layoutBox = m_boxTree.layoutBoxForRenderer(renderElement);
    auto firstIndex = m_inlineContent->firstBoxIndexForLayoutBox(layoutBox);
    if (!firstIndex)
        return { };

    return InlineIterator::boxFor(*m_inlineContent, *firstIndex);
}

InlineIterator::InlineBoxIterator LineLayout::firstInlineBoxFor(const RenderInline& renderInline) const
{
    if (!m_inlineContent)
        return { };

    auto& layoutBox = m_boxTree.layoutBoxForRenderer(renderInline);
    auto* box = m_inlineContent->firstBoxForLayoutBox(layoutBox);
    if (!box)
        return { };

    return InlineIterator::inlineBoxFor(*m_inlineContent, *box);
}

InlineIterator::InlineBoxIterator LineLayout::firstRootInlineBox() const
{
    if (!m_inlineContent || !m_inlineContent->hasContent())
        return { };

    return InlineIterator::inlineBoxFor(*m_inlineContent, m_inlineContent->boxes[0]);
}

InlineIterator::LineIterator LineLayout::firstLine() const
{
    if (!m_inlineContent)
        return { };

    return { InlineIterator::LineIteratorModernPath(*m_inlineContent, 0) };
}

InlineIterator::LineIterator LineLayout::lastLine() const
{
    if (!m_inlineContent)
        return { };

    return { InlineIterator::LineIteratorModernPath(*m_inlineContent, m_inlineContent->lines.isEmpty() ? 0 : m_inlineContent->lines.size() - 1) };
}

LayoutRect LineLayout::firstInlineBoxRect(const RenderInline& renderInline) const
{
    auto& layoutBox = m_boxTree.layoutBoxForRenderer(renderInline);

    if (auto* box = m_inlineContent->firstBoxForLayoutBox(layoutBox))
        return Layout::toLayoutRect(box->rect());

    return { };
}

LayoutRect LineLayout::enclosingBorderBoxRectFor(const RenderInline& renderInline) const
{
    if (!m_inlineContent)
        return { };

    // FIXME: This keeps the existing output.
    if (!m_inlineContent->hasContent())
        return { };

    return Layout::BoxGeometry::borderBoxRect(m_inlineFormattingState.boxGeometry(m_boxTree.layoutBoxForRenderer(renderInline)));
}

LayoutRect LineLayout::visualOverflowBoundingBoxRectFor(const RenderInline& renderInline) const
{
    if (!m_inlineContent)
        return { };

    auto& layoutBox = m_boxTree.layoutBoxForRenderer(renderInline);

    LayoutRect result;
    m_inlineContent->traverseNonRootInlineBoxes(layoutBox, [&](auto& inlineBox) {
        result.unite(Layout::toLayoutRect(inlineBox.inkOverflow()));
    });

    return result;
}

Vector<FloatRect> LineLayout::collectInlineBoxRects(const RenderInline& renderInline) const
{
    if (!m_inlineContent)
        return { };

    auto& layoutBox = m_boxTree.layoutBoxForRenderer(renderInline);

    Vector<FloatRect> result;
    m_inlineContent->traverseNonRootInlineBoxes(layoutBox, [&](auto& inlineBox) {
        result.append(inlineBox.rect());
    });

    return result;
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

    auto paintPhase = paintInfo.phase;

    auto shouldPaintForPhase = [&] {
        switch (paintPhase) {
        case PaintPhase::Foreground:
        case PaintPhase::EventRegion:
        case PaintPhase::TextClip:
        case PaintPhase::Selection:
        case PaintPhase::Outline:
        case PaintPhase::ChildOutlines:
        case PaintPhase::SelfOutline:
            return true;
        default:
            return false;
        }
    };

    if (!shouldPaintForPhase())
        return;

    auto damageRect = paintInfo.rect;
    damageRect.moveBy(-paintOffset);

    auto hasDamage = [&](auto& box) {
        if (box.style().visibility() != Visibility::Visible)
            return false;
        auto rect = enclosingLayoutRect(box.inkOverflow());
        flow().flipForWritingMode(rect);
        // FIXME: This should test for intersection but horizontal ink overflow is miscomputed in a few cases (like with negative letter-spacing).
        return damageRect.maxY() > rect.y() && damageRect.y() < rect.maxY();
    };

    auto shouldPaintBoxForPhase = [&](auto& box) {
        switch (paintPhase) {
        case PaintPhase::ChildOutlines: return box.isNonRootInlineBox();
        case PaintPhase::SelfOutline: return box.isRootInlineBox();
        case PaintPhase::Outline: return box.isInlineBox();
        default: return true;
        }
    };

    ListHashSet<RenderInline*> outlineObjects;

    for (auto& box : m_inlineContent->boxesForRect(damageRect)) {
        if (!shouldPaintBoxForPhase(box))
            continue;

        if (box.isLineBreak())
            continue;

        if (box.isInlineBox()) {
            if (!hasDamage(box))
                continue;

            PaintInfo inlineBoxPaintInfo(paintInfo);
            inlineBoxPaintInfo.phase = paintPhase == PaintPhase::ChildOutlines ? PaintPhase::Outline : paintPhase;
            inlineBoxPaintInfo.outlineObjects = &outlineObjects;

            InlineBoxPainter painter(*m_inlineContent, box, inlineBoxPaintInfo, paintOffset);
            painter.paint();
            continue;
        }

        if (box.text()) {
            if (!box.text()->length() || !hasDamage(box))
                continue;

            TextBoxPainter painter(*m_inlineContent, box, paintInfo, paintOffset);
            painter.paint();
            continue;
        }

        if (auto& renderer = m_boxTree.rendererForLayoutBox(box.layoutBox()); is<RenderBox>(renderer) && renderer.isReplacedOrInlineBlock()) {
            auto& renderBox = downcast<RenderBox>(renderer);
            if (!renderBox.hasSelfPaintingLayer() && paintInfo.shouldPaintWithinRoot(renderBox))
                renderBox.paintAsInlineBlock(paintInfo, paintOffset);
        }
    }

    for (auto* renderInline : outlineObjects)
        renderInline->paintOutline(paintInfo, paintOffset);
}

bool LineLayout::hitTest(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction hitTestAction)
{
    if (hitTestAction != HitTestForeground)
        return false;

    if (!m_inlineContent)
        return false;

    auto hitTestBoundingBox = locationInContainer.boundingBox();
    hitTestBoundingBox.moveBy(-accumulatedOffset);
    auto boxRange = m_inlineContent->boxesForRect(hitTestBoundingBox);

    for (auto& box : makeReversedRange(boxRange)) {
        auto& renderer = m_boxTree.rendererForLayoutBox(box.layoutBox());

        if (!box.isRootInlineBox() && is<RenderLayerModelObject>(renderer) && downcast<RenderLayerModelObject>(renderer).hasSelfPaintingLayer())
            continue;

        if (box.isAtomicInlineLevelBox()) {
            if (renderer.hitTest(request, result, locationInContainer, accumulatedOffset))
                return true;
            continue;
        }

        auto boxRect = Layout::toLayoutRect(box.rect());
        boxRect.moveBy(accumulatedOffset);

        if (!locationInContainer.intersects(boxRect))
            continue;

        auto& elementRenderer = is<RenderElement>(renderer) ? downcast<RenderElement>(renderer) : *renderer.parent();
        if (!elementRenderer.visibleToHitTesting(request))
            continue;
        
        renderer.updateHitTestResult(result, locationInContainer.point() - toLayoutSize(accumulatedOffset));
        if (result.addNodeToListBasedTestResult(renderer.nodeForHitTest(), request, locationInContainer, boxRect) == HitTestProgress::Stop)
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
            lineLayout->releaseCaches();
    }
}

void LineLayout::releaseCaches()
{
    m_inlineFormattingState.inlineItems().clear();
    if (m_inlineContent)
        m_inlineContent->releaseCaches();
}

void LineLayout::clearInlineContent()
{
    if (!m_inlineContent)
        return;
    m_inlineContent->clearAndDetach();
    m_inlineContent = nullptr;
}

Layout::InlineDamage& LineLayout::ensureLineDamage()
{
    if (!m_lineDamage)
        m_lineDamage = makeUnique<Layout::InlineDamage>();
    return *m_lineDamage;
}

#if ENABLE(TREE_DEBUGGING)
void LineLayout::outputLineTree(WTF::TextStream& stream, size_t depth) const
{
    showInlineContent(stream, *m_inlineContent, depth);
}
#endif

}
}

#endif
