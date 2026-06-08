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
#include "LayoutBoxInlines.h"
#include "LayoutInitialContainingBlock.h"
#include "RenderStyle+GettersInlines.h"
#include "RubyFormattingContext.h"
#include "TextUtil.h"
#include <ranges>
#include <wtf/ListHashSet.h>
#include <wtf/Range.h>
#include <wtf/text/MakeString.h>
#include <wtf/unicode/CharacterNames.h>

using WTF::Range;

namespace WebCore {
namespace Layout {

static inline LayoutUnit NODELETE marginLineLeft(const Layout::BoxGeometry& boxGeometry, WritingMode writingMode)
{
    return writingMode.isBidiLTR() ? boxGeometry.marginStart() : boxGeometry.marginEnd();
}

static inline LayoutUnit NODELETE marginLineRight(const Layout::BoxGeometry& boxGeometry, WritingMode writingMode)
{
    return writingMode.isBidiLTR() ? boxGeometry.marginEnd() : boxGeometry.marginStart();
}

static inline LayoutUnit NODELETE borderLineLeft(const Layout::BoxGeometry& boxGeometry, WritingMode writingMode)
{
    return writingMode.isBidiLTR() ? boxGeometry.borderStart() : boxGeometry.borderEnd();
}

static inline LayoutUnit NODELETE borderLineRight(const Layout::BoxGeometry& boxGeometry,  WritingMode writingMode)
{
    return writingMode.isBidiLTR() ? boxGeometry.borderEnd() : boxGeometry.borderStart();
}

static inline LayoutUnit NODELETE paddingLineLeft(const Layout::BoxGeometry& boxGeometry,  WritingMode writingMode)
{
    return writingMode.isBidiLTR() ? boxGeometry.paddingStart() : boxGeometry.paddingEnd();
}

static inline LayoutUnit NODELETE paddingLineRight(const Layout::BoxGeometry& boxGeometry,  WritingMode writingMode)
{
    return writingMode.isBidiLTR() ? boxGeometry.paddingEnd() : boxGeometry.paddingStart();
}

static inline EnumSet<InlineDisplay::Box::PositionWithinInlineLevelBox> NODELETE isFirstLastBox(const InlineLevelBox& inlineBox)
{
    auto positionWithinInlineLevelBox = EnumSet<InlineDisplay::Box::PositionWithinInlineLevelBox> { };
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
    boxes.reserveInitialCapacity(lineLayoutResult.runs.size() + lineBox().nonRootInlineLevelBoxes().size() + 1);

    auto contentNeedsBidiReordering = !lineLayoutResult.directionality.visualOrderList.isEmpty();
    if (contentNeedsBidiReordering)
        processBidiContent(lineLayoutResult, boxes);
    else
        processNonBidiContent(lineLayoutResult, boxes);

    insertRubyAnnotationBoxes(processRubyContent(boxes.mutableSpan(), lineLayoutResult), boxes);

    auto newDisplayBoxes = boxes.mutableSpan();
    collectInkOverflowForTextDecorations(newDisplayBoxes);
    collectInkOverflowForInlineBoxes(newDisplayBoxes);
    return boxes;
}

InlineDisplay::Boxes InlineDisplayContentBuilder::buildTextOnlyContent(const LineLayoutResult& lineLayoutResult)
{
    auto boxes = InlineDisplay::Boxes { };
    if (lineLayoutResult.runs.size() > 1)
        boxes.reserveInitialCapacity(lineLayoutResult.runs.size() + 1);

    auto rootInlineBoxRect = lineBox().logicalRectForRootInlineBox();
    rootInlineBoxRect.moveBy(m_displayLine.topLeft());

    appendRootInlineBoxDisplayBox(rootInlineBoxRect, true, boxes);

    for (auto& lineRun : lineLayoutResult.runs) {
        auto runRect = InlineRect { rootInlineBoxRect.top(), rootInlineBoxRect.left() + lineRun.logicalLeft(), lineRun.logicalWidth(), rootInlineBoxRect.height() };
        if (lineRun.isText()) {
            appendTextDisplayBox(lineRun, runRect, boxes);
            continue;
        }
        if (lineRun.isSoftLineBreak()) {
            appendSoftLineBreakDisplayBox(lineRun, runRect, boxes);
            continue;
        }
        ASSERT_NOT_REACHED();
    }
    collectInkOverflowForTextDecorations(boxes);
    return boxes;
}

static inline bool computeInkOverflowForInlineLevelBox(const RenderStyle& style, FloatRect& inkOverflow)
{
    auto hasInkOverflow = false;

    auto inflateWithOutline = [&] {
        if (!style.hasOutlineInVisualOverflow())
            return;
        inkOverflow.inflate(style.usedOutlineSize());
        hasInkOverflow = true;
    };
    inflateWithOutline();

    auto inflateWithBoxShadow = [&] {
        // FIXME: Use `Style::shadowOutsetExtent` to get all 4 extents at once after static cast to `int` in `shadowVerticalExtent` is understood.
        auto [topBoxShadow, bottomBoxShadow] = Style::shadowVerticalExtent(style.boxShadow(), style.usedZoomForLength());
        auto [leftBoxShadow, rightBoxShadow] = Style::shadowHorizontalExtent(style.boxShadow(), style.usedZoomForLength());
        if (!topBoxShadow && !bottomBoxShadow && !leftBoxShadow && !rightBoxShadow)
            return;
        inkOverflow.inflate(-leftBoxShadow.toFloat(), -topBoxShadow.toFloat(), rightBoxShadow.toFloat(), bottomBoxShadow.toFloat());
        hasInkOverflow = true;
    };
    inflateWithBoxShadow();

    return hasInkOverflow;
}

static inline bool hasInlineBoxInkOverflow(const InlineLevelBox& inlineBox, const RenderStyle& style)
{
    ASSERT(inlineBox.isInlineBox());
    return style.hasOutlineInVisualOverflow() || !style.boxShadow().isNone() || inlineBox.hasTextEmphasis();
}

static inline void adjustInkOverflowForInlineBox(const Box& layoutBox, const ElementBox& rootBox, const RenderStyle& style, FloatRect& inkOverflow)
{
    if (style.hasOutlineInVisualOverflow())
        inkOverflow.inflate(style.usedOutlineSize());

    if (!style.boxShadow().isNone()) {
        auto [topBoxShadow, bottomBoxShadow] = Style::shadowVerticalExtent(style.boxShadow(), style.usedZoomForLength());
        auto [leftBoxShadow, rightBoxShadow] = Style::shadowHorizontalExtent(style.boxShadow(), style.usedZoomForLength());
        if (topBoxShadow || bottomBoxShadow || leftBoxShadow || rightBoxShadow)
            inkOverflow.inflate(-leftBoxShadow.toFloat(), -topBoxShadow.toFloat(), rightBoxShadow.toFloat(), bottomBoxShadow.toFloat());
    }

    auto [textEmphasisAbove, textEmphasisBelow] = InlineFormattingUtils::textEmphasisForInlineBox(layoutBox, rootBox);
    if (textEmphasisAbove || textEmphasisBelow)
        inkOverflow.inflate(0.f, textEmphasisAbove, 0.f, textEmphasisBelow);
}

void InlineDisplayContentBuilder::appendTextDisplayBox(const Line::Run& lineRun, const InlineRect& textRunRect, InlineDisplay::Boxes& boxes)
{
    ASSERT(lineRun.isText() && is<InlineTextBox>(lineRun.layoutBox()));

    CheckedRef inlineTextBox = downcast<InlineTextBox>(lineRun.layoutBox());
    auto& style = isFirstFormattedLine() ? inlineTextBox->firstLineStyle() : inlineTextBox->style();
    auto& content = inlineTextBox->content();
    auto& text = lineRun.textContent();
    auto isContentful = true;

    m_hasSeenTextDecoration = m_hasSeenTextDecoration || (isFirstFormattedLine() ? inlineTextBox->parent().firstLineStyle().textDecorationLineInEffect() : inlineTextBox->parent().style().textDecorationLineInEffect());

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
            inkOverflow.inflate(ceilf(style.usedStrokeWidth(m_initialContaingBlockSize)));
        };
        addStrokeOverflow();

        auto addTextShadow = [&] {
            auto textShadow = Style::shadowOutsetExtent(style.textShadow(), style.usedZoomForLength());
            inkOverflow.inflate(-textShadow.top(), textShadow.right(), textShadow.bottom(), -textShadow.left());
        };
        addTextShadow();

        auto addGlyphOverflow = [&] {
            auto glyphOverflow = lineRun.glyphOverflow();
            if (glyphOverflow.isEmpty())
                return;

            // Maxed-out glyph overflow values indicate arithmetic overflow. Fallback to collecting overflow post-measure.
            constexpr size_t maximumAscent = 31;
            constexpr size_t maximumDescent = 7;
            if (glyphOverflow.top == maximumAscent || glyphOverflow.bottom == maximumDescent) {
                auto enclosingAscentAndDescent = TextUtil::enclosingGlyphBoundsForText(StringView(content).substring(text.start, text.length), style, inlineTextBox->shouldUseSimpleGlyphOverflowCodePath() ? TextUtil::ShouldUseSimpleGlyphOverflowCodePath::Yes : TextUtil::ShouldUseSimpleGlyphOverflowCodePath::No);
                auto& fontMetrics = style.metricsOfPrimaryFont();
                glyphOverflow.top = std::max(0.f, InlineFormattingUtils::snapToInt(-enclosingAscentAndDescent.ascent, inlineTextBox) - InlineFormattingUtils::ascent(fontMetrics, FontBaseline::Alphabetic, inlineTextBox));
                glyphOverflow.bottom = std::max(0.f, InlineFormattingUtils::snapToInt(enclosingAscentAndDescent.descent, inlineTextBox) - InlineFormattingUtils::descent(fontMetrics, FontBaseline::Alphabetic, inlineTextBox));
            }
            inkOverflow.inflate(glyphOverflow.top, { }, glyphOverflow.bottom, { });
        };
        addGlyphOverflow();

        return inkOverflow;
    }();

    m_contentHasInkOverflow = m_contentHasInkOverflow || (&inlineTextBox->parent() != &root() && textRunRect != inkOverflow);

    if (inlineTextBox->isCombined()) {
        static auto objectReplacementCharacterString = NeverDestroyed<String> { span(objectReplacementCharacter) };
        // The rendered text is the actual combined content, while the "original" one is blank.
        boxes.append({ lineIndex()
            , InlineDisplay::Box::Type::Text
            , inlineTextBox
            , lineRun.bidiLevel()
            , textRunRect
            , inkOverflow
            , isFirstFormattedLine()
            , lineRun.expansion()
            , InlineDisplay::Box::Text { text.start, 1, objectReplacementCharacterString, content }
            , isContentful
            , isLineFullyTruncatedInBlockDirection()
        });
        return;
    }

    auto adjustedContentToRender = [&] {
        return text.needsHyphen ? makeString(StringView(content).substring(text.start, text.length), style.hyphenString()) : String();
    };
    auto shapingBoundary = [&] {
        if (lineRun.isShapingBoundaryStart())
            return InlineDisplay::Box::Text::ShapingBoundary::Start;
        if (lineRun.isShapingBoundaryEnd())
            return InlineDisplay::Box::Text::ShapingBoundary::End;
        if (lineRun.isInsideShapingBoundary())
            return InlineDisplay::Box::Text::ShapingBoundary::Inside;
        return InlineDisplay::Box::Text::ShapingBoundary::NotApplicable;
    };
    boxes.append({ lineIndex()
        , lineRun.isWordSeparator() ? InlineDisplay::Box::Type::WordSeparator : InlineDisplay::Box::Type::Text
        , inlineTextBox
        , lineRun.bidiLevel()
        , textRunRect
        , inkOverflow
        , isFirstFormattedLine()
        , lineRun.expansion()
        , InlineDisplay::Box::Text { text.start, text.length, content, adjustedContentToRender(), text.needsHyphen, shapingBoundary() }
        , isContentful
        , isLineFullyTruncatedInBlockDirection()
    });
}

void InlineDisplayContentBuilder::appendSoftLineBreakDisplayBox(const Line::Run& lineRun, const InlineRect& softLineBreakRunRect, InlineDisplay::Boxes& boxes) const
{
    ASSERT(lineRun.textContent().length && is<InlineTextBox>(lineRun.layoutBox()));

    CheckedRef layoutBox = lineRun.layoutBox();
    auto& text = lineRun.textContent();
    auto isContentful = true;

    boxes.append({ lineIndex()
        , InlineDisplay::Box::Type::SoftLineBreak
        , layoutBox
        , lineRun.bidiLevel()
        , softLineBreakRunRect
        , softLineBreakRunRect
        , isFirstFormattedLine()
        , lineRun.expansion()
        , InlineDisplay::Box::Text { text.start, text.length, downcast<InlineTextBox>(layoutBox).content() }
        , isContentful
        , isLineFullyTruncatedInBlockDirection()
    });
}

void InlineDisplayContentBuilder::appendHardLineBreakDisplayBox(const Line::Run& lineRun, const InlineRect& lineBreakBoxRect, InlineDisplay::Boxes& boxes) const
{
    auto isContentful = true;
    boxes.append({ lineIndex()
        , InlineDisplay::Box::Type::LineBreakBox
        , lineRun.layoutBox()
        , lineRun.bidiLevel()
        , lineBreakBoxRect
        , lineBreakBoxRect
        , isFirstFormattedLine()
        , lineRun.expansion()
        , { }
        , isContentful
        , isLineFullyTruncatedInBlockDirection()
    });
}

void InlineDisplayContentBuilder::appendAtomicInlineLevelDisplayBox(const Line::Run& lineRun, const InlineRect& borderBoxRect, InlineDisplay::Boxes& boxes)
{
    ASSERT(lineRun.layoutBox().isAtomicInlineBox());
    CheckedRef layoutBox = lineRun.layoutBox();

    auto isContentful = true;
    auto inkOverflow = [&] {
        auto inkOverflow = FloatRect { borderBoxRect };
        CheckedRef style = isFirstFormattedLine() ? layoutBox->firstLineStyle() : layoutBox->style();
        computeInkOverflowForInlineLevelBox(style, inkOverflow);
        // Atomic inline box contribute to their inline box parents ink overflow at all times (e.g. <span><img></span>).
        m_contentHasInkOverflow = m_contentHasInkOverflow || &layoutBox->parent() != &root();
        return inkOverflow;
    };

    boxes.append({ lineIndex()
        , InlineDisplay::Box::Type::AtomicInlineBox
        , layoutBox
        , lineRun.bidiLevel()
        , borderBoxRect
        , inkOverflow()
        , isFirstFormattedLine()
        , lineRun.expansion()
        , { }
        , isContentful
        , isLineFullyTruncatedInBlockDirection()
    });
}

void InlineDisplayContentBuilder::appendBlockLevelDisplayBox(const Line::Run& lineRun, const InlineRect& borderBoxRect, InlineDisplay::Boxes& boxes)
{
    ASSERT(lineRun.isBlock());
    CheckedRef layoutBox = lineRun.layoutBox();

    auto isContentful = true;
    boxes.append({ lineIndex()
        , InlineDisplay::Box::Type::BlockLevelBox
        , layoutBox
        , lineRun.bidiLevel()
        , borderBoxRect
        , borderBoxRect
        , isFirstFormattedLine()
        , { }
        , { }
        , isContentful
        , { }
    });
}

void InlineDisplayContentBuilder::appendRootInlineBoxDisplayBox(const InlineRect& rootInlineBoxVisualRect, bool lineHasContent, InlineDisplay::Boxes& boxes) const
{
    boxes.append({ lineIndex()
        , InlineDisplay::Box::Type::RootInlineBox
        , root()
        , UBIDI_DEFAULT_LTR
        , rootInlineBoxVisualRect
        , rootInlineBoxVisualRect
        , isFirstFormattedLine()
        , { }
        , { }
        , lineHasContent
        , isLineFullyTruncatedInBlockDirection()
    });
}

static inline bool isNestedInlineBoxWithDifferentFontCascadeFromParent(const Box& layoutBox, bool isFirstFormattedLine)
{
    if (!layoutBox.parent().isInlineBox())
        return false;
    auto& style = isFirstFormattedLine ? layoutBox.firstLineStyle() : layoutBox.style();
    CheckedRef parentStyle = isFirstFormattedLine ? layoutBox.parent().firstLineStyle() : layoutBox.parent().style();
    return style.fontCascade() != parentStyle->fontCascade();
}

void InlineDisplayContentBuilder::appendInlineBoxDisplayBox(const Line::Run& lineRun, const InlineLevelBox& inlineBox, const InlineRect& inlineBoxBorderBox, InlineDisplay::Boxes& boxes)
{
    ASSERT(lineRun.layoutBox().isInlineBox());
    ASSERT(inlineBox.isInlineBox());
    ASSERT((inlineBox.isFirstBox() && lineRun.isInlineBoxStart()) || (!inlineBox.isFirstBox() && lineRun.isLineSpanningInlineBoxStart()));

    CheckedRef layoutBox = lineRun.layoutBox();
    m_hasSeenRubyBase = m_hasSeenRubyBase || layoutBox->isRubyBase();
    m_hasSeenNestedInlineBoxesWithDifferentFontCascade = m_hasSeenNestedInlineBoxesWithDifferentFontCascade || isNestedInlineBoxWithDifferentFontCascadeFromParent(layoutBox, isFirstFormattedLine());

    auto inkOverflow = [&] {
        CheckedRef style = isFirstFormattedLine() ? layoutBox->firstLineStyle() : layoutBox->style();
        auto inkOverflow = FloatRect { inlineBoxBorderBox };
        m_contentHasInkOverflow |= hasInlineBoxInkOverflow(inlineBox, style);
        return inkOverflow;
    };

    boxes.append({ lineIndex()
        , InlineDisplay::Box::Type::NonRootInlineBox
        , layoutBox
        , lineRun.bidiLevel()
        , inlineBoxBorderBox
        , inkOverflow()
        , isFirstFormattedLine()
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
        , isFirstFormattedLine()
        , { }
        , { }
        , isContentful
        , isLineFullyTruncatedInBlockDirection()
    });
}

void InlineDisplayContentBuilder::insertRubyAnnotationBoxes(const Vector<size_t>& rubyBaseStartIndexListWithAnnotation, InlineDisplay::Boxes& boxes)
{
    auto lineBoxLogicalRect = lineBox().logicalRect();
    auto writingMode = root().writingMode();
    auto isHorizontalWritingMode = writingMode.isHorizontal();

    for (auto baseIndex : rubyBaseStartIndexListWithAnnotation | std::views::reverse) {
        CheckedRef annotationBox = *boxes[baseIndex].layoutBox().associatedRubyAnnotationBox();
        auto annotationBorderBoxVisualRect = [&] {
            // FIXME: We may wanna go back to full logical geometry on BoxGeometry (instead of this with visual left) and resolve it when
            // render tree needs it.
            auto borderBoxLogicalRect = InlineRect { BoxGeometry::borderBoxRect(formattingContext().geometryForBox(annotationBox)) };
            borderBoxLogicalRect.setTop(borderBoxLogicalRect.top() - lineBoxLogicalRect.top());
            auto visualRect = mapInlineRectLogicalToVisual(borderBoxLogicalRect, lineBoxLogicalRect, writingMode);
            if (writingMode.isLineOverLeft())
                visualRect.setTop(borderBoxLogicalRect.left());
            isHorizontalWritingMode ? visualRect.moveVertically(lineBoxLogicalRect.top()) : visualRect.moveHorizontally(lineBoxLogicalRect.top());
            return visualRect;
        };
        auto borderBoxRect = annotationBorderBoxVisualRect();
        boxes.insert(baseIndex + 1, { lineIndex()
            , InlineDisplay::Box::Type::AtomicInlineBox
            , annotationBox
            , UBIDI_DEFAULT_LTR
            , borderBoxRect
            , borderBoxRect
            , isFirstFormattedLine()
            , { }
            , { }
            , true
            , isLineFullyTruncatedInBlockDirection()
        });
    }
}

void InlineDisplayContentBuilder::processNonBidiContent(const LineLayoutResult& lineLayoutResult, InlineDisplay::Boxes& boxes)
{
#ifndef NDEBUG
    auto hasContent = false;
    for (auto& lineRun : lineLayoutResult.runs)
        hasContent = hasContent || lineRun.isContentful();
    ASSERT(lineLayoutResult.directionality.inlineBaseDirection == TextDirection::LTR || !hasContent);
#endif
    auto& lineBox = this->lineBox();
    auto writingMode = root().style().writingMode();
    auto lineBoxLogicalRect = lineBox.logicalRect();
    auto lineBoxVisualOffset = m_displayLine.topLeft();
    auto lineHasBlockContent = lineLayoutResult.isBlockContent();

    auto rootLogicalRect = lineBox.logicalRectForRootInlineBox();
    auto rootVisualRect = mapInlineRectLogicalToVisual(rootLogicalRect, lineBoxLogicalRect, writingMode);
    rootVisualRect.moveBy(lineBoxVisualOffset);
    appendRootInlineBoxDisplayBox(rootVisualRect, lineBox.rootInlineBox().hasContent(), boxes);

    Vector<size_t> blockLevelOutOfFlowBoxList;
    auto textSpacingAdjustment = 0.f;
    for (size_t index = 0; index < lineLayoutResult.runs.size(); ++index) {
        auto& lineRun = lineLayoutResult.runs[index];
        if (lineRun.isWordBreakOpportunity() || lineRun.isInlineBoxEnd())
            continue;
        CheckedRef layoutBox = lineRun.layoutBox();

        if (lineRun.isOutOfFlow()) {
            if (layoutBox->style().originalDisplay().isInlineType()) {
                formattingContext().geometryForBox(layoutBox).setTopLeft({ lineBox.logicalRect().left() + lineBox.logicalRectForRootInlineBox().left() + lineRun.logicalLeft(), lineBox.logicalRect().top() });
                continue;
            }
            blockLevelOutOfFlowBoxList.append(index);
            continue;
        }

        auto adjustLogicalRectForTextSpacing = [&](InlineRect& rect) {
            rect.expandHorizontally(-textSpacingAdjustment);
            rect.moveHorizontally(textSpacingAdjustment);
        };

        auto logicalRect = [&]() -> InlineRect {
            if (lineRun.isText()) {
                auto rect = lineBox.logicalRectForTextRun(lineRun);
                if (textSpacingAdjustment) {
                    adjustLogicalRectForTextSpacing(rect);
                    // text-spacing adjustment is retrivied while handling the run corresponding to the inline box start and must be reset here.
                    textSpacingAdjustment = 0;
                }
                return rect;
            }
            if (lineRun.isSoftLineBreak())
                return lineBox.logicalRectForTextRun(lineRun);
            if (lineRun.isHardLineBreak())
                return lineBox.logicalRectForLineBreakBox(layoutBox);

            auto& boxGeometry = formattingContext().geometryForBox(layoutBox);
            if (lineRun.isAtomicInlineBox() || lineRun.isListMarker())
                return lineBox.logicalBorderBoxForAtomicInlineBox(layoutBox, boxGeometry);
            if (lineRun.isInlineBoxStart()) {
                auto rect = lineBox.logicalBorderBoxForInlineBox(layoutBox, boxGeometry);
                // The Run representing the InlineBoxStart transports the text spacing adjustment. The adjustment is used here and for the rect representing the following text node. Therefore, we should reset such adjustment when handling the next run which is text.
                if ((textSpacingAdjustment = lineRun.textSpacingAdjustment()))
                    adjustLogicalRectForTextSpacing(rect);
                return rect;
            }
            if (lineRun.isLineSpanningInlineBoxStart()) {
                // Ideally spanning inline boxes on block lines would not have borders and padding.
                if (lineHasBlockContent)
                    return lineBox.logicalContentBoxForInlineBox(layoutBox);
                return lineBox.logicalBorderBoxForInlineBox(layoutBox, boxGeometry);
            }
            if (lineRun.isBlock()) {
                ASSERT(lineHasBlockContent);
                auto borderBoxRect = BoxGeometry::borderBoxRect(boxGeometry);
                return { borderBoxRect.top(), borderBoxRect.left(), borderBoxRect.width(), borderBoxRect.height() };
            }

            ASSERT_NOT_REACHED();
            return { };
        }();

        auto visualRectRelativeToRoot = [&] {
            auto visualRect = mapInlineRectLogicalToVisual(logicalRect, lineBoxLogicalRect, writingMode);
            visualRect.moveBy(lineBoxVisualOffset);
            return visualRect;
        }();

        auto createDisplayBoxForRun = [&] {
            if (lineRun.isText())
                appendTextDisplayBox(lineRun, visualRectRelativeToRoot, boxes);
            else if (lineRun.isSoftLineBreak())
                appendSoftLineBreakDisplayBox(lineRun, visualRectRelativeToRoot, boxes);
            else if (lineRun.isHardLineBreak())
                appendHardLineBreakDisplayBox(lineRun, visualRectRelativeToRoot, boxes);
            else if (lineRun.isAtomicInlineBox() || lineRun.isListMarker())
                appendAtomicInlineLevelDisplayBox(lineRun, visualRectRelativeToRoot, boxes);
            else if (lineRun.isInlineBoxStart())
                appendInlineBoxDisplayBox(lineRun, lineBox.inlineLevelBoxFor(lineRun), visualRectRelativeToRoot, boxes);
            else if (lineRun.isLineSpanningInlineBoxStart()) {
                // Line-spanning inline boxes show up as DOMRects via getClientRects().
                // Empty rects are generally fine (e.g. <span><div></div></span>), but not when caused by intrusive floats preventing content from fitting on the line.
                auto canHaveDisplayBoxEvenWhenEmpty = lineLayoutResult.hasContentfulInFlowContent() || !lineLayoutResult.floatContent.hasIntrusiveFloat;
                if (canHaveDisplayBoxEvenWhenEmpty)
                    appendInlineBoxDisplayBox(lineRun, lineBox.inlineLevelBoxFor(lineRun), visualRectRelativeToRoot, boxes);
            } else if (lineRun.isBlock()) {
                // Block content should always be placed at the start of the content box even when floats shrink the line.
                auto adjustedVisualRect = [&] {
                    auto lineOffset = lineBoxLogicalRect.left() - lineLayoutResult.lineGeometry.initialLogicalTopLeft.x();
                    auto rect = visualRectRelativeToRoot;
                    writingMode.isHorizontal() ? rect.moveHorizontally(-lineOffset) : rect.moveVertically(-lineOffset);
                    return rect;
                };
                appendBlockLevelDisplayBox(lineRun, adjustedVisualRect(), boxes);
            } else
                ASSERT_NOT_REACHED();
        };
        createDisplayBoxForRun();

        auto updateAssociatedBoxGeometry = [&] {
            if (lineRun.isText() || lineRun.isSoftLineBreak())
                return;
            if (!lineLayoutResult.hasContentfulInFlowContent() && lineRun.isLineSpanningInlineBoxStart()) {
                // When a spanning inline box (e.g. <div>text<span><br></span></div>) lands on an empty line
                // (empty here means no content at all including line breaks, not just visually empty) then we
                // don't extend the spanning line box over to this line.
                return;
            }
            auto& boxGeometry = formattingContext().geometryForBox(layoutBox);
            auto borderBoxLogicalTopLeft = toLayoutPoint(lineBox.logicalRect().topLeft() + logicalRect.topLeft());
            if (lineRun.isInlineBoxStart() || lineRun.isLineSpanningInlineBoxStart()) {
                // Inline boxes need special (stretchy) box geometry handling.
                setInlineBoxGeometry(lineRun.layoutBox(), boxGeometry, { borderBoxLogicalTopLeft, logicalRect.size() }, lineBox.inlineLevelBoxFor(lineRun).isFirstBox());
                return;
            }
            boxGeometry.setTopLeft(borderBoxLogicalTopLeft);
            boxGeometry.setContentBoxSize({ logicalRect.width() - boxGeometry.horizontalBorderAndPadding(), logicalRect.height() - boxGeometry.verticalBorderAndPadding() });
        };
        updateAssociatedBoxGeometry();
    }
    setGeometryForBlockLevelOutOfFlowBoxes(blockLevelOutOfFlowBoxList, lineLayoutResult.runs);
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

    const Node& NODELETE root() const { return m_displayBoxNodes.first(); }
    Node& NODELETE at(size_t index) { return m_displayBoxNodes[index]; }
    const Node& NODELETE at(size_t index) const { return m_displayBoxNodes[index]; }

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
        if (!m_set.contains(elementBox))
            return { };
        while (m_set.last().ptr() != &elementBox) {
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
        m_set.add(elementBox);
    }

private:
    Vector<size_t> m_stack;
    ListHashSet<CheckedRef<const ElementBox>> m_set;
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
void InlineDisplayContentBuilder::adjustVisualGeometryForDisplayBox(size_t displayBoxNodeIndex, InlineLayoutUnit& contentLineRightEdge, InlineLayoutUnit lineBoxLogicalTop, const DisplayBoxTree& displayBoxTree, std::span<InlineDisplay::Box> boxes, const IsFirstLastIndexesMap& isFirstLastIndexesMap)
{
    auto rootWritingMode = root().writingMode();
    auto isHorizontalWritingMode = rootWritingMode.isHorizontal();
    // Non-inline box display boxes just need a horizontal adjustment while
    // inline box type of display boxes need
    // 1. horizontal adjustment and margin/border/padding start offsetting on the first box
    // 2. right edge computation including descendant content width and margin/border/padding end offsetting on the last box
    auto& displayBox = boxes[displayBoxTree.at(displayBoxNodeIndex).displayBoxIndex];
    CheckedRef layoutBox = displayBox.layoutBox();

    if (!displayBox.isNonRootInlineBox()) {
        auto lineLogicalLeft = rootWritingMode.isHorizontal()
            ? m_displayLine.left() : m_displayLine.top();

        if (displayBox.isAtomicInlineBox() || displayBox.isGenericInlineLevelBox()) {
            auto& boxGeometry = formattingContext().geometryForBox(layoutBox);
            auto boxMarginLineLeft = marginLineLeft(boxGeometry, layoutBox->parent().writingMode());

            auto borderBoxLeft = InlineLayoutUnit { lineLogicalLeft + contentLineRightEdge + boxMarginLineLeft };
            boxGeometry.setLeft(LayoutUnit { borderBoxLeft });
            auto logicalLeft = rootWritingMode.isLogicalLeftLineLeft()
                ? borderBoxLeft
                : lineBox().logicalRect().width() - (borderBoxLeft + boxGeometry.borderBoxWidth());
            setLogicalLeft(displayBox, logicalLeft, rootWritingMode);

            contentLineRightEdge += boxGeometry.marginBoxWidth();
        } else {
            auto wordSpacingMargin = displayBox.isWordSeparator() ? layoutBox->style().fontCascade().wordSpacing() : 0.0f;
            auto logicalLeft = contentLineRightEdge + wordSpacingMargin;
            auto logicalWidth = isHorizontalWritingMode ? displayBox.width() : displayBox.height();
            if (!rootWritingMode.isLogicalLeftLineLeft())
                logicalLeft = lineBox().logicalRect().width() - (logicalLeft + logicalWidth);
            setLogicalLeft(displayBox, lineLogicalLeft + logicalLeft, rootWritingMode);
            contentLineRightEdge += logicalWidth + wordSpacingMargin;
        }
        return;
    }

    auto& boxGeometry = formattingContext().geometryForBox(layoutBox);
    auto boxWritingMode = layoutBox->writingMode();
    auto boxIsLTR = boxWritingMode.isBidiLTR();
    auto isFirstLastIndexes = isFirstLastIndexesMap.get(layoutBox.ptr());
    auto isFirstBox = isFirstLastIndexes.first && *isFirstLastIndexes.first == displayBoxNodeIndex;
    auto isLastBox = isFirstLastIndexes.last && *isFirstLastIndexes.last == displayBoxNodeIndex;
    auto logicalRect = lineBox().logicalBorderBoxForInlineBox(layoutBox, boxGeometry);
    auto beforeInlineBoxContent = [&] {
        auto shouldApplyLeftSide = (boxIsLTR && isFirstBox) || (!boxIsLTR && isLastBox);
        if (shouldApplyLeftSide)
            contentLineRightEdge += marginLineLeft(boxGeometry, boxWritingMode);
        logicalRect.setLeft(contentLineRightEdge);
        if (shouldApplyLeftSide)
            contentLineRightEdge += borderLineLeft(boxGeometry, boxWritingMode) + paddingLineLeft(boxGeometry, boxWritingMode);
    };
    beforeInlineBoxContent();

    for (auto childDisplayBoxNodeIndex : displayBoxTree.at(displayBoxNodeIndex).children)
        adjustVisualGeometryForDisplayBox(childDisplayBoxNodeIndex, contentLineRightEdge, lineBoxLogicalTop, displayBoxTree, boxes, isFirstLastIndexesMap);

    auto afterInlineBoxContent = [&] {
        auto shouldApplyRightSide = (boxIsLTR && isLastBox) || (!boxIsLTR && isFirstBox);
        if (shouldApplyRightSide) {
            contentLineRightEdge += borderLineRight(boxGeometry, boxWritingMode) + paddingLineRight(boxGeometry, boxWritingMode);
            auto logicalWidth = contentLineRightEdge - logicalRect.left();
            contentLineRightEdge += layoutBox->isRubyBase()
                ? RubyFormattingContext::baseEndAdditionalLogicalWidth(layoutBox, displayBox, logicalWidth, formattingContext())
                : 0.f;
            logicalRect.setRight(contentLineRightEdge);
            contentLineRightEdge += marginLineRight(boxGeometry, boxWritingMode);
        } else
            logicalRect.setRight(contentLineRightEdge);
    };
    afterInlineBoxContent();

    auto visualRect = [&] {
        auto visualRect = mapInlineRectLogicalToVisual(logicalRect, lineBox().logicalRect(), rootWritingMode);
        visualRect.moveBy(m_displayLine.topLeft());
        return visualRect;
    }();
    displayBox.setRect(visualRect, visualRect);

    auto* inlineBox = lineBox().inlineLevelBoxFor(layoutBox);
    ASSERT(inlineBox);
    auto computeInkOverflow = [&] {
        auto inkOverflow = FloatRect { displayBox.visualRectIgnoringBlockDirection() };
        m_contentHasInkOverflow |= hasInlineBoxInkOverflow(*inlineBox, isFirstFormattedLine() ? layoutBox->firstLineStyle() : layoutBox->style());
        displayBox.adjustInkOverflow(inkOverflow);
    };
    computeInkOverflow();

    auto inlineBoxLeftInInlineDirectionVisualOrder = isHorizontalWritingMode ? displayBox.left() : displayBox.top();
    auto logicalWidthForBiDiFragment = isHorizontalWritingMode ? displayBox.width() : displayBox.height();
    auto logicalTopRelativeToRoot = lineBox().logicalRect().top() + logicalRect.top();
    setInlineBoxGeometry(layoutBox, boxGeometry, { logicalTopRelativeToRoot, inlineBoxLeftInInlineDirectionVisualOrder, logicalWidthForBiDiFragment, logicalRect.height() }, isFirstBox);
    if (inlineBox->hasContent())
        displayBox.setHasContent();
}

bool InlineDisplayContentBuilder::processBidiLinesWithNoContent(const LineLayoutResult& lineLayoutResult, InlineDisplay::Boxes& boxes)
{
    if (lineLayoutResult.hasContentfulInFlowContent())
        return false;

    processNonBidiContent(lineLayoutResult, boxes);
    if (m_displayLine.isLeftToRightInlineDirection())
        return true;

    for (auto& box : boxes) {
        if (!box.isInlineBox()) {
            boxes.clear();
            ASSERT_NOT_REACHED();
            return false;
        }
        if (box.isNonRootInlineBox()) {
            m_displayLine.isHorizontal() ? box.setLeft(m_displayLine.lineBoxRight()) : box.setTop(m_displayLine.lineBoxBottom());

            auto& inlineBoxGeometry = formattingContext().geometryForBox(box.layoutBox());
            m_displayLine.isHorizontal() ? inlineBoxGeometry.setLeft(LayoutUnit { box.left() }) : inlineBoxGeometry.setTop(LayoutUnit { box.top() });
        }
    }
    return true;
}

void InlineDisplayContentBuilder::processBidiContent(const LineLayoutResult& lineLayoutResult, InlineDisplay::Boxes& boxes)
{
    ASSERT(lineLayoutResult.directionality.visualOrderList.size() <= lineLayoutResult.runs.size());

    if (processBidiLinesWithNoContent(lineLayoutResult, boxes))
        return;

    AncestorStack ancestorStack;
    auto displayBoxTree = DisplayBoxTree { };
    ancestorStack.push({ }, root());

    auto& lineBox = this->lineBox();
    auto writingMode = root().writingMode();
    auto isHorizontalWritingMode = writingMode.isHorizontal();

    auto lineLogicalTop = isHorizontalWritingMode ? m_displayLine.top() : m_displayLine.left();
    auto lineLogicalLeft = isHorizontalWritingMode ? m_displayLine.left() : m_displayLine.top();
    // Note that hangable punctuation does not contribute to contentLogicalLeftIgnoringInlineDirection() as it is considered more of an overflow.
    auto contentLineLeftEdge = m_displayLine.contentLogicalLeftIgnoringInlineDirection() - (writingMode.isBidiLTR() ? lineLayoutResult.hangingContent.hangablePunctuationStartWidth : 0.f);
    auto hasInlineBox = false;
    auto createDisplayBoxesInVisualOrder = [&] {
        auto lineBoxLogicalRect = lineBox.logicalRect();
        auto lineBoxVisualOffset = m_displayLine.topLeft();
        auto rootLogicalRect = lineBox.logicalRectForRootInlineBox();
        rootLogicalRect.setLeft(m_displayLine.contentLogicalLeftIgnoringInlineDirection());
        auto rootVisualRect = mapInlineRectLogicalToVisual(rootLogicalRect, lineBoxLogicalRect, writingMode);
        rootVisualRect.moveBy(lineBoxVisualOffset);
        appendRootInlineBoxDisplayBox(rootVisualRect, lineBox.rootInlineBox().hasContent(), boxes);

        Vector<size_t> blockLevelOutOfFlowBoxList;
        auto contentLineRightEdge = contentLineLeftEdge;
        auto& inlineContent = lineLayoutResult.runs;
        for (size_t index = 0; index < lineLayoutResult.directionality.visualOrderList.size(); ++index) {
            auto logicalIndex = lineLayoutResult.directionality.visualOrderList[index];
            ASSERT(inlineContent[logicalIndex].bidiLevel() != InlineItem::opaqueBidiLevel || inlineContent[logicalIndex].isLineSpanningInlineBoxStart());

            auto& lineRun = inlineContent[logicalIndex];
            auto needsDisplayBoxOrGeometrySetting = !lineRun.isWordBreakOpportunity() && !lineRun.isInlineBoxEnd();
            if (!needsDisplayBoxOrGeometrySetting)
                continue;

            CheckedRef layoutBox = lineRun.layoutBox();
            auto parentDisplayBoxNodeIndex = ensureDisplayBoxForContainer(layoutBox->parent(), displayBoxTree, ancestorStack, boxes);
            hasInlineBox = hasInlineBox || (!lineRun.isBlock() && (parentDisplayBoxNodeIndex || lineRun.isInlineBoxStart() || lineRun.isLineSpanningInlineBoxStart()));

            if (lineRun.isOutOfFlow()) {
                if (layoutBox->style().originalDisplay().isInlineType()) {
                    // Note that out-of-flow handling (render tree integration) really only needs logical coords (not even "content in inline direction visual order").
                    formattingContext().geometryForBox(layoutBox).setTopLeft({ lineBox.logicalRect().left() + lineBox.logicalRectForRootInlineBox().left() + lineRun.logicalLeft(), lineBox.logicalRect().top() });
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
                if (lineRun.isAtomicInlineBox() || lineRun.isListMarker())
                    return lineBox.logicalBorderBoxForAtomicInlineBox(layoutBox, boxGeometry);
                if (lineRun.isBlock()) {
                    auto borderBoxRect = BoxGeometry::borderBoxRect(boxGeometry);
                    return { borderBoxRect.top(), borderBoxRect.left(), borderBoxRect.width(), borderBoxRect.height() };
                }
                ASSERT_NOT_REACHED();
                return { };
            }();
            logicalRect.setLeft(contentLineRightEdge);

            auto visualRectRelativeToRoot = [&] {
                auto visualRect = mapInlineRectLogicalToVisual(logicalRect, lineBoxLogicalRect, writingMode);
                visualRect.moveBy(lineBoxVisualOffset);
                return visualRect;
            }();

            if (lineRun.isText()) {
                auto wordSpacingMargin = lineRun.isWordSeparator() ? layoutBox->style().fontCascade().wordSpacing() : 0.0f;
                isHorizontalWritingMode ? visualRectRelativeToRoot.moveHorizontally(wordSpacingMargin) : visualRectRelativeToRoot.moveVertically(wordSpacingMargin);
                appendTextDisplayBox(lineRun, visualRectRelativeToRoot, boxes);
                contentLineRightEdge += logicalRect.width() + wordSpacingMargin;
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
                appendHardLineBreakDisplayBox(lineRun, visualRectRelativeToRoot, boxes);

                auto& boxGeometry = formattingContext().geometryForBox(layoutBox);
                boxGeometry.setTopLeft({ lineLogicalLeft + contentLineRightEdge, lineLogicalTop + logicalRect.top() });
                boxGeometry.setContentBoxSize(toLayoutSize(logicalRect.size()));

                displayBoxTree.append(parentDisplayBoxNodeIndex, boxes.size() - 1);
                continue;
            }
            if (lineRun.isAtomicInlineBox() || lineRun.isListMarker()) {
                auto parentWritingMode = layoutBox->parent().writingMode();
                auto& boxGeometry = formattingContext().geometryForBox(layoutBox);
                auto boxMarginLeft = marginLineLeft(boxGeometry, parentWritingMode);
                isHorizontalWritingMode ? visualRectRelativeToRoot.moveHorizontally(boxMarginLeft) : visualRectRelativeToRoot.moveVertically(boxMarginLeft);

                appendAtomicInlineLevelDisplayBox(lineRun, visualRectRelativeToRoot, boxes);
                boxGeometry.setTopLeft({ lineLogicalLeft + contentLineRightEdge, lineLogicalTop + logicalRect.top() });

                contentLineRightEdge += boxMarginLeft + logicalRect.width() + marginLineRight(boxGeometry, parentWritingMode);
                displayBoxTree.append(parentDisplayBoxNodeIndex, boxes.size() - 1);
                continue;
            }
            if (lineRun.isInlineBoxStart() || lineRun.isLineSpanningInlineBoxStart()) {
                auto isEmptyInlineBox = [&] {
                    // FIXME: Maybe we should not tag ruby bases with annotation boxes only contentful?
                    if (!lineBox.inlineLevelBoxFor(lineRun).hasContent())
                        return true;
                    if (!layoutBox->isRubyBase())
                        return false;
                    CheckedPtr rubyBaseLayoutBox = dynamicDowncast<ElementBox>(layoutBox.get());
                    if (!rubyBaseLayoutBox)
                        return false;
                    // Let's create empty inline boxes for ruby bases with annotation only.
                    if (!rubyBaseLayoutBox->firstChild() || (rubyBaseLayoutBox->firstChild() == rubyBaseLayoutBox->lastChild() && rubyBaseLayoutBox->firstChild()->isRubyAnnotationBox()))
                        return true;
                    // Let's check if we actually don't have a contentful run inside this ruby base.
                    for (size_t nextLogicalRunIndex = logicalIndex + 1; nextLogicalRunIndex < inlineContent.size(); ++nextLogicalRunIndex) {
                        auto& lineRun = inlineContent[nextLogicalRunIndex];
                        if (lineRun.isInlineBoxEnd() && &lineRun.layoutBox() == rubyBaseLayoutBox)
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
            if (lineRun.isBlock()) {
                ASSERT(!hasInlineBox);

                auto& boxGeometry = formattingContext().geometryForBox(layoutBox);
                auto boxMarginLeft = marginLineLeft(boxGeometry, writingMode);
                // Block content should always be placed at the start of the content box even when floats shrink the line.
                auto lineOffset = lineBoxLogicalRect.left() - lineLayoutResult.lineGeometry.initialLogicalTopLeft.x();
                isHorizontalWritingMode ? visualRectRelativeToRoot.moveHorizontally(lineOffset + boxMarginLeft) : visualRectRelativeToRoot.moveVertically(lineOffset + boxMarginLeft);

                auto updateEnclosingInlineBoxesGeometryWithBlock = [&] {
                    for (auto& displayBox : boxes) {
                        ASSERT(displayBox.isInlineBox());
                        if (!displayBox.isNonRootInlineBox())
                            continue;
                        displayBox.setRect(visualRectRelativeToRoot, visualRectRelativeToRoot);
                        displayBox.setHasContent();
                        setInlineBoxGeometry(displayBox.layoutBox(), formattingContext().geometryForBox(displayBox.layoutBox()), logicalRect, false);
                    }
                };
                updateEnclosingInlineBoxesGeometryWithBlock();

                appendBlockLevelDisplayBox(lineRun, visualRectRelativeToRoot, boxes);
                boxGeometry.setTopLeft({ lineLogicalLeft + contentLineRightEdge - lineOffset, lineLogicalTop + logicalRect.top() });
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
                CheckedRef layoutBox = boxes[index].layoutBox();
                auto* inlineLevelBox = lineBox.inlineLevelBoxFor(layoutBox);
                ASSERT(inlineLevelBox);
                auto isFirstBox = inlineLevelBox->isFirstBox();
                auto isLastBox = inlineLevelBox->isLastBox();
                if (!isFirstBox && !isLastBox)
                    continue;
                if (isFirstBox) {
                    auto isFirstLastIndexes = isFirstLastIndexesMap.get(layoutBox.ptr());
                    if (!isFirstLastIndexes.first || isLastBox)
                        isFirstLastIndexesMap.set(layoutBox.ptr(), IsFirstLastIndex { isFirstLastIndexes.first.value_or(index), isLastBox ? index : isFirstLastIndexes.last });
                    continue;
                }
                if (isLastBox) {
                    ASSERT(!isFirstBox);
                    isFirstLastIndexesMap.set(layoutBox.ptr(), IsFirstLastIndex { { }, index });
                    continue;
                }
            }
        };
        computeIsFirstIsLastBox();

        auto adjustVisualGeometryWithInlineBoxes = [&] {
            auto contentLineRightEdge = contentLineLeftEdge;

            for (auto childDisplayBoxNodeIndex : displayBoxTree.root().children)
                adjustVisualGeometryForDisplayBox(childDisplayBoxNodeIndex, contentLineRightEdge, lineLogicalTop, displayBoxTree, boxes.mutableSpan(), isFirstLastIndexesMap);
        };
        adjustVisualGeometryWithInlineBoxes();
    };
    handleInlineBoxes();

    auto closeInlineBoxes = [&] {
        for (auto& lineRun : lineLayoutResult.runs | std::views::reverse) {
            if (!lineRun.isInlineBoxStart() || lineRun.bidiLevel() != InlineItem::opaqueBidiLevel)
                break;
            // These are trailing inline box start runs (without the closing inline box end <span> <-line breaks here</span>).
            // They don't participate in visual reordering (see createDisplayBoxesInVisualOrder above) as we don't find them
            // empty at inline item list building time (see setBidiLevelForOpaqueInlineItems) due to trailing whitespace.
            // Non-empty inline boxes are normally get their display boxes generated when we process their content runs, but
            // these trailing runs have their content on the subsequent line(s).
            auto& inlineBox = lineBox.inlineLevelBoxFor(lineRun);
            appendInlineBoxDisplayBox(lineRun, inlineBox, { { }, m_displayLine.right(), { }, { } }, boxes);
            setInlineBoxGeometry(lineRun.layoutBox(), formattingContext().geometryForBox(lineRun.layoutBox()), { { }, lineBox.logicalRect().right(), { }, { } }, inlineBox.isFirstBox());
        }
    };
    closeInlineBoxes();
}

void InlineDisplayContentBuilder::collectInkOverflowForInlineBoxes(std::span<InlineDisplay::Box> boxes)
{
    if (!m_contentHasInkOverflow && !m_hasSeenNestedInlineBoxesWithDifferentFontCascade)
        return;
    // Visit the inline boxes and propagate ink overflow to their parents -except to the root inline box.
    // (e.g. <span style="font-size: 10px;">Small font size<span style="font-size: 300px;">Larger font size. This overflows the top most span.</span></span>).
    auto accumulatedInkOverflowRect = InlineRect { { }, { } };
    for (auto& displayBox : boxes | std::views::reverse) {
        auto mayHaveInkOverflow = displayBox.isText() || displayBox.isAtomicInlineBox() || displayBox.isGenericInlineLevelBox() || displayBox.isNonRootInlineBox();
        if (!mayHaveInkOverflow)
            continue;
        if (displayBox.isNonRootInlineBox()) {
            if (!accumulatedInkOverflowRect.isEmpty())
                displayBox.adjustInkOverflow(accumulatedInkOverflowRect);
            auto& layoutBox = displayBox.layoutBox();
            auto inkOverflowRect = displayBox.inkOverflow();
            adjustInkOverflowForInlineBox(layoutBox, root(), layoutBox.style(), inkOverflowRect);
            displayBox.adjustInkOverflow(inkOverflowRect);
        }

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

static inline size_t NODELETE runIndex(auto i, auto listSize, auto isBidiLTR)
{
    if (isBidiLTR)
        return i;
    auto lastIndex = listSize - 1;
    return lastIndex - i;
}

static inline void setGeometryForOutOfFlowBoxes(const Vector<size_t>& indexListOfOutOfFlowBoxes, std::optional<size_t> firstOutOfFlowIndexWithPreviousInflowSibling, const Line::RunList& lineRuns, const Vector<int32_t>& visualOrderList, InlineFormattingContext& formattingContext, const LineBox& lineBox, const ConstraintsForInlineContent& constraints)
{
    auto isBidiLTR = formattingContext.root().writingMode().isBidiLTR();
    auto outOfFlowBoxListSize = indexListOfOutOfFlowBoxes.size();

    auto outOfFlowBox = [&](size_t i) -> const Box& {
        auto outOfFlowRunIndex = indexListOfOutOfFlowBoxes[runIndex(i, outOfFlowBoxListSize, isBidiLTR)];
        return (visualOrderList.isEmpty() ? lineRuns[outOfFlowRunIndex] : lineRuns[visualOrderList[outOfFlowRunIndex]]).layoutBox();
    };
    // Set geometry on "before inflow content" boxes first, followed by the "after inflow content" list.
    auto beforeAfterBoundary = firstOutOfFlowIndexWithPreviousInflowSibling.value_or(outOfFlowBoxListSize);
    // These out of flow boxes "sit" on the line start (they are before any inflow content e.g. <div><div class=out-of-flow></div>some text<div>)
    auto logicalTopLeft = LayoutPoint { constraints.horizontal().logicalLeft, lineBox.logicalRect().top() };
    for (size_t i = 0; i < beforeAfterBoundary; ++i)
        formattingContext.geometryForBox(outOfFlowBox(i)).setTopLeft(logicalTopLeft);
    // These out of flow boxes are all _after_ an inflow content and get "wrapped" to the next line.
    logicalTopLeft.setY(lineBox.logicalRect().bottom());
    for (size_t i = beforeAfterBoundary; i < outOfFlowBoxListSize; ++i)
        formattingContext.geometryForBox(outOfFlowBox(i)).setTopLeft(logicalTopLeft);
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
    auto isBidiLTR = root().writingMode().isBidiLTR();

    auto firstContentfulInFlowRunIndex = std::optional<size_t> { };
    auto contentListSize = visualOrderList.isEmpty() ? lineRuns.size() : visualOrderList.size();
    for (size_t i = 0; i < contentListSize; ++i) {
        auto index = runIndex(i, contentListSize, isBidiLTR);
        auto& lineRun = visualOrderList.isEmpty() ? lineRuns[index] : lineRuns[visualOrderList[index]];
        if (lineRun.layoutBox().isInFlow() && Line::Run::isContentfulOrHasDecoration(lineRun, formattingContext)) {
            firstContentfulInFlowRunIndex = index;
            break;
        }
    }

    if (!firstContentfulInFlowRunIndex) {
        setGeometryForOutOfFlowBoxes(indexListOfOutOfFlowBoxes, { }, lineRuns, visualOrderList, formattingContext, lineBox(), constraints());
        return;
    }

    auto firstOutOfFlowIndexWithPreviousInflowSibling = std::optional<size_t> { };
    auto outOfFlowBoxListSize = indexListOfOutOfFlowBoxes.size();
    for (size_t i = 0; i < outOfFlowBoxListSize; ++i) {
        auto outOfFlowIndex = runIndex(i, outOfFlowBoxListSize, isBidiLTR);
        auto hasPreviousInflowSibling = (isBidiLTR && indexListOfOutOfFlowBoxes[outOfFlowIndex] > *firstContentfulInFlowRunIndex) || (!isBidiLTR && indexListOfOutOfFlowBoxes[outOfFlowIndex] < *firstContentfulInFlowRunIndex);
        if (hasPreviousInflowSibling) {
            firstOutOfFlowIndexWithPreviousInflowSibling = outOfFlowIndex;
            break;
        }
    }
    setGeometryForOutOfFlowBoxes(indexListOfOutOfFlowBoxes, firstOutOfFlowIndexWithPreviousInflowSibling, lineRuns, visualOrderList, formattingContext, lineBox(), constraints());
}

static float logicalBottomForTextDecorationContent(std::span<InlineDisplay::Box> boxes, bool isHorizontalWritingMode)
{
    auto logicalBottom = std::optional<float> { };
    for (auto& displayBox : boxes) {
        if (displayBox.isRootInlineBox())
            continue;
        if (!displayBox.style().textDecorationLineInEffect().hasUnderline())
            continue;
        if (displayBox.isText() || displayBox.style().textDecorationSkipInk() == TextDecorationSkipInk::None) {
            auto contentLogicalBottom = isHorizontalWritingMode ? displayBox.bottom() : displayBox.right();
            logicalBottom = logicalBottom ? std::max(*logicalBottom, contentLogicalBottom) : contentLogicalBottom;
        }
    }
    // This function is not called unless there's at least one run on the line with Style::TextDecorationLine::Flag::Underline.
    ASSERT(logicalBottom);
    return logicalBottom.value_or(0.f);
}

void InlineDisplayContentBuilder::collectInkOverflowForTextDecorations(std::span<InlineDisplay::Box> boxes)
{
    if (!m_hasSeenTextDecoration)
        return;

    auto logicalBottomForTextDecoration = std::optional<float> { };
    auto writingMode = root().writingMode();
    auto isHorizontalWritingMode = writingMode.isHorizontal();

    for (auto& displayBox : boxes) {
        if (!displayBox.isText())
            continue;

        CheckedRef parentStyle = displayBox.layoutBox().parent().style();
        auto textDecorations = parentStyle->textDecorationLineInEffect();
        if (!textDecorations)
            continue;

        auto decorationOverflow = [&] {
            if (!textDecorations.hasUnderline())
                return inkOverflowForDecorations(parentStyle);

            if (!logicalBottomForTextDecoration)
                logicalBottomForTextDecoration = logicalBottomForTextDecorationContent(boxes, isHorizontalWritingMode);
            auto textRunLogicalOffsetFromLineBottom = *logicalBottomForTextDecoration - (isHorizontalWritingMode ? displayBox.bottom() : displayBox.right());
            return inkOverflowForDecorations(parentStyle, { displayBox.height(), textRunLogicalOffsetFromLineBottom });
        }();

        if (!decorationOverflow.isZero()) {
            m_contentHasInkOverflow = true;
            auto inflatedInkOverflowRect = [&] {
                auto inkOverflowRect = displayBox.inkOverflow();
                switch (writingMode.blockDirection()) {
                case FlowDirection::TopToBottom:
                case FlowDirection::BottomToTop:
                    inkOverflowRect.inflate(decorationOverflow.left(), decorationOverflow.top(), decorationOverflow.right(), decorationOverflow.bottom());
                    break;
                case FlowDirection::LeftToRight:
                    inkOverflowRect.inflate(decorationOverflow.bottom(), decorationOverflow.right(), decorationOverflow.top(), decorationOverflow.left());
                    break;
                case FlowDirection::RightToLeft:
                    inkOverflowRect.inflate(decorationOverflow.top(), decorationOverflow.right(), decorationOverflow.bottom(), decorationOverflow.left());
                    break;
                default:
                    ASSERT_NOT_REACHED();
                    break;
                }
                return inkOverflowRect;
            };
            displayBox.adjustInkOverflow(inflatedInkOverflowRect());
        }
    }
}

size_t InlineDisplayContentBuilder::processRubyBase(size_t rubyBaseStart, std::span<InlineDisplay::Box> displayBoxes, Vector<WTF::Range<size_t>>& interlinearRubyColumnRangeList, Vector<size_t>& rubyBaseStartIndexListWithAnnotation)
{
    auto& formattingContext = this->formattingContext();
    auto& rubyBaseDisplayBox = displayBoxes[rubyBaseStart];
    CheckedRef rubyBaseLayoutBox = rubyBaseDisplayBox.layoutBox();
    ASSERT(rubyBaseDisplayBox.isInlineBox());
    auto baseBorderBoxLogicalRect = BoxGeometry::borderBoxRect(formattingContext.geometryForBox(rubyBaseLayoutBox));

    CheckedPtr annotationBox = rubyBaseLayoutBox->associatedRubyAnnotationBox();
    if (annotationBox)
        rubyBaseStartIndexListWithAnnotation.append(rubyBaseStart);

    auto rubyBaseEnd = displayBoxes.size();
    CheckedRef rubyBox = rubyBaseLayoutBox->parent();
    CheckedRef rubyBoxParent = rubyBox->parent();
    for (auto index = rubyBaseStart + 1; index < displayBoxes.size(); ++index) {
        CheckedRef baseContentLayoutBox = displayBoxes[index].layoutBox();
        if (baseContentLayoutBox->isRubyBase()) {
            index = processRubyBase(index, displayBoxes, interlinearRubyColumnRangeList, rubyBaseStartIndexListWithAnnotation);
            if (RubyFormattingContext::hasInterlinearAnnotation(baseContentLayoutBox)) {
                CheckedRef interlinearAnnotationBox = *baseContentLayoutBox->associatedRubyAnnotationBox();
                auto isNestedRubyBase = [&] {
                    for (auto* ancestor = &baseContentLayoutBox->parent(); ancestor != &root(); ancestor = &ancestor->parent()) {
                        if (ancestor->isRubyBase())
                            return ancestor == &rubyBaseLayoutBox.get();
                    }
                    return false;
                };
                if (isNestedRubyBase()) {
                    auto nestedAnnotationMarginBoxRect = BoxGeometry::marginBoxRect(formattingContext.geometryForBox(interlinearAnnotationBox));
                    baseBorderBoxLogicalRect.expandToContain(nestedAnnotationMarginBoxRect);
                }
            }

            if (index == displayBoxes.size()) {
                rubyBaseEnd = index;
                break;
            }
        }

        CheckedRef layoutBox = displayBoxes[index].layoutBox();
        if (&layoutBox->parent() == &rubyBox.get() || &layoutBox->parent() == &rubyBoxParent.get()) {
            rubyBaseEnd = index;
            break;
        }
    }

    if (RubyFormattingContext::hasInterlinearAnnotation(rubyBaseLayoutBox))
        interlinearRubyColumnRangeList.append({ rubyBaseStart, rubyBaseEnd });

    if (annotationBox) {
        auto placeAndSizeAnnotationBox = [&] {
            if (RubyFormattingContext::hasInterCharacterAnnotation(rubyBaseLayoutBox)) {
                auto letterSpacing = LayoutUnit { rubyBaseLayoutBox->style().usedLetterSpacing() };
                // FIXME: Consult the LineBox to see if letter spacing indeed applies.
                baseBorderBoxLogicalRect.setWidth(std::max(0_lu, baseBorderBoxLogicalRect.width() - letterSpacing));
            }

            // FIXME: There is a confusion here. The functions expect margin box.
            auto borderBoxLogicalTopLeft = RubyFormattingContext::placeAnnotationBox(rubyBaseLayoutBox, baseBorderBoxLogicalRect, formattingContext);
            auto contentBoxLogicalSize = RubyFormattingContext::sizeAnnotationBox(rubyBaseLayoutBox, baseBorderBoxLogicalRect, formattingContext);
            auto& annotationBoxGeometry = formattingContext.geometryForBox(*annotationBox);
            annotationBoxGeometry.setTopLeft(toLayoutPoint(borderBoxLogicalTopLeft));
            annotationBoxGeometry.setContentBoxSize(toLayoutSize(contentBoxLogicalSize));
        };
        placeAndSizeAnnotationBox();
    }
    return rubyBaseEnd;
}

Vector<size_t> InlineDisplayContentBuilder::processRubyContent(std::span<InlineDisplay::Box> displayBoxes, const LineLayoutResult& lineLayoutResult)
{
    if (root().isRubyAnnotationBox())
        RubyFormattingContext::adjustAnnotationContentWithAlignmentOffset(displayBoxes, lineLayoutResult.ruby.annotationAlignmentOffset, formattingContext());

    if (!m_hasSeenRubyBase)
        return { };

    HashSet<CheckedPtr<const Box>> lineSpanningRubyBaseList;
    for (auto& lineRun : lineLayoutResult.runs) {
        if (lineRun.isLineSpanningInlineBoxStart() && lineRun.layoutBox().isRubyBase())
            lineSpanningRubyBaseList.add(&lineRun.layoutBox());
    }

    RubyFormattingContext::adjustRubyBaseContentWithAlignmentOffset(displayBoxes, lineLayoutResult.ruby.baseAlignmentOffsetList, formattingContext());

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
    RubyFormattingContext::applyRubyOverhang(formattingContext(), lineBox().logicalRect().height(), displayBoxes, interlinearRubyColumnRangeList);
    return rubyBaseStartIndexListWithAnnotation;
}

void InlineDisplayContentBuilder::setInlineBoxGeometry(const Box& inlineBox, Layout::BoxGeometry& boxGeometry, const InlineRect& logicalRect, bool isFirstInlineBoxFragment)
{
    ASSERT(inlineBox.isInlineBox());

    auto borderBoxSize = LayoutSize { };
    if (!inlineBox.isRubyBase())
        borderBoxSize = { LayoutUnit::fromFloatCeil(logicalRect.width()), LayoutUnit::fromFloatCeil(logicalRect.height()) };
    else {
        // While flooring here may produce fractional overflow, it ensures ruby content would never trigger incorrect wrapping triggered by
        // inflated annotation boxes inside a shrink-to-fit container (where annotation box is sized to the ruby base but it also imposes a required minimum width)
        borderBoxSize = { LayoutUnit::fromFloatFloor(logicalRect.width()), LayoutUnit::fromFloatFloor(logicalRect.height()) };
    }

    auto borderBoxRect = Rect { toLayoutPoint(logicalRect.topLeft()), borderBoxSize };
    if (!isFirstInlineBoxFragment)
        borderBoxRect.expandToContain(BoxGeometry::borderBoxRect(boxGeometry));

    boxGeometry.setTopLeft(borderBoxRect.topLeft());
    boxGeometry.setContentBoxWidth(borderBoxRect.width() - boxGeometry.horizontalBorderAndPadding());
    boxGeometry.setContentBoxHeight(borderBoxRect.height() - boxGeometry.verticalBorderAndPadding());
}

template <typename BoxType, typename LayoutUnitType>
void InlineDisplayContentBuilder::setLogicalLeft(BoxType& box, LayoutUnitType logicalLeft, WritingMode writingMode) const
{
    switch (writingMode.blockDirection()) {
    case FlowDirection::TopToBottom:
    case FlowDirection::BottomToTop:
        box.setLeft(logicalLeft);
        break;
    case FlowDirection::LeftToRight:
    case FlowDirection::RightToLeft:
        box.setTop(logicalLeft);
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
}

void InlineDisplayContentBuilder::setLogicalRight(InlineDisplay::Box& displayBox, InlineLayoutUnit logicalRight, WritingMode writingMode) const
{
    switch (writingMode.blockDirection()) {
    case FlowDirection::TopToBottom:
    case FlowDirection::BottomToTop:
        displayBox.setRight(logicalRight);
        break;
    case FlowDirection::LeftToRight:
    case FlowDirection::RightToLeft:
        displayBox.setBottom(logicalRight);
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
}

}
}

