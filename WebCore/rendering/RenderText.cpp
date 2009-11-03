/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
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

#include "CharacterNames.h"
#include "FloatQuad.h"
#include "FrameView.h"
#include "InlineTextBox.h"
#include "Range.h"
#include "RenderArena.h"
#include "RenderBlock.h"
#include "RenderLayer.h"
#include "RenderView.h"
#include "Text.h"
#include "TextBreakIterator.h"
#include "VisiblePosition.h"
#include "break_lines.h"
#include <wtf/AlwaysInline.h>

using namespace std;
using namespace WTF;
using namespace Unicode;

namespace WebCore {

// FIXME: Move to StringImpl.h eventually.
static inline bool charactersAreAllASCII(StringImpl* text)
{
    return charactersAreAllASCII(text->characters(), text->length());
}

RenderText::RenderText(Node* node, PassRefPtr<StringImpl> str)
     : RenderObject(node)
     , m_minWidth(-1)
     , m_text(document()->displayStringModifiedByEncoding(str))
     , m_firstTextBox(0)
     , m_lastTextBox(0)
     , m_maxWidth(-1)
     , m_beginMinWidth(0)
     , m_endMinWidth(0)
     , m_hasTab(false)
     , m_linesDirty(false)
     , m_containsReversedText(false)
     , m_isAllASCII(charactersAreAllASCII(m_text.get()))
     , m_knownNotToUseFallbackFonts(false)
{
    ASSERT(m_text);

    setIsText();

    // FIXME: It would be better to call this only if !m_text->containsOnlyWhitespace().
    // But that might slow things down, and maybe should only be done if visuallyNonEmpty
    // is still false. Not making any change for now, but should consider in the future.
    view()->frameView()->setIsVisuallyNonEmpty();
}

#ifndef NDEBUG

RenderText::~RenderText()
{
    ASSERT(!m_firstTextBox);
    ASSERT(!m_lastTextBox);
}

#endif

const char* RenderText::renderName() const
{
    return "RenderText";
}

bool RenderText::isTextFragment() const
{
    return false;
}

bool RenderText::isWordBreak() const
{
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
        m_knownNotToUseFallbackFonts = false;
    }

    ETextTransform oldTransform = oldStyle ? oldStyle->textTransform() : TTNONE;
    ETextSecurity oldSecurity = oldStyle ? oldStyle->textSecurity() : TSNONE;

    if (oldTransform != style()->textTransform() || oldSecurity != style()->textSecurity()) {
        if (RefPtr<StringImpl> textToTransform = originalText())
            setText(textToTransform.release(), true);
    }
}

void RenderText::destroy()
{
    if (!documentBeingDestroyed()) {
        if (firstTextBox()) {
            if (isBR()) {
                RootInlineBox* next = firstTextBox()->root()->nextRootBox();
                if (next)
                    next->markDirty();
            }
            for (InlineTextBox* box = firstTextBox(); box; box = box->nextTextBox())
                box->remove();
        } else if (parent())
            parent()->dirtyLinesFromChangedChild(this);
    }
    deleteTextBoxes();
    RenderObject::destroy();
}

void RenderText::extractTextBox(InlineTextBox* box)
{
    checkConsistency();

    m_lastTextBox = box->prevTextBox();
    if (box == m_firstTextBox)
        m_firstTextBox = 0;
    if (box->prevTextBox())
        box->prevTextBox()->setNextLineBox(0);
    box->setPreviousLineBox(0);
    for (InlineRunBox* curr = box; curr; curr = curr->nextLineBox())
        curr->setExtracted();

    checkConsistency();
}

void RenderText::attachTextBox(InlineTextBox* box)
{
    checkConsistency();

    if (m_lastTextBox) {
        m_lastTextBox->setNextLineBox(box);
        box->setPreviousLineBox(m_lastTextBox);
    } else
        m_firstTextBox = box;
    InlineTextBox* last = box;
    for (InlineTextBox* curr = box; curr; curr = curr->nextTextBox()) {
        curr->setExtracted(false);
        last = curr;
    }
    m_lastTextBox = last;

    checkConsistency();
}

void RenderText::removeTextBox(InlineTextBox* box)
{
    checkConsistency();

    if (box == m_firstTextBox)
        m_firstTextBox = box->nextTextBox();
    if (box == m_lastTextBox)
        m_lastTextBox = box->prevTextBox();
    if (box->nextTextBox())
        box->nextTextBox()->setPreviousLineBox(box->prevTextBox());
    if (box->prevTextBox())
        box->prevTextBox()->setNextLineBox(box->nextTextBox());

    checkConsistency();
}

void RenderText::deleteTextBoxes()
{
    if (firstTextBox()) {
        RenderArena* arena = renderArena();
        InlineTextBox* next;
        for (InlineTextBox* curr = firstTextBox(); curr; curr = next) {
            next = curr->nextTextBox();
            curr->destroy(arena);
        }
        m_firstTextBox = m_lastTextBox = 0;
    }
}

PassRefPtr<StringImpl> RenderText::originalText() const
{
    Node* e = node();
    return e ? static_cast<Text*>(e)->dataImpl() : 0;
}

void RenderText::absoluteRects(Vector<IntRect>& rects, int tx, int ty)
{
    for (InlineTextBox* box = firstTextBox(); box; box = box->nextTextBox())
        rects.append(IntRect(tx + box->x(), ty + box->y(), box->width(), box->height()));
}

void RenderText::absoluteRectsForRange(Vector<IntRect>& rects, unsigned start, unsigned end, bool useSelectionHeight)
{
    // Work around signed/unsigned issues. This function takes unsigneds, and is often passed UINT_MAX
    // to mean "all the way to the end". InlineTextBox coordinates are unsigneds, so changing this 
    // function to take ints causes various internal mismatches. But selectionRect takes ints, and 
    // passing UINT_MAX to it causes trouble. Ideally we'd change selectionRect to take unsigneds, but 
    // that would cause many ripple effects, so for now we'll just clamp our unsigned parameters to INT_MAX.
    ASSERT(end == UINT_MAX || end <= INT_MAX);
    ASSERT(start <= INT_MAX);
    start = min(start, static_cast<unsigned>(INT_MAX));
    end = min(end, static_cast<unsigned>(INT_MAX));
    
    FloatPoint absPos = localToAbsolute(FloatPoint());

    for (InlineTextBox* box = firstTextBox(); box; box = box->nextTextBox()) {
        // Note: box->end() returns the index of the last character, not the index past it
        if (start <= box->start() && box->end() < end) {
            IntRect r = IntRect(absPos.x() + box->x(), absPos.y() + box->y(), box->width(), box->height());
            if (useSelectionHeight) {
                IntRect selectionRect = box->selectionRect(absPos.x(), absPos.y(), start, end);
                r.setHeight(selectionRect.height());
                r.setY(selectionRect.y());
            }
            rects.append(r);
        } else {
            unsigned realEnd = min(box->end() + 1, end);
            IntRect r = box->selectionRect(absPos.x(), absPos.y(), start, realEnd);
            if (!r.isEmpty()) {
                if (!useSelectionHeight) {
                    // change the height and y position because selectionRect uses selection-specific values
                    r.setHeight(box->height());
                    r.setY(absPos.y() + box->y());
                }
                rects.append(r);
            }
        }
    }
}

void RenderText::absoluteQuads(Vector<FloatQuad>& quads)
{
    for (InlineTextBox* box = firstTextBox(); box; box = box->nextTextBox())
        quads.append(localToAbsoluteQuad(FloatRect(box->x(), box->y(), box->width(), box->height())));
}

void RenderText::absoluteQuadsForRange(Vector<FloatQuad>& quads, unsigned start, unsigned end, bool useSelectionHeight)
{
    // Work around signed/unsigned issues. This function takes unsigneds, and is often passed UINT_MAX
    // to mean "all the way to the end". InlineTextBox coordinates are unsigneds, so changing this 
    // function to take ints causes various internal mismatches. But selectionRect takes ints, and 
    // passing UINT_MAX to it causes trouble. Ideally we'd change selectionRect to take unsigneds, but 
    // that would cause many ripple effects, so for now we'll just clamp our unsigned parameters to INT_MAX.
    ASSERT(end == UINT_MAX || end <= INT_MAX);
    ASSERT(start <= INT_MAX);
    start = min(start, static_cast<unsigned>(INT_MAX));
    end = min(end, static_cast<unsigned>(INT_MAX));
    
    for (InlineTextBox* box = firstTextBox(); box; box = box->nextTextBox()) {
        // Note: box->end() returns the index of the last character, not the index past it
        if (start <= box->start() && box->end() < end) {
            IntRect r = IntRect(box->x(), box->y(), box->width(), box->height());
            if (useSelectionHeight) {
                IntRect selectionRect = box->selectionRect(0, 0, start, end);
                r.setHeight(selectionRect.height());
                r.setY(selectionRect.y());
            }
            quads.append(localToAbsoluteQuad(FloatRect(r)));
        } else {
            unsigned realEnd = min(box->end() + 1, end);
            IntRect r = box->selectionRect(0, 0, start, realEnd);
            if (!r.isEmpty()) {
                if (!useSelectionHeight) {
                    // change the height and y position because selectionRect uses selection-specific values
                    r.setHeight(box->height());
                    r.setY(box->y());
                }
                quads.append(localToAbsoluteQuad(FloatRect(r)));
            }
        }
    }
}

InlineTextBox* RenderText::findNextInlineTextBox(int offset, int& pos) const
{
    // The text runs point to parts of the RenderText's m_text
    // (they don't include '\n')
    // Find the text run that includes the character at offset
    // and return pos, which is the position of the char in the run.

    if (!m_firstTextBox)
        return 0;

    InlineTextBox* s = m_firstTextBox;
    int off = s->len();
    while (offset > off && s->nextTextBox()) {
        s = s->nextTextBox();
        off = s->start() + s->len();
    }
    // we are now in the correct text run
    pos = (offset > off ? s->len() : s->len() - (off - offset) );
    return s;
}

VisiblePosition RenderText::positionForPoint(const IntPoint& point)
{
    if (!firstTextBox() || textLength() == 0)
        return createVisiblePosition(0, DOWNSTREAM);

    // Get the offset for the position, since this will take rtl text into account.
    int offset;

    // FIXME: We should be able to roll these special cases into the general cases in the loop below.
    if (firstTextBox() && point.y() <  firstTextBox()->root()->lineBottom() && point.x() < firstTextBox()->m_x) {
        // at the y coordinate of the first line or above
        // and the x coordinate is to the left of the first text box left edge
        offset = firstTextBox()->offsetForPosition(point.x());
        return createVisiblePosition(offset + firstTextBox()->start(), DOWNSTREAM);
    }
    if (lastTextBox() && point.y() >= lastTextBox()->root()->lineTop() && point.x() >= lastTextBox()->m_x + lastTextBox()->m_width) {
        // at the y coordinate of the last line or below
        // and the x coordinate is to the right of the last text box right edge
        offset = lastTextBox()->offsetForPosition(point.x());
        return createVisiblePosition(offset + lastTextBox()->start(), DOWNSTREAM);
    }

    InlineTextBox* lastBoxAbove = 0;
    for (InlineTextBox* box = firstTextBox(); box; box = box->nextTextBox()) {
        if (point.y() >= box->root()->lineTop()) {
            int bottom = box->root()->nextRootBox() ? box->root()->nextRootBox()->lineTop() : box->root()->lineBottom();
            if (point.y() < bottom) {
                offset = box->offsetForPosition(point.x());

                if (point.x() == box->m_x)
                    // the x coordinate is equal to the left edge of this box
                    // the affinity must be downstream so the position doesn't jump back to the previous line
                    return createVisiblePosition(offset + box->start(), DOWNSTREAM);

                if (point.x() < box->m_x + box->m_width)
                    // and the x coordinate is to the left of the right edge of this box
                    // check to see if position goes in this box
                    return createVisiblePosition(offset + box->start(), offset > 0 ? VP_UPSTREAM_IF_POSSIBLE : DOWNSTREAM);

                if (!box->prevOnLine() && point.x() < box->m_x)
                    // box is first on line
                    // and the x coordinate is to the left of the first text box left edge
                    return createVisiblePosition(offset + box->start(), DOWNSTREAM);

                if (!box->nextOnLine())
                    // box is last on line
                    // and the x coordinate is to the right of the last text box right edge
                    // generate VisiblePosition, use UPSTREAM affinity if possible
                    return createVisiblePosition(offset + box->start(), offset > 0 ? VP_UPSTREAM_IF_POSSIBLE : DOWNSTREAM);
            }
            lastBoxAbove = box;
        }
    }

    return createVisiblePosition(lastBoxAbove ? lastBoxAbove->start() + lastBoxAbove->len() : 0, DOWNSTREAM);
}

IntRect RenderText::localCaretRect(InlineBox* inlineBox, int caretOffset, int* extraWidthToEndOfLine)
{
    if (!inlineBox)
        return IntRect();

    ASSERT(inlineBox->isInlineTextBox());
    if (!inlineBox->isInlineTextBox())
        return IntRect();

    InlineTextBox* box = static_cast<InlineTextBox*>(inlineBox);

    int height = box->root()->lineBottom() - box->root()->lineTop();
    int top = box->root()->lineTop();

    int left = box->positionForOffset(caretOffset);

    // Distribute the caret's width to either side of the offset.
    int caretWidthLeftOfOffset = caretWidth / 2;
    left -= caretWidthLeftOfOffset;
    int caretWidthRightOfOffset = caretWidth - caretWidthLeftOfOffset;

    int rootLeft = box->root()->x();
    int rootRight = rootLeft + box->root()->width();
    // FIXME: should we use the width of the root inline box or the
    // width of the containing block for this?
    if (extraWidthToEndOfLine)
        *extraWidthToEndOfLine = (box->root()->width() + rootLeft) - (left + 1);

    RenderBlock* cb = containingBlock();
    if (style()->autoWrap()) {
        int availableWidth = cb->lineWidth(top, false);
        if (box->direction() == LTR)
            left = min(left, rootLeft + availableWidth - caretWidthRightOfOffset);
        else
            left = max(left, cb->x());
    } else {
        // If there is no wrapping, the caret can leave its containing block, but not its root line box.
        if (cb->style()->direction() == LTR) {
            int rightEdge = max(cb->width(), rootRight);
            left = min(left, rightEdge - caretWidthRightOfOffset);
            left = max(left, rootLeft);
        } else {
            int leftEdge = min(cb->x(), rootLeft);
            left = max(left, leftEdge);
            left = min(left, rootRight - caretWidth);
        }
    }

    return IntRect(left, top, caretWidth, height);
}

ALWAYS_INLINE int RenderText::widthFromCache(const Font& f, int start, int len, int xPos, HashSet<const SimpleFontData*>* fallbackFonts) const
{
    if (f.isFixedPitch() && !f.isSmallCaps() && m_isAllASCII) {
        int monospaceCharacterWidth = f.spaceWidth();
        int tabWidth = allowTabs() ? monospaceCharacterWidth * 8 : 0;
        int w = 0;
        bool isSpace;
        bool previousCharWasSpace = true; // FIXME: Preserves historical behavior, but seems wrong for start > 0.
        for (int i = start; i < start + len; i++) {
            char c = (*m_text)[i];
            if (c <= ' ') {
                if (c == ' ' || c == '\n') {
                    w += monospaceCharacterWidth;
                    isSpace = true;
                } else if (c == '\t') {
                    w += tabWidth ? tabWidth - ((xPos + w) % tabWidth) : monospaceCharacterWidth;
                    isSpace = true;
                } else
                    isSpace = false;
            } else {
                w += monospaceCharacterWidth;
                isSpace = false;
            }
            if (isSpace && !previousCharWasSpace)
                w += f.wordSpacing();
            previousCharWasSpace = isSpace;
        }
        return w;
    }

    return f.width(TextRun(text()->characters() + start, len, allowTabs(), xPos), fallbackFonts);
}

void RenderText::trimmedPrefWidths(int leadWidth,
                                   int& beginMinW, bool& beginWS,
                                   int& endMinW, bool& endWS,
                                   bool& hasBreakableChar, bool& hasBreak,
                                   int& beginMaxW, int& endMaxW,
                                   int& minW, int& maxW, bool& stripFrontSpaces)
{
    bool collapseWhiteSpace = style()->collapseWhiteSpace();
    if (!collapseWhiteSpace)
        stripFrontSpaces = false;

    if (m_hasTab || prefWidthsDirty())
        calcPrefWidths(leadWidth);

    beginWS = !stripFrontSpaces && m_hasBeginWS;
    endWS = m_hasEndWS;

    int len = textLength();

    if (!len || (stripFrontSpaces && m_text->containsOnlyWhitespace())) {
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

    if ((*m_text)[0] == ' ' || ((*m_text)[0] == '\n' && !style()->preserveNewline()) || (*m_text)[0] == '\t') {
        const Font& f = style()->font(); // FIXME: This ignores first-line.
        if (stripFrontSpaces) {
            const UChar space = ' ';
            int spaceWidth = f.width(TextRun(&space, 1));
            maxW -= spaceWidth;
        } else
            maxW += f.wordSpacing();
    }

    stripFrontSpaces = collapseWhiteSpace && m_hasEndWS;

    if (!style()->autoWrap() || minW > maxW)
        minW = maxW;

    // Compute our max widths by scanning the string for newlines.
    if (hasBreak) {
        const Font& f = style()->font(); // FIXME: This ignores first-line.
        bool firstLine = true;
        beginMaxW = maxW;
        endMaxW = maxW;
        for (int i = 0; i < len; i++) {
            int linelen = 0;
            while (i + linelen < len && (*m_text)[i + linelen] != '\n')
                linelen++;

            if (linelen) {
                endMaxW = widthFromCache(f, i, linelen, leadWidth + endMaxW, 0);
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

static inline bool isSpaceAccordingToStyle(UChar c, RenderStyle* style)
{
    return c == ' ' || (c == noBreakSpace && style->nbspMode() == SPACE);
}

int RenderText::minPrefWidth() const
{
    if (prefWidthsDirty())
        const_cast<RenderText*>(this)->calcPrefWidths(0);
        
    return m_minWidth;
}

int RenderText::maxPrefWidth() const
{
    if (prefWidthsDirty())
        const_cast<RenderText*>(this)->calcPrefWidths(0);
        
    return m_maxWidth;
}

void RenderText::calcPrefWidths(int leadWidth)
{
    HashSet<const SimpleFontData*> fallbackFonts;
    calcPrefWidths(leadWidth, fallbackFonts);
    if (fallbackFonts.isEmpty())
        m_knownNotToUseFallbackFonts = true;
}

void RenderText::calcPrefWidths(int leadWidth, HashSet<const SimpleFontData*>& fallbackFonts)
{
    ASSERT(m_hasTab || prefWidthsDirty() || !m_knownNotToUseFallbackFonts);

    m_minWidth = 0;
    m_beginMinWidth = 0;
    m_endMinWidth = 0;
    m_maxWidth = 0;

    if (isBR())
        return;

    int currMinWidth = 0;
    int currMaxWidth = 0;
    m_hasBreakableChar = false;
    m_hasBreak = false;
    m_hasTab = false;
    m_hasBeginWS = false;
    m_hasEndWS = false;

    const Font& f = style()->font(); // FIXME: This ignores first-line.
    int wordSpacing = style()->wordSpacing();
    int len = textLength();
    const UChar* txt = characters();
    bool needsWordSpacing = false;
    bool ignoringSpaces = false;
    bool isSpace = false;
    bool firstWord = true;
    bool firstLine = true;
    int nextBreakable = -1;
    int lastWordBoundary = 0;

    bool breakNBSP = style()->autoWrap() && style()->nbspMode() == SPACE;
    bool breakAll = (style()->wordBreak() == BreakAllWordBreak || style()->wordBreak() == BreakWordBreak) && style()->autoWrap();

    for (int i = 0; i < len; i++) {
        UChar c = txt[i];

        bool previousCharacterIsSpace = isSpace;

        bool isNewline = false;
        if (c == '\n') {
            if (style()->preserveNewline()) {
                m_hasBreak = true;
                isNewline = true;
                isSpace = false;
            } else
                isSpace = true;
        } else if (c == '\t') {
            if (!style()->collapseWhiteSpace()) {
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

        if (!ignoringSpaces && style()->collapseWhiteSpace() && previousCharacterIsSpace && isSpace)
            ignoringSpaces = true;

        if (ignoringSpaces && !isSpace)
            ignoringSpaces = false;

        // Ignore spaces and soft hyphens
        if (ignoringSpaces) {
            ASSERT(lastWordBoundary == i);
            lastWordBoundary++;
            continue;
        } else if (c == softHyphen) {
            currMaxWidth += widthFromCache(f, lastWordBoundary, i - lastWordBoundary, leadWidth + currMaxWidth, &fallbackFonts);
            lastWordBoundary = i + 1;
            continue;
        }

        bool hasBreak = breakAll || isBreakable(txt, i, len, nextBreakable, breakNBSP);
        bool betweenWords = true;
        int j = i;
        while (c != '\n' && !isSpaceAccordingToStyle(c, style()) && c != '\t' && c != softHyphen) {
            j++;
            if (j == len)
                break;
            c = txt[j];
            if (isBreakable(txt, j, len, nextBreakable, breakNBSP))
                break;
            if (breakAll) {
                betweenWords = false;
                break;
            }
        }

        int wordLen = j - i;
        if (wordLen) {
            int w = widthFromCache(f, i, wordLen, leadWidth + currMaxWidth, &fallbackFonts);
            currMinWidth += w;
            if (betweenWords) {
                if (lastWordBoundary == i)
                    currMaxWidth += w;
                else
                    currMaxWidth += widthFromCache(f, lastWordBoundary, j - lastWordBoundary, leadWidth + currMaxWidth, &fallbackFonts);
                lastWordBoundary = j;
            }

            bool isSpace = (j < len) && isSpaceAccordingToStyle(c, style());
            bool isCollapsibleWhiteSpace = (j < len) && style()->isCollapsibleWhiteSpace(c);
            if (j < len && style()->autoWrap())
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
                m_beginMinWidth = hasBreak ? 0 : w;
            }
            m_endMinWidth = w;

            if (currMinWidth > m_minWidth)
                m_minWidth = currMinWidth;
            currMinWidth = 0;

            i += wordLen - 1;
        } else {
            // Nowrap can never be broken, so don't bother setting the
            // breakable character boolean. Pre can only be broken if we encounter a newline.
            if (style()->autoWrap() || isNewline)
                m_hasBreakableChar = true;

            if (currMinWidth > m_minWidth)
                m_minWidth = currMinWidth;
            currMinWidth = 0;

            if (isNewline) { // Only set if preserveNewline was true and we saw a newline.
                if (firstLine) {
                    firstLine = false;
                    leadWidth = 0;
                    if (!style()->autoWrap())
                        m_beginMinWidth = currMaxWidth;
                }

                if (currMaxWidth > m_maxWidth)
                    m_maxWidth = currMaxWidth;
                currMaxWidth = 0;
            } else {
                currMaxWidth += f.width(TextRun(txt + i, 1, allowTabs(), leadWidth + currMaxWidth));
                needsWordSpacing = isSpace && !previousCharacterIsSpace && i == len - 1;
            }
            ASSERT(lastWordBoundary == i);
            lastWordBoundary++;
        }
    }

    if ((needsWordSpacing && len > 1) || (ignoringSpaces && !firstWord))
        currMaxWidth += wordSpacing;

    m_minWidth = max(currMinWidth, m_minWidth);
    m_maxWidth = max(currMaxWidth, m_maxWidth);

    if (!style()->autoWrap())
        m_minWidth = m_maxWidth;

    if (style()->whiteSpace() == PRE) {
        if (firstLine)
            m_beginMinWidth = m_maxWidth;
        m_endMinWidth = currMaxWidth;
    }

    setPrefWidthsDirty(false);
}

bool RenderText::containsOnlyWhitespace(unsigned from, unsigned len) const
{
    unsigned currPos;
    for (currPos = from;
         currPos < from + len && ((*m_text)[currPos] == '\n' || (*m_text)[currPos] == ' ' || (*m_text)[currPos] == '\t');
         currPos++) { }
    return currPos >= (from + len);
}

IntPoint RenderText::firstRunOrigin() const
{
    return IntPoint(firstRunX(), firstRunY());
}

int RenderText::firstRunX() const
{
    return m_firstTextBox ? m_firstTextBox->m_x : 0;
}

int RenderText::firstRunY() const
{
    return m_firstTextBox ? m_firstTextBox->m_y : 0;
}
    
void RenderText::setSelectionState(SelectionState state)
{
    InlineTextBox* box;

    RenderObject::setSelectionState(state);
    if (state == SelectionStart || state == SelectionEnd || state == SelectionBoth) {
        int startPos, endPos;
        selectionStartEnd(startPos, endPos);
        if (selectionState() == SelectionStart) {
            endPos = textLength();

            // to handle selection from end of text to end of line
            if (startPos != 0 && startPos == endPos)
                startPos = endPos - 1;
        } else if (selectionState() == SelectionEnd)
            startPos = 0;

        for (box = firstTextBox(); box; box = box->nextTextBox()) {
            if (box->isSelected(startPos, endPos)) {
                RootInlineBox* line = box->root();
                if (line)
                    line->setHasSelectedChildren(true);
            }
        }
    } else {
        for (box = firstTextBox(); box; box = box->nextTextBox()) {
            RootInlineBox* line = box->root();
            if (line)
                line->setHasSelectedChildren(state == SelectionInside);
        }
    }

    // The returned value can be null in case of an orphaned tree.
    if (RenderBlock* cb = containingBlock())
        cb->setSelectionState(state);
}

void RenderText::setTextWithOffset(PassRefPtr<StringImpl> text, unsigned offset, unsigned len, bool force)
{
    unsigned oldLen = textLength();
    unsigned newLen = text->length();
    int delta = newLen - oldLen;
    unsigned end = len ? offset + len - 1 : offset;

    RootInlineBox* firstRootBox = 0;
    RootInlineBox* lastRootBox = 0;

    bool dirtiedLines = false;

    // Dirty all text boxes that include characters in between offset and offset+len.
    for (InlineTextBox* curr = firstTextBox(); curr; curr = curr->nextTextBox()) {
        // Text run is entirely before the affected range.
        if (curr->end() < offset)
            continue;

        // Text run is entirely after the affected range.
        if (curr->start() > end) {
            curr->offsetRun(delta);
            RootInlineBox* root = curr->root();
            if (!firstRootBox) {
                firstRootBox = root;
                if (!dirtiedLines) {
                    // The affected area was in between two runs. Go ahead and mark the root box of
                    // the run after the affected area as dirty.
                    firstRootBox->markDirty();
                    dirtiedLines = true;
                }
            }
            lastRootBox = root;
        } else if (curr->end() >= offset && curr->end() <= end) {
            // Text run overlaps with the left end of the affected range.
            curr->dirtyLineBoxes();
            dirtiedLines = true;
        } else if (curr->start() <= offset && curr->end() >= end) {
            // Text run subsumes the affected range.
            curr->dirtyLineBoxes();
            dirtiedLines = true;
        } else if (curr->start() <= end && curr->end() >= end) {
            // Text run overlaps with right end of the affected range.
            curr->dirtyLineBoxes();
            dirtiedLines = true;
        }
    }

    // Now we have to walk all of the clean lines and adjust their cached line break information
    // to reflect our updated offsets.
    if (lastRootBox)
        lastRootBox = lastRootBox->nextRootBox();
    if (firstRootBox) {
        RootInlineBox* prev = firstRootBox->prevRootBox();
        if (prev)
            firstRootBox = prev;
    } else if (lastTextBox()) {
        ASSERT(!lastRootBox);
        firstRootBox = lastTextBox()->root();
        firstRootBox->markDirty();
        dirtiedLines = true;
    }
    for (RootInlineBox* curr = firstRootBox; curr && curr != lastRootBox; curr = curr->nextRootBox()) {
        if (curr->lineBreakObj() == this && curr->lineBreakPos() > end)
            curr->setLineBreakPos(curr->lineBreakPos() + delta);
    }

    // If the text node is empty, dirty the line where new text will be inserted.
    if (!firstTextBox() && parent()) {
        parent()->dirtyLinesFromChangedChild(this);
        dirtiedLines = true;
    }

    m_linesDirty = dirtiedLines;
    setText(text, force);
}

static inline bool isInlineFlowOrEmptyText(RenderObject* o)
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

UChar RenderText::previousCharacter()
{
    // find previous text renderer if one exists
    RenderObject* previousText = this;
    while ((previousText = previousText->previousInPreOrder()))
        if (!isInlineFlowOrEmptyText(previousText))
            break;
    UChar prev = ' ';
    if (previousText && previousText->isText())
        if (StringImpl* previousString = toRenderText(previousText)->text())
            prev = (*previousString)[previousString->length() - 1];
    return prev;
}

void RenderText::setTextInternal(PassRefPtr<StringImpl> text)
{
    ASSERT(text);
    m_text = document()->displayStringModifiedByEncoding(text);
    ASSERT(m_text);

#if ENABLE(SVG)
    if (isSVGText()) {
        if (style() && style()->whiteSpace() == PRE) {
            // Spec: When xml:space="preserve", the SVG user agent will do the following using a
            // copy of the original character data content. It will convert all newline and tab
            // characters into space characters. Then, it will draw all space characters, including
            // leading, trailing and multiple contiguous space characters.

            m_text = m_text->replace('\n', ' ');

            // If xml:space="preserve" is set, white-space is set to "pre", which
            // preserves leading, trailing & contiguous space character for us.
       } else {
            // Spec: When xml:space="default", the SVG user agent will do the following using a
            // copy of the original character data content. First, it will remove all newline
            // characters. Then it will convert all tab characters into space characters.
            // Then, it will strip off all leading and trailing space characters.
            // Then, all contiguous space characters will be consolidated.    

           m_text = m_text->replace('\n', StringImpl::empty());

           // If xml:space="default" is set, white-space is set to "nowrap", which handles
           // leading, trailing & contiguous space character removal for us.
        }

        m_text = m_text->replace('\t', ' ');
    }
#endif

    if (style()) {
        switch (style()->textTransform()) {
            case TTNONE:
                break;
            case CAPITALIZE: {
                m_text = m_text->capitalize(previousCharacter());
                break;
            }
            case UPPERCASE:
                m_text = m_text->upper();
                break;
            case LOWERCASE:
                m_text = m_text->lower();
                break;
        }

        // We use the same characters here as for list markers.
        // See the listMarkerText function in RenderListMarker.cpp.
        switch (style()->textSecurity()) {
            case TSNONE:
                break;
            case TSCIRCLE:
                m_text = m_text->secure(whiteBullet);
                break;
            case TSDISC:
                m_text = m_text->secure(bullet);
                break;
            case TSSQUARE:
                m_text = m_text->secure(blackSquare);
        }
    }

    ASSERT(m_text);
    ASSERT(!isBR() || (textLength() == 1 && (*m_text)[0] == '\n'));

    m_isAllASCII = charactersAreAllASCII(m_text.get());
}

void RenderText::setText(PassRefPtr<StringImpl> text, bool force)
{
    ASSERT(text);

    if (!force && equal(m_text.get(), text.get()))
        return;

    setTextInternal(text);
    setNeedsLayoutAndPrefWidthsRecalc();
    m_knownNotToUseFallbackFonts = false;
}

int RenderText::lineHeight(bool firstLine, bool) const
{
    // Always use the interior line height of the parent (e.g., if our parent is an inline block).
    return parent()->lineHeight(firstLine, true);
}

void RenderText::dirtyLineBoxes(bool fullLayout)
{
    if (fullLayout)
        deleteTextBoxes();
    else if (!m_linesDirty) {
        for (InlineTextBox* box = firstTextBox(); box; box = box->nextTextBox())
            box->dirtyLineBoxes();
    }
    m_linesDirty = false;
}

InlineTextBox* RenderText::createTextBox()
{
    return new (renderArena()) InlineTextBox(this);
}

InlineTextBox* RenderText::createInlineTextBox()
{
    InlineTextBox* textBox = createTextBox();
    if (!m_firstTextBox)
        m_firstTextBox = m_lastTextBox = textBox;
    else {
        m_lastTextBox->setNextLineBox(textBox);
        textBox->setPreviousLineBox(m_lastTextBox);
        m_lastTextBox = textBox;
    }
    textBox->setIsText(true);
    return textBox;
}

void RenderText::positionLineBox(InlineBox* box)
{
    InlineTextBox* s = static_cast<InlineTextBox*>(box);

    // FIXME: should not be needed!!!
    if (!s->len()) {
        // We want the box to be destroyed.
        s->remove();
        s->destroy(renderArena());
        m_firstTextBox = m_lastTextBox = 0;
        return;
    }

    m_containsReversedText |= s->direction() == RTL;
}

unsigned RenderText::width(unsigned from, unsigned len, int xPos, bool firstLine, HashSet<const SimpleFontData*>* fallbackFonts) const
{
    if (from >= textLength())
        return 0;

    if (from + len > textLength())
        len = textLength() - from;

    return width(from, len, style(firstLine)->font(), xPos, fallbackFonts);
}

unsigned RenderText::width(unsigned from, unsigned len, const Font& f, int xPos, HashSet<const SimpleFontData*>* fallbackFonts) const
{
    ASSERT(from + len <= textLength());
    if (!characters())
        return 0;

    int w;
    if (&f == &style()->font()) {
        if (!style()->preserveNewline() && !from && len == textLength()) {
            if (fallbackFonts) {
                if (prefWidthsDirty() || !m_knownNotToUseFallbackFonts) {
                    const_cast<RenderText*>(this)->calcPrefWidths(0, *fallbackFonts);
                    if (fallbackFonts->isEmpty())
                        m_knownNotToUseFallbackFonts = true;
                }
                w = m_maxWidth;
            } else
                w = maxPrefWidth();
        } else
            w = widthFromCache(f, from, len, xPos, fallbackFonts);
    } else
        w = f.width(TextRun(text()->characters() + from, len, allowTabs(), xPos), fallbackFonts);

    return w;
}

IntRect RenderText::linesBoundingBox() const
{
    IntRect result;
    
    ASSERT(!firstTextBox() == !lastTextBox());  // Either both are null or both exist.
    if (firstTextBox() && lastTextBox()) {
        // Return the width of the minimal left side and the maximal right side.
        int leftSide = 0;
        int rightSide = 0;
        for (InlineTextBox* curr = firstTextBox(); curr; curr = curr->nextTextBox()) {
            if (curr == firstTextBox() || curr->x() < leftSide)
                leftSide = curr->x();
            if (curr == firstTextBox() || curr->x() + curr->width() > rightSide)
                rightSide = curr->x() + curr->width();
        }
        result.setWidth(rightSide - leftSide);
        result.setX(leftSide);
        result.setHeight(lastTextBox()->y() + lastTextBox()->height() - firstTextBox()->y());
        result.setY(firstTextBox()->y());
    }

    return result;
}

IntRect RenderText::clippedOverflowRectForRepaint(RenderBoxModelObject* repaintContainer)
{
    RenderObject* cb = containingBlock();
    return cb->clippedOverflowRectForRepaint(repaintContainer);
}

IntRect RenderText::selectionRectForRepaint(RenderBoxModelObject* repaintContainer, bool clipToVisibleContent)
{
    ASSERT(!needsLayout());

    if (selectionState() == SelectionNone)
        return IntRect();
    RenderBlock* cb =  containingBlock();
    if (!cb)
        return IntRect();

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

    IntRect rect;
    for (InlineTextBox* box = firstTextBox(); box; box = box->nextTextBox())
        rect.unite(box->selectionRect(0, 0, startPos, endPos));

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
    InlineTextBox* box = firstTextBox();
    if (!box)
        return 0;
    int minOffset = box->start();
    for (box = box->nextTextBox(); box; box = box->nextTextBox())
        minOffset = min<int>(minOffset, box->start());
    return minOffset;
}

int RenderText::caretMaxOffset() const
{
    InlineTextBox* box = lastTextBox();
    if (!box)
        return textLength();
    int maxOffset = box->start() + box->len();
    for (box = box->prevTextBox(); box; box = box->prevTextBox())
        maxOffset = max<int>(maxOffset, box->start() + box->len());
    return maxOffset;
}

unsigned RenderText::caretMaxRenderedOffset() const
{
    int l = 0;
    for (InlineTextBox* box = firstTextBox(); box; box = box->nextTextBox())
        l += box->len();
    return l;
}

int RenderText::previousOffset(int current) const
{
    StringImpl* si = m_text.get();
    TextBreakIterator* iterator = cursorMovementIterator(si->characters(), si->length());
    if (!iterator)
        return current - 1;

    long result = textBreakPreceding(iterator, current);
    if (result == TextBreakDone)
        result = current - 1;

#ifdef BUILDING_ON_TIGER
    // ICU 3.2 allows character breaks before a half-width Katakana voiced mark.
    if (static_cast<unsigned>(result) < si->length()) {
        UChar character = (*si)[result];
        if (character == 0xFF9E || character == 0xFF9F)
            --result;
    }
#endif

    return result;
}

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

int RenderText::previousOffsetForBackwardDeletion(int current) const
{
#if PLATFORM(MAC)
    UChar32 character;
    while (current > 0) {
        if (U16_IS_TRAIL((*m_text)[--current]))
            --current;
        if (current < 0)
            break;

        UChar32 character = m_text->characterStartingAt(current);

        // We don't combine characters in Armenian ... Limbu range for backward deletion.
        if ((character >= 0x0530) && (character < 0x1950))
            break;

        if (u_isbase(character) && (character != 0xFF9E) && (character != 0xFF9F))
            break;
    }

    if (current <= 0)
        return current;

    // Hangul
    character = m_text->characterStartingAt(current);
    if (((character >= HANGUL_CHOSEONG_START) && (character <= HANGUL_JONGSEONG_END)) || ((character >= HANGUL_SYLLABLE_START) && (character <= HANGUL_SYLLABLE_END))) {
        HangulState state;
        HangulState initialState;

        if (character < HANGUL_JUNGSEONG_START)
            state = HangulStateL;
        else if (character < HANGUL_JONGSEONG_START)
            state = HangulStateV;
        else if (character < HANGUL_SYLLABLE_START)
            state = HangulStateT;
        else
            state = isHangulLVT(character) ? HangulStateLVT : HangulStateLV;

        initialState = state;

        while (current > 0 && ((character = m_text->characterStartingAt(current - 1)) >= HANGUL_CHOSEONG_START) && (character <= HANGUL_SYLLABLE_END) && ((character <= HANGUL_JONGSEONG_END) || (character >= HANGUL_SYLLABLE_START))) {
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
    return current - 1;
#endif
}

int RenderText::nextOffset(int current) const
{
    StringImpl* si = m_text.get();
    TextBreakIterator* iterator = cursorMovementIterator(si->characters(), si->length());
    if (!iterator)
        return current + 1;

    long result = textBreakFollowing(iterator, current);
    if (result == TextBreakDone)
        result = current + 1;

#ifdef BUILDING_ON_TIGER
    // ICU 3.2 allows character breaks before a half-width Katakana voiced mark.
    if (static_cast<unsigned>(result) < si->length()) {
        UChar character = (*si)[result];
        if (character == 0xFF9E || character == 0xFF9F)
            ++result;
    }
#endif

    return result;
}

#ifndef NDEBUG

void RenderText::checkConsistency() const
{
#ifdef CHECK_CONSISTENCY
    const InlineTextBox* prev = 0;
    for (const InlineTextBox* child = m_firstTextBox; child != 0; child = child->nextTextBox()) {
        ASSERT(child->object() == this);
        ASSERT(child->prevTextBox() == prev);
        prev = child;
    }
    ASSERT(prev == m_lastTextBox);
#endif
}

#endif

} // namespace WebCore
