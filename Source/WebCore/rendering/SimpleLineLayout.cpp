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
#include "SimpleLineLayout.h"

#include "FontCache.h"
#include "Frame.h"
#include "GraphicsContext.h"
#include "HTMLTextFormControlElement.h"
#include "HitTestLocation.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "InlineTextBox.h"
#include "LineWidth.h"
#include "PaintInfo.h"
#include "RenderBlockFlow.h"
#include "RenderStyle.h"
#include "RenderText.h"
#include "RenderTextControl.h"
#include "RenderView.h"
#include "Settings.h"
#include "SimpleLineLayoutFunctions.h"
#include "Text.h"
#include "TextPaintStyle.h"
#include "break_lines.h"

namespace WebCore {
namespace SimpleLineLayout {

template <typename CharacterType>
static bool canUseForText(const CharacterType* text, unsigned length, const SimpleFontData& fontData)
{
    // FIXME: <textarea maxlength=0> generates empty text node.
    if (!length)
        return false;
    for (unsigned i = 0; i < length; ++i) {
        UChar character = text[i];
        if (character == ' ')
            continue;

        // These would be easy to support.
        if (character == noBreakSpace)
            return false;
        if (character == softHyphen)
            return false;

        UCharDirection direction = u_charDirection(character);
        if (direction == U_RIGHT_TO_LEFT || direction == U_RIGHT_TO_LEFT_ARABIC
            || direction == U_RIGHT_TO_LEFT_EMBEDDING || direction == U_RIGHT_TO_LEFT_OVERRIDE
            || direction == U_LEFT_TO_RIGHT_EMBEDDING || direction == U_LEFT_TO_RIGHT_OVERRIDE
            || direction == U_POP_DIRECTIONAL_FORMAT || direction == U_BOUNDARY_NEUTRAL)
            return false;

        if (!fontData.glyphForCharacter(character))
            return false;
    }
    return true;
}

static bool canUseForText(const RenderText& textRenderer, const SimpleFontData& fontData)
{
    if (textRenderer.is8Bit())
        return canUseForText(textRenderer.characters8(), textRenderer.textLength(), fontData);
    return canUseForText(textRenderer.characters16(), textRenderer.textLength(), fontData);
}

bool canUseFor(const RenderBlockFlow& flow)
{
#if !PLATFORM(MAC) && !PLATFORM(GTK) && !PLATFORM(EFL)
    // FIXME: Non-mac platforms are hitting ASSERT(run.charactersLength() >= run.length())
    // https://bugs.webkit.org/show_bug.cgi?id=123338
    return false;
#else
    if (!flow.frame().settings().simpleLineLayoutEnabled())
        return false;
    if (!flow.firstChild())
        return false;
    // This currently covers <blockflow>#text</blockflow> case.
    // The <blockflow><inline>#text</inline></blockflow> case is also popular and should be relatively easy to cover.
    if (flow.firstChild() != flow.lastChild())
        return false;
    if (!flow.firstChild()->isText())
        return false;
    if (!flow.isHorizontalWritingMode())
        return false;
    if (flow.flowThreadState() != RenderObject::NotInsideFlowThread)
        return false;
    if (flow.hasOutline())
        return false;
    if (flow.isRubyText() || flow.isRubyBase())
        return false;
    if (flow.parent()->isDeprecatedFlexibleBox())
        return false;
    // FIXME: Implementation of wrap=hard looks into lineboxes.
    if (flow.parent()->isTextArea() && flow.parent()->element()->fastHasAttribute(HTMLNames::wrapAttr))
        return false;
    // FIXME: Placeholders do something strange.
    if (flow.parent()->isTextControl() && toRenderTextControl(*flow.parent()).textFormControlElement().placeholderElement())
        return false;
    // These tests only works during layout. Outside layout this function may give false positives.
    if (flow.view().layoutState()) {
#if ENABLE(CSS_SHAPES) && ENABLE(CSS_SHAPE_INSIDE)
        if (flow.view().layoutState()->shapeInsideInfo())
            return false;
#endif
        if (flow.view().layoutState()->m_columnInfo)
            return false;
    }
    const RenderStyle& style = flow.style();
    if (style.textDecorationsInEffect() != TextDecorationNone)
        return false;
    if (style.textAlign() == JUSTIFY)
        return false;
    // Non-visible overflow should be pretty easy to support.
    if (style.overflowX() != OVISIBLE || style.overflowY() != OVISIBLE)
        return false;
    if (!style.textIndent().isZero())
        return false;
    if (!style.wordSpacing().isZero() || style.letterSpacing())
        return false;
    if (style.textTransform() != TTNONE)
        return false;
    if (!style.isLeftToRightDirection())
        return false;
    if (style.lineBoxContain() != RenderStyle::initialLineBoxContain())
        return false;
    if (style.writingMode() != TopToBottomWritingMode)
        return false;
    if (style.lineBreak() != LineBreakAuto)
        return false;
    if (style.wordBreak() != NormalWordBreak)
        return false;
    if (style.unicodeBidi() != UBNormal || style.rtlOrdering() != LogicalOrder)
        return false;
    if (style.lineAlign() != LineAlignNone || style.lineSnap() != LineSnapNone)
        return false;
    if (style.hyphens() == HyphensAuto)
        return false;
    if (style.textEmphasisFill() != TextEmphasisFillFilled || style.textEmphasisMark() != TextEmphasisMarkNone)
        return false;
    if (style.textShadow())
        return false;
#if ENABLE(CSS_SHAPES) && ENABLE(CSS_SHAPE_INSIDE)
    if (style.resolvedShapeInside())
        return true;
#endif
    if (style.textOverflow() || (flow.isAnonymousBlock() && flow.parent()->style().textOverflow()))
        return false;
    if (style.hasPseudoStyle(FIRST_LINE) || style.hasPseudoStyle(FIRST_LETTER))
        return false;
    if (style.hasTextCombine())
        return false;
    if (style.backgroundClip() == TextFillBox)
        return false;
    if (style.borderFit() == BorderFitLines)
        return false;
    const RenderText& textRenderer = toRenderText(*flow.firstChild());
    if (flow.containsFloats()) {
        // We can't use the code path if any lines would need to be shifted below floats. This is because we don't keep per-line y coordinates.
        // It is enough to test the first line width only as currently all floats must be overhanging.
        if (textRenderer.minLogicalWidth() > LineWidth(const_cast<RenderBlockFlow&>(flow), false, DoNotIndentText).availableWidth())
            return false;
    }
    if (textRenderer.isCombineText() || textRenderer.isCounter() || textRenderer.isQuote() || textRenderer.isTextFragment()
        || textRenderer.isSVGInlineText())
        return false;
    if (style.font().codePath(TextRun(textRenderer.text())) != Font::Simple)
        return false;
    if (style.font().isSVGFont())
        return false;

    // We assume that all lines have metrics based purely on the primary font.
    auto& primaryFontData = *style.font().primaryFont();
    if (primaryFontData.isLoading())
        return false;
    if (!canUseForText(textRenderer, primaryFontData))
        return false;

    return true;
#endif
}

struct Style {
    Style(const RenderStyle& style)
        : font(style.font())
        , textAlign(style.textAlign())
        , collapseWhitespace(style.collapseWhiteSpace())
        , preserveNewline(style.preserveNewline())
        , wrapLines(style.autoWrap())
        , breakWordOnOverflow(style.overflowWrap() == BreakOverflowWrap && (wrapLines || preserveNewline))
        , spaceWidth(font.width(TextRun(&space, 1)))
        , tabWidth(collapseWhitespace ? 0 : style.tabSize())
    {
    }
    const Font& font;
    ETextAlign textAlign;
    bool collapseWhitespace;
    bool preserveNewline;
    bool wrapLines;
    bool breakWordOnOverflow;
    float spaceWidth;
    unsigned tabWidth;
};

static inline bool isWhitespace(UChar character, bool preserveNewline)
{
    return character == ' ' || character == '\t' || (!preserveNewline && character == '\n');
}

template <typename CharacterType>
static inline unsigned skipWhitespaces(const CharacterType* text, unsigned offset, unsigned length, bool preserveNewline)
{
    for (; offset < length; ++offset) {
        if (!isWhitespace(text[offset], preserveNewline))
            return offset;
    }
    return length;
}

template <typename CharacterType>
static float textWidth(const RenderText& renderText, const CharacterType* text, unsigned textLength, unsigned from, unsigned to, float xPosition, const Style& style)
{
    if (style.font.isFixedPitch() || (!from && to == textLength))
        return renderText.width(from, to - from, style.font, xPosition, nullptr, nullptr);

    TextRun run(text + from, to - from);
    run.setXPos(xPosition);
    run.setCharactersLength(textLength - from);
    run.setTabSize(!!style.tabWidth, style.tabWidth);

    ASSERT(run.charactersLength() >= run.length());

    return style.font.width(run);
}

template <typename CharacterType>
static float measureWord(unsigned start, unsigned end, float lineWidth, const Style& style, const CharacterType* text, unsigned textLength, const RenderText& textRenderer)
{
    if (text[start] == ' ' && end == start + 1)
        return style.spaceWidth;

    bool measureWithEndSpace = style.collapseWhitespace && end < textLength && text[end] == ' ';
    if (measureWithEndSpace)
        ++end;
    float width = textWidth(textRenderer, text, textLength, start, end, lineWidth, style);

    return measureWithEndSpace ? width - style.spaceWidth : width;
}

template <typename CharacterType>
Vector<Run, 4> createLineRuns(unsigned lineStart, LineWidth& lineWidth, LazyLineBreakIterator& lineBreakIterator, const Style& style, const CharacterType* text, unsigned textLength, const RenderText& textRenderer)
{
    Vector<Run, 4> lineRuns;
    lineRuns.uncheckedAppend(Run(lineStart, 0));

    unsigned wordEnd = lineStart;
    while (wordEnd < textLength) {
        ASSERT(!style.collapseWhitespace || !isWhitespace(text[wordEnd], style.preserveNewline));

        unsigned wordStart = wordEnd;

        if (style.preserveNewline && text[wordStart] == '\n') {
            ++wordEnd;
            // FIXME: This creates a dedicated run for newline. This is wasteful and unnecessary but it keeps test results unchanged.
            if (wordStart > lineStart)
                lineRuns.append(Run(wordStart, lineRuns.last().right));
            lineRuns.last().right = lineRuns.last().left;
            lineRuns.last().end = wordEnd;
            break;
        }

        if (!style.collapseWhitespace && isWhitespace(text[wordStart], style.preserveNewline))
            wordEnd = wordStart + 1;
        else
            wordEnd = nextBreakablePosition<CharacterType, false>(lineBreakIterator, text, textLength, wordStart + 1);

        bool wordIsPrecededByWhitespace = style.collapseWhitespace && wordStart > lineStart && isWhitespace(text[wordStart - 1], style.preserveNewline);
        if (wordIsPrecededByWhitespace)
            --wordStart;

        float wordWidth = measureWord(wordStart, wordEnd, lineWidth.committedWidth(), style, text, textLength, textRenderer);

        lineWidth.addUncommittedWidth(wordWidth);

        if (style.wrapLines) {
            // Move to the next line if the current one is full and we have something on it.
            if (!lineWidth.fitsOnLine() && lineWidth.committedWidth())
                break;

            // This is for white-space: pre-wrap which requires special handling for end line whitespace.
            if (!style.collapseWhitespace && lineWidth.fitsOnLine() && wordEnd < textLength && isWhitespace(text[wordEnd], style.preserveNewline)) {
                // Look ahead to see if the next whitespace would fit.
                float whitespaceWidth = textWidth(textRenderer, text, textLength, wordEnd, wordEnd + 1, lineWidth.committedWidth(), style);
                if (!lineWidth.fitsOnLineIncludingExtraWidth(whitespaceWidth)) {
                    // If not eat away the rest of the whitespace on the line.
                    unsigned whitespaceEnd = skipWhitespaces(text, wordEnd, textLength, style.preserveNewline);
                    // Include newline to this run too.
                    if (whitespaceEnd < textLength && text[whitespaceEnd] == '\n')
                        ++whitespaceEnd;
                    lineRuns.last().end = whitespaceEnd;
                    lineRuns.last().right = lineWidth.availableWidth();
                    break;
                }
            }
        }

        if (wordStart > lineRuns.last().end) {
            // There were more than one consecutive whitespace.
            ASSERT(wordIsPrecededByWhitespace);
            // Include space to the end of the previous run.
            lineRuns.last().end++;
            lineRuns.last().right += style.spaceWidth;
            // Start a new run on the same line.
            lineRuns.append(Run(wordStart + 1, lineRuns.last().right));
        }

        if (!lineWidth.fitsOnLine() && style.breakWordOnOverflow) {
            // Backtrack and start measuring character-by-character.
            lineWidth.addUncommittedWidth(-lineWidth.uncommittedWidth());
            unsigned splitEnd = wordStart;
            for (; splitEnd < wordEnd; ++splitEnd) {
                float charWidth = textWidth(textRenderer, text, textLength, splitEnd, splitEnd + 1, 0, style);
                lineWidth.addUncommittedWidth(charWidth);
                if (!lineWidth.fitsOnLine() && splitEnd > lineStart)
                    break;
                lineWidth.commit();
            }
            lineRuns.last().end = splitEnd;
            lineRuns.last().right = lineWidth.committedWidth();
            // To match line boxes, set single-space-only line width to zero.
            if (text[lineRuns.last().start] == ' ' && lineRuns.last().start + 1 == lineRuns.last().end)
                lineRuns.last().right = lineRuns.last().left;
            break;
        }

        lineWidth.commit();

        lineRuns.last().right = lineWidth.committedWidth();
        lineRuns.last().end = wordEnd;

        if (style.collapseWhitespace)
            wordEnd = skipWhitespaces(text, wordEnd, textLength, style.preserveNewline);

        if (!lineWidth.fitsOnLine() && style.wrapLines) {
            // The first run on the line overflows.
            ASSERT(lineRuns.size() == 1);
            break;
        }
    }
    return lineRuns;
}

static float computeLineLeft(ETextAlign textAlign, const LineWidth& lineWidth)
{
    float remainingWidth = lineWidth.availableWidth() - lineWidth.committedWidth();
    float left = lineWidth.logicalLeftOffset();
    switch (textAlign) {
    case LEFT:
    case WEBKIT_LEFT:
    case TASTART:
        return left;
    case RIGHT:
    case WEBKIT_RIGHT:
    case TAEND:
        return left + std::max<float>(remainingWidth, 0);
    case CENTER:
    case WEBKIT_CENTER:
        return left + std::max<float>(remainingWidth / 2, 0);
    case JUSTIFY:
        break;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

static void adjustRunOffsets(Vector<Run, 4>& lineRuns, float adjustment)
{
    if (!adjustment)
        return;
    for (unsigned i = 0; i < lineRuns.size(); ++i) {
        lineRuns[i].left += adjustment;
        lineRuns[i].right += adjustment;
    }
}

template <typename CharacterType>
void createTextRuns(Layout::RunVector& runs, unsigned& lineCount, RenderBlockFlow& flow, RenderText& textRenderer)
{
    const Style style(flow.style());

    const CharacterType* text = textRenderer.text()->characters<CharacterType>();
    const unsigned textLength = textRenderer.textLength();

    LayoutUnit borderAndPaddingBefore = flow.borderAndPaddingBefore();
    LayoutUnit lineHeight = lineHeightFromFlow(flow);

    LazyLineBreakIterator lineBreakIterator(textRenderer.text(), flow.style().locale());

    unsigned lineEnd = 0;
    while (lineEnd < textLength) {
        if (style.collapseWhitespace)
            lineEnd = skipWhitespaces(text, lineEnd, textLength, style.preserveNewline);

        unsigned lineStart = lineEnd;

        // LineWidth reads the current y position from the flow so keep it updated.
        flow.setLogicalHeight(lineHeight * lineCount + borderAndPaddingBefore);
        LineWidth lineWidth(flow, false, DoNotIndentText);

        auto lineRuns = createLineRuns(lineStart, lineWidth, lineBreakIterator, style, text, textLength, textRenderer);

        lineEnd = lineRuns.last().end;
        if (lineStart == lineEnd)
            continue;

        lineRuns.last().isEndOfLine = true;

        float lineLeft = computeLineLeft(style.textAlign, lineWidth);
        adjustRunOffsets(lineRuns, lineLeft);

        for (unsigned i = 0; i < lineRuns.size(); ++i)
            runs.append(lineRuns[i]);

        ++lineCount;
    }
}

std::unique_ptr<Layout> create(RenderBlockFlow& flow)
{
    Layout::RunVector runs;
    unsigned lineCount = 0;

    RenderText& textRenderer = toRenderText(*flow.firstChild());
    ASSERT(!textRenderer.firstTextBox());

    if (textRenderer.is8Bit())
        createTextRuns<LChar>(runs, lineCount, flow, textRenderer);
    else
        createTextRuns<UChar>(runs, lineCount, flow, textRenderer);

    textRenderer.clearNeedsLayout();

    return Layout::create(runs, lineCount);
}

std::unique_ptr<Layout> Layout::create(const RunVector& runVector, unsigned lineCount)
{
    void* slot = WTF::fastMalloc(sizeof(Layout) + sizeof(Run) * runVector.size());
    return std::unique_ptr<Layout>(new (NotNull, slot) Layout(runVector, lineCount));
}

Layout::Layout(const RunVector& runVector, unsigned lineCount)
    : m_lineCount(lineCount)
    , m_runCount(runVector.size())
{
    memcpy(m_runs, runVector.data(), m_runCount * sizeof(Run));
}

}
}
