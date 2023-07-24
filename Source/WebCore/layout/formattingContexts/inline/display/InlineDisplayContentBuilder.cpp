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

#include "FontCascade.h"
#include "InlineFormattingGeometry.h"
#include "InlineTextBoxStyle.h"
#include "LayoutBoxGeometry.h"
#include "LayoutInitialContainingBlock.h"
#include "RenderStyleInlines.h"
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

InlineDisplayContentBuilder::InlineDisplayContentBuilder(const InlineFormattingContext& formattingContext, InlineFormattingState& formattingState, const InlineDisplay::Line& displayLine, size_t lineIndex)
    : m_formattingContext(formattingContext)
    , m_formattingState(formattingState)
    , m_displayLine(displayLine)
    , m_lineIndex(lineIndex)
{
    auto& initialContainingBlockGeometry = m_formattingState.layoutState().geometryForBox(FormattingContext::initialContainingBlock(root()));
    m_initialContaingBlockSize = ceiledIntSize(LayoutSize { initialContainingBlockGeometry.contentBoxWidth(), initialContainingBlockGeometry.contentBoxHeight() });
}

InlineDisplay::Boxes InlineDisplayContentBuilder::build(const LineBuilder::LayoutResult& lineLayoutResult, const LineBox& lineBox)
{
    auto boxes = InlineDisplay::Boxes { };
    boxes.reserveInitialCapacity(lineLayoutResult.inlineContent.size() + lineBox.nonRootInlineLevelBoxes().size() + 1);

    auto contentNeedsBidiReordering = !lineLayoutResult.directionality.visualOrderList.isEmpty();
    if (contentNeedsBidiReordering)
        processBidiContent(lineLayoutResult, lineBox, boxes);
    else
        processNonBidiContent(lineLayoutResult, lineBox, boxes);
    processFloatBoxes(lineLayoutResult);

    collectInkOverflowForTextDecorations(boxes);
    collectInkOverflowForInlineBoxes(boxes);
    return boxes;
}

static inline bool computeInkOverflowForInlineLevelBox(const RenderStyle& style, FloatRect& inkOverflow)
{
    auto hasVisualOverflow = false;

    auto inflateWithOutline = [&] {
        if (!style.hasOutlineInVisualOverflow())
            return;
        inkOverflow.inflate(style.outlineSize());
        hasVisualOverflow = true;
    };
    inflateWithOutline();

    auto inflateWithBoxShadow = [&] {
        auto topBoxShadow = LayoutUnit { };
        auto bottomBoxShadow = LayoutUnit { };
        style.getBoxShadowVerticalExtent(topBoxShadow, bottomBoxShadow);

        auto leftBoxShadow = LayoutUnit { };
        auto rightBoxShadow = LayoutUnit { };
        style.getBoxShadowHorizontalExtent(leftBoxShadow, rightBoxShadow);
        if (!topBoxShadow && !bottomBoxShadow && !leftBoxShadow && !rightBoxShadow)
            return;
        inkOverflow.inflate(-leftBoxShadow.toFloat(), -topBoxShadow.toFloat(), rightBoxShadow.toFloat(), bottomBoxShadow.toFloat());
        hasVisualOverflow = true;
    };
    inflateWithBoxShadow();

    return hasVisualOverflow;
}

static inline bool computeInkOverflowForInlineBox(const InlineLevelBox& inlineBox, const RenderStyle& style, FloatRect& inkOverflow)
{
    ASSERT(inlineBox.isInlineBox());
    auto hasVisualOverflow = computeInkOverflowForInlineLevelBox(style, inkOverflow);

    auto inflateWithAnnotation = [&] {
        if (!inlineBox.hasAnnotation())
            return;
        inkOverflow.inflate(0.f, inlineBox.annotationAbove().value_or(0.f), 0.f, inlineBox.annotationBelow().value_or(0.f));
        hasVisualOverflow = true;
    };
    inflateWithAnnotation();

    return hasVisualOverflow;
}

void InlineDisplayContentBuilder::appendTextDisplayBox(const Line::Run& lineRun, const InlineRect& textRunRect, InlineDisplay::Boxes& boxes)
{
    ASSERT(lineRun.textContent() && is<InlineTextBox>(lineRun.layoutBox()));

    auto& inlineTextBox = downcast<InlineTextBox>(lineRun.layoutBox());
    auto& style = !m_lineIndex ? inlineTextBox.firstLineStyle() : inlineTextBox.style();
    auto& content = inlineTextBox.content();
    auto& text = lineRun.textContent();
    auto isContentful = true;

    auto inkOverflow = [&] {
        auto inkOverflow = textRunRect;

        auto addStrokeOverflow = [&] {
            inkOverflow.inflate(ceilf(style.computedStrokeWidth(m_initialContaingBlockSize)));
        };
        addStrokeOverflow();

        auto addTextShadow = [&] {
            auto textShadow = style.textShadowExtent();
            inkOverflow.inflate(-textShadow.top(), textShadow.right(), textShadow.bottom(), -textShadow.left());
        };
        addTextShadow();

        auto addLetterSpacingOverflow = [&] {
            auto letterSpacing = style.fontCascade().letterSpacing();
            if (letterSpacing >= 0)
                return;
            // Last letter's negative spacing shrinks logical rect. Push it to ink overflow.
            inkOverflow.expand(-letterSpacing, { });
        };
        addLetterSpacingOverflow();

        auto addGlyphOverflow = [&] {
            if (inlineTextBox.canUseSimpleFontCodePath()) {
                // canUseSimpleFontCodePath maps to CodePath::Simple (and content with potential glyph overflow would says CodePath::SimpleWithGlyphOverflow).
                return;
            }
            auto enclosingAscentAndDescent = TextUtil::enclosingGlyphBoundsForText(StringView(content).substring(text->start, text->length), style);
            // FIXME: Take fallback fonts into account.
            auto& fontMetrics = style.metricsOfPrimaryFont();
            auto topOverflow = std::max(0.f, ceilf(-enclosingAscentAndDescent.ascent) - fontMetrics.ascent());
            auto bottomOverflow = std::max(0.f, ceilf(enclosingAscentAndDescent.descent) - fontMetrics.descent());
            inkOverflow.inflate(topOverflow, { }, bottomOverflow, { });
        };
        addGlyphOverflow();

        return inkOverflow;
    };

    if (inlineTextBox.isCombined()) {
        static auto objectReplacementCharacterString = NeverDestroyed<String> { &objectReplacementCharacter, 1 };
        // The rendered text is the actual combined content, while the "original" one is blank.
        boxes.append({ m_lineIndex
            , InlineDisplay::Box::Type::Text
            , inlineTextBox
            , lineRun.bidiLevel()
            , textRunRect
            , inkOverflow()
            , lineRun.expansion()
            , InlineDisplay::Box::Text { text->start, 1, objectReplacementCharacterString, content }
            , isContentful
            , isLineFullyTruncatedInBlockDirection()
        });
        return;
    }

    auto adjustedContentToRender = [&] {
        return text->needsHyphen ? makeString(StringView(content).substring(text->start, text->length), style.hyphenString()) : String();
    };
    boxes.append({ m_lineIndex
        , lineRun.isWordSeparator() ? InlineDisplay::Box::Type::WordSeparator : InlineDisplay::Box::Type::Text
        , inlineTextBox
        , lineRun.bidiLevel()
        , textRunRect
        , inkOverflow()
        , lineRun.expansion()
        , InlineDisplay::Box::Text { text->start, text->length, content, adjustedContentToRender(), text->needsHyphen }
        , isContentful
        , isLineFullyTruncatedInBlockDirection()
    });
}

void InlineDisplayContentBuilder::appendSoftLineBreakDisplayBox(const Line::Run& lineRun, const InlineRect& softLineBreakRunRect, InlineDisplay::Boxes& boxes)
{
    ASSERT(lineRun.textContent() && is<InlineTextBox>(lineRun.layoutBox()));

    auto& layoutBox = lineRun.layoutBox();
    auto& text = lineRun.textContent();
    auto isContentful = true;

    boxes.append({ m_lineIndex
        , InlineDisplay::Box::Type::SoftLineBreak
        , layoutBox
        , lineRun.bidiLevel()
        , softLineBreakRunRect
        , softLineBreakRunRect
        , lineRun.expansion()
        , InlineDisplay::Box::Text { text->start, text->length, downcast<InlineTextBox>(layoutBox).content() }
        , isContentful
        , isLineFullyTruncatedInBlockDirection()
    });
}

void InlineDisplayContentBuilder::appendHardLineBreakDisplayBox(const Line::Run& lineRun, const InlineRect& lineBreakBoxRect, InlineDisplay::Boxes& boxes)
{
    auto& layoutBox = lineRun.layoutBox();
    auto isContentful = true;

    boxes.append({ m_lineIndex
        , InlineDisplay::Box::Type::LineBreakBox
        , layoutBox
        , lineRun.bidiLevel()
        , lineBreakBoxRect
        , lineBreakBoxRect
        , lineRun.expansion()
        , { }
        , isContentful
        , isLineFullyTruncatedInBlockDirection()
    });

    auto& boxGeometry = formattingState().boxGeometry(layoutBox);
    boxGeometry.setLogicalTopLeft(toLayoutPoint(lineBreakBoxRect.topLeft()));
    boxGeometry.setContentBoxHeight(toLayoutUnit(lineBreakBoxRect.height()));
}

void InlineDisplayContentBuilder::appendAtomicInlineLevelDisplayBox(const Line::Run& lineRun, const InlineRect& borderBoxRect, InlineDisplay::Boxes& boxes)
{
    ASSERT(lineRun.layoutBox().isAtomicInlineLevelBox());

    auto& layoutBox = lineRun.layoutBox();
    auto& style = !m_lineIndex ? layoutBox.firstLineStyle() : layoutBox.style();
    auto isContentful = true;
    auto inkOverflow = [&] {
        auto inkOverflow = FloatRect { borderBoxRect };
        computeInkOverflowForInlineLevelBox(style, inkOverflow);
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
        , isContentful
        , isLineFullyTruncatedInBlockDirection()
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

void InlineDisplayContentBuilder::appendRootInlineBoxDisplayBox(const InlineRect& rootInlineBoxVisualRect, bool linehasContent, InlineDisplay::Boxes& boxes)
{
    boxes.append({ m_lineIndex
        , InlineDisplay::Box::Type::RootInlineBox
        , root()
        , UBIDI_DEFAULT_LTR
        , rootInlineBoxVisualRect
        , rootInlineBoxVisualRect
        , { }
        , { }
        , linehasContent
        , isLineFullyTruncatedInBlockDirection()
    });
}

void InlineDisplayContentBuilder::appendInlineBoxDisplayBox(const Line::Run& lineRun, const InlineLevelBox& inlineBox, const InlineRect& inlineBoxBorderBox, bool linehasContent, InlineDisplay::Boxes& boxes)
{
    ASSERT(lineRun.layoutBox().isInlineBox());
    ASSERT(inlineBox.isInlineBox());

    auto& layoutBox = lineRun.layoutBox();
    // This inline box showed up first on this line.
    ASSERT(inlineBox.isFirstBox());
    setInlineBoxGeometry(layoutBox, inlineBoxBorderBox, true);

    if (!linehasContent) {
        // While "<div><span> </span></div>" produces empty line with no display boxes, we still need to assign box geometry to the associated inline box.
        return;
    }

    auto inkOverflow = [&] {
        auto& style = !m_lineIndex ? layoutBox.firstLineStyle() : layoutBox.style();
        auto inkOverflow = FloatRect { inlineBoxBorderBox };
        m_contentHasInkOverflow = computeInkOverflowForInlineBox(inlineBox, style, inkOverflow) || m_contentHasInkOverflow;
        return inkOverflow;
    };

    boxes.append({ m_lineIndex
        , InlineDisplay::Box::Type::NonRootInlineBox
        , layoutBox
        , lineRun.bidiLevel()
        , inlineBoxBorderBox
        , inkOverflow()
        , { }
        , { }
        , inlineBox.hasContent()
        , isLineFullyTruncatedInBlockDirection()
        , isFirstLastBox(inlineBox)
    });
}

void InlineDisplayContentBuilder::appendSpanningInlineBoxDisplayBox(const Line::Run& lineRun, const InlineLevelBox& inlineBox, const InlineRect& inlineBoxBorderBox, bool linehasContent, InlineDisplay::Boxes& boxes)
{
    ASSERT(lineRun.layoutBox().isInlineBox());
    ASSERT(inlineBox.isInlineBox());
    ASSERT(!inlineBox.isFirstBox());

    if (!linehasContent) {
        // When a spanning inline box (e.g. <div>text<span><br></span></div>) lands on an empty line
        // (empty here means no content at all including line breaks, not just visually empty) then we
        // don't extend the spanning line box over to this line.
        return;
    }

    // Middle or end of the inline box. Let's stretch the box as needed.
    auto& layoutBox = lineRun.layoutBox();
    setInlineBoxGeometry(layoutBox, inlineBoxBorderBox, false);
    auto inkOverflow = [&] {
        auto& style = !m_lineIndex ? layoutBox.firstLineStyle() : layoutBox.style();
        auto inkOverflow = FloatRect { inlineBoxBorderBox };
        m_contentHasInkOverflow = computeInkOverflowForInlineBox(inlineBox, style, inkOverflow) || m_contentHasInkOverflow;
        return inkOverflow;
    };

    boxes.append({ m_lineIndex
        , InlineDisplay::Box::Type::NonRootInlineBox
        , layoutBox
        , lineRun.bidiLevel()
        , inlineBoxBorderBox
        , inkOverflow()
        , { }
        , { }
        , inlineBox.hasContent()
        , isLineFullyTruncatedInBlockDirection()
        , isFirstLastBox(inlineBox)
    });
}

void InlineDisplayContentBuilder::appendInlineDisplayBoxAtBidiBoundary(const Box& layoutBox, InlineDisplay::Boxes& boxes)
{
    // Geometries for inline boxes at bidi boundaries are computed at a post-process step.
    auto isContentful = true;
    boxes.append({ m_lineIndex
        , InlineDisplay::Box::Type::NonRootInlineBox
        , layoutBox
        , UBIDI_DEFAULT_LTR
        , { }
        , { }
        , { }
        , { }
        , isContentful
        , isLineFullyTruncatedInBlockDirection()
    });
}

void InlineDisplayContentBuilder::processNonBidiContent(const LineBuilder::LayoutResult& lineLayoutResult, const LineBox& lineBox, InlineDisplay::Boxes& boxes)
{
#ifndef NDEBUG
    auto hasContent = false;
    for (auto& lineRun : lineLayoutResult.inlineContent)
        hasContent = hasContent || lineRun.isContentful();
    ASSERT(lineLayoutResult.directionality.inlineBaseDirection == TextDirection::LTR || !hasContent);
#endif
    auto writingMode = root().style().writingMode();
    auto contentStartInVisualOrder = m_displayLine.topLeft();

    appendRootInlineBoxDisplayBox(flipRootInlineBoxRectToVisualForWritingMode(lineBox.logicalRectForRootInlineBox(), writingMode), lineBox.rootInlineBox().hasContent(), boxes);

    for (auto& lineRun : lineLayoutResult.inlineContent) {
        auto& layoutBox = lineRun.layoutBox();

        auto visualRectRelativeToRoot = [&](auto logicalRect) {
            auto visualRect = flipLogicalRectToVisualForWritingModeWithinLine(logicalRect, lineBox.logicalRect(), writingMode);
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
        if (lineRun.isBox() || lineRun.isListMarker()) {
            appendAtomicInlineLevelDisplayBox(lineRun
                , visualRectRelativeToRoot(lineBox.logicalBorderBoxForAtomicInlineLevelBox(layoutBox, formattingState().boxGeometry(layoutBox)))
                , boxes);
            continue;
        }
        if (lineRun.isInlineBoxStart()) {
            appendInlineBoxDisplayBox(lineRun
                , lineBox.inlineLevelBoxFor(lineRun)
                , visualRectRelativeToRoot(lineBox.logicalBorderBoxForInlineBox(layoutBox, formattingState().boxGeometry(layoutBox)))
                , lineBox.hasContent()
                , boxes);
            continue;
        }
        if (lineRun.isLineSpanningInlineBoxStart()) {
            appendSpanningInlineBoxDisplayBox(lineRun
                , lineBox.inlineLevelBoxFor(lineRun)
                , visualRectRelativeToRoot(lineBox.logicalBorderBoxForInlineBox(layoutBox, formattingState().boxGeometry(layoutBox)))
                , lineBox.hasContent()
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
    std::optional<size_t> unwind(const ElementBox& elementBox)
    {
        // Unwind the stack all the way to container box.
        if (!m_set.contains(&elementBox))
            return { };
        while (m_set.last() != &elementBox) {
            m_stack.removeLast();
            m_set.removeLast();
        }
        // Root is always a common ancestor.
        ASSERT(!m_stack.isEmpty());
        return m_stack.last();
    }

    void push(size_t displayBoxNodeIndexForelementBox, const ElementBox& elementBox)
    {
        m_stack.append(displayBoxNodeIndexForelementBox);
        ASSERT(!m_set.contains(&elementBox));
        m_set.add(&elementBox);
    }

private:
    Vector<size_t> m_stack;
    ListHashSet<const ElementBox*> m_set;
};

static inline size_t createdDisplayBoxNodeForElementBoxAndPushToAncestorStack(const ElementBox& elementBox, size_t displayBoxIndex, size_t parentDisplayBoxNodeIndex, DisplayBoxTree& displayBoxTree, AncestorStack& ancestorStack)
{
    auto displayBoxNodeIndex = displayBoxTree.append(parentDisplayBoxNodeIndex, displayBoxIndex);
    ancestorStack.push(displayBoxNodeIndex, elementBox);
    return displayBoxNodeIndex;
}

size_t InlineDisplayContentBuilder::ensureDisplayBoxForContainer(const ElementBox& elementBox, DisplayBoxTree& displayBoxTree, AncestorStack& ancestorStack, InlineDisplay::Boxes& boxes)
{
    ASSERT(elementBox.isInlineBox() || &elementBox == &root());
    if (auto lowestCommonAncestorIndex = ancestorStack.unwind(elementBox))
        return *lowestCommonAncestorIndex;
    auto enclosingDisplayBoxNodeIndexForContainer = ensureDisplayBoxForContainer(elementBox.parent(), displayBoxTree, ancestorStack, boxes);
    appendInlineDisplayBoxAtBidiBoundary(elementBox, boxes);
    return createdDisplayBoxNodeForElementBoxAndPushToAncestorStack(elementBox, boxes.size() - 1, enclosingDisplayBoxNodeIndexForContainer, displayBoxTree, ancestorStack);
}

struct IsFirstLastIndex {
    std::optional<size_t> first;
    std::optional<size_t> last;
};
using IsFirstLastIndexesMap = HashMap<const Box*, IsFirstLastIndex>;
void InlineDisplayContentBuilder::adjustVisualGeometryForDisplayBox(size_t displayBoxNodeIndex, InlineLayoutUnit& contentRightInInlineDirectionVisualOrder, InlineLayoutUnit lineBoxLogicalTop, const DisplayBoxTree& displayBoxTree, InlineDisplay::Boxes& boxes, const LineBox& lineBox, const IsFirstLastIndexesMap& isFirstLastIndexesMap)
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
        auto visualRect = flipLogicalRectToVisualForWritingModeWithinLine({ logicalRect.top(), contentRightInInlineDirectionVisualOrder, { }, logicalRect.height() }, lineBox.logicalRect(), writingMode);
        isHorizontalWritingMode ? visualRect.moveVertically(m_displayLine.top()) : visualRect.moveHorizontally(m_displayLine.left());
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

    auto* inlineBox = lineBox.inlineLevelBoxFor(layoutBox);
    ASSERT(inlineBox);
    auto computeInkOverflow = [&] {
        auto inkOverflow = FloatRect { displayBox.visualRectIgnoringBlockDirection() };
        m_contentHasInkOverflow = computeInkOverflowForInlineBox(*inlineBox, !m_lineIndex ? layoutBox.firstLineStyle() : layoutBox.style(), inkOverflow) || m_contentHasInkOverflow;
        displayBox.adjustInkOverflow(inkOverflow);
    };
    computeInkOverflow();

    setInlineBoxGeometry(layoutBox, displayBox.visualRectIgnoringBlockDirection(), isFirstBox);
    if (inlineBox->hasContent())
        displayBox.setHasContent();
}

void InlineDisplayContentBuilder::processBidiContent(const LineBuilder::LayoutResult& lineLayoutResult, const LineBox& lineBox, InlineDisplay::Boxes& boxes)
{
    ASSERT(lineLayoutResult.directionality.visualOrderList.size() <= lineLayoutResult.inlineContent.size());

    AncestorStack ancestorStack;
    auto displayBoxTree = DisplayBoxTree { };
    ancestorStack.push({ }, root());

    auto writingMode = root().style().writingMode();
    auto isHorizontalWritingMode = WebCore::isHorizontalWritingMode(writingMode);

    auto lineLogicalTop = isHorizontalWritingMode ? m_displayLine.top() : m_displayLine.left();
    auto lineLogicalLeft = isHorizontalWritingMode ? m_displayLine.left() : m_displayLine.top();
    auto contentStartInInlineDirectionVisualOrder = lineLogicalLeft + m_displayLine.contentLogicalLeftIgnoringInlineDirection();
    auto hasInlineBox = false;
    auto createDisplayBoxesInVisualOrder = [&] {

        auto rootInlineBoxVidsualRectInInlineDirection = lineBox.logicalRectForRootInlineBox();
        rootInlineBoxVidsualRectInInlineDirection.setLeft(m_displayLine.contentLogicalLeftIgnoringInlineDirection());
        appendRootInlineBoxDisplayBox(flipRootInlineBoxRectToVisualForWritingMode(rootInlineBoxVidsualRectInInlineDirection, root().style().writingMode()), lineBox.rootInlineBox().hasContent(), boxes);

        auto contentRightInInlineDirectionVisualOrder = contentStartInInlineDirectionVisualOrder;
        auto& inlineContent = lineLayoutResult.inlineContent;
        for (auto visualOrder : lineLayoutResult.directionality.visualOrderList) {
            ASSERT(inlineContent[visualOrder].bidiLevel() != InlineItem::opaqueBidiLevel);

            auto& lineRun = inlineContent[visualOrder];
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
            if (lineRun.isBox() || lineRun.isListMarker()) {
                auto isLeftToRightDirection = layoutBox.parent().style().isLeftToRightDirection();
                auto& boxGeometry = formattingState().boxGeometry(layoutBox);
                auto logicalRect = lineBox.logicalBorderBoxForAtomicInlineLevelBox(layoutBox, boxGeometry);
                auto visualRect = visualRectRelativeToRoot(logicalRect);
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
                } else if (!lineBox.inlineLevelBoxFor(lineRun).hasContent()) {
                    appendInlineDisplayBoxAtBidiBoundary(layoutBox, boxes);
                    createdDisplayBoxNodeForElementBoxAndPushToAncestorStack(downcast<ElementBox>(layoutBox), boxes.size() - 1, parentDisplayBoxNodeIndex, displayBoxTree, ancestorStack);
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
                auto* inlineLevelBox = lineBox.inlineLevelBoxFor(layoutBox);
                ASSERT(inlineLevelBox);
                auto isFirstBox = inlineLevelBox->isFirstBox();
                auto isLastBox = inlineLevelBox->isLastBox();
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

    auto handleTrailingOpenInlineBoxes = [&] {
        for (auto& lineRun : makeReversedRange(lineLayoutResult.inlineContent)) {
            if (!lineRun.isInlineBoxStart() || lineRun.bidiLevel() != InlineItem::opaqueBidiLevel)
                break;
            // These are trailing inline box start runs (without the closing inline box end <span> <-line breaks here</span>).
            // They don't participate in visual reordering (see createDisplayBoxesInVisualOrder above) as they we don't find them
            // empty at inline building time (see setBidiLevelForOpaqueInlineItems) due to trailing whitespace.
            // Non-empty inline boxes are normally get their display boxes generated when we process their content runs, but
            // these trailing runs have their content on the subsequent line(s).
            appendInlineBoxDisplayBox(lineRun
                , lineBox.inlineLevelBoxFor(lineRun)
                , boxes.last().visualRectIgnoringBlockDirection()
                , lineBox.hasContent()
                , boxes);
        }
    };
    handleTrailingOpenInlineBoxes();
}

void InlineDisplayContentBuilder::processFloatBoxes(const LineBuilder::LayoutResult&)
{
    // Float boxes are not part of the inline content so we don't construct inline display boxes for them.
    // However box geometry still needs flipping from logical to visual.
    // FIXME: Figure out how to preserve logical coordinates for subsequent layout frames and have visual output the same time. For the time being
    // this is done at LineLayout::constructContent.  
}

void InlineDisplayContentBuilder::collectInkOverflowForInlineBoxes(InlineDisplay::Boxes& boxes)
{
    if (!m_contentHasInkOverflow)
        return;
    // Visit the inline boxes and propagate ink overflow to their parents -except to the root inline box.
    // (e.g. <span style="font-size: 10px;">Small font size<span style="font-size: 300px;">Larger font size. This overflows the top most span.</span></span>).
    auto accumulatedInkOverflowRect = InlineRect { { }, { } };
    for (size_t index = boxes.size(); index--;) {
        auto& displayBox = boxes[index];

        auto mayHaveInkOverflow = displayBox.isText() || displayBox.isAtomicInlineLevelBox() || displayBox.isGenericInlineLevelBox() || displayBox.isNonRootInlineBox();
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

static float logicalBottomForTextDecorationContent(const InlineDisplay::Boxes& boxes, bool isHorizontalWritingMode)
{
    auto logicalBottom = std::optional<float> { };
    for (auto& displayBox : boxes) {
        if (displayBox.isRootInlineBox())
            continue;
        if (!displayBox.style().textDecorationsInEffect().contains(TextDecorationLine::Underline))
            continue;
        if (displayBox.isText() || displayBox.style().textDecorationSkipInk() == TextDecorationSkipInk::None) {
            auto contentLogicalBottom = isHorizontalWritingMode ? displayBox.bottom() : displayBox.right();
            logicalBottom = logicalBottom ? std::max(*logicalBottom, contentLogicalBottom) : contentLogicalBottom;
        }
    }
    // This function is not called unless there's at least one run on the line with TextDecorationLine::Underline.
    ASSERT(logicalBottom);
    return logicalBottom.value_or(0.f);
}

void InlineDisplayContentBuilder::collectInkOverflowForTextDecorations(InlineDisplay::Boxes& boxes)
{
    auto logicalBottomForTextDecoration = std::optional<float> { };
    auto writingMode = root().style().writingMode();
    auto isHorizontalWritingMode = WebCore::isHorizontalWritingMode(writingMode);

    for (auto& displayBox : boxes) {
        if (!displayBox.isText())
            continue;

        auto& style = displayBox.style();
        auto textDecorations = style.textDecorationsInEffect();
        if (!textDecorations)
            continue;

        auto decorationOverflow = [&] {
            if (!textDecorations.contains(TextDecorationLine::Underline))
                return visualOverflowForDecorations(style);

            if (!logicalBottomForTextDecoration)
                logicalBottomForTextDecoration = logicalBottomForTextDecorationContent(boxes, isHorizontalWritingMode);
            auto textRunLogicalOffsetFromLineBottom = *logicalBottomForTextDecoration - (isHorizontalWritingMode ? displayBox.bottom() : displayBox.right());
            return visualOverflowForDecorations(style, m_displayLine.baselineType(), { displayBox.height(), textRunLogicalOffsetFromLineBottom });
        }();

        if (!decorationOverflow.isEmpty()) {
            m_contentHasInkOverflow = true;
            auto inflatedVisualOverflowRect = [&] {
                auto inkOverflowRect = displayBox.inkOverflow();
                switch (writingMode) {
                case WritingMode::TopToBottom:
                    inkOverflowRect.inflate(decorationOverflow.left, decorationOverflow.top, decorationOverflow.right, decorationOverflow.bottom);
                    break;
                case WritingMode::LeftToRight:
                    inkOverflowRect.inflate(decorationOverflow.bottom, decorationOverflow.right, decorationOverflow.top, decorationOverflow.left);
                    break;
                case WritingMode::RightToLeft:
                    inkOverflowRect.inflate(decorationOverflow.top, decorationOverflow.right, decorationOverflow.bottom, decorationOverflow.left);
                    break;
                default:
                    ASSERT_NOT_REACHED();
                    break;
                }
                return inkOverflowRect;
            };
            displayBox.adjustInkOverflow(inflatedVisualOverflowRect());
        }
    }
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

InlineRect InlineDisplayContentBuilder::flipRootInlineBoxRectToVisualForWritingMode(const InlineRect& rootInlineBoxLogicalRect, WritingMode writingMode) const
{
    switch (writingMode) {
    case WritingMode::TopToBottom: {
        auto visualRect = rootInlineBoxLogicalRect;
        visualRect.moveBy({ m_displayLine.left(), m_displayLine.top() });
        return visualRect;
    }
    case WritingMode::LeftToRight:
    case WritingMode::RightToLeft: {
        // See InlineFormattingGeometry for more info.
        auto visualRect = InlineRect { rootInlineBoxLogicalRect.left(), rootInlineBoxLogicalRect.top(), rootInlineBoxLogicalRect.height(), rootInlineBoxLogicalRect.width() };
        visualRect.moveBy({ m_displayLine.left(), m_displayLine.top() });
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

}
}

