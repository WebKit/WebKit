/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Andrew Wellington (proton@wiretapped.net)
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "RenderText.h"

#include "AXObjectCache.h"
#include "EllipsisBox.h"
#include "FloatQuad.h"
#include "Frame.h"
#include "FrameView.h"
#include "Hyphenation.h"
#include "InlineTextBox.h"
#include "Range.h"
#include "RenderBlock.h"
#include "RenderCombineText.h"
#include "RenderLayer.h"
#include "RenderView.h"
#include "Settings.h"
#include "SimpleLineLayoutFunctions.h"
#include "Text.h"
#include "TextBreakIterator.h"
#include "TextResourceDecoder.h"
#include "VisiblePosition.h"
#include "break_lines.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringBuffer.h>
#include <wtf/unicode/CharacterNames.h>

#if PLATFORM(IOS)
#include "Document.h"
#include "EditorClient.h"
#include "LogicalSelectionOffsetCaches.h"
#include "Page.h"
#include "SelectionRect.h"
#endif

using namespace WTF;
using namespace Unicode;

namespace WebCore {

struct SameSizeAsRenderText : public RenderObject {
    uint32_t bitfields : 16;
#if ENABLE(IOS_TEXT_AUTOSIZING)
    float candidateTextSize;
#endif
    float widths[4];
    String text;
    void* pointers[2];
};

COMPILE_ASSERT(sizeof(RenderText) == sizeof(SameSizeAsRenderText), RenderText_should_stay_small);

class SecureTextTimer;
typedef HashMap<RenderText*, SecureTextTimer*> SecureTextTimerMap;
static SecureTextTimerMap* gSecureTextTimers = 0;

class SecureTextTimer : public TimerBase {
public:
    SecureTextTimer(RenderText* renderText)
        : m_renderText(renderText)
        , m_lastTypedCharacterOffset(-1)
    {
    }

    void restartWithNewText(unsigned lastTypedCharacterOffset)
    {
        m_lastTypedCharacterOffset = lastTypedCharacterOffset;
        const Settings& settings = m_renderText->frame().settings();
        startOneShot(settings.passwordEchoDurationInSeconds());
    }
    void invalidate() { m_lastTypedCharacterOffset = -1; }
    unsigned lastTypedCharacterOffset() { return m_lastTypedCharacterOffset; }

private:
    virtual void fired()
    {
        ASSERT(gSecureTextTimers->contains(m_renderText));
        m_renderText->setText(m_renderText->text(), true /* forcing setting text as it may be masked later */);
    }

    RenderText* m_renderText;
    int m_lastTypedCharacterOffset;
};

static HashMap<const RenderText*, String>& originalTextMap()
{
    static NeverDestroyed<HashMap<const RenderText*, String>> map;
    return map;
}

static void makeCapitalized(String* string, UChar previous)
{
    // FIXME: Need to change this to use u_strToTitle instead of u_totitle and to consider locale.

    if (string->isNull())
        return;

    unsigned length = string->length();
    const StringImpl& stringImpl = *string->impl();

    if (length >= std::numeric_limits<unsigned>::max())
        CRASH();

    StringBuffer<UChar> stringWithPrevious(length + 1);
    stringWithPrevious[0] = previous == noBreakSpace ? ' ' : previous;
    for (unsigned i = 1; i < length + 1; i++) {
        // Replace &nbsp with a real space since ICU no longer treats &nbsp as a word separator.
        if (stringImpl[i - 1] == noBreakSpace)
            stringWithPrevious[i] = ' ';
        else
            stringWithPrevious[i] = stringImpl[i - 1];
    }

    TextBreakIterator* boundary = wordBreakIterator(StringView(stringWithPrevious.characters(), length + 1));
    if (!boundary)
        return;

    StringBuilder result;
    result.reserveCapacity(length);

    int32_t endOfWord;
    int32_t startOfWord = textBreakFirst(boundary);
    for (endOfWord = textBreakNext(boundary); endOfWord != TextBreakDone; startOfWord = endOfWord, endOfWord = textBreakNext(boundary)) {
        if (startOfWord) // Ignore first char of previous string
            result.append(stringImpl[startOfWord - 1] == noBreakSpace ? noBreakSpace : u_totitle(stringWithPrevious[startOfWord]));
        for (int i = startOfWord + 1; i < endOfWord; i++)
            result.append(stringImpl[i - 1]);
    }

    *string = result.toString();
}

RenderText::RenderText(Text& textNode, const String& text)
    : RenderObject(textNode)
    , m_hasTab(false)
    , m_linesDirty(false)
    , m_containsReversedText(false)
    , m_isAllASCII(text.containsOnlyASCII())
    , m_knownToHaveNoOverflowAndNoFallbackFonts(false)
    , m_useBackslashAsYenSymbol(false)
    , m_originalTextDiffersFromRendered(false)
#if ENABLE(IOS_TEXT_AUTOSIZING)
    , m_candidateComputedTextSize(0)
#endif
    , m_minWidth(-1)
    , m_maxWidth(-1)
    , m_beginMinWidth(0)
    , m_endMinWidth(0)
    , m_text(text)
{
    ASSERT(!m_text.isNull());
    setIsText();
    m_canUseSimpleFontCodePath = computeCanUseSimpleFontCodePath();
    view().frameView().incrementVisuallyNonEmptyCharacterCount(textLength());
}

RenderText::RenderText(Document& document, const String& text)
    : RenderObject(document)
    , m_hasTab(false)
    , m_linesDirty(false)
    , m_containsReversedText(false)
    , m_isAllASCII(text.containsOnlyASCII())
    , m_knownToHaveNoOverflowAndNoFallbackFonts(false)
    , m_useBackslashAsYenSymbol(false)
    , m_originalTextDiffersFromRendered(false)
#if ENABLE(IOS_TEXT_AUTOSIZING)
    , m_candidateComputedTextSize(0)
#endif
    , m_minWidth(-1)
    , m_maxWidth(-1)
    , m_beginMinWidth(0)
    , m_endMinWidth(0)
    , m_text(text)
{
    ASSERT(!m_text.isNull());
    setIsText();
    m_canUseSimpleFontCodePath = computeCanUseSimpleFontCodePath();
    view().frameView().incrementVisuallyNonEmptyCharacterCount(textLength());
}

RenderText::~RenderText()
{
    if (m_originalTextDiffersFromRendered)
        originalTextMap().remove(this);
}

const char* RenderText::renderName() const
{
    return "RenderText";
}

Text* RenderText::textNode() const
{
    return toText(RenderObject::node());
}

bool RenderText::isTextFragment() const
{
    return false;
}

bool RenderText::computeUseBackslashAsYenSymbol() const
{
    const RenderStyle& style = this->style();
    const FontDescription& fontDescription = style.font().fontDescription();
    if (style.font().useBackslashAsYenSymbol())
        return true;
    if (fontDescription.isSpecifiedFont())
        return false;
    const TextEncoding* encoding = document().decoder() ? &document().decoder()->encoding() : 0;
    if (encoding && encoding->backslashAsCurrencySymbol() != '\\')
        return true;
    return false;
}

void RenderText::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    // There is no need to ever schedule repaints from a style change of a text run, since
    // we already did this for the parent of the text run.
    // We do have to schedule layouts, though, since a style change can force us to
    // need to relayout.
    if (diff == StyleDifferenceLayout) {
        setNeedsLayoutAndPrefWidthsRecalc();
        m_knownToHaveNoOverflowAndNoFallbackFonts = false;
    }

    const RenderStyle& newStyle = style();
    bool needsResetText = false;
    if (!oldStyle) {
        m_useBackslashAsYenSymbol = computeUseBackslashAsYenSymbol();
        needsResetText = m_useBackslashAsYenSymbol;
    } else if (oldStyle->font().useBackslashAsYenSymbol() != newStyle.font().useBackslashAsYenSymbol()) {
        m_useBackslashAsYenSymbol = computeUseBackslashAsYenSymbol();
        needsResetText = true;
    }

    ETextTransform oldTransform = oldStyle ? oldStyle->textTransform() : TTNONE;
    ETextSecurity oldSecurity = oldStyle ? oldStyle->textSecurity() : TSNONE;
    if (needsResetText || oldTransform != newStyle.textTransform() || oldSecurity != newStyle.textSecurity())
        RenderText::setText(originalText(), true);
}

void RenderText::removeAndDestroyTextBoxes()
{
    if (!documentBeingDestroyed())
        m_lineBoxes.removeAllFromParent(*this);
#if !ASSERT_WITH_SECURITY_IMPLICATION_DISABLED
    else
        m_lineBoxes.invalidateParentChildLists();
#endif
    m_lineBoxes.deleteAll();
}

void RenderText::willBeDestroyed()
{
    if (SecureTextTimer* secureTextTimer = gSecureTextTimers ? gSecureTextTimers->take(this) : 0)
        delete secureTextTimer;

    removeAndDestroyTextBoxes();
    RenderObject::willBeDestroyed();
}

void RenderText::deleteLineBoxesBeforeSimpleLineLayout()
{
    m_lineBoxes.deleteAll();
}

String RenderText::originalText() const
{
    return m_originalTextDiffersFromRendered ? originalTextMap().get(this) : m_text;
}

void RenderText::absoluteRects(Vector<IntRect>& rects, const LayoutPoint& accumulatedOffset) const
{
    if (auto layout = simpleLineLayout()) {
        rects.appendVector(collectTextAbsoluteRects(*this, *layout, accumulatedOffset));
        return;
    }
    rects.appendVector(m_lineBoxes.absoluteRects(accumulatedOffset));
}

Vector<IntRect> RenderText::absoluteRectsForRange(unsigned start, unsigned end, bool useSelectionHeight, bool* wasFixed) const
{
    const_cast<RenderText&>(*this).ensureLineBoxes();

    // Work around signed/unsigned issues. This function takes unsigneds, and is often passed UINT_MAX
    // to mean "all the way to the end". InlineTextBox coordinates are unsigneds, so changing this 
    // function to take ints causes various internal mismatches. But selectionRect takes ints, and 
    // passing UINT_MAX to it causes trouble. Ideally we'd change selectionRect to take unsigneds, but 
    // that would cause many ripple effects, so for now we'll just clamp our unsigned parameters to INT_MAX.
    ASSERT(end == UINT_MAX || end <= INT_MAX);
    ASSERT(start <= INT_MAX);
    start = std::min(start, static_cast<unsigned>(INT_MAX));
    end = std::min(end, static_cast<unsigned>(INT_MAX));
    
    return m_lineBoxes.absoluteRectsForRange(*this, start, end, useSelectionHeight, wasFixed);
}

#if PLATFORM(IOS)
// This function is similar in spirit to addLineBoxRects, but returns rectangles
// which are annotated with additional state which helps the iPhone draw selections in its unique way.
// Full annotations are added in this class.
void RenderText::collectSelectionRects(Vector<SelectionRect>& rects, unsigned start, unsigned end)
{
    // FIXME: Work around signed/unsigned issues. This function takes unsigneds, and is often passed UINT_MAX
    // to mean "all the way to the end". InlineTextBox coordinates are unsigneds, so changing this 
    // function to take ints causes various internal mismatches. But selectionRect takes ints, and 
    // passing UINT_MAX to it causes trouble. Ideally we'd change selectionRect to take unsigneds, but 
    // that would cause many ripple effects, so for now we'll just clamp our unsigned parameters to INT_MAX.
    ASSERT(end == std::numeric_limits<unsigned>::max() || end <= std::numeric_limits<int>::max());
    ASSERT(start <= std::numeric_limits<int>::max());
    start = std::min(start, static_cast<unsigned>(std::numeric_limits<int>::max()));
    end = std::min(end, static_cast<unsigned>(std::numeric_limits<int>::max()));

    for (InlineTextBox* box = firstTextBox(); box; box = box->nextTextBox()) {
        LayoutRect rect;
        // Note, box->end() returns the index of the last character, not the index past it.
        if (start <= box->start() && box->end() < end)
            rect = box->localSelectionRect(start, end);
        else {
            unsigned realEnd = std::min(box->end() + 1, end);
            rect = box->localSelectionRect(start, realEnd);
            if (rect.isEmpty())
                continue;
        }

        if (box->root().isFirstAfterPageBreak()) {
            if (box->isHorizontal())
                rect.shiftYEdgeTo(box->root().lineTopWithLeading());
            else
                rect.shiftXEdgeTo(box->root().lineTopWithLeading());
        }

        RenderBlock* containingBlock = this->containingBlock();
        // Map rect, extended left to leftOffset, and right to rightOffset, through transforms to get minX and maxX.
        LogicalSelectionOffsetCaches cache(*containingBlock);
        LayoutUnit leftOffset = containingBlock->logicalLeftSelectionOffset(*containingBlock, box->logicalTop(), cache);
        LayoutUnit rightOffset = containingBlock->logicalRightSelectionOffset(*containingBlock, box->logicalTop(), cache);
        LayoutRect extentsRect = rect;
        if (box->isHorizontal()) {
            extentsRect.setX(leftOffset);
            extentsRect.setWidth(rightOffset - leftOffset);
        } else {
            extentsRect.setY(leftOffset);
            extentsRect.setHeight(rightOffset - leftOffset);
        }
        extentsRect = localToAbsoluteQuad(FloatRect(extentsRect)).enclosingBoundingBox();
        if (!box->isHorizontal())
            extentsRect = extentsRect.transposedRect();
        bool isFirstOnLine = !box->previousOnLineExists();
        bool isLastOnLine = !box->nextOnLineExists();
        if (containingBlock->isRubyBase() || containingBlock->isRubyText())
            isLastOnLine = !containingBlock->containingBlock()->inlineBoxWrapper()->nextOnLineExists();

        bool containsStart = box->start() <= start && box->end() + 1 >= start;
        bool containsEnd = box->start() <= end && box->end() + 1 >= end;

        bool isFixed = false;
        IntRect absRect = localToAbsoluteQuad(FloatRect(rect), false, &isFixed).enclosingBoundingBox();
        bool boxIsHorizontal = !box->isSVGInlineTextBox() ? box->isHorizontal() : !style().svgStyle().isVerticalWritingMode();
        // If the containing block is an inline element, we want to check the inlineBoxWrapper orientation
        // to determine the orientation of the block. In this case we also use the inlineBoxWrapper to
        // determine if the element is the last on the line.
        if (containingBlock->inlineBoxWrapper()) {
            if (containingBlock->inlineBoxWrapper()->isHorizontal() != boxIsHorizontal) {
                boxIsHorizontal = containingBlock->inlineBoxWrapper()->isHorizontal();
                isLastOnLine = !containingBlock->inlineBoxWrapper()->nextOnLineExists();
            }
        }

        rects.append(SelectionRect(absRect, box->direction(), extentsRect.x(), extentsRect.maxX(), extentsRect.maxY(), 0, box->isLineBreak(), isFirstOnLine, isLastOnLine, containsStart, containsEnd, boxIsHorizontal, isFixed, containingBlock->isRubyText(), columnNumberForOffset(absRect.x())));
    }
}
#endif

Vector<FloatQuad> RenderText::absoluteQuadsClippedToEllipsis() const
{
    if (auto layout = simpleLineLayout()) {
        ASSERT(style().textOverflow() != TextOverflowEllipsis);
        return collectTextAbsoluteQuads(*this, *layout, nullptr);
    }
    return m_lineBoxes.absoluteQuads(*this, nullptr, RenderTextLineBoxes::ClipToEllipsis);
}

void RenderText::absoluteQuads(Vector<FloatQuad>& quads, bool* wasFixed) const
{
    if (auto layout = simpleLineLayout()) {
        quads.appendVector(collectTextAbsoluteQuads(*this, *layout, wasFixed));
        return;
    }
    quads.appendVector(m_lineBoxes.absoluteQuads(*this, wasFixed, RenderTextLineBoxes::NoClipping));
}

Vector<FloatQuad> RenderText::absoluteQuadsForRange(unsigned start, unsigned end, bool useSelectionHeight, bool* wasFixed) const
{
    const_cast<RenderText&>(*this).ensureLineBoxes();

    // Work around signed/unsigned issues. This function takes unsigneds, and is often passed UINT_MAX
    // to mean "all the way to the end". InlineTextBox coordinates are unsigneds, so changing this 
    // function to take ints causes various internal mismatches. But selectionRect takes ints, and 
    // passing UINT_MAX to it causes trouble. Ideally we'd change selectionRect to take unsigneds, but 
    // that would cause many ripple effects, so for now we'll just clamp our unsigned parameters to INT_MAX.
    ASSERT(end == UINT_MAX || end <= INT_MAX);
    ASSERT(start <= INT_MAX);
    start = std::min(start, static_cast<unsigned>(INT_MAX));
    end = std::min(end, static_cast<unsigned>(INT_MAX));
    
    return m_lineBoxes.absoluteQuadsForRange(*this, start, end, useSelectionHeight, wasFixed);
}

VisiblePosition RenderText::positionForPoint(const LayoutPoint& point)
{
    ensureLineBoxes();

    return m_lineBoxes.positionForPoint(*this, point);
}

LayoutRect RenderText::localCaretRect(InlineBox* inlineBox, int caretOffset, LayoutUnit* extraWidthToEndOfLine)
{
    if (!inlineBox)
        return LayoutRect();

    InlineTextBox* box = toInlineTextBox(inlineBox);
    float left = box->positionForOffset(caretOffset);
    return box->root().computeCaretRect(left, caretWidth, extraWidthToEndOfLine);
}

ALWAYS_INLINE float RenderText::widthFromCache(const Font& f, int start, int len, float xPos, HashSet<const SimpleFontData*>* fallbackFonts, GlyphOverflow* glyphOverflow, const RenderStyle& style) const
{
    if (style.hasTextCombine() && isCombineText()) {
        const RenderCombineText& combineText = toRenderCombineText(*this);
        if (combineText.isCombined())
            return combineText.combinedTextWidth(f);
    }

    if (f.isFixedPitch() && !f.isSmallCaps() && m_isAllASCII && (!glyphOverflow || !glyphOverflow->computeBounds)) {
        float monospaceCharacterWidth = f.spaceWidth();
        float w = 0;
        bool isSpace;
        ASSERT(m_text);
        StringImpl& text = *m_text.impl();
        for (int i = start; i < start + len; i++) {
            char c = text[i];
            if (c <= ' ') {
                if (c == ' ' || c == '\n') {
                    w += monospaceCharacterWidth;
                    isSpace = true;
                } else if (c == '\t') {
                    if (style.collapseWhiteSpace()) {
                        w += monospaceCharacterWidth;
                        isSpace = true;
                    } else {
                        w += f.tabWidth(style.tabSize(), xPos + w);
                        isSpace = false;
                    }
                } else
                    isSpace = false;
            } else {
                w += monospaceCharacterWidth;
                isSpace = false;
            }
            if (isSpace && i > start)
                w += f.wordSpacing();
        }
        return w;
    }

    TextRun run = RenderBlock::constructTextRun(const_cast<RenderText*>(this), f, this, start, len, style);
    run.setCharactersLength(textLength() - start);
    ASSERT(run.charactersLength() >= run.length());

    run.setCharacterScanForCodePath(!canUseSimpleFontCodePath());
    run.setTabSize(!style.collapseWhiteSpace(), style.tabSize());
    run.setXPos(xPos);
    return f.width(run, fallbackFonts, glyphOverflow);
}

void RenderText::trimmedPrefWidths(float leadWidth,
                                   float& beginMinW, bool& beginWS,
                                   float& endMinW, bool& endWS,
                                   bool& hasBreakableChar, bool& hasBreak,
                                   float& beginMaxW, float& endMaxW,
                                   float& minW, float& maxW, bool& stripFrontSpaces)
{
    const RenderStyle& style = this->style();
    bool collapseWhiteSpace = style.collapseWhiteSpace();
    if (!collapseWhiteSpace)
        stripFrontSpaces = false;

    if (m_hasTab || preferredLogicalWidthsDirty())
        computePreferredLogicalWidths(leadWidth);

    beginWS = !stripFrontSpaces && m_hasBeginWS;
    endWS = m_hasEndWS;

    int len = textLength();

    if (!len || (stripFrontSpaces && text()->containsOnlyWhitespace())) {
        beginMinW = 0;
        endMinW = 0;
        beginMaxW = 0;
        endMaxW = 0;
        minW = 0;
        maxW = 0;
        hasBreak = false;
        return;
    }

    minW = m_minWidth;
    maxW = m_maxWidth;

    beginMinW = m_beginMinWidth;
    endMinW = m_endMinWidth;

    hasBreakableChar = m_hasBreakableChar;
    hasBreak = m_hasBreak;

    ASSERT(m_text);
    StringImpl& text = *m_text.impl();
    if (text[0] == ' ' || (text[0] == '\n' && !style.preserveNewline()) || text[0] == '\t') {
        const Font& font = style.font(); // FIXME: This ignores first-line.
        if (stripFrontSpaces) {
            const UChar space = ' ';
            float spaceWidth = font.width(RenderBlock::constructTextRun(this, font, &space, 1, style));
            maxW -= spaceWidth;
        } else
            maxW += font.wordSpacing();
    }

    stripFrontSpaces = collapseWhiteSpace && m_hasEndWS;

    if (!style.autoWrap() || minW > maxW)
        minW = maxW;

    // Compute our max widths by scanning the string for newlines.
    if (hasBreak) {
        const Font& f = style.font(); // FIXME: This ignores first-line.
        bool firstLine = true;
        beginMaxW = maxW;
        endMaxW = maxW;
        for (int i = 0; i < len; i++) {
            int linelen = 0;
            while (i + linelen < len && text[i + linelen] != '\n')
                linelen++;

            if (linelen) {
                endMaxW = widthFromCache(f, i, linelen, leadWidth + endMaxW, 0, 0, style);
                if (firstLine) {
                    firstLine = false;
                    leadWidth = 0;
                    beginMaxW = endMaxW;
                }
                i += linelen;
            } else if (firstLine) {
                beginMaxW = 0;
                firstLine = false;
                leadWidth = 0;
            }

            if (i == len - 1)
                // A <pre> run that ends with a newline, as in, e.g.,
                // <pre>Some text\n\n<span>More text</pre>
                endMaxW = 0;
        }
    }
}

static inline bool isSpaceAccordingToStyle(UChar c, const RenderStyle& style)
{
    return c == ' ' || (c == noBreakSpace && style.nbspMode() == SPACE);
}

float RenderText::minLogicalWidth() const
{
    if (preferredLogicalWidthsDirty())
        const_cast<RenderText*>(this)->computePreferredLogicalWidths(0);
        
    return m_minWidth;
}

float RenderText::maxLogicalWidth() const
{
    if (preferredLogicalWidthsDirty())
        const_cast<RenderText*>(this)->computePreferredLogicalWidths(0);
        
    return m_maxWidth;
}

void RenderText::computePreferredLogicalWidths(float leadWidth)
{
    HashSet<const SimpleFontData*> fallbackFonts;
    GlyphOverflow glyphOverflow;
    computePreferredLogicalWidths(leadWidth, fallbackFonts, glyphOverflow);
    if (fallbackFonts.isEmpty() && !glyphOverflow.left && !glyphOverflow.right && !glyphOverflow.top && !glyphOverflow.bottom)
        m_knownToHaveNoOverflowAndNoFallbackFonts = true;
}

static inline float hyphenWidth(RenderText* renderer, const Font& font)
{
    const RenderStyle& style = renderer->style();
    return font.width(RenderBlock::constructTextRun(renderer, font, style.hyphenString().string(), style));
}

static float maxWordFragmentWidth(RenderText* renderer, const RenderStyle& style, const Font& font, StringView word, int minimumPrefixLength, unsigned minimumSuffixLength, int& suffixStart, HashSet<const SimpleFontData*>& fallbackFonts, GlyphOverflow& glyphOverflow)
{
    suffixStart = 0;
    if (word.length() <= minimumSuffixLength)
        return 0;

    Vector<int, 8> hyphenLocations;
    int hyphenLocation = word.length() - minimumSuffixLength;
    while ((hyphenLocation = lastHyphenLocation(word, hyphenLocation, style.locale())) >= minimumPrefixLength)
        hyphenLocations.append(hyphenLocation);

    if (hyphenLocations.isEmpty())
        return 0;

    hyphenLocations.reverse();

    float minimumFragmentWidthToConsider = font.pixelSize() * 5 / 4 + hyphenWidth(renderer, font);
    float maxFragmentWidth = 0;
    for (size_t k = 0; k < hyphenLocations.size(); ++k) {
        int fragmentLength = hyphenLocations[k] - suffixStart;
        StringBuilder fragmentWithHyphen;
        fragmentWithHyphen.append(word.substring(suffixStart, fragmentLength));
        fragmentWithHyphen.append(style.hyphenString());

        TextRun run = RenderBlock::constructTextRun(renderer, font, fragmentWithHyphen.deprecatedCharacters(), fragmentWithHyphen.length(), style);
        run.setCharactersLength(fragmentWithHyphen.length());
        run.setCharacterScanForCodePath(!renderer->canUseSimpleFontCodePath());
        float fragmentWidth = font.width(run, &fallbackFonts, &glyphOverflow);

        // Narrow prefixes are ignored. See tryHyphenating in RenderBlockLineLayout.cpp.
        if (fragmentWidth <= minimumFragmentWidthToConsider)
            continue;

        suffixStart += fragmentLength;
        maxFragmentWidth = std::max(maxFragmentWidth, fragmentWidth);
    }

    return maxFragmentWidth;
}

void RenderText::computePreferredLogicalWidths(float leadWidth, HashSet<const SimpleFontData*>& fallbackFonts, GlyphOverflow& glyphOverflow)
{
    ASSERT(m_hasTab || preferredLogicalWidthsDirty() || !m_knownToHaveNoOverflowAndNoFallbackFonts);

    m_minWidth = 0;
    m_beginMinWidth = 0;
    m_endMinWidth = 0;
    m_maxWidth = 0;

    float currMinWidth = 0;
    float currMaxWidth = 0;
    m_hasBreakableChar = false;
    m_hasBreak = false;
    m_hasTab = false;
    m_hasBeginWS = false;
    m_hasEndWS = false;

    const RenderStyle& style = this->style();
    const Font& font = style.font(); // FIXME: This ignores first-line.
    float wordSpacing = font.wordSpacing();
    int len = textLength();
    LazyLineBreakIterator breakIterator(m_text, style.locale());
    bool needsWordSpacing = false;
    bool ignoringSpaces = false;
    bool isSpace = false;
    bool firstWord = true;
    bool firstLine = true;
    int nextBreakable = -1;
    int lastWordBoundary = 0;

    // Non-zero only when kerning is enabled, in which case we measure words with their trailing
    // space, then subtract its width.
    float wordTrailingSpaceWidth = font.typesettingFeatures() & Kerning ? font.width(RenderBlock::constructTextRun(this, font, &space, 1, style), &fallbackFonts) + wordSpacing : 0;

    // If automatic hyphenation is allowed, we keep track of the width of the widest word (or word
    // fragment) encountered so far, and only try hyphenating words that are wider.
    float maxWordWidth = std::numeric_limits<float>::max();
    int minimumPrefixLength = 0;
    int minimumSuffixLength = 0;
    if (style.hyphens() == HyphensAuto && canHyphenate(style.locale())) {
        maxWordWidth = 0;

        // Map 'hyphenate-limit-{before,after}: auto;' to 2.
        minimumPrefixLength = style.hyphenationLimitBefore();
        if (minimumPrefixLength < 0)
            minimumPrefixLength = 2;

        minimumSuffixLength = style.hyphenationLimitAfter();
        if (minimumSuffixLength < 0)
            minimumSuffixLength = 2;
    }

    int firstGlyphLeftOverflow = -1;

    bool breakNBSP = style.autoWrap() && style.nbspMode() == SPACE;
    bool breakAll = (style.wordBreak() == BreakAllWordBreak || style.wordBreak() == BreakWordBreak) && style.autoWrap();

    for (int i = 0; i < len; i++) {
        UChar c = uncheckedCharacterAt(i);

        bool previousCharacterIsSpace = isSpace;

        bool isNewline = false;
        if (c == '\n') {
            if (style.preserveNewline()) {
                m_hasBreak = true;
                isNewline = true;
                isSpace = false;
            } else
                isSpace = true;
        } else if (c == '\t') {
            if (!style.collapseWhiteSpace()) {
                m_hasTab = true;
                isSpace = false;
            } else
                isSpace = true;
        } else
            isSpace = c == ' ';

        if ((isSpace || isNewline) && !i)
            m_hasBeginWS = true;
        if ((isSpace || isNewline) && i == len - 1)
            m_hasEndWS = true;

        if (!ignoringSpaces && style.collapseWhiteSpace() && previousCharacterIsSpace && isSpace)
            ignoringSpaces = true;

        if (ignoringSpaces && !isSpace)
            ignoringSpaces = false;

        // Ignore spaces and soft hyphens
        if (ignoringSpaces) {
            ASSERT(lastWordBoundary == i);
            lastWordBoundary++;
            continue;
        } else if (c == softHyphen && style.hyphens() != HyphensNone) {
            currMaxWidth += widthFromCache(font, lastWordBoundary, i - lastWordBoundary, leadWidth + currMaxWidth, &fallbackFonts, &glyphOverflow, style);
            if (firstGlyphLeftOverflow < 0)
                firstGlyphLeftOverflow = glyphOverflow.left;
            lastWordBoundary = i + 1;
            continue;
        }

        bool hasBreak = breakAll || isBreakable(breakIterator, i, nextBreakable, breakNBSP);
        bool betweenWords = true;
        int j = i;
        while (c != '\n' && !isSpaceAccordingToStyle(c, style) && c != '\t' && (c != softHyphen || style.hyphens() == HyphensNone)) {
            j++;
            if (j == len)
                break;
            c = uncheckedCharacterAt(j);
            if (isBreakable(breakIterator, j, nextBreakable, breakNBSP) && characterAt(j - 1) != softHyphen)
                break;
            if (breakAll) {
                betweenWords = false;
                break;
            }
        }

        int wordLen = j - i;
        if (wordLen) {
            bool isSpace = (j < len) && isSpaceAccordingToStyle(c, style);
            float w;
            if (wordTrailingSpaceWidth && isSpace)
                w = widthFromCache(font, i, wordLen + 1, leadWidth + currMaxWidth, &fallbackFonts, &glyphOverflow, style) - wordTrailingSpaceWidth;
            else {
                w = widthFromCache(font, i, wordLen, leadWidth + currMaxWidth, &fallbackFonts, &glyphOverflow, style);
                if (c == softHyphen && style.hyphens() != HyphensNone)
                    currMinWidth += hyphenWidth(this, font);
            }

            if (w > maxWordWidth) {
                int suffixStart;
                float maxFragmentWidth = maxWordFragmentWidth(this, style, font, StringView(m_text).substring(i, wordLen), minimumPrefixLength, minimumSuffixLength, suffixStart, fallbackFonts, glyphOverflow);

                if (suffixStart) {
                    float suffixWidth;
                    if (wordTrailingSpaceWidth && isSpace)
                        suffixWidth = widthFromCache(font, i + suffixStart, wordLen - suffixStart + 1, leadWidth + currMaxWidth, 0, 0, style) - wordTrailingSpaceWidth;
                    else
                        suffixWidth = widthFromCache(font, i + suffixStart, wordLen - suffixStart, leadWidth + currMaxWidth, 0, 0, style);

                    maxFragmentWidth = std::max(maxFragmentWidth, suffixWidth);

                    currMinWidth += maxFragmentWidth - w;
                    maxWordWidth = std::max(maxWordWidth, maxFragmentWidth);
                } else
                    maxWordWidth = w;
            }

            if (firstGlyphLeftOverflow < 0)
                firstGlyphLeftOverflow = glyphOverflow.left;
            currMinWidth += w;
            if (betweenWords) {
                if (lastWordBoundary == i)
                    currMaxWidth += w;
                else
                    currMaxWidth += widthFromCache(font, lastWordBoundary, j - lastWordBoundary, leadWidth + currMaxWidth, &fallbackFonts, &glyphOverflow, style);
                lastWordBoundary = j;
            }

            bool isCollapsibleWhiteSpace = (j < len) && style.isCollapsibleWhiteSpace(c);
            if (j < len && style.autoWrap())
                m_hasBreakableChar = true;

            // Add in wordSpacing to our currMaxWidth, but not if this is the last word on a line or the
            // last word in the run.
            if (wordSpacing && (isSpace || isCollapsibleWhiteSpace) && !containsOnlyWhitespace(j, len-j))
                currMaxWidth += wordSpacing;

            if (firstWord) {
                firstWord = false;
                // If the first character in the run is breakable, then we consider ourselves to have a beginning
                // minimum width of 0, since a break could occur right before our run starts, preventing us from ever
                // being appended to a previous text run when considering the total minimum width of the containing block.
                if (hasBreak)
                    m_hasBreakableChar = true;
                m_beginMinWidth = hasBreak ? 0 : currMinWidth;
            }
            m_endMinWidth = currMinWidth;

            if (currMinWidth > m_minWidth)
                m_minWidth = currMinWidth;
            currMinWidth = 0;

            i += wordLen - 1;
        } else {
            // Nowrap can never be broken, so don't bother setting the
            // breakable character boolean. Pre can only be broken if we encounter a newline.
            if (style.autoWrap() || isNewline)
                m_hasBreakableChar = true;

            if (currMinWidth > m_minWidth)
                m_minWidth = currMinWidth;
            currMinWidth = 0;

            if (isNewline) { // Only set if preserveNewline was true and we saw a newline.
                if (firstLine) {
                    firstLine = false;
                    leadWidth = 0;
                    if (!style.autoWrap())
                        m_beginMinWidth = currMaxWidth;
                }

                if (currMaxWidth > m_maxWidth)
                    m_maxWidth = currMaxWidth;
                currMaxWidth = 0;
            } else {
                TextRun run = RenderBlock::constructTextRun(this, font, this, i, 1, style);
                run.setCharactersLength(len - i);
                ASSERT(run.charactersLength() >= run.length());
                run.setTabSize(!style.collapseWhiteSpace(), style.tabSize());
                run.setXPos(leadWidth + currMaxWidth);

                currMaxWidth += font.width(run, &fallbackFonts);
                glyphOverflow.right = 0;
                needsWordSpacing = isSpace && !previousCharacterIsSpace && i == len - 1;
            }
            ASSERT(lastWordBoundary == i);
            lastWordBoundary++;
        }
    }

    if (firstGlyphLeftOverflow > 0)
        glyphOverflow.left = firstGlyphLeftOverflow;

    if ((needsWordSpacing && len > 1) || (ignoringSpaces && !firstWord))
        currMaxWidth += wordSpacing;

    m_minWidth = std::max(currMinWidth, m_minWidth);
    m_maxWidth = std::max(currMaxWidth, m_maxWidth);

    if (!style.autoWrap())
        m_minWidth = m_maxWidth;

    if (style.whiteSpace() == PRE) {
        if (firstLine)
            m_beginMinWidth = m_maxWidth;
        m_endMinWidth = currMaxWidth;
    }

    setPreferredLogicalWidthsDirty(false);
}

bool RenderText::isAllCollapsibleWhitespace() const
{
    const RenderStyle& style = this->style();
    unsigned length = textLength();
    if (is8Bit()) {
        for (unsigned i = 0; i < length; ++i) {
            if (!style.isCollapsibleWhiteSpace(characters8()[i]))
                return false;
        }
        return true;
    }
    for (unsigned i = 0; i < length; ++i) {
        if (!style.isCollapsibleWhiteSpace(characters16()[i]))
            return false;
    }
    return true;
}
    
bool RenderText::containsOnlyWhitespace(unsigned from, unsigned len) const
{
    ASSERT(m_text);
    StringImpl& text = *m_text.impl();
    unsigned currPos;
    for (currPos = from;
         currPos < from + len && (text[currPos] == '\n' || text[currPos] == ' ' || text[currPos] == '\t');
         currPos++) { }
    return currPos >= (from + len);
}

FloatPoint RenderText::firstRunOrigin() const
{
    return IntPoint(firstRunX(), firstRunY());
}

float RenderText::firstRunX() const
{
    return firstTextBox() ? firstTextBox()->x() : 0;
}

float RenderText::firstRunY() const
{
    return firstTextBox() ? firstTextBox()->y() : 0;
}
    
void RenderText::setSelectionState(SelectionState state)
{
    if (state != SelectionNone)
        ensureLineBoxes();

    RenderObject::setSelectionState(state);

    if (canUpdateSelectionOnRootLineBoxes())
        m_lineBoxes.setSelectionState(*this, state);

    // The containing block can be null in case of an orphaned tree.
    RenderBlock* containingBlock = this->containingBlock();
    if (containingBlock && !containingBlock->isRenderView())
        containingBlock->setSelectionState(state);
}

void RenderText::setTextWithOffset(const String& text, unsigned offset, unsigned len, bool force)
{
    if (!force && m_text == text)
        return;

    int delta = text.length() - textLength();
    unsigned end = len ? offset + len - 1 : offset;

    m_linesDirty = simpleLineLayout() || m_lineBoxes.dirtyRange(*this, offset, end, delta);

    setText(text, force || m_linesDirty);
}

static inline bool isInlineFlowOrEmptyText(const RenderObject* o)
{
    if (o->isRenderInline())
        return true;
    if (!o->isText())
        return false;
    StringImpl* text = toRenderText(o)->text();
    if (!text)
        return true;
    return !text->length();
}

UChar RenderText::previousCharacter() const
{
    // find previous text renderer if one exists
    const RenderObject* previousText = this;
    while ((previousText = previousText->previousInPreOrder()))
        if (!isInlineFlowOrEmptyText(previousText))
            break;
    UChar prev = ' ';
    if (previousText && previousText->isText())
        if (StringImpl* previousString = toRenderText(previousText)->text())
            prev = (*previousString)[previousString->length() - 1];
    return prev;
}

void applyTextTransform(const RenderStyle& style, String& text, UChar previousCharacter)
{
    switch (style.textTransform()) {
    case TTNONE:
        break;
    case CAPITALIZE:
        makeCapitalized(&text, previousCharacter);
        break;
    case UPPERCASE:
        text = text.upper(style.locale());
        break;
    case LOWERCASE:
        text = text.lower(style.locale());
        break;
    }
}

void RenderText::setTextInternal(const String& text)
{
    ASSERT(!text.isNull());

    if (m_originalTextDiffersFromRendered) {
        originalTextMap().remove(this);
        m_originalTextDiffersFromRendered = false;
    }
    String originalText = text;

    m_text = text;

    if (m_useBackslashAsYenSymbol)
        m_text.replace('\\', yenSign);

    ASSERT(m_text);

    applyTextTransform(style(), m_text, previousCharacter());

    // We use the same characters here as for list markers.
    // See the listMarkerText function in RenderListMarker.cpp.
    switch (style().textSecurity()) {
    case TSNONE:
        break;
    case TSCIRCLE:
#if PLATFORM(IOS)
        secureText(blackCircle);
#else
        secureText(whiteBullet);
#endif
        break;
    case TSDISC:
#if PLATFORM(IOS)
        secureText(blackCircle);
#else
        secureText(bullet);
#endif
        break;
    case TSSQUARE:
#if PLATFORM(IOS)
        secureText(blackCircle);
#else
        secureText(blackSquare);
#endif
    }

    ASSERT(!m_text.isNull());

    m_isAllASCII = m_text.containsOnlyASCII();
    m_canUseSimpleFontCodePath = computeCanUseSimpleFontCodePath();

    if (m_text != originalText) {
        originalTextMap().add(this, originalText);
        m_originalTextDiffersFromRendered = true;
    }
}

void RenderText::secureText(UChar mask)
{
    if (!textLength())
        return;

    int lastTypedCharacterOffsetToReveal = -1;
    String revealedText;
    SecureTextTimer* secureTextTimer = gSecureTextTimers ? gSecureTextTimers->get(this) : 0;
    if (secureTextTimer && secureTextTimer->isActive()) {
        lastTypedCharacterOffsetToReveal = secureTextTimer->lastTypedCharacterOffset();
        if (lastTypedCharacterOffsetToReveal >= 0)
            revealedText.append(m_text[lastTypedCharacterOffsetToReveal]);
    }

    m_text.fill(mask);
    if (lastTypedCharacterOffsetToReveal >= 0) {
        m_text.replace(lastTypedCharacterOffsetToReveal, 1, revealedText);
        // m_text may be updated later before timer fires. We invalidate the lastTypedCharacterOffset to avoid inconsistency.
        secureTextTimer->invalidate();
    }
}

void RenderText::setText(const String& text, bool force)
{
    ASSERT(!text.isNull());

    if (!force && text == originalText())
        return;

    setTextInternal(text);
    setNeedsLayoutAndPrefWidthsRecalc();
    m_knownToHaveNoOverflowAndNoFallbackFonts = false;

    if (parent()->isRenderBlockFlow())
        toRenderBlockFlow(parent())->invalidateLineLayoutPath();
    
    if (AXObjectCache* cache = document().existingAXObjectCache())
        cache->textChanged(this);
}

String RenderText::textWithoutConvertingBackslashToYenSymbol() const
{
    if (!m_useBackslashAsYenSymbol || style().textSecurity() != TSNONE)
        return text();

    String text = originalText();
    applyTextTransform(style(), text, previousCharacter());
    return text;
}

void RenderText::dirtyLineBoxes(bool fullLayout)
{
    if (fullLayout)
        m_lineBoxes.deleteAll();
    else if (!m_linesDirty)
        m_lineBoxes.dirtyAll();
    m_linesDirty = false;
}

std::unique_ptr<InlineTextBox> RenderText::createTextBox()
{
    return std::make_unique<InlineTextBox>(*this);
}

void RenderText::positionLineBox(InlineTextBox& textBox)
{
    // FIXME: should not be needed!!!
    if (!textBox.len()) {
        // We want the box to be destroyed.
        textBox.removeFromParent();
        m_lineBoxes.remove(textBox);
        delete &textBox;
        return;
    }

    m_containsReversedText |= !textBox.isLeftToRightDirection();
}

void RenderText::ensureLineBoxes()
{
    if (!parent()->isRenderBlockFlow())
        return;
    toRenderBlockFlow(parent())->ensureLineBoxes();
}

const SimpleLineLayout::Layout* RenderText::simpleLineLayout() const
{
    if (!parent()->isRenderBlockFlow())
        return nullptr;
    return toRenderBlockFlow(parent())->simpleLineLayout();
}

float RenderText::width(unsigned from, unsigned len, float xPos, bool firstLine, HashSet<const SimpleFontData*>* fallbackFonts, GlyphOverflow* glyphOverflow) const
{
    if (from >= textLength())
        return 0;

    if (from + len > textLength())
        len = textLength() - from;

    const RenderStyle& lineStyle = firstLine ? firstLineStyle() : style();
    return width(from, len, lineStyle.font(), xPos, fallbackFonts, glyphOverflow);
}

float RenderText::width(unsigned from, unsigned len, const Font& f, float xPos, HashSet<const SimpleFontData*>* fallbackFonts, GlyphOverflow* glyphOverflow) const
{
    ASSERT(from + len <= textLength());
    if (!textLength())
        return 0;

    const RenderStyle& style = this->style();
    float w;
    if (&f == &style.font()) {
        if (!style.preserveNewline() && !from && len == textLength() && (!glyphOverflow || !glyphOverflow->computeBounds)) {
            if (fallbackFonts) {
                ASSERT(glyphOverflow);
                if (preferredLogicalWidthsDirty() || !m_knownToHaveNoOverflowAndNoFallbackFonts) {
                    const_cast<RenderText*>(this)->computePreferredLogicalWidths(0, *fallbackFonts, *glyphOverflow);
                    if (fallbackFonts->isEmpty() && !glyphOverflow->left && !glyphOverflow->right && !glyphOverflow->top && !glyphOverflow->bottom)
                        m_knownToHaveNoOverflowAndNoFallbackFonts = true;
                }
                w = m_maxWidth;
            } else
                w = maxLogicalWidth();
        } else
            w = widthFromCache(f, from, len, xPos, fallbackFonts, glyphOverflow, style);
    } else {
        TextRun run = RenderBlock::constructTextRun(const_cast<RenderText*>(this), f, this, from, len, style);
        run.setCharactersLength(textLength() - from);
        ASSERT(run.charactersLength() >= run.length());

        run.setCharacterScanForCodePath(!canUseSimpleFontCodePath());
        run.setTabSize(!style.collapseWhiteSpace(), style.tabSize());
        run.setXPos(xPos);
        w = f.width(run, fallbackFonts, glyphOverflow);
    }

    return w;
}

IntRect RenderText::linesBoundingBox() const
{
    if (auto layout = simpleLineLayout())
        return SimpleLineLayout::computeTextBoundingBox(*this, *layout);

    return m_lineBoxes.boundingBox(*this);
}

LayoutRect RenderText::linesVisualOverflowBoundingBox() const
{
    ASSERT(!simpleLineLayout());
    return m_lineBoxes.visualOverflowBoundingBox(*this);
}

LayoutRect RenderText::clippedOverflowRectForRepaint(const RenderLayerModelObject* repaintContainer) const
{
    RenderObject* rendererToRepaint = containingBlock();

    // Do not cross self-painting layer boundaries.
    RenderObject& enclosingLayerRenderer = enclosingLayer()->renderer();
    if (&enclosingLayerRenderer != rendererToRepaint && !rendererToRepaint->isDescendantOf(&enclosingLayerRenderer))
        rendererToRepaint = &enclosingLayerRenderer;

    // The renderer we chose to repaint may be an ancestor of repaintContainer, but we need to do a repaintContainer-relative repaint.
    if (repaintContainer && repaintContainer != rendererToRepaint && !rendererToRepaint->isDescendantOf(repaintContainer))
        return repaintContainer->clippedOverflowRectForRepaint(repaintContainer);

    return rendererToRepaint->clippedOverflowRectForRepaint(repaintContainer);
}

LayoutRect RenderText::selectionRectForRepaint(const RenderLayerModelObject* repaintContainer, bool clipToVisibleContent)
{
    ASSERT(!needsLayout());
    ASSERT(!simpleLineLayout());

    if (selectionState() == SelectionNone)
        return LayoutRect();
    RenderBlock* cb = containingBlock();
    if (!cb)
        return LayoutRect();

    // Now calculate startPos and endPos for painting selection.
    // We include a selection while endPos > 0
    int startPos, endPos;
    if (selectionState() == SelectionInside) {
        // We are fully selected.
        startPos = 0;
        endPos = textLength();
    } else {
        selectionStartEnd(startPos, endPos);
        if (selectionState() == SelectionStart)
            endPos = textLength();
        else if (selectionState() == SelectionEnd)
            startPos = 0;
    }

    if (startPos == endPos)
        return IntRect();

    LayoutRect rect = m_lineBoxes.selectionRectForRange(startPos, endPos);

    if (clipToVisibleContent)
        computeRectForRepaint(repaintContainer, rect);
    else {
        if (cb->hasColumns())
            cb->adjustRectForColumns(rect);

        rect = localToContainerQuad(FloatRect(rect), repaintContainer).enclosingBoundingBox();
    }

    return rect;
}

int RenderText::caretMinOffset() const
{
    if (auto layout = simpleLineLayout())
        return SimpleLineLayout::findTextCaretMinimumOffset(*this, *layout);
    return m_lineBoxes.caretMinOffset();
}

int RenderText::caretMaxOffset() const
{
    if (auto layout = simpleLineLayout())
        return SimpleLineLayout::findTextCaretMaximumOffset(*this, *layout);
    return m_lineBoxes.caretMaxOffset(*this);
}

unsigned RenderText::countRenderedCharacterOffsetsUntil(unsigned offset) const
{
    ASSERT(!simpleLineLayout());
    return m_lineBoxes.countCharacterOffsetsUntil(offset);
}

bool RenderText::containsRenderedCharacterOffset(unsigned offset) const
{
    ASSERT(!simpleLineLayout());
    return m_lineBoxes.containsOffset(*this, offset, RenderTextLineBoxes::CharacterOffset);
}

bool RenderText::containsCaretOffset(unsigned offset) const
{
    if (auto layout = simpleLineLayout())
        return SimpleLineLayout::containsTextCaretOffset(*this, *layout, offset);
    return m_lineBoxes.containsOffset(*this, offset, RenderTextLineBoxes::CaretOffset);
}

bool RenderText::hasRenderedText() const
{
    if (auto layout = simpleLineLayout())
        return SimpleLineLayout::isTextRendered(*this, *layout);
    return m_lineBoxes.hasRenderedText();
}

int RenderText::previousOffset(int current) const
{
    if (isAllASCII() || m_text.is8Bit())
        return current - 1;

    StringImpl* textImpl = m_text.impl();
    TextBreakIterator* iterator = cursorMovementIterator(StringView(textImpl->characters16(), textImpl->length()));
    if (!iterator)
        return current - 1;

    long result = textBreakPreceding(iterator, current);
    if (result == TextBreakDone)
        result = current - 1;


    return result;
}

#if PLATFORM(COCOA) || PLATFORM(EFL)

#define HANGUL_CHOSEONG_START (0x1100)
#define HANGUL_CHOSEONG_END (0x115F)
#define HANGUL_JUNGSEONG_START (0x1160)
#define HANGUL_JUNGSEONG_END (0x11A2)
#define HANGUL_JONGSEONG_START (0x11A8)
#define HANGUL_JONGSEONG_END (0x11F9)
#define HANGUL_SYLLABLE_START (0xAC00)
#define HANGUL_SYLLABLE_END (0xD7AF)
#define HANGUL_JONGSEONG_COUNT (28)

enum HangulState {
    HangulStateL,
    HangulStateV,
    HangulStateT,
    HangulStateLV,
    HangulStateLVT,
    HangulStateBreak
};

inline bool isHangulLVT(UChar32 character)
{
    return (character - HANGUL_SYLLABLE_START) % HANGUL_JONGSEONG_COUNT;
}

inline bool isMark(UChar32 c)
{
    int8_t charType = u_charType(c);
    return charType == U_NON_SPACING_MARK || charType == U_ENCLOSING_MARK || charType == U_COMBINING_SPACING_MARK;
}

inline bool isRegionalIndicator(UChar32 c)
{
    // National flag emoji each consists of a pair of regional indicator symbols.
    return 0x1F1E6 <= c && c <= 0x1F1FF;
}

#endif

int RenderText::previousOffsetForBackwardDeletion(int current) const
{
#if PLATFORM(COCOA) || PLATFORM(EFL)
    ASSERT(m_text);
    StringImpl& text = *m_text.impl();
    UChar32 character;
    bool sawRegionalIndicator = false;
    while (current > 0) {
        if (U16_IS_TRAIL(text[--current]))
            --current;
        if (current < 0)
            break;

        UChar32 character = text.characterStartingAt(current);

        if (sawRegionalIndicator) {
            // We don't check if the pair of regional indicator symbols before current position can actually be combined
            // into a flag, and just delete it. This may not agree with how the pair is rendered in edge cases,
            // but is good enough in practice.
            if (isRegionalIndicator(character))
                break;
            // Don't delete a preceding character that isn't a regional indicator symbol.
            U16_FWD_1_UNSAFE(text, current);
        }

        // We don't combine characters in Armenian ... Limbu range for backward deletion.
        if ((character >= 0x0530) && (character < 0x1950))
            break;

        if (isRegionalIndicator(character)) {
            sawRegionalIndicator = true;
            continue;
        }

        if (!isMark(character) && (character != 0xFF9E) && (character != 0xFF9F))
            break;
    }

    if (current <= 0)
        return current;

    // Hangul
    character = text.characterStartingAt(current);
    if (((character >= HANGUL_CHOSEONG_START) && (character <= HANGUL_JONGSEONG_END)) || ((character >= HANGUL_SYLLABLE_START) && (character <= HANGUL_SYLLABLE_END))) {
        HangulState state;

        if (character < HANGUL_JUNGSEONG_START)
            state = HangulStateL;
        else if (character < HANGUL_JONGSEONG_START)
            state = HangulStateV;
        else if (character < HANGUL_SYLLABLE_START)
            state = HangulStateT;
        else
            state = isHangulLVT(character) ? HangulStateLVT : HangulStateLV;

        while (current > 0 && ((character = text.characterStartingAt(current - 1)) >= HANGUL_CHOSEONG_START) && (character <= HANGUL_SYLLABLE_END) && ((character <= HANGUL_JONGSEONG_END) || (character >= HANGUL_SYLLABLE_START))) {
            switch (state) {
            case HangulStateV:
                if (character <= HANGUL_CHOSEONG_END)
                    state = HangulStateL;
                else if ((character >= HANGUL_SYLLABLE_START) && (character <= HANGUL_SYLLABLE_END) && !isHangulLVT(character))
                    state = HangulStateLV;
                else if (character > HANGUL_JUNGSEONG_END)
                    state = HangulStateBreak;
                break;
            case HangulStateT:
                if ((character >= HANGUL_JUNGSEONG_START) && (character <= HANGUL_JUNGSEONG_END))
                    state = HangulStateV;
                else if ((character >= HANGUL_SYLLABLE_START) && (character <= HANGUL_SYLLABLE_END))
                    state = (isHangulLVT(character) ? HangulStateLVT : HangulStateLV);
                else if (character < HANGUL_JUNGSEONG_START)
                    state = HangulStateBreak;
                break;
            default:
                state = (character < HANGUL_JUNGSEONG_START) ? HangulStateL : HangulStateBreak;
                break;
            }
            if (state == HangulStateBreak)
                break;

            --current;
        }
    }

    return current;
#else
    // Platforms other than Mac delete by one code point.
    if (U16_IS_TRAIL(m_text[--current]))
        --current;
    if (current < 0)
        current = 0;
    return current;
#endif
}

int RenderText::nextOffset(int current) const
{
    if (isAllASCII() || m_text.is8Bit())
        return current + 1;

    StringImpl* textImpl = m_text.impl();
    TextBreakIterator* iterator = cursorMovementIterator(StringView(textImpl->characters16(), textImpl->length()));
    if (!iterator)
        return current + 1;

    long result = textBreakFollowing(iterator, current);
    if (result == TextBreakDone)
        result = current + 1;

    return result;
}

bool RenderText::computeCanUseSimpleFontCodePath() const
{
    if (isAllASCII() || m_text.is8Bit())
        return true;
    return Font::characterRangeCodePath(characters16(), length()) == Font::Simple;
}

void RenderText::momentarilyRevealLastTypedCharacter(unsigned lastTypedCharacterOffset)
{
    if (!gSecureTextTimers)
        gSecureTextTimers = new SecureTextTimerMap;

    SecureTextTimer* secureTextTimer = gSecureTextTimers->get(this);
    if (!secureTextTimer) {
        secureTextTimer = new SecureTextTimer(this);
        gSecureTextTimers->add(this, secureTextTimer);
    }
    secureTextTimer->restartWithNewText(lastTypedCharacterOffset);
}

} // namespace WebCore
