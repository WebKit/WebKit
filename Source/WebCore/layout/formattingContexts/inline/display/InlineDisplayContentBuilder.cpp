/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "InlineDisplayContentBuilder.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "FontCascade.h"
#include "LayoutBoxGeometry.h"
#include "LayoutInitialContainingBlock.h"
#include "RuntimeEnabledFeatures.h"
#include "TextUtil.h"
#include <wtf/ListHashSet.h>
#include <wtf/Range.h>

using WTF::Range;

namespace WebCore {
namespace Layout {

static inline LayoutUnit marginLeftInInlineDirection(const Layout::BoxGeometry& boxGeometry, bool isLeftToRightDirection)
{
    return isLeftToRightDirection ? boxGeometry.marginStart() : boxGeometry.marginEnd();
}

static inline LayoutUnit marginRightInInlineDirection(const Layout::BoxGeometry& boxGeometry, bool isLeftToRightDirection)
{
    return isLeftToRightDirection ? boxGeometry.marginEnd() : boxGeometry.marginStart();
}

static inline LayoutUnit borderLeftInInlineDirection(const Layout::BoxGeometry& boxGeometry, bool isLeftToRightDirection)
{
    return isLeftToRightDirection ? boxGeometry.borderStart() : boxGeometry.borderEnd();
}

static inline LayoutUnit borderRightInInlineDirection(const Layout::BoxGeometry& boxGeometry, bool isLeftToRightDirection)
{
    return isLeftToRightDirection ? boxGeometry.borderEnd() : boxGeometry.borderStart();
}

static inline LayoutUnit paddingLeftInInlineDirection(const Layout::BoxGeometry& boxGeometry, bool isLeftToRightDirection)
{
    return isLeftToRightDirection ? boxGeometry.paddingStart().value_or(0_lu) : boxGeometry.paddingEnd().value_or(0_lu);
}

static inline LayoutUnit paddingRightInInlineDirection(const Layout::BoxGeometry& boxGeometry, bool isLeftToRightDirection)
{
    return isLeftToRightDirection ? boxGeometry.paddingEnd().value_or(0_lu) : boxGeometry.paddingStart().value_or(0_lu);
}

static inline OptionSet<InlineDisplay::Box::PositionWithinInlineLevelBox> isFirstLastBox(const InlineLevelBox& inlineBox)
{
    auto positionWithinInlineLevelBox = OptionSet<InlineDisplay::Box::PositionWithinInlineLevelBox> { };
    if (inlineBox.isFirstBox())
        positionWithinInlineLevelBox.add(InlineDisplay::Box::PositionWithinInlineLevelBox::First);
    if (inlineBox.isLastBox())
        positionWithinInlineLevelBox.add(InlineDisplay::Box::PositionWithinInlineLevelBox::Last);
    return positionWithinInlineLevelBox;
}

InlineDisplayContentBuilder::InlineDisplayContentBuilder(const ContainerBox& formattingContextRoot, InlineFormattingState& formattingState)
    : m_formattingContextRoot(formattingContextRoot)
    , m_formattingState(formattingState)
{
}

DisplayBoxes InlineDisplayContentBuilder::build(const LineBuilder::LineContent& lineContent, const LineBox& lineBox, const InlineDisplay::Line& displayLine, const size_t lineIndex)
{
    DisplayBoxes boxes;
    boxes.reserveInitialCapacity(lineContent.runs.size() + lineBox.nonRootInlineLevelBoxes().size() + 1);

    m_lineIndex = lineIndex;
    // Every line starts with a root box, even the empty ones.
    auto rootInlineBoxRect = flipRootInlineBoxRectToVisualForWritingMode(lineBox.logicalRectForRootInlineBox(), displayLine, root().style().writingMode());
    boxes.append({ m_lineIndex, InlineDisplay::Box::Type::RootInlineBox, root(), UBIDI_DEFAULT_LTR, rootInlineBoxRect, rootInlineBoxRect, { }, { }, lineBox.rootInlineBox().hasContent() });

    auto contentNeedsBidiReordering = !lineContent.visualOrderList.isEmpty();
    if (contentNeedsBidiReordering)
        processBidiContent(lineContent, lineBox, displayLine, boxes);
    else
        processNonBidiContent(lineContent, lineBox, displayLine, boxes);
    processOverflownRunsForEllipsis(boxes, displayLine.right());
    collectInkOverflowForInlineBoxes(boxes);
    return boxes;
}

static inline bool computeBoxShadowInkOverflow(const RenderStyle& style, FloatRect& inkOverflow)
{
    auto topBoxShadow = LayoutUnit { };
    auto bottomBoxShadow = LayoutUnit { };
    style.getBoxShadowBlockDirectionExtent(topBoxShadow, bottomBoxShadow);

    auto leftBoxShadow = LayoutUnit { };
    auto rightBoxShadow = LayoutUnit { };
    style.getBoxShadowInlineDirectionExtent(leftBoxShadow, rightBoxShadow);
    if (!topBoxShadow && !bottomBoxShadow && !leftBoxShadow && !rightBoxShadow)
        return false;
    inkOverflow.inflate(leftBoxShadow.toFloat(), topBoxShadow.toFloat(), rightBoxShadow.toFloat(), bottomBoxShadow.toFloat());
    return true;
}

void InlineDisplayContentBuilder::appendTextDisplayBox(const Line::Run& lineRun, const InlineRect& textRunRect, DisplayBoxes& boxes)
{
    ASSERT(lineRun.textContent() && is<InlineTextBox>(lineRun.layoutBox()));

    auto& layoutBox = lineRun.layoutBox();
    auto& style = !m_lineIndex ? layoutBox.firstLineStyle() : layoutBox.style();

    auto inkOverflow = [&] {
        auto initialContaingBlockSize = RuntimeEnabledFeatures::sharedFeatures().layoutFormattingContextIntegrationEnabled()
            ? formattingState().layoutState().viewportSize()
            : formattingState().layoutState().geometryForBox(layoutBox.initialContainingBlock()).contentBox().size();
        auto strokeOverflow = ceilf(style.computedStrokeWidth(ceiledIntSize(initialContaingBlockSize)));
        auto inkOverflow = textRunRect;
        inkOverflow.inflate(strokeOverflow);
        auto letterSpacing = style.fontCascade().letterSpacing();
        if (letterSpacing < 0) {
            // Last letter's negative spacing shrinks logical rect. Push it to ink overflow.
            inkOverflow.expand(-letterSpacing, { });
        }
        return inkOverflow;
    };
    auto content = downcast<InlineTextBox>(layoutBox).content();
    auto text = lineRun.textContent();
    auto adjustedContentToRender = [&] {
        return text->needsHyphen ? makeString(content.substring(text->start, text->length), style.hyphenString()) : String();
    };
    boxes.append({ m_lineIndex
        , lineRun.isWordSeparator() ? InlineDisplay::Box::Type::WordSeparator : InlineDisplay::Box::Type::Text
        , layoutBox
        , lineRun.bidiLevel()
        , textRunRect
        , inkOverflow()
        , lineRun.expansion()
        , InlineDisplay::Box::Text { text->start, text->length, content, adjustedContentToRender(), text->needsHyphen }
        , true
        , { }
    });
}

void InlineDisplayContentBuilder::appendSoftLineBreakDisplayBox(const Line::Run& lineRun, const InlineRect& softLineBreakRunRect, DisplayBoxes& boxes)
{
    ASSERT(lineRun.textContent() && is<InlineTextBox>(lineRun.layoutBox()));

    auto& layoutBox = lineRun.layoutBox();
    auto& text = lineRun.textContent();

    boxes.append({ m_lineIndex
        , InlineDisplay::Box::Type::SoftLineBreak
        , layoutBox
        , lineRun.bidiLevel()
        , softLineBreakRunRect
        , softLineBreakRunRect
        , lineRun.expansion()
        , InlineDisplay::Box::Text { text->start, text->length, downcast<InlineTextBox>(layoutBox).content() }
    });
}

void InlineDisplayContentBuilder::appendHardLineBreakDisplayBox(const Line::Run& lineRun, const InlineRect& lineBreakBoxRect, DisplayBoxes& boxes)
{
    auto& layoutBox = lineRun.layoutBox();

    boxes.append({ m_lineIndex
        , InlineDisplay::Box::Type::LineBreakBox
        , layoutBox
        , lineRun.bidiLevel()
        , lineBreakBoxRect
        , lineBreakBoxRect
        , lineRun.expansion()
        , { }
    });

    auto& boxGeometry = formattingState().boxGeometry(layoutBox);
    boxGeometry.setLogicalTopLeft(toLayoutPoint(lineBreakBoxRect.topLeft()));
    boxGeometry.setContentBoxHeight(toLayoutUnit(lineBreakBoxRect.height()));
}

void InlineDisplayContentBuilder::appendAtomicInlineLevelDisplayBox(const Line::Run& lineRun, const InlineRect& borderBoxRect, DisplayBoxes& boxes)
{
    ASSERT(lineRun.layoutBox().isAtomicInlineLevelBox());

    auto& layoutBox = lineRun.layoutBox();
    auto inkOverflow = [&] {
        auto inkOverflow = FloatRect { borderBoxRect };
        computeBoxShadowInkOverflow(!m_lineIndex ? layoutBox.firstLineStyle() : layoutBox.style(), inkOverflow);
        // Atomic inline box contribute to their inline box parents ink overflow at all times (e.g. <span><img></span>).
        m_contentHasInkOverflow = m_contentHasInkOverflow || &layoutBox.parent() != &root();
        return inkOverflow;
    };
    boxes.append({ m_lineIndex
        , InlineDisplay::Box::Type::AtomicInlineLevelBox
        , layoutBox
        , lineRun.bidiLevel()
        , borderBoxRect
        , inkOverflow()
        , lineRun.expansion()
        , { }
    });
    // Note that inline boxes are relative to the line and their top position can be negative.
    // Atomic inline boxes are all set. Their margin/border/content box geometries are already computed. We just have to position them here.
    formattingState().boxGeometry(layoutBox).setLogicalTopLeft(toLayoutPoint(borderBoxRect.topLeft()));
}

void InlineDisplayContentBuilder::setInlineBoxGeometry(const Box& layoutBox, const InlineRect& rect, bool isFirstInlineBoxFragment)
{
    auto adjustedSize = LayoutSize { LayoutUnit::fromFloatCeil(rect.width()), LayoutUnit::fromFloatCeil(rect.height()) };
    auto adjustedRect = Rect { LayoutPoint { rect.topLeft() }, adjustedSize };
    auto& boxGeometry = formattingState().boxGeometry(layoutBox);
    if (!isFirstInlineBoxFragment) {
        auto enclosingBorderBoxRect = BoxGeometry::borderBoxRect(boxGeometry);
        enclosingBorderBoxRect.expandToContain(adjustedRect);
        adjustedRect = enclosingBorderBoxRect;
    }
    boxGeometry.setLogicalTopLeft(adjustedRect.topLeft());
    auto contentBoxHeight = adjustedRect.height() - (boxGeometry.verticalBorder() + boxGeometry.verticalPadding().value_or(0_lu));
    auto contentBoxWidth = adjustedRect.width() - (boxGeometry.horizontalBorder() + boxGeometry.horizontalPadding().value_or(0_lu));
    boxGeometry.setContentBoxHeight(contentBoxHeight);
    boxGeometry.setContentBoxWidth(contentBoxWidth);
}

void InlineDisplayContentBuilder::appendInlineBoxDisplayBox(const Line::Run& lineRun, const InlineLevelBox& inlineBox, const InlineRect& inlineBoxBorderBox, bool linehasContent, DisplayBoxes& boxes)
{
    ASSERT(lineRun.layoutBox().isInlineBox());

    auto& layoutBox = lineRun.layoutBox();

    if (!linehasContent) {
        // FIXME: It's expected to not have any boxes on empty lines. We should reconsider this.
        setInlineBoxGeometry(layoutBox, inlineBoxBorderBox, true);
        return;
    }

    auto inkOverflow = [&] {
        auto inkOverflow = FloatRect { inlineBoxBorderBox };
        m_contentHasInkOverflow = computeBoxShadowInkOverflow(!m_lineIndex ? layoutBox.firstLineStyle() : layoutBox.style(), inkOverflow) || m_contentHasInkOverflow;
        return inkOverflow;
    };
    ASSERT(inlineBox.isInlineBox());
    ASSERT(inlineBox.isFirstBox());
    boxes.append({ m_lineIndex
        , InlineDisplay::Box::Type::NonRootInlineBox
        , layoutBox
        , lineRun.bidiLevel()
        , inlineBoxBorderBox
        , inkOverflow()
        , { }
        , { }
        , inlineBox.hasContent()
        , isFirstLastBox(inlineBox)
    });
    // This inline box showed up first on this line.
    setInlineBoxGeometry(layoutBox, inlineBoxBorderBox, true);
}

void InlineDisplayContentBuilder::appendSpanningInlineBoxDisplayBox(const Line::Run& lineRun, const InlineLevelBox& inlineBox, const InlineRect& inlineBoxBorderBox, DisplayBoxes& boxes)
{
    ASSERT(lineRun.layoutBox().isInlineBox());

    auto& layoutBox = lineRun.layoutBox();
    auto inkOverflow = [&] {
        auto inkOverflow = FloatRect { inlineBoxBorderBox };
        m_contentHasInkOverflow = computeBoxShadowInkOverflow(!m_lineIndex ? layoutBox.firstLineStyle() : layoutBox.style(), inkOverflow) || m_contentHasInkOverflow;
        return inkOverflow;
    };
    ASSERT(!inlineBox.isFirstBox());
    boxes.append({ m_lineIndex
        , InlineDisplay::Box::Type::NonRootInlineBox
        , layoutBox
        , lineRun.bidiLevel()
        , inlineBoxBorderBox
        , inkOverflow()
        , { }
        , { }
        , inlineBox.hasContent()
        , isFirstLastBox(inlineBox)
    });
    // Middle or end of the inline box. Let's stretch the box as needed.
    setInlineBoxGeometry(layoutBox, inlineBoxBorderBox, false);
}

void InlineDisplayContentBuilder::appendInlineDisplayBoxAtBidiBoundary(const Box& layoutBox, DisplayBoxes& boxes)
{
    // Geometries for inline boxes at bidi boundaries are computed at a post-process step.
    boxes.append({ m_lineIndex
        , InlineDisplay::Box::Type::NonRootInlineBox
        , layoutBox
        , UBIDI_DEFAULT_LTR
        , { }
        , { }
        , { }
        , { }
    });
}

void InlineDisplayContentBuilder::processNonBidiContent(const LineBuilder::LineContent& lineContent, const LineBox& lineBox, const InlineDisplay::Line& displayLine, DisplayBoxes& boxes)
{
#ifndef NDEBUG
    auto hasContent = false;
    for (auto& lineRun : lineContent.runs)
        hasContent = hasContent || lineRun.isText() || lineRun.isBox();
    ASSERT(lineContent.inlineBaseDirection == TextDirection::LTR || !hasContent);
#endif
    auto writingMode = root().style().writingMode();
    auto contentStartInVisualOrder = movePointHorizontallyForWritingMode(displayLine.topLeft(), displayLine.contentLogicalOffset(), writingMode);

    for (auto& lineRun : lineContent.runs) {
        auto& layoutBox = lineRun.layoutBox();

        auto visualRectRelativeToRoot = [&](auto logicalRect) {
            auto isContentRun = !lineRun.isInlineBoxStart() && !lineRun.isInlineBoxEnd() && !lineRun.isLineSpanningInlineBoxStart();
            auto visualRect = isContentRun ? flipLogicalRectToVisualForWritingModeWithinLine(logicalRect, lineBox.logicalRect(), writingMode)
                : flipLogicalRectToVisualForWritingMode(logicalRect, writingMode);
            visualRect.moveBy(contentStartInVisualOrder);
            return visualRect;
        };

        if (lineRun.isText()) {
            appendTextDisplayBox(lineRun, visualRectRelativeToRoot(lineBox.logicalRectForTextRun(lineRun)), boxes);
            continue;
        }
        if (lineRun.isSoftLineBreak()) {
            appendSoftLineBreakDisplayBox(lineRun, visualRectRelativeToRoot(lineBox.logicalRectForTextRun(lineRun)), boxes);
            continue;
        }
        if (lineRun.isHardLineBreak()) {
            appendHardLineBreakDisplayBox(lineRun, visualRectRelativeToRoot(lineBox.logicalRectForLineBreakBox(layoutBox)), boxes);
            continue;
        }
        if (lineRun.isBox()) {
            appendAtomicInlineLevelDisplayBox(lineRun
                , visualRectRelativeToRoot(lineBox.logicalBorderBoxForAtomicInlineLevelBox(layoutBox, formattingState().boxGeometry(layoutBox)))
                , boxes);
            continue;
        }
        if (lineRun.isInlineBoxStart()) {
            appendInlineBoxDisplayBox(lineRun
                , lineBox.inlineLevelBoxForLayoutBox(lineRun.layoutBox())
                , visualRectRelativeToRoot(lineBox.logicalBorderBoxForInlineBox(layoutBox, formattingState().boxGeometry(layoutBox)))
                , lineBox.hasContent()
                , boxes);
            continue;
        }
        if (lineRun.isLineSpanningInlineBoxStart()) {
            if (!lineBox.hasContent()) {
                // When a spanning inline box (e.g. <div>text<span><br></span></div>) lands on an empty line
                // (empty here means no content at all including line breaks, not just visually empty) then we
                // don't extend the spanning line box over to this line -also there is no next line in cases like this.
                continue;
            }
            appendSpanningInlineBoxDisplayBox(lineRun
                , lineBox.inlineLevelBoxForLayoutBox(lineRun.layoutBox())
                , visualRectRelativeToRoot(lineBox.logicalBorderBoxForInlineBox(layoutBox, formattingState().boxGeometry(layoutBox)))
                , boxes);
            continue;
        }
        ASSERT(lineRun.isInlineBoxEnd() || lineRun.isWordBreakOpportunity());
    }
}

struct DisplayBoxTree {
public:
    struct Node {
        // Node's parent index in m_displayBoxNodes.
        std::optional<size_t> parentIndex;
        // Associated display box index in DisplayBoxes.
        size_t displayBoxIndex { 0 };
        // Child indexes in m_displayBoxNodes.
        Vector<size_t> children { };
    };

    DisplayBoxTree()
    {
        m_displayBoxNodes.append({ });
    }

    const Node& root() const { return m_displayBoxNodes.first(); }
    Node& at(size_t index) { return m_displayBoxNodes[index]; }
    const Node& at(size_t index) const { return m_displayBoxNodes[index]; }

    size_t append(size_t parentNodeIndex, size_t childDisplayBoxIndex)
    {
        auto childDisplayBoxNodeIndex = m_displayBoxNodes.size();
        m_displayBoxNodes.append({ parentNodeIndex, childDisplayBoxIndex });
        at(parentNodeIndex).children.append(childDisplayBoxNodeIndex);
        return childDisplayBoxNodeIndex;
    }

private:
    Vector<Node, 10> m_displayBoxNodes;
};

struct AncestorStack {
    std::optional<size_t> unwind(const ContainerBox& containerBox)
    {
        // Unwind the stack all the way to container box.
        if (!m_set.contains(&containerBox))
            return { };
        while (m_set.last() != &containerBox) {
            m_stack.removeLast();
            m_set.removeLast();
        }
        // Root is always a common ancestor.
        ASSERT(!m_stack.isEmpty());
        return m_stack.last();
    }

    void push(size_t displayBoxNodeIndexForContainerBox, const ContainerBox& containerBox)
    {
        m_stack.append(displayBoxNodeIndexForContainerBox);
        ASSERT(!m_set.contains(&containerBox));
        m_set.add(&containerBox);
    }

private:
    Vector<size_t> m_stack;
    ListHashSet<const ContainerBox*> m_set;
};

static inline size_t createdDisplayBoxNodeForContainerBoxAndPushToAncestorStack(const ContainerBox& containerBox, size_t displayBoxIndex, size_t parentDisplayBoxNodeIndex, DisplayBoxTree& displayBoxTree, AncestorStack& ancestorStack)
{
    auto displayBoxNodeIndex = displayBoxTree.append(parentDisplayBoxNodeIndex, displayBoxIndex);
    ancestorStack.push(displayBoxNodeIndex, containerBox);
    return displayBoxNodeIndex;
}

size_t InlineDisplayContentBuilder::ensureDisplayBoxForContainer(const ContainerBox& containerBox, DisplayBoxTree& displayBoxTree, AncestorStack& ancestorStack, DisplayBoxes& boxes)
{
    ASSERT(containerBox.isInlineBox() || &containerBox == &root());
    if (auto lowestCommonAncestorIndex = ancestorStack.unwind(containerBox))
        return *lowestCommonAncestorIndex;
    auto enclosingDisplayBoxNodeIndexForContainer = ensureDisplayBoxForContainer(containerBox.parent(), displayBoxTree, ancestorStack, boxes);
    appendInlineDisplayBoxAtBidiBoundary(containerBox, boxes);
    return createdDisplayBoxNodeForContainerBoxAndPushToAncestorStack(containerBox, boxes.size() - 1, enclosingDisplayBoxNodeIndexForContainer, displayBoxTree, ancestorStack);
}

struct IsFirstLastIndex {
    std::optional<size_t> first;
    std::optional<size_t> last;
};
using IsFirstLastIndexesMap = HashMap<const Box*, IsFirstLastIndex>;
void InlineDisplayContentBuilder::adjustVisualGeometryForDisplayBox(size_t displayBoxNodeIndex, InlineLayoutUnit& contentRightInInlineDirectionVisualOrder, InlineLayoutUnit lineBoxLogicalTop, const DisplayBoxTree& displayBoxTree, DisplayBoxes& boxes, const LineBox& lineBox, const IsFirstLastIndexesMap& isFirstLastIndexesMap)
{
    auto writingMode = root().style().writingMode();
    auto isHorizontalWritingMode = WebCore::isHorizontalWritingMode(writingMode);
    // Non-inline box display boxes just need a horizontal adjustment while
    // inline box type of display boxes need
    // 1. horizontal adjustment and margin/border/padding start offsetting on the first box
    // 2. right edge computation including descendant content width and margin/border/padding end offsetting on the last box
    auto& displayBox = boxes[displayBoxTree.at(displayBoxNodeIndex).displayBoxIndex];
    auto& layoutBox = displayBox.layoutBox();

    if (!displayBox.isNonRootInlineBox()) {
        if (displayBox.isAtomicInlineLevelBox() || displayBox.isGenericInlineLevelBox()) {
            auto isLeftToRightDirection = layoutBox.parent().style().isLeftToRightDirection();
            auto& boxGeometry = formattingState().boxGeometry(layoutBox);
            auto boxMarginLeft = marginLeftInInlineDirection(boxGeometry, isLeftToRightDirection);

            auto borderBoxLeft = LayoutUnit { contentRightInInlineDirectionVisualOrder + boxMarginLeft };
            boxGeometry.setLogicalLeft(borderBoxLeft);
            setLeftForWritingMode(displayBox, borderBoxLeft, writingMode);

            contentRightInInlineDirectionVisualOrder += boxGeometry.marginBoxWidth();
        } else {
            auto wordSpacingMargin = displayBox.isWordSeparator() ? layoutBox.style().fontCascade().wordSpacing() : 0.0f;
            setLeftForWritingMode(displayBox, contentRightInInlineDirectionVisualOrder + wordSpacingMargin, writingMode);
            contentRightInInlineDirectionVisualOrder += (isHorizontalWritingMode ? displayBox.width() : displayBox.height()) + wordSpacingMargin;
        }
        return;
    }

    auto& boxGeometry = formattingState().boxGeometry(layoutBox);
    auto isLeftToRightDirection = layoutBox.style().isLeftToRightDirection();
    auto isFirstLastIndexes = isFirstLastIndexesMap.get(&layoutBox);
    auto isFirstBox = isFirstLastIndexes.first && *isFirstLastIndexes.first == displayBoxNodeIndex;
    auto isLastBox = isFirstLastIndexes.last && *isFirstLastIndexes.last == displayBoxNodeIndex;
    auto beforeInlineBoxContent = [&] {
        auto logicalRect = lineBox.logicalBorderBoxForInlineBox(layoutBox, boxGeometry);
        auto visualRect = flipLogicalRectToVisualForWritingMode({ lineBoxLogicalTop + logicalRect.top(), contentRightInInlineDirectionVisualOrder, { }, logicalRect.height() }, writingMode);
        displayBox.setRect(visualRect, visualRect);

        auto shouldApplyLeftSide = (isLeftToRightDirection && isFirstBox) || (!isLeftToRightDirection && isLastBox);
        if (!shouldApplyLeftSide)
            return;

        contentRightInInlineDirectionVisualOrder += marginLeftInInlineDirection(boxGeometry, isLeftToRightDirection);
        setLeftForWritingMode(displayBox, contentRightInInlineDirectionVisualOrder, writingMode);
        contentRightInInlineDirectionVisualOrder += borderLeftInInlineDirection(boxGeometry, isLeftToRightDirection) + paddingLeftInInlineDirection(boxGeometry, isLeftToRightDirection);
    };
    beforeInlineBoxContent();

    for (auto childDisplayBoxNodeIndex : displayBoxTree.at(displayBoxNodeIndex).children)
        adjustVisualGeometryForDisplayBox(childDisplayBoxNodeIndex, contentRightInInlineDirectionVisualOrder, lineBoxLogicalTop, displayBoxTree, boxes, lineBox, isFirstLastIndexesMap);

    auto afterInlineBoxContent = [&] {
        auto shouldApplyRightSide = (isLeftToRightDirection && isLastBox) || (!isLeftToRightDirection && isFirstBox);
        if (!shouldApplyRightSide)
            return setRightForWritingMode(displayBox, contentRightInInlineDirectionVisualOrder, writingMode);

        contentRightInInlineDirectionVisualOrder += borderRightInInlineDirection(boxGeometry, isLeftToRightDirection) + paddingRightInInlineDirection(boxGeometry, isLeftToRightDirection);
        setRightForWritingMode(displayBox, contentRightInInlineDirectionVisualOrder, writingMode);
        contentRightInInlineDirectionVisualOrder += marginRightInInlineDirection(boxGeometry, isLeftToRightDirection);
    };
    afterInlineBoxContent();

    auto computeInkOverflow = [&] {
        auto inkOverflow = FloatRect { displayBox.rect() };
        m_contentHasInkOverflow = computeBoxShadowInkOverflow(!m_lineIndex ? layoutBox.firstLineStyle() : layoutBox.style(), inkOverflow) || m_contentHasInkOverflow;
        displayBox.adjustInkOverflow(inkOverflow);
    };
    computeInkOverflow();

    setInlineBoxGeometry(layoutBox, displayBox.rect(), isFirstBox);
    if (lineBox.inlineLevelBoxForLayoutBox(layoutBox).hasContent())
        displayBox.setHasContent();
}

void InlineDisplayContentBuilder::processBidiContent(const LineBuilder::LineContent& lineContent, const LineBox& lineBox, const InlineDisplay::Line& displayLine, DisplayBoxes& boxes)
{
    ASSERT(lineContent.visualOrderList.size() <= lineContent.runs.size());

    AncestorStack ancestorStack;
    auto displayBoxTree = DisplayBoxTree { };
    ancestorStack.push({ }, root());

    auto writingMode = root().style().writingMode();
    auto isHorizontalWritingMode = WebCore::isHorizontalWritingMode(writingMode);

    auto lineLogicalTop = isHorizontalWritingMode ? displayLine.top() : displayLine.left();
    auto lineLogicalLeft = isHorizontalWritingMode ? displayLine.left() : displayLine.top();
    auto contentStartInInlineDirectionVisualOrder = lineLogicalLeft + displayLine.contentLogicalOffset();
    auto hasInlineBox = false;
    auto createDisplayBoxesInVisualOrder = [&] {

        auto contentRightInInlineDirectionVisualOrder = contentStartInInlineDirectionVisualOrder;
        auto& runs = lineContent.runs;
        for (auto visualOrder : lineContent.visualOrderList) {
            ASSERT(runs[visualOrder].bidiLevel() != InlineItem::opaqueBidiLevel);

            auto& lineRun = runs[visualOrder];
            auto& layoutBox = lineRun.layoutBox();

            auto needsDisplayBox = !lineRun.isWordBreakOpportunity() && !lineRun.isInlineBoxEnd();
            if (!needsDisplayBox)
                continue;

            auto visualRectRelativeToRoot = [&](auto logicalRect) {
                auto visualRect = flipLogicalRectToVisualForWritingModeWithinLine(logicalRect, lineBox.logicalRect(), writingMode);
                if (isHorizontalWritingMode) {
                    visualRect.setLeft(contentRightInInlineDirectionVisualOrder);
                    visualRect.moveVertically(lineLogicalTop);
                } else {
                    visualRect.setTop(contentRightInInlineDirectionVisualOrder);
                    visualRect.moveHorizontally(lineLogicalTop);
                }
                return visualRect;
            };

            auto parentDisplayBoxNodeIndex = ensureDisplayBoxForContainer(layoutBox.parent(), displayBoxTree, ancestorStack, boxes);
            hasInlineBox = hasInlineBox || parentDisplayBoxNodeIndex || lineRun.isInlineBoxStart() || lineRun.isLineSpanningInlineBoxStart();
            if (lineRun.isText()) {
                auto logicalRect = lineBox.logicalRectForTextRun(lineRun);
                auto visualRect = visualRectRelativeToRoot(logicalRect);
                auto wordSpacingMargin = lineRun.isWordSeparator() ? layoutBox.style().fontCascade().wordSpacing() : 0.0f;

                isHorizontalWritingMode ? visualRect.moveHorizontally(wordSpacingMargin) : visualRect.moveVertically(wordSpacingMargin);
                appendTextDisplayBox(lineRun, visualRect, boxes);
                contentRightInInlineDirectionVisualOrder += logicalRect.width() + wordSpacingMargin;
                displayBoxTree.append(parentDisplayBoxNodeIndex, boxes.size() - 1);
                continue;
            }
            if (lineRun.isSoftLineBreak()) {
                auto visualRect = visualRectRelativeToRoot(lineBox.logicalRectForTextRun(lineRun));
                ASSERT((isHorizontalWritingMode && !visualRect.width()) || (!isHorizontalWritingMode && !visualRect.height()));
                appendSoftLineBreakDisplayBox(lineRun, visualRect, boxes);
                displayBoxTree.append(parentDisplayBoxNodeIndex, boxes.size() - 1);
                continue;
            }
            if (lineRun.isHardLineBreak()) {
                auto visualRect = visualRectRelativeToRoot(lineBox.logicalRectForLineBreakBox(layoutBox));
                ASSERT((isHorizontalWritingMode && !visualRect.width()) || (!isHorizontalWritingMode && !visualRect.height()));
                appendHardLineBreakDisplayBox(lineRun, visualRect, boxes);
                displayBoxTree.append(parentDisplayBoxNodeIndex, boxes.size() - 1);
                continue;
            }
            if (lineRun.isBox()) {
                auto& boxGeometry = formattingState().boxGeometry(layoutBox);
                auto logicalRect = lineBox.logicalBorderBoxForAtomicInlineLevelBox(layoutBox, boxGeometry);
                auto visualRect = visualRectRelativeToRoot(logicalRect);

                auto isLeftToRightDirection = layoutBox.parent().style().isLeftToRightDirection();
                auto boxMarginLeft = marginLeftInInlineDirection(boxGeometry, isLeftToRightDirection);
                isHorizontalWritingMode ? visualRect.moveHorizontally(boxMarginLeft) : visualRect.moveVertically(boxMarginLeft);
                appendAtomicInlineLevelDisplayBox(lineRun, visualRect, boxes);
                contentRightInInlineDirectionVisualOrder += boxMarginLeft + logicalRect.width() + marginRightInInlineDirection(boxGeometry, isLeftToRightDirection);
                displayBoxTree.append(parentDisplayBoxNodeIndex, boxes.size() - 1);
                continue;
            }
            if (lineRun.isInlineBoxStart() || lineRun.isLineSpanningInlineBoxStart()) {
                // FIXME: While we should only get here with empty inline boxes, there are
                // some cases where the inline box has some content on the paragraph level (at bidi split) but line breaking renders it empty
                // or their content is completely collapsed.
                // Such inline boxes should also be handled here.
                if (!lineBox.hasContent()) {
                    // FIXME: It's expected to not have any inline boxes on empty lines. They make the line taller. We should reconsider this.
                    setInlineBoxGeometry(layoutBox, { { }, { } }, true);
                } else if (!lineBox.inlineLevelBoxForLayoutBox(layoutBox).hasContent()) {
                    appendInlineDisplayBoxAtBidiBoundary(layoutBox, boxes);
                    createdDisplayBoxNodeForContainerBoxAndPushToAncestorStack(downcast<ContainerBox>(layoutBox), boxes.size() - 1, parentDisplayBoxNodeIndex, displayBoxTree, ancestorStack);
                }
                continue;
            }
            ASSERT_NOT_REACHED();
        }
    };
    createDisplayBoxesInVisualOrder();

    auto handleInlineBoxes = [&] {
        if (!hasInlineBox)
            return;

        IsFirstLastIndexesMap isFirstLastIndexesMap;
        auto computeIsFirstIsLastBox = [&] {
            ASSERT(boxes[0].isRootInlineBox());
            for (size_t index = 1; index < boxes.size(); ++index) {
                if (!boxes[index].isNonRootInlineBox())
                    continue;
                auto& layoutBox = boxes[index].layoutBox();
                auto isFirstBox = lineBox.inlineLevelBoxForLayoutBox(layoutBox).isFirstBox();
                auto isLastBox = lineBox.inlineLevelBoxForLayoutBox(layoutBox).isLastBox();
                if (!isFirstBox && !isLastBox)
                    continue;
                if (isFirstBox) {
                    auto isFirstLastIndexes = isFirstLastIndexesMap.get(&layoutBox);
                    if (!isFirstLastIndexes.first || isLastBox)
                        isFirstLastIndexesMap.set(&layoutBox, IsFirstLastIndex { isFirstLastIndexes.first.value_or(index), isLastBox ? index : isFirstLastIndexes.last });
                    continue;
                }
                if (isLastBox) {
                    ASSERT(!isFirstBox);
                    isFirstLastIndexesMap.set(&layoutBox, IsFirstLastIndex { { }, index });
                    continue;
                }
            }
        };
        computeIsFirstIsLastBox();

        auto adjustVisualGeometryWithInlineBoxes = [&] {
            auto contentRightInInlineDirectionVisualOrder = contentStartInInlineDirectionVisualOrder;

            for (auto childDisplayBoxNodeIndex : displayBoxTree.root().children)
                adjustVisualGeometryForDisplayBox(childDisplayBoxNodeIndex, contentRightInInlineDirectionVisualOrder, lineLogicalTop, displayBoxTree, boxes, lineBox, isFirstLastIndexesMap);
        };
        adjustVisualGeometryWithInlineBoxes();
    };
    handleInlineBoxes();
}

void InlineDisplayContentBuilder::processOverflownRunsForEllipsis(DisplayBoxes& boxes, InlineLayoutUnit lineBoxRight)
{
    if (root().style().textOverflow() != TextOverflow::Ellipsis)
        return;
    auto& rootInlineBox = boxes[0];
    ASSERT(rootInlineBox.isRootInlineBox());

    if (rootInlineBox.right() <= lineBoxRight) {
        ASSERT(boxes.last().right() <= lineBoxRight);
        return;
    }

    static MainThreadNeverDestroyed<const AtomString> ellipsisStr(&horizontalEllipsis, 1);
    auto ellipsisRun = WebCore::TextRun { ellipsisStr->string() };
    auto ellipsisWidth = !m_lineIndex ? root().firstLineStyle().fontCascade().width(ellipsisRun) : root().style().fontCascade().width(ellipsisRun);
    auto firstTruncatedBoxIndex = boxes.size();

    for (auto index = boxes.size(); index--;) {
        auto& displayBox = boxes[index];

        if (displayBox.left() >= lineBoxRight) {
            // Fully overflown boxes are collapsed.
            displayBox.truncate();
            continue;
        }

        // We keep truncating content until after we can accommodate the ellipsis content
        // 1. fully truncate in case of inline level boxes (ie non-text content) or if ellipsis content is wider than the overflowing one.
        // 2. partially truncated to make room for the ellipsis box.
        auto availableRoomForEllipsis = lineBoxRight - displayBox.left();
        if (availableRoomForEllipsis <= ellipsisWidth) {
            // Can't accommodate the ellipsis content here. We need to truncate non-overflowing boxes too.
            displayBox.truncate();
            continue;
        }

        auto truncatedWidth = InlineLayoutUnit { };
        if (displayBox.isText()) {
            auto text = *displayBox.text();
            // FIXME: Check if it needs adjustment for RTL direction.
            truncatedWidth = TextUtil::breakWord(downcast<InlineTextBox>(displayBox.layoutBox()), text.start(), text.length(), displayBox.width(), availableRoomForEllipsis - ellipsisWidth, { }, displayBox.style().fontCascade()).logicalWidth;
        }
        displayBox.truncate(truncatedWidth);
        firstTruncatedBoxIndex = index;
        break;
    }
    ASSERT(firstTruncatedBoxIndex < boxes.size());
    // Collapse truncated runs.
    auto contentRight = boxes[firstTruncatedBoxIndex].right();
    for (auto index = firstTruncatedBoxIndex + 1; index < boxes.size(); ++index)
        boxes[index].moveHorizontally(contentRight - boxes[index].left());
    // And append the ellipsis box as the trailing item.
    auto ellispisBoxRect = InlineRect { rootInlineBox.top(), contentRight, ellipsisWidth, rootInlineBox.height() };
    boxes.append({ m_lineIndex
        , InlineDisplay::Box::Type::Ellipsis
        , root()
        , UBIDI_DEFAULT_LTR
        , ellispisBoxRect
        , ellispisBoxRect
        , { }
        , InlineDisplay::Box::Text { 0, 1, ellipsisStr->string() } });
}

void InlineDisplayContentBuilder::collectInkOverflowForInlineBoxes(DisplayBoxes& boxes)
{
    if (!m_contentHasInkOverflow)
        return;
    // Visit the inline boxes and propagate ink overflow to their parents -except to the root inline box.
    // (e.g. <span style="font-size: 10px;">Small font size<span style="font-size: 300px;">Larger font size. This overflows the top most span.</span></span>).
    auto accumulatedInkOverflowRect = InlineRect { { }, { } };
    for (size_t index = boxes.size(); index--;) {
        auto& displayBox = boxes[index];

        auto mayHaveInkOverflow = displayBox.isAtomicInlineLevelBox() || displayBox.isGenericInlineLevelBox() || displayBox.isNonRootInlineBox();
        if (!mayHaveInkOverflow)
            continue;
        if (displayBox.isNonRootInlineBox() && !accumulatedInkOverflowRect.isEmpty())
            displayBox.adjustInkOverflow(accumulatedInkOverflowRect);

        // We stop collecting ink overflow for at root inline box (i.e. don't inflate the root inline box with the inline content here).
        auto parentBoxIsRoot = &displayBox.layoutBox().parent() == &root();
        if (parentBoxIsRoot)
            accumulatedInkOverflowRect = InlineRect { { }, { } };
        else if (accumulatedInkOverflowRect.isEmpty())
            accumulatedInkOverflowRect = displayBox.inkOverflow();
        else
            accumulatedInkOverflowRect.expandToContain(displayBox.inkOverflow());
    }
}

void InlineDisplayContentBuilder::computeIsFirstIsLastBoxForInlineContent(DisplayBoxes& boxes)
{
    HashMap<const Box*, size_t> lastDisplayBoxForLayoutBoxIndexes;

    ASSERT(boxes[0].isRootInlineBox());
    boxes[0].setIsFirstForLayoutBox(true);
    size_t lastRootInlineBoxIndex = 0;

    for (size_t index = 1; index < boxes.size(); ++index) {
        auto& displayBox = boxes[index];
        if (displayBox.isRootInlineBox()) {
            lastRootInlineBoxIndex = index;
            continue;
        }
        auto& layoutBox = displayBox.layoutBox();
        if (lastDisplayBoxForLayoutBoxIndexes.set(&layoutBox, index).isNewEntry)
            displayBox.setIsFirstForLayoutBox(true);
    }
    for (auto index : lastDisplayBoxForLayoutBoxIndexes.values())
        boxes[index].setIsLastForLayoutBox(true);

    boxes[lastRootInlineBoxIndex].setIsLastForLayoutBox(true);
}

InlineRect InlineDisplayContentBuilder::flipLogicalRectToVisualForWritingModeWithinLine(const InlineRect& logicalRect, const InlineRect& lineLogicalRect, WritingMode writingMode) const
{
    switch (writingMode) {
    case WritingMode::TopToBottom:
        return logicalRect;
    case WritingMode::LeftToRight: {
        // Flip content such that the top (visual left) is now relative to the line bottom instead of the line top.
        auto bottomOffset = lineLogicalRect.height() - logicalRect.bottom();
        return { logicalRect.left(), bottomOffset, logicalRect.height(), logicalRect.width() };
    }
    case WritingMode::RightToLeft:
        // See InlineFormattingGeometry for more info.
        return { logicalRect.left(), logicalRect.top(), logicalRect.height(), logicalRect.width() };
    default:
        ASSERT_NOT_REACHED();
        break;
    }
    return logicalRect;
}

InlineRect InlineDisplayContentBuilder::flipLogicalRectToVisualForWritingMode(const InlineRect& logicalRect, WritingMode writingMode) const
{
    switch (writingMode) {
    case WritingMode::TopToBottom:
        return logicalRect;
    case WritingMode::LeftToRight:
    case WritingMode::RightToLeft:
        // See InlineFormattingGeometry for more info.
        return { logicalRect.left(), logicalRect.top(), logicalRect.height(), logicalRect.width() };
    default:
        ASSERT_NOT_REACHED();
        break;
    }
    return logicalRect;
}

InlineRect InlineDisplayContentBuilder::flipRootInlineBoxRectToVisualForWritingMode(const InlineRect& rootInlineBoxLogicalRect, const InlineDisplay::Line& displayLine, WritingMode writingMode) const
{
    switch (writingMode) {
    case WritingMode::TopToBottom: {
        auto visualRect = rootInlineBoxLogicalRect;
        visualRect.moveBy({ displayLine.left() + displayLine.contentLogicalOffset(), displayLine.top() });
        return visualRect;
    }
    case WritingMode::LeftToRight:
    case WritingMode::RightToLeft: {
        // See InlineFormattingGeometry for more info.
        auto visualRect = InlineRect { rootInlineBoxLogicalRect.left(), rootInlineBoxLogicalRect.top(), rootInlineBoxLogicalRect.height(), rootInlineBoxLogicalRect.width() };
        visualRect.moveBy({ displayLine.left(), displayLine.top() + displayLine.contentLogicalOffset() });
        return visualRect;
    }
    default:
        ASSERT_NOT_REACHED();
        break;
    }
    return rootInlineBoxLogicalRect;
}

void InlineDisplayContentBuilder::setLeftForWritingMode(InlineDisplay::Box& displayBox, InlineLayoutUnit logicalLeft, WritingMode writingMode) const
{
    switch (writingMode) {
    case WritingMode::TopToBottom:
        displayBox.setLeft(logicalLeft);
        break;
    case WritingMode::LeftToRight:
    case WritingMode::RightToLeft:
        displayBox.setTop(logicalLeft);
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
}

void InlineDisplayContentBuilder::setRightForWritingMode(InlineDisplay::Box& displayBox, InlineLayoutUnit logicalRight, WritingMode writingMode) const
{
    switch (writingMode) {
    case WritingMode::TopToBottom:
        displayBox.setRight(logicalRight);
        break;
    case WritingMode::LeftToRight:
    case WritingMode::RightToLeft:
        displayBox.setBottom(logicalRight);
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
}

InlineLayoutPoint InlineDisplayContentBuilder::movePointHorizontallyForWritingMode(const InlineLayoutPoint& logicalPoint, InlineLayoutUnit horizontalOffset, WritingMode writingMode) const
{
    auto visualPoint = logicalPoint;
    switch (writingMode) {
    case WritingMode::TopToBottom:
        visualPoint.moveBy(FloatPoint { horizontalOffset, { } });
        break;
    case WritingMode::LeftToRight:
    case WritingMode::RightToLeft: {
        visualPoint.moveBy(FloatPoint { { }, horizontalOffset });
        break;
    }
    default:
        ASSERT_NOT_REACHED();
        break;
    }
    return visualPoint;
}

}
}

#endif
