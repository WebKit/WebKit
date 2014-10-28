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
    if (!flow.frame().settings().simpleLineLayoutEnabled())
        return false;
    if (!flow.firstChild())
        return false;
    // This currently covers <blockflow>#text</blockflow> case.
    // The <blockflow><inline>#text</inline></blockflow> case is also popular and should be relatively easy to cover.
    if (flow.firstChild() != flow.lastChild())
        return false;
    if (!is<RenderText>(flow.firstChild()))
        return false;
    if (!flow.isHorizontalWritingMode())
        return false;
    if (flow.flowThreadState() != RenderObject::NotInsideFlowThread)
        return false;
    // Printing does pagination without a flow thread.
    if (flow.document().paginated())
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
    if (is<RenderTextControl>(*flow.parent()) && downcast<RenderTextControl>(*flow.parent()).textFormControlElement().placeholderElement())
        return false;
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
    const RenderText& textRenderer = downcast<RenderText>(*flow.firstChild());
    if (flow.containsFloats()) {
        // We can't use the code path if any lines would need to be shifted below floats. This is because we don't keep per-line y coordinates.
        float minimumWidthNeeded = textRenderer.minLogicalWidth();
        for (auto& floatRenderer : *flow.floatingObjectSet()) {
            ASSERT(floatRenderer);
            float availableWidth = flow.availableLogicalWidthForLine(floatRenderer->y(), false);
            if (availableWidth < minimumWidthNeeded)
                return false;
        }
    }
    if (textRenderer.isCombineText() || textRenderer.isCounter() || textRenderer.isQuote() || textRenderer.isTextFragment()
        || textRenderer.isSVGInlineText())
        return false;
    if (style.font().codePath(TextRun(textRenderer.text())) != Font::Simple)
        return false;
    if (style.font().primaryFont()->isSVGFont())
        return false;

    // We assume that all lines have metrics based purely on the primary font.
    auto& primaryFontData = *style.font().primaryFont();
    if (primaryFontData.isLoading())
        return false;
    if (!canUseForText(textRenderer, primaryFontData))
        return false;

    return true;
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

template <typename CharacterType>
static inline unsigned skipWhitespace(const CharacterType* text, unsigned offset, unsigned length, bool preserveNewline, unsigned& spaceCount)
{
    spaceCount = 0;
    for (; offset < length; ++offset) {
        bool isSpace = text[offset] == ' ';
        if (!(isSpace || text[offset] == '\t' || (!preserveNewline && text[offset] == '\n')))
            return offset;
        if (isSpace)
            ++spaceCount;
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

static float computeLineLeft(ETextAlign textAlign, float availableWidth, float committedWidth, float logicalLeftOffset)
{
    float remainingWidth = availableWidth - committedWidth;
    float left = logicalLeftOffset;
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

struct TextFragment {
    TextFragment()
        : start(0)
        , isCollapsedWhitespace(false)
        , end(0)
        , isWhitespaceOnly(false)
        , isBreakable(false)
        , mustBreak(false)
        , width(0)
    { }

    TextFragment(unsigned textStart, unsigned textEnd, unsigned textWidth)
        : start(textStart)
        , isCollapsedWhitespace(false)
        , end(textEnd)
        , isWhitespaceOnly(false)
        , isBreakable(false)
        , mustBreak(false)
        , width(textWidth)
    { }

    bool isEmpty() const
    {
        return start == end;
    }

    unsigned start : 31;
    bool isCollapsedWhitespace : 1;
    unsigned end : 31;
    bool isWhitespaceOnly : 1;
    bool isBreakable;
    bool mustBreak;
    float width;
};

struct LineState {
    LineState()
        : availableWidth(0)
        , logicalLeftOffset(0)
        , uncommittedStart(0)
        , uncommittedEnd(0)
        , uncommittedLeft(0)
        , uncommittedWidth(0)
        , committedWidth(0)
        , end(0)
        , trailingWhitespaceWidth(0)
    {
    }

    void commitAndCreateRun(Layout::RunVector& lineRuns)
    {
        if (uncommittedStart == uncommittedEnd)
            return;

        lineRuns.append(Run(uncommittedStart, uncommittedEnd, uncommittedLeft, uncommittedLeft + uncommittedWidth, false));
        // Move uncommitted to committed.
        committedWidth += uncommittedWidth;

        uncommittedStart = uncommittedEnd;
        uncommittedLeft += uncommittedWidth;
        uncommittedWidth = 0;
    }

    void addUncommitted(const TextFragment& fragment)
    {
        addUncommitted(fragment.end, fragment.width, fragment.isWhitespaceOnly);
    }

    void addUncommitted(unsigned fragmentEnd, float fragmentWidth, bool whitespaceOnly)
    {
        unsigned uncomittedFragmentLength = fragmentEnd - uncommittedEnd;
        uncommittedWidth += fragmentWidth;
        uncommittedEnd = fragmentEnd;
        end = uncommittedEnd;
        trailingWhitespaceWidth = whitespaceOnly ? fragmentWidth : 0;
        trailingWhitespaceLength = whitespaceOnly ? uncomittedFragmentLength  : 0;
    }

    void addUncommittedWhitespace(float whitespaceWidth)
    {
        addUncommitted(uncommittedEnd + 1, whitespaceWidth, true);
    }

    void jumpTo(unsigned newPositon, float leftX)
    {
        end = newPositon;

        uncommittedStart = newPositon;
        uncommittedEnd = newPositon;
        uncommittedLeft = leftX;
        uncommittedWidth = 0;
    }

    float width() const
    {
        return committedWidth + uncommittedWidth;
    }

    bool fits(float extra) const
    {
        return availableWidth >= width() + extra;
    }

    void removeCommittedTrailingWhitespace()
    {
        ASSERT(!uncommittedWidth);
        committedWidth -= trailingWhitespaceWidth;
        trailingWhitespaceWidth = 0;
        trailingWhitespaceLength = 0;
    }

    float availableWidth;
    float logicalLeftOffset;

    unsigned uncommittedStart;
    unsigned uncommittedEnd;
    float uncommittedLeft;
    float uncommittedWidth;
    float committedWidth;
    unsigned end; // End position of the current line.
    float trailingWhitespaceWidth; // Use this to remove trailing whitespace without re-mesuring the text.
    float trailingWhitespaceLength;
};

template <typename CharacterType>
static void removeTrailingWhitespace(LineState& line, TextFragment& fragmentForNextLine, Layout::RunVector& lineRuns, const Style& style, const CharacterType* text, unsigned textLength)
{
    bool preWrap = style.wrapLines && !style.collapseWhitespace;
    // Trailing whitespace gets removed when we either collapse whitespace or pre-wrap is present.
    if (!(style.collapseWhitespace || preWrap))
        return;

    ASSERT(lineRuns.size());
    Run& lastRun = lineRuns.last();

    unsigned originalLineEnd = line.end;
    bool trailingPreWrapWhitespaceNeedsToBeRemoved = false;
    // When pre-wrap is present, trailing whitespace needs to be removed:
    // 1. from the "next line": when at least the first charater fits. When even the first whitespace is wider that the available width, we don't remove any whitespace at all.
    // 2. from this line: remove whitespace, unless it's the only fragment on the line -so removing the whitesapce would produce an empty line.
    if (preWrap) {
        if (fragmentForNextLine.isWhitespaceOnly && !fragmentForNextLine.isEmpty() && line.availableWidth >= line.committedWidth) {
            line.end = fragmentForNextLine.end;
            fragmentForNextLine = TextFragment();
        }
        if (line.trailingWhitespaceLength)
            trailingPreWrapWhitespaceNeedsToBeRemoved = !(line.committedWidth == line.trailingWhitespaceWidth); // Check if we've got only whitespace on this line.
    }
    if (line.trailingWhitespaceLength && (style.collapseWhitespace || trailingPreWrapWhitespaceNeedsToBeRemoved)) {
        lastRun.right -= line.trailingWhitespaceWidth;
        lastRun.end -= line.trailingWhitespaceLength;
        if (lastRun.start == lastRun.end)
            lineRuns.removeLast();
        line.removeCommittedTrailingWhitespace();
    }

    // If we skipped any whitespace and now the line end is a "preserved" newline, skip the newline too as we are wrapping the line here already.
    if (originalLineEnd != line.end && style.preserveNewline && line.end < textLength && text[line.end] == '\n')
        ++line.end;
}

template <typename CharacterType>
static void initializeLine(LineState& line, const Style& style, const CharacterType* text, unsigned textLength)
{
    line.committedWidth = 0;
    line.uncommittedLeft = 0;
    line.trailingWhitespaceWidth = 0;
    line.trailingWhitespaceLength = 0;
    // Skip leading whitespace if collapsing whitespace, unless there's an uncommitted fragment pushed from the previous line.
    // FIXME: Be smarter when the run from the previous line does not fit the current line. Right now, we just reprocess it.
    if (line.uncommittedWidth && !line.fits(line.uncommittedWidth))
        line.jumpTo(line.uncommittedStart, 0); // Start over with this fragment.
    else if (!line.uncommittedWidth) {
        unsigned spaceCount = 0;
        line.jumpTo(style.collapseWhitespace ? skipWhitespace(text, line.end, textLength, style.preserveNewline, spaceCount) : line.end, 0);
    }
}

template <typename CharacterType>
static TextFragment splitFragmentToFitLine(TextFragment& fragmentToSplit, float availableWidth, bool keepAtLeastOneCharacter, const RenderText& textRenderer,
    const CharacterType* text, unsigned textLength, const Style& style)
{
    // Fast path for single char fragments.
    if (fragmentToSplit.start + 1 == fragmentToSplit.end) {
        if (keepAtLeastOneCharacter)
            return TextFragment();

        TextFragment fragmentForNextLine(fragmentToSplit);
        fragmentToSplit.end = fragmentToSplit.start;
        fragmentToSplit.width = 0;
        return fragmentForNextLine;
    }
    // Simple binary search to find out what fits the current line.
    // FIXME: add surrogate pair support.
    unsigned left = fragmentToSplit.start;
    unsigned right = fragmentToSplit.end - 1; // We can ignore the last character. It surely does not fit.
    float width = 0;
    while (left < right) {
        unsigned middle = (left + right) / 2;
        width = textWidth(textRenderer, text, textLength, fragmentToSplit.start, middle + 1, 0, style);
        if (availableWidth > width)
            left = middle + 1;
        else if (availableWidth < width)
            right = middle;
        else {
            right = middle + 1;
            break;
        }
    }

    if (keepAtLeastOneCharacter && right == fragmentToSplit.start)
        ++right;
    TextFragment fragmentForNextLine(fragmentToSplit);
    fragmentToSplit.end = right;
    fragmentToSplit.width = fragmentToSplit.isEmpty() ? 0 : textWidth(textRenderer, text, textLength, fragmentToSplit.start, right, 0, style);

    fragmentForNextLine.start = fragmentToSplit.end;
    fragmentForNextLine.width -= fragmentToSplit.width;
    return fragmentForNextLine;
}

template <typename CharacterType>
static TextFragment nextFragment(unsigned previousFragmentEnd, LazyLineBreakIterator& lineBreakIterator, const Style& style, const CharacterType* text, unsigned textLength,
    float xPosition, const RenderText& textRenderer)
{
    // A fragment can have
    // 1. new line character when preserveNewline is on (not considered as whitespace) or
    // 2. whitespace (collasped, non-collapsed multi or single) or
    // 3. non-whitespace characters.
    TextFragment fragment;
    fragment.mustBreak = style.preserveNewline && text[previousFragmentEnd] == '\n';
    unsigned spaceCount = 0;
    unsigned whitespaceEnd = previousFragmentEnd;
    if (!fragment.mustBreak)
        whitespaceEnd = skipWhitespace(text, previousFragmentEnd, textLength, style.preserveNewline, spaceCount);
    fragment.isWhitespaceOnly = previousFragmentEnd < whitespaceEnd;
    fragment.start = previousFragmentEnd;
    if (fragment.isWhitespaceOnly)
        fragment.end = whitespaceEnd;
    else if (fragment.mustBreak)
        fragment.end = fragment.start + 1;
    else
        fragment.end = nextBreakablePosition<CharacterType, false>(lineBreakIterator, text, textLength, previousFragmentEnd + 1);
    bool multiple = fragment.start + 1 < fragment.end;
    fragment.isCollapsedWhitespace = multiple && fragment.isWhitespaceOnly && style.collapseWhitespace;
    // Non-collapsed whitespace or just plain words when "break word on overflow" is on can wrap.
    fragment.isBreakable = multiple && ((fragment.isWhitespaceOnly && !fragment.isCollapsedWhitespace) || (!fragment.isWhitespaceOnly && style.breakWordOnOverflow));

    // Compute fragment width or just use the pre-computed whitespace widths.
    unsigned fragmentLength = fragment.end - fragment.start;
    if (fragment.isCollapsedWhitespace)
        fragment.width = style.spaceWidth;
    else if (fragment.mustBreak)
        fragment.width = 0; // Newline character's width is 0.
    else if (fragmentLength == spaceCount) // Space only.
        fragment.width = style.spaceWidth * spaceCount;
    else
        fragment.width = textWidth(textRenderer, text, textLength, fragment.start, fragment.end, xPosition, style);
    return fragment;
}

template <typename CharacterType>
bool createLineRuns(LineState& line, Layout::RunVector& lineRuns, unsigned& lineCount, LazyLineBreakIterator& lineBreakIterator, const Style& style, const CharacterType* text,
    unsigned textLength, const RenderText& textRenderer)
{
    unsigned previousNumberOfRuns = lineRuns.size();
    TextFragment fragmentForNextLine;
    initializeLine(line, style, text, textLength);
    bool lineCanBeWrapped = style.wrapLines || style.breakWordOnOverflow;
    while (line.end < textLength) {
        // Find the next text fragment. Start from the end of the previous fragment -current line end.
        TextFragment fragment = nextFragment(line.end, lineBreakIterator, style, text, textLength, line.width(), textRenderer);
        if ((lineCanBeWrapped && !line.fits(fragment.width)) || fragment.mustBreak) {
            // Overflow wrapping behaviour:
            // 1. Newline character: wraps the line unless it's treated as whitespace.
            // 2. Whitesapce collapse on: whitespace is skipped.
            // 3. Whitespace collapse off: whitespace is wrapped.
            // 4. First, non-whitespace fragment is either wrapped or kept on the line. (depends on overflow-wrap)
            // 5. Non-whitespace fragment when there's already another fragment on the line gets pushed to the next line.
            bool isFirstFragment = !line.width();
            if (fragment.mustBreak) {
                if (isFirstFragment)
                    line.addUncommitted(fragment);
                else {
                    // No need to add the new line fragment if there's already content on the line. We are about to close this line anyway.
                    ++line.end;
                }
            } else if (style.collapseWhitespace && fragment.isWhitespaceOnly) {
                // Whitespace collapse is on: whitespace that doesn't fit is simply skipped.
                line.end = fragment.end;
            } else if (fragment.isWhitespaceOnly || ((isFirstFragment && style.breakWordOnOverflow) || !style.wrapLines)) { // !style.wrapLines: bug138102(preserve existing behavior)
                // Whitespace collapse is off or non-whitespace content. split the fragment; (modified)fragment -> this line, fragmentForNextLine -> next line.
                // When this is the only (first) fragment, the first character stays on the line, even if it does not fit.
                fragmentForNextLine = splitFragmentToFitLine(fragment, line.availableWidth - line.width(), isFirstFragment, textRenderer, text, textLength, style);
                if (!fragment.isEmpty()) {
                    // Whitespace fragments can get pushed entirely to the next line.
                    line.addUncommitted(fragment);
                }
            } else if (isFirstFragment) {
                // Non-breakable non-whitespace first fragment. Add it to the current line. -it overflows though.
                line.addUncommitted(fragment);
            } else {
                // Non-breakable non-whitespace fragment when there's already a fragment on the line. Push it to the next line.
                fragmentForNextLine = fragment;
            }
            break;
        }
        // When the current fragment is collapsed whitespace, we need to create a run for what we've processed so far.
        if (fragment.isCollapsedWhitespace) {
            // One trailing whitespace to preserve.
            line.addUncommittedWhitespace(style.spaceWidth);
            line.commitAndCreateRun(lineRuns);
            // And skip the collapsed whitespace.
            line.jumpTo(fragment.end, line.width() + fragment.width - style.spaceWidth);
        } else
            line.addUncommitted(fragment);
    }
    line.commitAndCreateRun(lineRuns);
    // Postprocessing the runs we added for this line.
    if (previousNumberOfRuns != lineRuns.size()) {
        removeTrailingWhitespace(line, fragmentForNextLine, lineRuns, style, text, textLength);
        lineRuns.last().isEndOfLine = true;
        // Adjust runs' position by taking line's alignment into account.
        if (float lineLeft = computeLineLeft(style.textAlign, line.availableWidth, line.committedWidth, line.logicalLeftOffset)) {
            for (unsigned i = previousNumberOfRuns; i < lineRuns.size(); ++i) {
                lineRuns[i].left += lineLeft;
                lineRuns[i].right += lineLeft;
            }
        }
        ++lineCount;
    }
    if (!fragmentForNextLine.isEmpty())
        line.addUncommitted(fragmentForNextLine);
    return line.end < textLength || line.uncommittedWidth;
}

static void updateLineConstrains(const RenderBlockFlow& flow, float& availableWidth, float& logicalLeftOffset)
{
    LayoutUnit height = flow.logicalHeight();
    LayoutUnit logicalHeight = flow.minLineHeightForReplacedRenderer(false, 0);
    float logicalRightOffset = flow.logicalRightOffsetForLine(height, false, logicalHeight);
    logicalLeftOffset = flow.logicalLeftOffsetForLine(height, false, logicalHeight);
    availableWidth = std::max<float>(0, logicalRightOffset - logicalLeftOffset);
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
    LineState line;

    do {
        flow.setLogicalHeight(lineHeight * lineCount + borderAndPaddingBefore);
        updateLineConstrains(flow, line.availableWidth, line.logicalLeftOffset);
    } while (createLineRuns(line, runs, lineCount, lineBreakIterator, style, text, textLength, textRenderer));
}

std::unique_ptr<Layout> create(RenderBlockFlow& flow)
{
    Layout::RunVector runs;
    unsigned lineCount = 0;

    RenderText& textRenderer = downcast<RenderText>(*flow.firstChild());
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
