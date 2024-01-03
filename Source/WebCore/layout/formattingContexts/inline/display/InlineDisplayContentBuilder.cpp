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
#include "InlineContentAligner.h"
#include "InlineFormattingUtils.h"
#include "InlineTextBoxStyle.h"
#include "LayoutBoxGeometry.h"
#include "LayoutInitialContainingBlock.h"
#include "RenderStyleInlines.h"
#include "RubyFormattingContext.h"
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

static inline void convertBoxGeometryForWritingMode(BoxGeometry& boxGeometry, LayoutPoint borderBoxVisualTopLeft, WritingMode writingMode, bool isLeftToRightInlineDirection = true)
{
    auto isHorizontalWritingMode = WebCore::isHorizontalWritingMode(writingMode);
    auto isFlippedBlocksWritingMode = WebCore::isFlippedWritingMode(writingMode);

    boxGeometry.setTopLeft(borderBoxVisualTopLeft);
    if (isHorizontalWritingMode && !isFlippedBlocksWritingMode && isLeftToRightInlineDirection)
        return;

    auto flipHorizontalDecorationValues = [&] {
        boxGeometry.setHorizontalMargin({ boxGeometry.marginEnd(), boxGeometry.marginStart() });
        boxGeometry.setHorizontalBorder({ boxGeometry.borderEnd(), boxGeometry.borderStart() });
        if (boxGeometry.horizontalPadding())
            boxGeometry.setHorizontalPadding(Layout::HorizontalEdges { *boxGeometry.paddingEnd(), *boxGeometry.paddingStart() });
    };

    auto flipVerticalDecorationValues = [&] {
        boxGeometry.setVerticalMargin({ boxGeometry.marginAfter(), boxGeometry.marginBefore() });
        boxGeometry.setVerticalBorder({ boxGeometry.borderAfter(), boxGeometry.borderBefore() });
        if (boxGeometry.verticalPadding())
            boxGeometry.setVerticalPadding(Layout::VerticalEdges { *boxGeometry.paddingAfter(), *boxGeometry.paddingBefore() });
    };

    if (isHorizontalWritingMode) {
        if (!isLeftToRightInlineDirection)
            flipHorizontalDecorationValues();

        if (isFlippedBlocksWritingMode)
            flipVerticalDecorationValues();
        return;
    }

    // vertical-lr/rl with inline direction of ltr/rtl.
    auto contentBoxLogicalWidth = boxGeometry.contentBoxWidth();
    boxGeometry.setContentBoxWidth(boxGeometry.contentBoxHeight());
    boxGeometry.setContentBoxHeight(contentBoxLogicalWidth);

    auto horizontalSpaceForScrollbar = boxGeometry.horizontalSpaceForScrollbar();
    boxGeometry.setHorizontalSpaceForScrollbar(boxGeometry.verticalSpaceForScrollbar());
    boxGeometry.setVerticalSpaceForScrollbar(horizontalSpaceForScrollbar);

    auto verticalMargin = boxGeometry.verticalMargin();
    auto verticalBorder = VerticalEdges { boxGeometry.borderBefore(), boxGeometry.borderAfter() };
    boxGeometry.setVerticalMargin({ boxGeometry.marginBefore(), boxGeometry.marginAfter() });
    boxGeometry.setVerticalBorder({ boxGeometry.borderBefore(), boxGeometry.borderAfter() });
    auto verticalPadding = std::optional<VerticalEdges> { };
    if (boxGeometry.horizontalPadding()) {
        if (boxGeometry.verticalPadding())
            verticalPadding = { *boxGeometry.paddingBefore(), *boxGeometry.paddingAfter() };
        boxGeometry.setVerticalPadding(Layout::VerticalEdges { *boxGeometry.paddingStart(), *boxGeometry.paddingEnd() });
    }

    // Flip inline direction now that we have the horizontal decoration resolved.
    if (!isLeftToRightInlineDirection)
        flipHorizontalDecorationValues();

    // Flip horizontal decoration depending on whether the block direction is left to right or right to left.
    boxGeometry.setHorizontalMargin(!isFlippedBlocksWritingMode ? BoxGeometry::HorizontalMargin { verticalMargin.before, verticalMargin.after } : BoxGeometry::HorizontalMargin { verticalMargin.after, verticalMargin.before });
    boxGeometry.setHorizontalBorder(!isFlippedBlocksWritingMode ? Layout::HorizontalEdges { verticalBorder.top, verticalBorder.bottom } : Layout::HorizontalEdges { verticalBorder.bottom, verticalBorder.top });
    auto horizontalPadding = std::optional<Layout::HorizontalEdges> { };
    if (verticalPadding)
        horizontalPadding = !isFlippedBlocksWritingMode ? Layout::HorizontalEdges { verticalPadding->top, verticalPadding->bottom } : Layout::HorizontalEdges { verticalPadding->bottom, verticalPadding->top };
    boxGeometry.setHorizontalPadding(horizontalPadding);
}

static InlineLayoutPoint flipLogicalPointToVisualForWritingModeWithinLine(const InlineLayoutPoint& logicalPoint, const InlineRect& lineLogicalRect, WritingMode writingMode)
{
    switch (writingModeToBlockFlowDirection(writingMode)) {
    case BlockFlowDirection::TopToBottom:
        return logicalPoint;
    case BlockFlowDirection::BottomToTop:
        return { logicalPoint.x(), lineLogicalRect.height() - logicalPoint.y() };
    case BlockFlowDirection::RightToLeft:
    case BlockFlowDirection::LeftToRight:
        // FIXME: Find out why flipped vertical mode does not need special handling.
        return { logicalPoint.y(), logicalPoint.x() };
    default:
        ASSERT_NOT_REACHED();
        break;
    }
    return logicalPoint;
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

InlineDisplayContentBuilder::InlineDisplayContentBuilder(InlineFormattingContext& formattingContext, const ConstraintsForInlineContent& constraints, const LineBox& lineBox, const InlineDisplay::Line& displayLine)
    : m_formattingContext(formattingContext)
    , m_constraints(constraints)
    , m_lineBox(lineBox)
    , m_displayLine(displayLine)
{
    auto& initialContainingBlockGeometry = m_formattingContext.geometryForBox(FormattingContext::initialContainingBlock(root()), InlineFormattingContext::EscapeReason::InkOverflowNeedsInitialContiningBlockForStrokeWidth);
    m_initialContaingBlockSize = ceiledIntSize(LayoutSize { initialContainingBlockGeometry.contentBoxWidth(), initialContainingBlockGeometry.contentBoxHeight() });
}

InlineDisplay::Boxes InlineDisplayContentBuilder::build(const LineLayoutResult& lineLayoutResult)
{
    auto boxes = InlineDisplay::Boxes { };
    boxes.reserveInitialCapacity(lineLayoutResult.inlineContent.size() + lineBox().nonRootInlineLevelBoxes().size() + 1);

    auto contentNeedsBidiReordering = !lineLayoutResult.directionality.visualOrderList.isEmpty();
    if (contentNeedsBidiReordering)
        processBidiContent(lineLayoutResult, boxes);
    else
        processNonBidiContent(lineLayoutResult, boxes);
    processFloatBoxes(lineLayoutResult);
    processRubyContent(boxes, lineLayoutResult);

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
    auto& style = !lineIndex() ? inlineTextBox.firstLineStyle() : inlineTextBox.style();
    auto& content = inlineTextBox.content();
    auto& text = lineRun.textContent();
    auto isContentful = true;

    auto inkOverflow = [&] {
        auto inkOverflow = textRunRect;

        auto addLetterSpacingOverflow = [&] {
            auto letterSpacing = style.fontCascade().letterSpacing();
            if (letterSpacing >= 0)
                return;
            // Large negative letter spacing can produce text boxes with negative width (when glyphs position order gets completely backwards (123 turns into 321 starting at an offset))
            // Such spacing should go to ink overflow.
            auto textRunWidth = textRunRect.width();
            if (textRunWidth < 0) {
                inkOverflow.setWidth({ });
                inkOverflow.shiftLeftTo(textRunWidth);
            }
            // Last letter's negative spacing shrinks logical rect. Push it to ink overflow.
            inkOverflow.expand(-letterSpacing, { });
        };
        addLetterSpacingOverflow();

        auto addStrokeOverflow = [&] {
            inkOverflow.inflate(ceilf(style.computedStrokeWidth(m_initialContaingBlockSize)));
        };
        addStrokeOverflow();

        auto addTextShadow = [&] {
            auto textShadow = style.textShadowExtent();
            inkOverflow.inflate(-textShadow.top(), textShadow.right(), textShadow.bottom(), -textShadow.left());
        };
        addTextShadow();

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
        boxes.append({ lineIndex()
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
    boxes.append({ lineIndex()
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

    boxes.append({ lineIndex()
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
    auto isContentful = true;
    boxes.append({ lineIndex()
        , InlineDisplay::Box::Type::LineBreakBox
        , lineRun.layoutBox()
        , lineRun.bidiLevel()
        , lineBreakBoxRect
        , lineBreakBoxRect
        , lineRun.expansion()
        , { }
        , isContentful
        , isLineFullyTruncatedInBlockDirection()
    });
}

void InlineDisplayContentBuilder::appendAtomicInlineLevelDisplayBox(const Line::Run& lineRun, const InlineRect& borderBoxRect, InlineDisplay::Boxes& boxes)
{
    ASSERT(lineRun.layoutBox().isAtomicInlineLevelBox());
    auto& layoutBox = lineRun.layoutBox();

    auto isContentful = true;
    auto inkOverflow = [&] {
        auto inkOverflow = FloatRect { borderBoxRect };
        auto& style = !lineIndex() ? layoutBox.firstLineStyle() : layoutBox.style();
        computeInkOverflowForInlineLevelBox(style, inkOverflow);
        // Atomic inline box contribute to their inline box parents ink overflow at all times (e.g. <span><img></span>).
        m_contentHasInkOverflow = m_contentHasInkOverflow || &layoutBox.parent() != &root();
        return inkOverflow;
    };

    boxes.append({ lineIndex()
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
}

void InlineDisplayContentBuilder::appendRootInlineBoxDisplayBox(const InlineRect& rootInlineBoxVisualRect, bool lineHasContent, InlineDisplay::Boxes& boxes)
{
    boxes.append({ lineIndex()
        , InlineDisplay::Box::Type::RootInlineBox
        , root()
        , UBIDI_DEFAULT_LTR
        , rootInlineBoxVisualRect
        , rootInlineBoxVisualRect
        , { }
        , { }
        , lineHasContent
        , isLineFullyTruncatedInBlockDirection()
    });
}

void InlineDisplayContentBuilder::appendInlineBoxDisplayBox(const Line::Run& lineRun, const InlineLevelBox& inlineBox, const InlineRect& inlineBoxBorderBox, InlineDisplay::Boxes& boxes)
{
    ASSERT(lineRun.layoutBox().isInlineBox());
    ASSERT(inlineBox.isInlineBox());
    ASSERT((inlineBox.isFirstBox() && lineRun.isInlineBoxStart()) || (!inlineBox.isFirstBox() && lineRun.isLineSpanningInlineBoxStart()));

    auto& layoutBox = lineRun.layoutBox();
    m_hasSeenRubyBase = m_hasSeenRubyBase || layoutBox.isRubyBase();

    auto inkOverflow = [&] {
        auto& style = !lineIndex() ? layoutBox.firstLineStyle() : layoutBox.style();
        auto inkOverflow = FloatRect { inlineBoxBorderBox };
        m_contentHasInkOverflow = computeInkOverflowForInlineBox(inlineBox, style, inkOverflow) || m_contentHasInkOverflow;
        return inkOverflow;
    };

    boxes.append({ lineIndex()
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
    m_hasSeenRubyBase = m_hasSeenRubyBase || layoutBox.isRubyBase();
    boxes.append({ lineIndex()
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

void InlineDisplayContentBuilder::insertRubyAnnotationBox(const Box& annotationBox, size_t insertionPosition, const InlineRect& borderBoxRect, InlineDisplay::Boxes& boxes)
{
    boxes.insert(insertionPosition, { lineIndex()
        , InlineDisplay::Box::Type::AtomicInlineLevelBox
        , annotationBox
        , UBIDI_DEFAULT_LTR
        , borderBoxRect
        , borderBoxRect
        , { }
        , { }
        , true
        , isLineFullyTruncatedInBlockDirection()
    });
}

void InlineDisplayContentBuilder::processNonBidiContent(const LineLayoutResult& lineLayoutResult, InlineDisplay::Boxes& boxes)
{
#ifndef NDEBUG
    auto hasContent = false;
    for (auto& lineRun : lineLayoutResult.inlineContent)
        hasContent = hasContent || lineRun.isContentful();
    ASSERT(lineLayoutResult.directionality.inlineBaseDirection == TextDirection::LTR || !hasContent);
#endif
    auto& lineBox = this->lineBox();
    auto writingMode = root().style().writingMode();
    auto isHorizontalWritingMode = WebCore::isHorizontalWritingMode(writingMode);
    auto isFlippedBlocksWritingMode = WebCore::isFlippedWritingMode(writingMode);
    auto contentStartInVisualOrder = m_displayLine.topLeft();
    Vector<size_t> blockLevelOutOfFlowBoxList;

    appendRootInlineBoxDisplayBox(flipRootInlineBoxRectToVisualForWritingMode(lineBox.logicalRectForRootInlineBox(), writingMode), lineBox.rootInlineBox().hasContent(), boxes);

    for (size_t index = 0; index < lineLayoutResult.inlineContent.size(); ++index) {
        auto& lineRun = lineLayoutResult.inlineContent[index];
        if (lineRun.isWordBreakOpportunity() || lineRun.isInlineBoxEnd())
            continue;
        auto& layoutBox = lineRun.layoutBox();

        if (lineRun.isOpaque()) {
            if (layoutBox.style().isOriginalDisplayInlineType()) {
                auto visualTopLeft = flipLogicalPointToVisualForWritingModeWithinLine({ lineBox.logicalRectForRootInlineBox().left() + lineRun.logicalLeft(), { } }, lineBox.logicalRect(), writingMode);
                visualTopLeft += contentStartInVisualOrder;
                formattingContext().geometryForBox(layoutBox).setTopLeft(toLayoutPoint(visualTopLeft));
                continue;
            }
            blockLevelOutOfFlowBoxList.append(index);
            continue;
        }

        auto logicalRect = [&]() -> InlineRect {
            if (lineRun.isText() || lineRun.isSoftLineBreak())
                return lineBox.logicalRectForTextRun(lineRun);
            if (lineRun.isHardLineBreak())
                return lineBox.logicalRectForLineBreakBox(layoutBox);

            auto& boxGeometry = formattingContext().geometryForBox(layoutBox);
            if (lineRun.isBox() || lineRun.isListMarker())
                return lineBox.logicalBorderBoxForAtomicInlineLevelBox(layoutBox, boxGeometry);
            if (lineRun.isInlineBoxStart() || lineRun.isLineSpanningInlineBoxStart())
                return lineBox.logicalBorderBoxForInlineBox(layoutBox, boxGeometry);
            ASSERT_NOT_REACHED();
            return { };
        }();
        auto visualRectRelativeToRoot = [&] {
            auto visualRect = flipLogicalRectToVisualForWritingModeWithinLine(logicalRect, lineBox.logicalRect(), writingMode);
            visualRect.moveBy(contentStartInVisualOrder);
            return visualRect;
        }();

        auto createDisplayBoxForRun = [&] {
            if (lineRun.isText())
                appendTextDisplayBox(lineRun, visualRectRelativeToRoot, boxes);
            else if (lineRun.isSoftLineBreak())
                appendSoftLineBreakDisplayBox(lineRun, visualRectRelativeToRoot, boxes);
            else if (lineRun.isHardLineBreak())
                appendHardLineBreakDisplayBox(lineRun, visualRectRelativeToRoot, boxes);
            else if (lineRun.isBox() || lineRun.isListMarker())
                appendAtomicInlineLevelDisplayBox(lineRun, visualRectRelativeToRoot, boxes);
            else if (lineRun.isInlineBoxStart() || lineRun.isLineSpanningInlineBoxStart()) {
                // Do not generate display boxes for inline boxes on non-contentful lines (e.g. <span></span>)
                if (lineBox.hasContent())
                    appendInlineBoxDisplayBox(lineRun, lineBox.inlineLevelBoxFor(lineRun), visualRectRelativeToRoot, boxes);
            } else
                ASSERT_NOT_REACHED();
        };
        createDisplayBoxForRun();

        auto updateAssociatedBoxGeometry = [&] {
            if (lineRun.isText() || lineRun.isSoftLineBreak())
                return;
            if (!lineBox.hasContent() && lineRun.isLineSpanningInlineBoxStart()) {
                // When a spanning inline box (e.g. <div>text<span><br></span></div>) lands on an empty line
                // (empty here means no content at all including line breaks, not just visually empty) then we
                // don't extend the spanning line box over to this line.
                return;
            }
            auto& boxGeometry = formattingContext().geometryForBox(layoutBox);
            if (lineRun.isInlineBoxStart() || lineRun.isLineSpanningInlineBoxStart()) {
                // Inline boxes need special (stretchy) box geometry handling.
                setInlineBoxGeometry(boxGeometry, visualRectRelativeToRoot, lineBox.inlineLevelBoxFor(lineRun).isFirstBox());
                return;
            }
            boxGeometry.setTopLeft(toLayoutPoint(visualRectRelativeToRoot.topLeft()));
            if (lineRun.isHardLineBreak())
                boxGeometry.setContentBoxHeight(toLayoutUnit(logicalRect.height()));
            if (!isHorizontalWritingMode || isFlippedBlocksWritingMode)
                convertBoxGeometryForWritingMode(boxGeometry, toLayoutPoint(visualRectRelativeToRoot.topLeft()), writingMode);
        };
        updateAssociatedBoxGeometry();
    }
    setGeometryForBlockLevelOutOfFlowBoxes(blockLevelOutOfFlowBoxList, lineLayoutResult.inlineContent);
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

    void push(size_t displayBoxNodeIndexForContainer, const ElementBox& elementBox)
    {
        m_stack.append(displayBoxNodeIndexForContainer);
        ASSERT(!m_set.contains(&elementBox));
        m_set.add(&elementBox);
    }

private:
    Vector<size_t> m_stack;
    ListHashSet<const ElementBox*> m_set;
};

static inline size_t createDisplayBoxNodeForContainerAndPushToAncestorStack(const ElementBox& elementBox, size_t displayBoxIndex, size_t parentDisplayBoxNodeIndex, DisplayBoxTree& displayBoxTree, AncestorStack& ancestorStack)
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
    return createDisplayBoxNodeForContainerAndPushToAncestorStack(elementBox, boxes.size() - 1, enclosingDisplayBoxNodeIndexForContainer, displayBoxTree, ancestorStack);
}

struct IsFirstLastIndex {
    std::optional<size_t> first;
    std::optional<size_t> last;
};
using IsFirstLastIndexesMap = HashMap<const Box*, IsFirstLastIndex>;
void InlineDisplayContentBuilder::adjustVisualGeometryForDisplayBox(size_t displayBoxNodeIndex, InlineLayoutUnit& contentRightInInlineDirectionVisualOrder, InlineLayoutUnit lineBoxLogicalTop, const DisplayBoxTree& displayBoxTree, InlineDisplay::Boxes& boxes, const IsFirstLastIndexesMap& isFirstLastIndexesMap)
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
            auto& boxGeometry = formattingContext().geometryForBox(layoutBox);
            auto boxMarginLeft = marginLeftInInlineDirection(boxGeometry, isLeftToRightDirection);

            auto borderBoxLeft = InlineLayoutUnit { contentRightInInlineDirectionVisualOrder + boxMarginLeft };
            setLeftForWritingMode(boxGeometry, LayoutUnit { borderBoxLeft }, writingMode);
            setLeftForWritingMode(displayBox, borderBoxLeft, writingMode);

            contentRightInInlineDirectionVisualOrder += boxGeometry.marginBoxWidth();
        } else {
            auto wordSpacingMargin = displayBox.isWordSeparator() ? layoutBox.style().fontCascade().wordSpacing() : 0.0f;
            setLeftForWritingMode(displayBox, contentRightInInlineDirectionVisualOrder + wordSpacingMargin, writingMode);
            contentRightInInlineDirectionVisualOrder += (isHorizontalWritingMode ? displayBox.width() : displayBox.height()) + wordSpacingMargin;
        }
        return;
    }

    auto& boxGeometry = formattingContext().geometryForBox(layoutBox);
    auto isLeftToRightDirection = layoutBox.style().isLeftToRightDirection();
    auto isFirstLastIndexes = isFirstLastIndexesMap.get(&layoutBox);
    auto isFirstBox = isFirstLastIndexes.first && *isFirstLastIndexes.first == displayBoxNodeIndex;
    auto isLastBox = isFirstLastIndexes.last && *isFirstLastIndexes.last == displayBoxNodeIndex;
    auto beforeInlineBoxContent = [&] {
        auto logicalRect = lineBox().logicalBorderBoxForInlineBox(layoutBox, boxGeometry);
        auto visualRect = flipLogicalRectToVisualForWritingModeWithinLine({ logicalRect.top(), contentRightInInlineDirectionVisualOrder, { }, logicalRect.height() }, lineBox().logicalRect(), writingMode);
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
        adjustVisualGeometryForDisplayBox(childDisplayBoxNodeIndex, contentRightInInlineDirectionVisualOrder, lineBoxLogicalTop, displayBoxTree, boxes, isFirstLastIndexesMap);

    auto afterInlineBoxContent = [&] {
        auto shouldApplyRightSide = (isLeftToRightDirection && isLastBox) || (!isLeftToRightDirection && isFirstBox);
        if (!shouldApplyRightSide)
            return setRightForWritingMode(displayBox, contentRightInInlineDirectionVisualOrder, writingMode);

        contentRightInInlineDirectionVisualOrder += borderRightInInlineDirection(boxGeometry, isLeftToRightDirection) + paddingRightInInlineDirection(boxGeometry, isLeftToRightDirection);
        contentRightInInlineDirectionVisualOrder += layoutBox.isRubyBase() ? RubyFormattingContext::baseEndAdditionalLogicalWidth(layoutBox, displayBox, contentRightInInlineDirectionVisualOrder - displayBox.left(), formattingContext()) : 0.f;
        setRightForWritingMode(displayBox, contentRightInInlineDirectionVisualOrder, writingMode);
        contentRightInInlineDirectionVisualOrder += marginRightInInlineDirection(boxGeometry, isLeftToRightDirection);
    };
    afterInlineBoxContent();

    auto* inlineBox = lineBox().inlineLevelBoxFor(layoutBox);
    ASSERT(inlineBox);
    auto computeInkOverflow = [&] {
        auto inkOverflow = FloatRect { displayBox.visualRectIgnoringBlockDirection() };
        m_contentHasInkOverflow = computeInkOverflowForInlineBox(*inlineBox, !lineIndex() ? layoutBox.firstLineStyle() : layoutBox.style(), inkOverflow) || m_contentHasInkOverflow;
        displayBox.adjustInkOverflow(inkOverflow);
    };
    computeInkOverflow();

    setInlineBoxGeometry(boxGeometry, displayBox.visualRectIgnoringBlockDirection(), isFirstBox);
    if (inlineBox->hasContent())
        displayBox.setHasContent();
}

void InlineDisplayContentBuilder::processBidiContent(const LineLayoutResult& lineLayoutResult, InlineDisplay::Boxes& boxes)
{
    ASSERT(lineLayoutResult.directionality.visualOrderList.size() <= lineLayoutResult.inlineContent.size());

    AncestorStack ancestorStack;
    auto displayBoxTree = DisplayBoxTree { };
    ancestorStack.push({ }, root());

    auto& lineBox = this->lineBox();
    auto writingMode = root().style().writingMode();
    auto isHorizontalWritingMode = WebCore::isHorizontalWritingMode(writingMode);

    auto lineLogicalTop = isHorizontalWritingMode ? m_displayLine.top() : m_displayLine.left();
    auto lineLogicalLeft = isHorizontalWritingMode ? m_displayLine.left() : m_displayLine.top();
    auto contentStartInInlineDirectionVisualOrder = lineLogicalLeft + m_displayLine.contentLogicalLeftIgnoringInlineDirection();
    auto hasInlineBox = false;
    auto createDisplayBoxesInVisualOrder = [&] {

        Vector<size_t> blockLevelOutOfFlowBoxList;
        auto rootInlineBoxVisualRectInInlineDirection = lineBox.logicalRectForRootInlineBox();
        rootInlineBoxVisualRectInInlineDirection.setLeft(m_displayLine.contentLogicalLeftIgnoringInlineDirection());
        appendRootInlineBoxDisplayBox(flipRootInlineBoxRectToVisualForWritingMode(rootInlineBoxVisualRectInInlineDirection, root().style().writingMode()), lineBox.rootInlineBox().hasContent(), boxes);

        auto contentRightInInlineDirectionVisualOrder = contentStartInInlineDirectionVisualOrder;
        auto& inlineContent = lineLayoutResult.inlineContent;
        for (size_t index = 0; index < lineLayoutResult.directionality.visualOrderList.size(); ++index) {
            auto logicalIndex = lineLayoutResult.directionality.visualOrderList[index];
            ASSERT(inlineContent[logicalIndex].bidiLevel() != InlineItem::opaqueBidiLevel);

            auto& lineRun = inlineContent[logicalIndex];
            auto needsDisplayBoxOrGeometrySetting = !lineRun.isWordBreakOpportunity() && !lineRun.isInlineBoxEnd();
            if (!needsDisplayBoxOrGeometrySetting)
                continue;

            auto& layoutBox = lineRun.layoutBox();
            auto parentDisplayBoxNodeIndex = ensureDisplayBoxForContainer(layoutBox.parent(), displayBoxTree, ancestorStack, boxes);
            hasInlineBox = hasInlineBox || parentDisplayBoxNodeIndex || lineRun.isInlineBoxStart() || lineRun.isLineSpanningInlineBoxStart();

            if (lineRun.isOpaque()) {
                if (layoutBox.style().isOriginalDisplayInlineType()) {
                    auto& boxGeometry = formattingContext().geometryForBox(layoutBox);
                    isHorizontalWritingMode ? boxGeometry.setTopLeft({ contentRightInInlineDirectionVisualOrder, m_displayLine.top() }) : boxGeometry.setTopLeft({ m_displayLine.left(), contentRightInInlineDirectionVisualOrder });
                    continue;
                }
                blockLevelOutOfFlowBoxList.append(index);
                continue;
            }

            auto logicalRect = [&]() -> InlineRect {
                if (lineRun.isText() || lineRun.isSoftLineBreak())
                    return lineBox.logicalRectForTextRun(lineRun);
                if (lineRun.isHardLineBreak())
                    return lineBox.logicalRectForLineBreakBox(layoutBox);
                if (lineRun.isInlineBoxStart() || lineRun.isLineSpanningInlineBoxStart()) {
                    // Computed at a later stage.
                    return { { }, { } };
                }
                auto& boxGeometry = formattingContext().geometryForBox(layoutBox);
                if (lineRun.isBox() || lineRun.isListMarker())
                    return lineBox.logicalBorderBoxForAtomicInlineLevelBox(layoutBox, boxGeometry);
                ASSERT_NOT_REACHED();
                return { };
            }();

            auto visualRectRelativeToRoot = [&] {
                auto visualRect = flipLogicalRectToVisualForWritingModeWithinLine(logicalRect, lineBox.logicalRect(), writingMode);
                if (isHorizontalWritingMode) {
                    visualRect.setLeft(contentRightInInlineDirectionVisualOrder);
                    visualRect.moveVertically(lineLogicalTop);
                    return visualRect;
                }
                visualRect.setTop(contentRightInInlineDirectionVisualOrder);
                visualRect.moveHorizontally(lineLogicalTop);
                return visualRect;
            }();

            if (lineRun.isText()) {
                auto wordSpacingMargin = lineRun.isWordSeparator() ? layoutBox.style().fontCascade().wordSpacing() : 0.0f;
                isHorizontalWritingMode ? visualRectRelativeToRoot.moveHorizontally(wordSpacingMargin) : visualRectRelativeToRoot.moveVertically(wordSpacingMargin);
                appendTextDisplayBox(lineRun, visualRectRelativeToRoot, boxes);
                contentRightInInlineDirectionVisualOrder += logicalRect.width() + wordSpacingMargin;
                displayBoxTree.append(parentDisplayBoxNodeIndex, boxes.size() - 1);
                continue;
            }
            if (lineRun.isSoftLineBreak()) {
                ASSERT((isHorizontalWritingMode && !visualRectRelativeToRoot.width()) || (!isHorizontalWritingMode && !visualRectRelativeToRoot.height()));
                appendSoftLineBreakDisplayBox(lineRun, visualRectRelativeToRoot, boxes);
                displayBoxTree.append(parentDisplayBoxNodeIndex, boxes.size() - 1);
                continue;
            }
            if (lineRun.isHardLineBreak()) {
                ASSERT((isHorizontalWritingMode && !visualRectRelativeToRoot.width()) || (!isHorizontalWritingMode && !visualRectRelativeToRoot.height()));
                auto& boxGeometry = formattingContext().geometryForBox(layoutBox);

                appendHardLineBreakDisplayBox(lineRun, visualRectRelativeToRoot, boxes);
                boxGeometry.setTopLeft(toLayoutPoint(visualRectRelativeToRoot.topLeft()));
                boxGeometry.setContentBoxHeight(toLayoutUnit(visualRectRelativeToRoot.height()));

                displayBoxTree.append(parentDisplayBoxNodeIndex, boxes.size() - 1);
                continue;
            }
            if (lineRun.isBox() || lineRun.isListMarker()) {
                auto isLeftToRightDirection = layoutBox.parent().style().isLeftToRightDirection();
                auto& boxGeometry = formattingContext().geometryForBox(layoutBox);
                auto boxMarginLeft = marginLeftInInlineDirection(boxGeometry, isLeftToRightDirection);
                isHorizontalWritingMode ? visualRectRelativeToRoot.moveHorizontally(boxMarginLeft) : visualRectRelativeToRoot.moveVertically(boxMarginLeft);

                appendAtomicInlineLevelDisplayBox(lineRun, visualRectRelativeToRoot, boxes);
                boxGeometry.setTopLeft(toLayoutPoint(visualRectRelativeToRoot.topLeft()));

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
                    setInlineBoxGeometry(formattingContext().geometryForBox(layoutBox), { { }, { } }, true);
                    continue;
                }
                auto isEmptyInlineBox = [&] {
                    // FIXME: Maybe we should not tag ruby bases with annotation boxes only contentful?
                    if (!lineBox.inlineLevelBoxFor(lineRun).hasContent())
                        return true;
                    if (!layoutBox.isRubyBase() || !is<ElementBox>(layoutBox))
                        return false;
                    auto& rubyBaseLayoutBox = downcast<ElementBox>(layoutBox);
                    // Let's create empty inline boxes for ruby bases with annotation only.
                    if (!rubyBaseLayoutBox.firstChild() || (rubyBaseLayoutBox.firstChild() == rubyBaseLayoutBox.lastChild() && rubyBaseLayoutBox.firstChild()->isRubyAnnotationBox()))
                        return true;
                    // Let's check if we actually don't have a contentful run inside this ruby base.
                    for (size_t nextLogicalRunIndex = logicalIndex + 1; nextLogicalRunIndex < inlineContent.size(); ++nextLogicalRunIndex) {
                        auto& lineRun = inlineContent[nextLogicalRunIndex];
                        if (lineRun.isInlineBoxEnd() && &lineRun.layoutBox() == &rubyBaseLayoutBox)
                            break;
                        if (lineRun.isContentful())
                            return false;
                    }
                    return true;
                };
                if (isEmptyInlineBox()) {
                    appendInlineDisplayBoxAtBidiBoundary(layoutBox, boxes);
                    createDisplayBoxNodeForContainerAndPushToAncestorStack(downcast<ElementBox>(layoutBox), boxes.size() - 1, parentDisplayBoxNodeIndex, displayBoxTree, ancestorStack);
                }
                continue;
            }
            ASSERT_NOT_REACHED();
        }
        setGeometryForBlockLevelOutOfFlowBoxes(blockLevelOutOfFlowBoxList, inlineContent, lineLayoutResult.directionality.visualOrderList);
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
                adjustVisualGeometryForDisplayBox(childDisplayBoxNodeIndex, contentRightInInlineDirectionVisualOrder, lineLogicalTop, displayBoxTree, boxes, isFirstLastIndexesMap);
        };
        adjustVisualGeometryWithInlineBoxes();
    };
    handleInlineBoxes();

    auto handleTrailingOpenInlineBoxes = [&] {
        for (auto& lineRun : makeReversedRange(lineLayoutResult.inlineContent)) {
            if (!lineRun.isInlineBoxStart() || lineRun.bidiLevel() != InlineItem::opaqueBidiLevel)
                break;
            // These are trailing inline box start runs (without the closing inline box end <span> <-line breaks here</span>).
            // They don't participate in visual reordering (see createDisplayBoxesInVisualOrder above) as we don't find them
            // empty at inline item list building time (see setBidiLevelForOpaqueInlineItems) due to trailing whitespace.
            // Non-empty inline boxes are normally get their display boxes generated when we process their content runs, but
            // these trailing runs have their content on the subsequent line(s).
            auto& inlineBox = lineBox.inlineLevelBoxFor(lineRun);
            auto visualRect = boxes.last().visualRectIgnoringBlockDirection();
            appendInlineBoxDisplayBox(lineRun, inlineBox, visualRect, boxes);
            setInlineBoxGeometry(formattingContext().geometryForBox(lineRun.layoutBox()), { visualRect }, inlineBox.isFirstBox());
        }
    };
    handleTrailingOpenInlineBoxes();
}

void InlineDisplayContentBuilder::processFloatBoxes(const LineLayoutResult&)
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

static inline size_t runIndex(auto i, auto listSize, auto isLeftToRightDirection)
{
    if (isLeftToRightDirection)
        return i;
    auto lastIndex = listSize - 1;
    return lastIndex - i;
}

static inline void setGeometryForOutOfFlowBoxes(const Vector<size_t>& indexListOfOutOfFlowBoxes, std::optional<size_t> firstOutOfFlowIndexWithPreviousInflowSibling, const Line::RunList& lineRuns, const Vector<int32_t>& visualOrderList, InlineFormattingContext& formattingContext, const LineBox&, const InlineDisplay::Line& displayLine, const ConstraintsForInlineContent& constraints)
{
    auto& rootStyle = formattingContext.root().style();
    auto writingMode = rootStyle.writingMode();
    auto isLeftToRightDirection = rootStyle.isLeftToRightDirection();
    auto isHorizontalWritingMode = WebCore::isHorizontalWritingMode(writingMode);
    auto outOfFlowBoxListSize = indexListOfOutOfFlowBoxes.size();

    auto outOfFlowBox = [&](size_t i) -> const Box& {
        auto outOfFlowRunIndex = indexListOfOutOfFlowBoxes[runIndex(i, outOfFlowBoxListSize, isLeftToRightDirection)];
        return (visualOrderList.isEmpty() ? lineRuns[outOfFlowRunIndex] : lineRuns[visualOrderList[outOfFlowRunIndex]]).layoutBox();
    };
    // Set geometry on "before inflow content" boxes first, followed by the "after inflow content" list.
    auto beforeAfterBoundary = firstOutOfFlowIndexWithPreviousInflowSibling.value_or(outOfFlowBoxListSize);
    // These out of flow boxes "sit" on the line start (they are before any inflow content e.g. <div><div class=out-of-flow></div>some text<div>)
    auto visualLeftInInlineDirection = isLeftToRightDirection ? constraints.horizontal().logicalLeft : constraints.visualLeft() + constraints.horizontal().logicalWidth;
    auto visualTopLeft = isHorizontalWritingMode ? LayoutPoint(visualLeftInInlineDirection, displayLine.top()) : LayoutPoint(displayLine.left(), visualLeftInInlineDirection);
    for (size_t i = 0; i < beforeAfterBoundary; ++i)
        formattingContext.geometryForBox(outOfFlowBox(i)).setTopLeft(visualTopLeft);
    // These out of flow boxes are all _after_ an inflow content and get "wrapped" to the next line.
    isHorizontalWritingMode ? visualTopLeft.setY(displayLine.bottom()) : visualTopLeft.setX(displayLine.right());
    for (size_t i = beforeAfterBoundary; i < outOfFlowBoxListSize; ++i)
        formattingContext.geometryForBox(outOfFlowBox(i)).setTopLeft(visualTopLeft);
}

void InlineDisplayContentBuilder::setGeometryForBlockLevelOutOfFlowBoxes(const Vector<size_t>& indexListOfOutOfFlowBoxes, const Line::RunList& lineRuns, const Vector<int32_t>& visualOrderList)
{
    if (indexListOfOutOfFlowBoxes.isEmpty())
        return;

    // Block level boxes are placed either at the start of the line or "under" depending whether they have previous inflow sibling.
    // Here we figure out if a particular out of flow box has an inflow sibling or not.
    // 1. Find the first inflow content. Any out of flow box after this gets moved _under_ the line box.
    // 2. Loop through the out of flow boxes (indexListOfOutOfFlowBoxes) and set their vertical geometry depending whether they are before or after the first inflow content.
    // Note that there's an extra layer of directionality here: in case of right to left inline direction, the before inflow content check starts from the right edge and progresses in a leftward manner
    // and not in visual order. However this is not logical order either (which is more about bidi than inline direction). So LTR starts at the left while RTL starts at the right and in
    // both cases jumping from run to run in bidi order.
    auto& formattingContext = this->formattingContext();
    auto isLeftToRightDirection = root().style().isLeftToRightDirection();

    auto firstContentfulInFlowRunIndex = std::optional<size_t> { };
    auto contentListSize = visualOrderList.isEmpty() ? lineRuns.size() : visualOrderList.size();
    for (size_t i = 0; i < contentListSize; ++i) {
        auto index = runIndex(i, contentListSize, isLeftToRightDirection);
        auto& lineRun = visualOrderList.isEmpty() ? lineRuns[index] : lineRuns[visualOrderList[index]];
        if (lineRun.layoutBox().isInFlow() && Line::Run::isContentfulOrHasDecoration(lineRun, formattingContext)) {
            firstContentfulInFlowRunIndex = index;
            break;
        }
    }

    if (!firstContentfulInFlowRunIndex) {
        setGeometryForOutOfFlowBoxes(indexListOfOutOfFlowBoxes, { }, lineRuns, visualOrderList, formattingContext, lineBox(), m_displayLine, constraints());
        return;
    }

    auto firstOutOfFlowIndexWithPreviousInflowSibling = std::optional<size_t> { };
    auto outOfFlowBoxListSize = indexListOfOutOfFlowBoxes.size();
    for (size_t i = 0; i < outOfFlowBoxListSize; ++i) {
        auto outOfFlowIndex = runIndex(i, outOfFlowBoxListSize, isLeftToRightDirection);
        auto hasPreviousInflowSibling = (isLeftToRightDirection && indexListOfOutOfFlowBoxes[outOfFlowIndex] > *firstContentfulInFlowRunIndex) || (!isLeftToRightDirection && indexListOfOutOfFlowBoxes[outOfFlowIndex] < *firstContentfulInFlowRunIndex);
        if (hasPreviousInflowSibling) {
            firstOutOfFlowIndexWithPreviousInflowSibling = outOfFlowIndex;
            break;
        }
    }
    setGeometryForOutOfFlowBoxes(indexListOfOutOfFlowBoxes, firstOutOfFlowIndexWithPreviousInflowSibling, lineRuns, visualOrderList, formattingContext, lineBox(), m_displayLine, constraints());
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
                switch (writingModeToBlockFlowDirection(writingMode)) {
                case BlockFlowDirection::TopToBottom:
                case BlockFlowDirection::BottomToTop:
                    inkOverflowRect.inflate(decorationOverflow.left, decorationOverflow.top, decorationOverflow.right, decorationOverflow.bottom);
                    break;
                case BlockFlowDirection::LeftToRight:
                    inkOverflowRect.inflate(decorationOverflow.bottom, decorationOverflow.right, decorationOverflow.top, decorationOverflow.left);
                    break;
                case BlockFlowDirection::RightToLeft:
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

size_t InlineDisplayContentBuilder::processRubyBase(size_t rubyBaseStart, InlineDisplay::Boxes& displayBoxes, Vector<WTF::Range<size_t>>& interlinearRubyColumnRangeList, Vector<size_t>& rubyBaseStartIndexListWithAnnotation)
{
    auto& formattingContext = this->formattingContext();
    auto& rubyBaseLayoutBox = displayBoxes[rubyBaseStart].layoutBox();
    auto baseMarginBoxRect = BoxGeometry::marginBoxRect(formattingContext.geometryForBox(rubyBaseLayoutBox));
    auto* annotationBox = rubyBaseLayoutBox.associatedRubyAnnotationBox();

    if (annotationBox)
        rubyBaseStartIndexListWithAnnotation.append(rubyBaseStart);

    auto rubyBaseEnd = displayBoxes.size();
    auto& rubyBox = rubyBaseLayoutBox.parent();
    auto& rubyBoxParent = rubyBox.parent();
    for (auto index = rubyBaseStart + 1; index < displayBoxes.size(); ++index) {
        auto& baseContentLayoutBox = displayBoxes[index].layoutBox();
        if (baseContentLayoutBox.isRubyBase()) {
            index = processRubyBase(index, displayBoxes, interlinearRubyColumnRangeList, rubyBaseStartIndexListWithAnnotation);
            if (RubyFormattingContext::hasInterlinearAnnotation(baseContentLayoutBox)) {
                auto& interlinearAnnotationBox = *baseContentLayoutBox.associatedRubyAnnotationBox();
                auto isNestedRubyBase = [&] {
                    for (auto* ancestor = &baseContentLayoutBox.parent(); ancestor != &root(); ancestor = &ancestor->parent()) {
                        if (ancestor->isRubyBase())
                            return ancestor == &rubyBaseLayoutBox;
                    }
                    return false;
                };
                if (isNestedRubyBase()) {
                    auto nestedAnnotationMarginBoxRect = BoxGeometry::marginBoxRect(formattingContext.geometryForBox(interlinearAnnotationBox));
                    baseMarginBoxRect.expandToContain(nestedAnnotationMarginBoxRect);
                }
            }

            if (index == displayBoxes.size()) {
                rubyBaseEnd = index;
                break;
            }
        }

        auto& layoutBox = displayBoxes[index].layoutBox();
        if (&layoutBox.parent() == &rubyBox || &layoutBox.parent() == &rubyBoxParent) {
            rubyBaseEnd = index;
            break;
        }
    }

    if (RubyFormattingContext::hasInterlinearAnnotation(rubyBaseLayoutBox))
        interlinearRubyColumnRangeList.append({ rubyBaseStart, rubyBaseEnd });

    if (annotationBox) {
        auto placeAndSizeAnnotationBox = [&] {
            auto adjustedBaseMarginBoxRect = baseMarginBoxRect;
            if (RubyFormattingContext::hasInterCharacterAnnotation(rubyBaseLayoutBox)) {
                auto letterSpacing = LayoutUnit { rubyBaseLayoutBox.style().letterSpacing() };
                // FIXME: Consult the LineBox to see if letter spacing indeed applies.
                adjustedBaseMarginBoxRect.setWidth(std::max(0_lu, adjustedBaseMarginBoxRect.width() - letterSpacing));
            }

            auto visualBorderBoxTopLeft = RubyFormattingContext::placeAnnotationBox(rubyBaseLayoutBox, adjustedBaseMarginBoxRect, formattingContext);
            auto visualContentBoxSize = RubyFormattingContext::sizeAnnotationBox(rubyBaseLayoutBox, formattingContext);
            auto& annotationBoxGeometry = formattingContext.geometryForBox(*annotationBox);
            annotationBoxGeometry.setTopLeft(toLayoutPoint(visualBorderBoxTopLeft));
            annotationBoxGeometry.setContentBoxSize(toLayoutSize(visualContentBoxSize));
        };
        placeAndSizeAnnotationBox();
    }
    return rubyBaseEnd;
}

void InlineDisplayContentBuilder::processRubyContent(InlineDisplay::Boxes& displayBoxes, const LineLayoutResult& lineLayoutResult)
{
    if (root().isRubyAnnotationBox())
        RubyFormattingContext::applyAnnotationAlignmentOffset(displayBoxes, lineLayoutResult.ruby.annotationAlignmentOffset, formattingContext());

    if (!m_hasSeenRubyBase)
        return;

    HashSet<CheckedPtr<const Box>> lineSpanningRubyBaseList;
    for (auto& lineRun : lineLayoutResult.inlineContent) {
        if (lineRun.isLineSpanningInlineBoxStart() && lineRun.layoutBox().isRubyBase())
            lineSpanningRubyBaseList.add(&lineRun.layoutBox());
    }

    auto rubyBasesMayHaveCollapsed = !lineLayoutResult.directionality.visualOrderList.isEmpty();
    RubyFormattingContext::applyAlignmentOffsetList(displayBoxes, lineLayoutResult.ruby.baseAlignmentOffsetList, rubyBasesMayHaveCollapsed ? RubyFormattingContext::RubyBasesMayNeedResizing::Yes : RubyFormattingContext::RubyBasesMayNeedResizing::No, formattingContext());

    Vector<WTF::Range<size_t>> interlinearRubyColumnRangeList;
    Vector<size_t> rubyBaseStartIndexListWithAnnotation;
    for (size_t index = 1; index < displayBoxes.size(); ++index) {
        auto& displayBox = displayBoxes[index];
        if (!displayBox.isNonRootInlineBox() || !displayBox.layoutBox().isRubyBase())
            continue;
        if (lineSpanningRubyBaseList.contains(&displayBox.layoutBox())) {
            // FIXME: Base content split across multiple lines don't affect annotation atm.
            continue;
        }

        index = processRubyBase(index, displayBoxes, interlinearRubyColumnRangeList, rubyBaseStartIndexListWithAnnotation);
    }
    applyRubyOverhang(displayBoxes, interlinearRubyColumnRangeList);

    for (auto baseIndex : makeReversedRange(rubyBaseStartIndexListWithAnnotation)) {
        auto& annotationBox = *displayBoxes[baseIndex].layoutBox().associatedRubyAnnotationBox();
        auto visualBorderBoxRect = BoxGeometry::borderBoxRect(formattingContext().geometryForBox(annotationBox));
        insertRubyAnnotationBox(annotationBox, baseIndex + 1, { visualBorderBoxRect }, displayBoxes);
    }
}

void InlineDisplayContentBuilder::applyRubyOverhang(InlineDisplay::Boxes& displayBoxes, const Vector<WTF::Range<size_t>>& interlinearRubyColumnRangeList)
{
    // FIXME: We are only supposed to apply overhang when annotation box is wider than base, but at this point we can't tell (this needs to be addressed together with annotation box sizing).
    if (interlinearRubyColumnRangeList.isEmpty())
        return;

    auto isHorizontalWritingMode = root().style().isHorizontalWritingMode();
    auto& formattingContext = this->formattingContext();
    for (auto startEndPair : interlinearRubyColumnRangeList) {
        ASSERT(startEndPair);
        if (startEndPair.distance() == 1)
            continue;

        auto rubyBaseStart = startEndPair.begin();
        auto& rubyBaseLayoutBox = displayBoxes[rubyBaseStart].layoutBox();
        ASSERT(rubyBaseLayoutBox.isRubyBase());
        ASSERT(RubyFormattingContext::hasInterlinearAnnotation(rubyBaseLayoutBox));

        auto beforeOverhang = RubyFormattingContext::overhangForAnnotationBefore(rubyBaseLayoutBox, rubyBaseStart, displayBoxes, formattingContext);
        auto afterOverhang = RubyFormattingContext::overhangForAnnotationAfter(rubyBaseLayoutBox, { rubyBaseStart, startEndPair.end() }, displayBoxes, formattingContext);

        // FIXME: If this turns out to be a pref bottleneck, make sure we pass in the accumulated shift to overhangForAnnotationBefore/after and
        // offset all box geometry as we check for overlap.
        auto moveBoxRangeToVisualLeft = [&](auto start, auto end, auto shiftValue) {
            for (auto index = start; index <= end; ++index) {
                isHorizontalWritingMode ? displayBoxes[index].moveHorizontally(LayoutUnit { -shiftValue }) : displayBoxes[index].moveVertically(LayoutUnit { -shiftValue });
                auto updateAnnotationGeometryIfNeeded = [&] {
                    auto& layoutBox = displayBoxes[index].layoutBox();
                    if (!layoutBox.isRubyBase() || !layoutBox.associatedRubyAnnotationBox())
                        return;
                    auto& annotationBoxGeometry = formattingContext.geometryForBox(*layoutBox.associatedRubyAnnotationBox());
                    isHorizontalWritingMode ? annotationBoxGeometry.moveHorizontally(LayoutUnit { -shiftValue }) : annotationBoxGeometry.moveVertically(LayoutUnit { -shiftValue });
                };
                updateAnnotationGeometryIfNeeded();
            }
        };
        if (beforeOverhang)
            moveBoxRangeToVisualLeft(rubyBaseStart, displayBoxes.size() - 1, beforeOverhang);
        if (afterOverhang) {
            auto hasJustifiedAdjacentAfterContent = [&] {
                if (startEndPair.end() == displayBoxes.size())
                    return false;
                auto& afterRubyBaseDisplayBox = displayBoxes[startEndPair.end()];
                if (afterRubyBaseDisplayBox.layoutBox().isRubyBase()) {
                    // Adjacent content is also a ruby base.
                    return false;
                }
                return !!afterRubyBaseDisplayBox.expansion().horizontalExpansion;
            };
            if (hasJustifiedAdjacentAfterContent()) {
                auto& afterRubyBaseDisplayBox = displayBoxes[startEndPair.end()];
                auto expansion = afterRubyBaseDisplayBox.expansion();
                auto inflateValue = afterOverhang + beforeOverhang;
                afterRubyBaseDisplayBox.setExpansion({ expansion.behavior, expansion.horizontalExpansion + inflateValue });
                afterRubyBaseDisplayBox.expandHorizontally(inflateValue);
            }
            moveBoxRangeToVisualLeft(startEndPair.end(), displayBoxes.size() - 1, afterOverhang);
        }
    }
}

void InlineDisplayContentBuilder::setInlineBoxGeometry(Layout::BoxGeometry& boxGeometry, const InlineRect& rect, bool isFirstInlineBoxFragment)
{
    auto adjustedSize = LayoutSize { LayoutUnit::fromFloatCeil(rect.width()), LayoutUnit::fromFloatCeil(rect.height()) };
    auto adjustedRect = Rect { LayoutPoint { rect.topLeft() }, adjustedSize };
    if (!isFirstInlineBoxFragment) {
        auto enclosingBorderBoxRect = BoxGeometry::borderBoxRect(boxGeometry);
        enclosingBorderBoxRect.expandToContain(adjustedRect);
        adjustedRect = enclosingBorderBoxRect;
    }
    boxGeometry.setTopLeft(adjustedRect.topLeft());
    auto contentBoxHeight = adjustedRect.height() - (boxGeometry.verticalBorder() + boxGeometry.verticalPadding().value_or(0_lu));
    auto contentBoxWidth = adjustedRect.width() - (boxGeometry.horizontalBorder() + boxGeometry.horizontalPadding().value_or(0_lu));
    boxGeometry.setContentBoxHeight(contentBoxHeight);
    boxGeometry.setContentBoxWidth(contentBoxWidth);
}

InlineRect InlineDisplayContentBuilder::flipLogicalRectToVisualForWritingModeWithinLine(const InlineRect& logicalRect, const InlineRect& lineLogicalRect, WritingMode writingMode) const
{
    switch (writingModeToBlockFlowDirection(writingMode)) {
    case BlockFlowDirection::TopToBottom:
        return logicalRect;
    case BlockFlowDirection::BottomToTop: {
        auto bottomOffset = lineLogicalRect.height() - logicalRect.bottom();
        return { bottomOffset, logicalRect.left(), logicalRect.width(), logicalRect.height() };
    }
    case BlockFlowDirection::LeftToRight: {
        // Flip content such that the top (visual left) is now relative to the line bottom instead of the line top.
        auto bottomOffset = lineLogicalRect.height() - logicalRect.bottom();
        return { logicalRect.left(), bottomOffset, logicalRect.height(), logicalRect.width() };
    }
    case BlockFlowDirection::RightToLeft:
        // See InlineFormattingUtils for more info.
        return { logicalRect.left(), logicalRect.top(), logicalRect.height(), logicalRect.width() };
    default:
        ASSERT_NOT_REACHED();
        break;
    }
    return logicalRect;
}

InlineRect InlineDisplayContentBuilder::flipRootInlineBoxRectToVisualForWritingMode(const InlineRect& rootInlineBoxLogicalRect, WritingMode writingMode) const
{
    switch (writingModeToBlockFlowDirection(writingMode)) {
    case BlockFlowDirection::TopToBottom:
    case BlockFlowDirection::BottomToTop: {
        auto visualRect = rootInlineBoxLogicalRect;
        visualRect.moveBy({ m_displayLine.left(), m_displayLine.top() });
        return visualRect;
    }
    case BlockFlowDirection::LeftToRight:
    case BlockFlowDirection::RightToLeft: {
        // See InlineFormattingUtils for more info.
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

template <typename BoxType, typename LayoutUnitType>
void InlineDisplayContentBuilder::setLeftForWritingMode(BoxType& box, LayoutUnitType logicalLeft, WritingMode writingMode) const
{
    switch (writingModeToBlockFlowDirection(writingMode)) {
    case BlockFlowDirection::TopToBottom:
    case BlockFlowDirection::BottomToTop:
        box.setLeft(logicalLeft);
        break;
    case BlockFlowDirection::LeftToRight:
    case BlockFlowDirection::RightToLeft:
        box.setTop(logicalLeft);
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
}

void InlineDisplayContentBuilder::setRightForWritingMode(InlineDisplay::Box& displayBox, InlineLayoutUnit logicalRight, WritingMode writingMode) const
{
    switch (writingModeToBlockFlowDirection(writingMode)) {
    case BlockFlowDirection::TopToBottom:
    case BlockFlowDirection::BottomToTop:
        displayBox.setRight(logicalRight);
        break;
    case BlockFlowDirection::LeftToRight:
    case BlockFlowDirection::RightToLeft:
        displayBox.setBottom(logicalRight);
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
}

}
}

