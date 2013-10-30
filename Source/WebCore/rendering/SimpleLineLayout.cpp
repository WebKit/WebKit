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
#include "GraphicsContext.h"
#include "HitTestLocation.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "InlineTextBox.h"
#include "LineWidth.h"
#include "PaintInfo.h"
#include "RenderBlockFlow.h"
#include "RenderStyle.h"
#include "RenderText.h"
#include "RenderView.h"
#include "SimpleLineLayoutResolver.h"
#include "Text.h"
#include "TextPaintStyle.h"
#include "break_lines.h"
#include <wtf/unicode/Unicode.h>

namespace WebCore {
namespace SimpleLineLayout {

static inline bool isWhitespace(UChar character)
{
    return character == ' ' || character == '\t' || character == '\n';
}

bool canUseFor(const RenderBlockFlow& flow)
{
#if !PLATFORM(MAC) && !PLATFORM(GTK) && !PLATFORM(EFL)
    // FIXME: Non-mac platforms are hitting ASSERT(run.charactersLength() >= run.length())
    // https://bugs.webkit.org/show_bug.cgi?id=123338
    return false;
#endif
    if (!flow.firstChild())
        return false;
    // This currently covers <blockflow>#text</blockflow> case.
    // The <blockflow><inline>#text</inline></blockflow> case is also popular and should be relatively easy to cover.
    if (flow.firstChild() != flow.lastChild())
        return false;
    if (!flow.firstChild()->isText())
        return false;
    // Supporting floats would be very beneficial.
    if (flow.containsFloats())
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
    // These tests only works during layout. Outside layout this function may give false positives.
    if (flow.view().layoutState()) {
#if ENABLE(CSS_SHAPES)
        if (flow.view().layoutState()->shapeInsideInfo())
            return false;
#endif
        if (flow.view().layoutState()->m_columnInfo)
            return false;
    }
    const RenderStyle& style = flow.style();
    if (style.textAlign() == JUSTIFY)
        return false;
    // Non-visible overflow should be pretty easy to support.
    if (style.overflowX() != OVISIBLE || style.overflowY() != OVISIBLE)
        return false;
    // Pre/no-wrap would be very helpful to support.
    if (style.whiteSpace() != NORMAL)
        return false;
    if (!style.textIndent().isZero())
        return false;
    if (style.wordSpacing() || style.letterSpacing())
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
#if ENABLE(CSS_SHAPES)
    if (style.resolvedShapeInside())
        return true;
#endif
    if (style.textOverflow() || (flow.isAnonymousBlock() && flow.parent()->style().textOverflow()))
        return false;
    if (style.hasPseudoStyle(FIRST_LINE) || style.hasPseudoStyle(FIRST_LETTER))
        return false;
    if (style.hasTextCombine())
        return false;
    if (style.overflowWrap() != NormalOverflowWrap)
        return false;
    if (style.backgroundClip() == TextFillBox)
        return false;
    if (style.borderFit() == BorderFitLines)
        return false;
    const RenderText& textRenderer = toRenderText(*flow.firstChild());
    if (textRenderer.isCombineText() || textRenderer.isCounter() || textRenderer.isQuote() || textRenderer.isTextFragment()
#if ENABLE(SVG)
        || textRenderer.isSVGInlineText()
#endif
        )
        return false;
    if (style.font().codePath(TextRun(textRenderer.text())) != Font::Simple)
        return false;

    // We assume that all lines have metrics based purely on the primary font.
    auto& primaryFontData = *style.font().primaryFont();
    if (primaryFontData.isLoading())
        return false;

    unsigned length = textRenderer.textLength();
    for (unsigned i = 0; i < length; ++i) {
        UChar character = textRenderer.characterAt(i);
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

        if (!primaryFontData.glyphForCharacter(character))
            return false;
    }
    return true;
}

static inline unsigned skipWhitespaces(const RenderText& textRenderer, unsigned offset, unsigned length)
{
    for (; offset < length; ++offset) {
        if (!isWhitespace(textRenderer.characterAt(offset)))
            break;
    }
    return offset;
}

static float textWidth(const RenderText& text, unsigned from, unsigned length, float xPosition, const RenderStyle& style)
{
    if (style.font().isFixedPitch() || (!from && length == text.textLength()))
        return text.width(from, length, style.font(), xPosition, nullptr, nullptr);
    // FIXME: Add templated UChar/LChar paths.
    TextRun run = text.is8Bit() ? TextRun(text.characters8() + from, length) : TextRun(text.characters16() + from, length);
    run.setCharactersLength(text.textLength() - from);
    ASSERT(run.charactersLength() >= run.length());

    run.setXPos(xPosition);
    return style.font().width(run);
}

static float computeLineLeft(ETextAlign textAlign, float remainingWidth)
{
    switch (textAlign) {
    case LEFT:
    case WEBKIT_LEFT:
    case TASTART:
        return 0;
    case RIGHT:
    case WEBKIT_RIGHT:
    case TAEND:
        return std::max<float>(remainingWidth, 0);
    case CENTER:
    case WEBKIT_CENTER:
        return std::max<float>(remainingWidth / 2, 0);
    case JUSTIFY:
        break;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

static void adjustRunOffsets(Vector<Run, 4>& lineRuns, ETextAlign textAlign, float lineWidth, float availableWidth)
{
    float lineLeft = computeLineLeft(textAlign, availableWidth - lineWidth);
    for (unsigned i = 0; i < lineRuns.size(); ++i) {
        lineRuns[i].left = floor(lineLeft + lineRuns[i].left);
        lineRuns[i].right = ceil(lineLeft + lineRuns[i].right);
    }
}

std::unique_ptr<Layout> create(RenderBlockFlow& flow)
{
    RenderText& textRenderer = toRenderText(*flow.firstChild());
    ASSERT(!textRenderer.firstTextBox());

    const RenderStyle& style = flow.style();
    const unsigned textLength = textRenderer.textLength();

    ETextAlign textAlign = style.textAlign();
    float wordTrailingSpaceWidth = style.font().width(TextRun(&space, 1));

    LazyLineBreakIterator lineBreakIterator(textRenderer.text(), style.locale());
    int nextBreakable = -1;

    Layout::RunVector runs;
    unsigned lineCount = 0;

    unsigned lineEndOffset = 0;
    while (lineEndOffset < textLength) {
        lineEndOffset = skipWhitespaces(textRenderer, lineEndOffset, textLength);
        unsigned lineStartOffset = lineEndOffset;
        unsigned wordEndOffset = lineEndOffset;
        LineWidth lineWidth(flow, false, DoNotIndentText);

        Vector<Run, 4> lineRuns;
        lineRuns.uncheckedAppend(Run(lineStartOffset, 0));

        while (wordEndOffset < textLength) {
            ASSERT(!isWhitespace(textRenderer.characterAt(wordEndOffset)));

            bool previousWasSpaceBetweenWords = wordEndOffset > lineStartOffset && isWhitespace(textRenderer.characterAt(wordEndOffset - 1));
            unsigned wordStartOffset = previousWasSpaceBetweenWords ? wordEndOffset - 1 : wordEndOffset;

            ++wordEndOffset;
            while (wordEndOffset < textLength) {
                if (wordEndOffset > lineStartOffset && isBreakable(lineBreakIterator, wordEndOffset, nextBreakable, false))
                    break;
                ++wordEndOffset;
            }

            unsigned wordLength = wordEndOffset - wordStartOffset;
            bool includeEndSpace = wordEndOffset < textLength && textRenderer.characterAt(wordEndOffset) == ' ';
            float wordWidth;
            if (includeEndSpace)
                wordWidth = textWidth(textRenderer, wordStartOffset, wordLength + 1, lineWidth.committedWidth(), style) - wordTrailingSpaceWidth;
            else
                wordWidth = textWidth(textRenderer, wordStartOffset, wordLength, lineWidth.committedWidth(), style);

            lineWidth.addUncommittedWidth(wordWidth);

            // Move to the next line if the current one is full and we have something on it.
            if (!lineWidth.fitsOnLine() && lineWidth.committedWidth())
                break;

            if (wordStartOffset > lineEndOffset) {
                // There were more than one consecutive whitespace.
                ASSERT(previousWasSpaceBetweenWords);
                // Include space to the end of the previous run.
                lineRuns.last().textLength++;
                lineRuns.last().right += wordTrailingSpaceWidth;
                // Start a new run on the same line.
                lineRuns.append(Run(wordStartOffset + 1, lineRuns.last().right));
            }

            lineWidth.commit();

            lineRuns.last().right = lineWidth.committedWidth();
            lineRuns.last().textLength = wordEndOffset - lineRuns.last().textOffset;

            lineEndOffset = wordEndOffset;
            wordEndOffset = skipWhitespaces(textRenderer, wordEndOffset, textLength);

            if (!lineWidth.fitsOnLine()) {
                // The first run on the line overflows.
                ASSERT(lineRuns.size() == 1);
                break;
            }
        }
        if (lineStartOffset == lineEndOffset)
            continue;

        adjustRunOffsets(lineRuns, textAlign, lineWidth.committedWidth(), lineWidth.availableWidth());

        for (unsigned i = 0; i < lineRuns.size(); ++i)
            runs.append(lineRuns[i]);

        runs.last().isEndOfLine = true;
        ++lineCount;
    }

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
