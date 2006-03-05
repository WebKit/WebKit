/**
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
 * Copyright (C) 2006 Andrew Wellington (proton@wiretapped.net)
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"
#include "RenderText.h"

#include "InlineTextBox.h"
#include "Pen.h"
#include "RenderBlock.h"
#include "VisiblePosition.h"
#include "break_lines.h"
#include "dom2_rangeimpl.h"
#include "render_arena.h"
#include <kxmlcore/AlwaysInline.h>
#include <unicode/ubrk.h>

namespace WebCore {

static UBreakIterator* getCharacterBreakIterator(const DOMStringImpl* i)
{
    // The locale is currently ignored when determining character cluster breaks.
    // This may change in the future, according to Deborah Goldsmith.
    static bool createdIterator = false;
    static UBreakIterator* iterator;
    UErrorCode status;
    if (!createdIterator) {
        status = U_ZERO_ERROR;
        iterator = ubrk_open(UBRK_CHARACTER, "en_us", 0, 0, &status);
        createdIterator = true;
    }
    if (!iterator)
        return 0;

    status = U_ZERO_ERROR;
    ubrk_setText(iterator, reinterpret_cast<const UChar*>(i->unicode()), i->length(), &status);
    if (status != U_ZERO_ERROR)
        return 0;

    return iterator;
}

int RenderText::previousOffset(int current) const
{
    UBreakIterator* iterator = getCharacterBreakIterator(str.get());
    if (!iterator)
        return current - 1;

    long result = ubrk_preceding(iterator, current);
    if (result == UBRK_DONE)
        result = current - 1;

    return result;
}

int RenderText::nextOffset(int current) const
{
    UBreakIterator* iterator = getCharacterBreakIterator(str.get());
    if (!iterator)
        return current + 1;
    
    long result = ubrk_following(iterator, current);
    if (result == UBRK_DONE)
        result = current + 1;

    return result;
}

RenderText::RenderText(DOM::NodeImpl* node, DOMStringImpl *_str)
     : RenderObject(node), str(_str), m_firstTextBox(0), m_lastTextBox(0)
     , m_minWidth(-1), m_maxWidth(-1), m_selectionState(SelectionNone)
     , m_linesDirty(false), m_containsReversedText(false)
     , m_allAsciiChecked(false), m_allAscii(false)
     , m_monospaceCharacterWidth(0)
{
    setRenderText();
    if (str)
        str = str->replace('\\', backslashAsCurrencySymbol());
    KHTMLAssert(!str || !str->length() || str->unicode());
}

void RenderText::setStyle(RenderStyle *_style)
{
    if ( style() != _style ) {
        bool needToTransformText = (!style() && _style->textTransform() != TTNONE) ||
                                   (style() && style()->textTransform() != _style->textTransform());

        RenderObject::setStyle( _style );

        if (needToTransformText) {
            RefPtr<DOMStringImpl> textToTransform = originalString();
            if (textToTransform)
                setText(textToTransform.get(), true);
        }
        // setText also calls cacheWidths(), so there is no need to call it again in that case.
        else
            cacheWidths();
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
        }
        else if (parent())
            parent()->dirtyLinesFromChangedChild(this);
    }
    deleteTextBoxes();
    RenderObject::destroy();
}

void RenderText::extractTextBox(InlineTextBox* box)
{
    m_lastTextBox = box->prevTextBox();
    if (box == m_firstTextBox)
        m_firstTextBox = 0;
    if (box->prevTextBox())
        box->prevTextBox()->setNextLineBox(0);
    box->setPreviousLineBox(0);
    for (InlineRunBox* curr = box; curr; curr = curr->nextLineBox())
        curr->setExtracted();
}

void RenderText::attachTextBox(InlineTextBox* box)
{
    if (m_lastTextBox) {
        m_lastTextBox->setNextLineBox(box);
        box->setPreviousLineBox(m_lastTextBox);
    }
    else
        m_firstTextBox = box;
    InlineTextBox* last = box;
    for (InlineTextBox* curr = box; curr; curr = curr->nextTextBox()) {
        curr->setExtracted(false);
        last = curr;
    }
    m_lastTextBox = last;
}

void RenderText::removeTextBox(InlineTextBox* box)
{
    if (box == m_firstTextBox)
        m_firstTextBox = box->nextTextBox();
    if (box == m_lastTextBox)
        m_lastTextBox = box->prevTextBox();
    if (box->nextTextBox())
        box->nextTextBox()->setPreviousLineBox(box->prevTextBox());
    if (box->prevTextBox())
        box->prevTextBox()->setNextLineBox(box->nextTextBox());
}

void RenderText::deleteTextBoxes()
{
    if (firstTextBox()) {
        RenderArena* arena = renderArena();
        InlineTextBox *curr = firstTextBox(), *next = 0;
        while (curr) {
            next = curr->nextTextBox();
            curr->destroy(arena);
            curr = next;
        }
        m_firstTextBox = m_lastTextBox = 0;
    }
}

bool RenderText::isTextFragment() const
{
    return false;
}

PassRefPtr<DOMStringImpl> RenderText::originalString() const
{
    return element() ? element()->string() : 0;
}

void RenderText::absoluteRects(QValueList<IntRect>& rects, int _tx, int _ty)
{
    for (InlineTextBox* box = firstTextBox(); box; box = box->nextTextBox())
        rects.append(IntRect(_tx + box->xPos(), 
                           _ty + box->yPos(), 
                           box->width(), 
                           box->height()));
}

QValueList<IntRect> RenderText::lineBoxRects()
{
    QValueList<IntRect> rects;
    int x = 0, y = 0;
    absolutePosition(x, y);
    absoluteRects(rects, x, y);
    return rects;
}

InlineTextBox* RenderText::findNextInlineTextBox(int offset, int &pos) const
{
    // The text runs point to parts of the rendertext's str string
    // (they don't include '\n')
    // Find the text run that includes the character at @p offset
    // and return pos, which is the position of the char in the run.

    if (!m_firstTextBox)
        return 0;
    
    InlineTextBox* s = m_firstTextBox;
    int off = s->m_len;
    while (offset > off && s->nextTextBox())
    {
        s = s->nextTextBox();
        off = s->m_start + s->m_len;
    }
    // we are now in the correct text run
    pos = (offset > off ? s->m_len : s->m_len - (off - offset) );
    return s;
}

VisiblePosition RenderText::positionForCoordinates(int _x, int _y)
{
    if (!firstTextBox() || stringLength() == 0)
        return VisiblePosition(element(), 0, DOWNSTREAM);

    int absx, absy;
    RenderBlock *cb = containingBlock();
    cb->absolutePosition(absx, absy);
    if (cb->hasOverflowClip())
        cb->layer()->subtractScrollOffset(absx, absy);

    if (firstTextBox() && _y < absy + firstTextBox()->root()->bottomOverflow() && _x < absx + firstTextBox()->m_x) {
        // at the y coordinate of the first line or above
        // and the x coordinate is to the left than the first text box left edge
        return VisiblePosition(element(), firstTextBox()->m_start, DOWNSTREAM);
    }

    if (lastTextBox() && _y >= absy + lastTextBox()->root()->topOverflow() && _x >= absx + lastTextBox()->m_x + lastTextBox()->m_width) {
        // at the y coordinate of the last line or below
        // and the x coordinate is to the right than the last text box right edge
        return VisiblePosition(element(), lastTextBox()->m_start + lastTextBox()->m_len, DOWNSTREAM);
    }

    for (InlineTextBox *box = firstTextBox(); box; box = box->nextTextBox()) {
        if (_y >= absy + box->root()->topOverflow() && _y < absy + box->root()->bottomOverflow()) {
            if (_x < absx + box->m_x + box->m_width) {
                // and the x coordinate is to the left of the right edge of this box
                // check to see if position goes in this box
                int offset = box->offsetForPosition(_x - absx);
                if (offset != -1)
                    return VisiblePosition(element(), offset + box->m_start, VP_UPSTREAM_IF_POSSIBLE);
            }
            else if (!box->prevOnLine() && _x < absx + box->m_x) {
                // box is first on line
                // and the x coordinate is to the left of the first text box left edge
                return VisiblePosition(element(), box->m_start, DOWNSTREAM);
            }
            else if (!box->nextOnLine() && _x >= absx + box->m_x + box->m_width) {
                // box is last on line
                // and the x coordinate is to the right of the last text box right edge
                // generate VisiblePosition, use UPSTREAM affinity if possible
                return VisiblePosition(element(), box->m_start + box->m_len, VP_UPSTREAM_IF_POSSIBLE);
            }
        }
    }
    
    return VisiblePosition(element(), 0, DOWNSTREAM);
}

#if __GNUC__
static RenderObject *firstRendererOnNextLine(InlineBox *box) __attribute__ ((unused));
#endif
static RenderObject *firstRendererOnNextLine(InlineBox *box)
{
    if (!box)
        return 0;

    RootInlineBox *root = box->root();
    if (!root)
        return 0;
        
    if (root->endsWithBreak())
        return 0;
    
    RootInlineBox *nextRoot = root->nextRootBox();
    if (!nextRoot)
        return 0;
    
    InlineBox *firstChild = nextRoot->firstChild();
    if (!firstChild)
        return 0;

    return firstChild->object();
}

static RenderObject *lastRendererOnPrevLine(InlineBox *box)
{
    if (!box)
        return 0;
    
    RootInlineBox *root = box->root();
    if (!root)
        return 0;
    
    if (root->endsWithBreak())
        return 0;
    
    RootInlineBox *prevRoot = root->prevRootBox();
    if (!prevRoot)
        return 0;
    
    InlineBox *lastChild = prevRoot->lastChild();
    if (!lastChild)
        return 0;
    
    return lastChild->object();
}

bool RenderText::atLineWrap(InlineTextBox *box, int offset)
{
    if (box->nextTextBox() && !box->nextOnLine() && offset == box->m_start + box->m_len) {
        // Take special care because in preformatted text, the newlines
        // are in between the text boxes (i.e. not in any box's m_start
        // thru m_start+m_len-1), even though they are rendered.
        if (!style()->preserveNewline() || (*str)[offset] != '\n')
            return true;
    }
    
    return false;
}

IntRect RenderText::caretRect(int offset, EAffinity affinity, int *extraWidthToEndOfLine)
{
    if (!firstTextBox() || stringLength() == 0)
        return IntRect();

    // Find the text box for the given offset
    InlineTextBox *box = 0;
    for (box = firstTextBox(); box; box = box->nextTextBox()) {
        if ((offset >= box->m_start) && (offset <= box->m_start + box->m_len)) {
            // Check if downstream affinity would make us move to the next line.
            if (affinity == DOWNSTREAM && atLineWrap(box, offset)) {
                // Use the next text box
                box = box->nextTextBox();
                offset = box->m_start;
            } else {
                InlineTextBox *prevBox = box->prevTextBox();
                if (offset == box->m_start && affinity == UPSTREAM && prevBox && !box->prevOnLine()) {
                    if (prevBox) {
                        box = prevBox;
                        offset = box->m_start + box->m_len;
                    } else {
                        RenderObject *object = lastRendererOnPrevLine(box);
                        if (object)
                            return object->caretRect(0, affinity);
                    }
                }
            }
            break;
        }
    }
    
    if (!box) {
        return IntRect();
    }

    int height = box->root()->bottomOverflow() - box->root()->topOverflow();
    int top = box->root()->topOverflow();

    int left = box->positionForOffset(offset);

    // FIXME: should we use the width of the root inline box or the
    // width of the containing block for this?
    if (extraWidthToEndOfLine)
        *extraWidthToEndOfLine = (box->root()->width() + box->root()->xPos()) - (left + 1);

    int absx, absy;
    absolutePosition(absx,absy);
    left += absx;
    top += absy;

    // FIXME: Need the +1 to match caret position of other programs on Macintosh.
    // Would be better to somehow derive it once we understand exactly why it's needed.
    left += 1;

    RenderBlock *cb = containingBlock();
    int availableWidth = cb->lineWidth(top);
    if (style()->autoWrap())
        left = kMin(left, absx + box->m_x + availableWidth - 1);
    
    return IntRect(left, top, 1, height);
}

void RenderText::posOfChar(int chr, int &x, int &y)
{
    absolutePosition(x, y, false);

    int pos;
    if (InlineTextBox* s = findNextInlineTextBox(chr, pos)) {
        // s is the line containing the character
        x += s->m_x; // this is the x of the beginning of the line, but it's good enough for now
        y += s->m_y;
    }
}

bool RenderText::allAscii() const
{
    if (m_allAsciiChecked)
        return m_allAscii;
    m_allAsciiChecked = true;
    
    unsigned i;
    for (i = 0; i < str->length(); i++)
        if ((*str)[i].unicode() >= 0x7f) {
            m_allAscii = false;
            return m_allAscii;
        }
    
    m_allAscii = true;
    
    return m_allAscii;
}

bool RenderText::shouldUseMonospaceCache(const Font *f) const
{
    return (f && f->isFixedPitch() && allAscii() && !style()->font().isSmallCaps());
}

// We cache the width of the ' ' character for <pre> text.  We could go further
// and cache a widths array for all styles, at the expense of increasing the size of the
// RenderText.
void RenderText::cacheWidths()
{
    const Font *f = font(false);
    if (shouldUseMonospaceCache(f)) {
        QChar c(' ');
        m_monospaceCharacterWidth = (int)f->floatWidth(&c, 1, 0, 1, 0, 0);
    } else {
        m_monospaceCharacterWidth = 0;
    }
}

ALWAYS_INLINE int RenderText::widthFromCache(const Font *f, int start, int len, int tabWidth, int xpos) const
{
    if (m_monospaceCharacterWidth != 0) {
        int i, w = 0;
        for (i = start; i < start+len; i++) {
            QChar c = (*str)[i];
            int dir = c.direction();
            if (dir != QChar::DirNSM && dir != QChar::DirBN) {
                if (c == '\t' && tabWidth != 0) {
                    w += tabWidth - ((xpos + w) % tabWidth);
                } else
                    w += m_monospaceCharacterWidth;
                if (c.isSpace() && i>start && !(*str)[i-1].isSpace())
                    w += f->wordSpacing();        
            }
        }

        return w;
    }
    
    return f->width(str->s, str->l, start, len, tabWidth, xpos);
}

void RenderText::trimmedMinMaxWidth(int leadWidth,
                                    int& beginMinW, bool& beginWS, 
                                    int& endMinW, bool& endWS,
                                    bool& hasBreakableChar, bool& hasBreak,
                                    int& beginMaxW, int& endMaxW,
                                    int& minW, int& maxW, bool& stripFrontSpaces)
{
    bool collapseWhiteSpace = style()->collapseWhiteSpace();
    if (!collapseWhiteSpace)
        stripFrontSpaces = false;
    
    int len = str->length();
    if (len == 0 || (stripFrontSpaces && str->containsOnlyWhitespace())) {
        maxW = 0;
        hasBreak = false;
        return;
    }
    
    // if the text has a variable width tab, we need to call 
    if (m_hasTab)
        calcMinMaxWidth(leadWidth);
    
    minW = m_minWidth;
    maxW = m_maxWidth;
    beginWS = !stripFrontSpaces && m_hasBeginWS;
    endWS = m_hasEndWS;
    
    beginMinW = m_beginMinWidth;
    endMinW = m_endMinWidth;
    
    hasBreakableChar = m_hasBreakableChar;
    hasBreak = m_hasBreak;

    if (stripFrontSpaces && ((*str)[0] == ' ' || ((*str)[0] == '\n' && !style()->preserveNewline()) || (*str)[0] == '\t')) {
        const Font *f = font(false); // FIXME: Why is it ok to ignore first-line here?
        QChar space[1]; space[0] = ' ';
        int spaceWidth = f->width(space, 1);
        maxW -= spaceWidth + f->wordSpacing();
    }
    
    stripFrontSpaces = collapseWhiteSpace && m_hasEndWS;
    
    if (!style()->autoWrap() || minW > maxW)
        minW = maxW;

    // Compute our max widths by scanning the string for newlines.
    if (hasBreak) {
        const Font *f = font(false);
        bool firstLine = true;
        beginMaxW = endMaxW = maxW;
        for (int i = 0; i < len; i++)
        {
            int linelen = 0;
            while (i+linelen < len && (*str)[i+linelen] != '\n')
                linelen++;
                
            if (linelen)
            {
                endMaxW = widthFromCache(f, i, linelen, tabWidth(), leadWidth + endMaxW);
                if (firstLine) {
                    firstLine = false;
                    leadWidth = 0;
                    beginMaxW = endMaxW;
                }
                i += linelen;
            }
            else if (firstLine) {
                beginMaxW = 0;
                firstLine = false;
                leadWidth = 0;
            }
    
            if (i == len-1)
                // A <pre> run that ends with a newline, as in, e.g.,
                // <pre>Some text\n\n<span>More text</pre>
                endMaxW = 0;
        }
    }
}

void RenderText::calcMinMaxWidth()
{
    // Use 0 for the leadWidth.   If the text contains a variable width tab, the real width
    // will get measured when trimmedMinMaxWidth calls again with the real leadWidth.
    KHTMLAssert( !minMaxKnown() );
    calcMinMaxWidth(0);
}

void RenderText::calcMinMaxWidth(int leadWidth)
{
    // ### calc Min and Max width...
    m_minWidth = m_beginMinWidth = m_endMinWidth = 0;
    m_maxWidth = 0;

    if (isBR())
        return;
        
    int currMinWidth = 0;
    int currMaxWidth = 0;
    m_hasBreakableChar = m_hasBreak = m_hasTab = m_hasBeginWS = m_hasEndWS = false;
    
    // FIXME: not 100% correct for first-line
    const Font *f = font(false);
    int wordSpacing = style()->wordSpacing();
    int len = str->l;
    QChar *txt = str->s;
    bool needsWordSpacing = false;
    bool ignoringSpaces = false;
    bool isSpace = false;
    bool firstWord = true;
    bool firstLine = true;
    int nextBreakable = -1;
    for(int i = 0; i < len; i++)
    {
        QChar c = txt[i];
        
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
        } else {
            isSpace = c == ' ';
        }
        
        if ((isSpace || isNewline) && i == 0)
            m_hasBeginWS = true;
        if ((isSpace || isNewline) && i == len-1)
            m_hasEndWS = true;
            
        if (!ignoringSpaces && style()->collapseWhiteSpace() && previousCharacterIsSpace && isSpace)
            ignoringSpaces = true;
        
        if (ignoringSpaces && !isSpace)
            ignoringSpaces = false;
        
        // Ignore spaces and soft hyphens
        if (ignoringSpaces || c.unicode() == SOFT_HYPHEN) {
            continue;
        }
        
        bool hasBreak = isBreakable(txt, i, len, nextBreakable);
        int j = i;
        while (c != '\n' && c != ' ' && c != '\t' && c.unicode() != SOFT_HYPHEN) {
            j++;
            if (j == len)
                break;
            c = txt[j];
            if (isBreakable(txt, j, len, nextBreakable))
                break;
        }
            
        int wordlen = j - i;
        if (wordlen) {
            int w = widthFromCache(f, i, wordlen, tabWidth(), leadWidth + currMaxWidth);
            currMinWidth += w;
            currMaxWidth += w;
            
            bool isSpace = (j < len) && c == ' ';
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
            
            if (currMinWidth > m_minWidth) m_minWidth = currMinWidth;
            currMinWidth = 0;
            
            i += wordlen-1;
        }
        else {
            // Nowrap can never be broken, so don't bother setting the
            // breakable character boolean. Pre can only be broken if we encounter a newline.     
            if (style()->autoWrap() || isNewline)
                m_hasBreakableChar = true;

            if (currMinWidth > m_minWidth) m_minWidth = currMinWidth;
            currMinWidth = 0;
            
            if (isNewline) { // Only set if preserveNewline was true and we saw a newline.
                if (firstLine) {
                    firstLine = false;
                    leadWidth = 0;
                    m_beginMinWidth = currMaxWidth;
                }
                
                if (currMaxWidth > m_maxWidth) m_maxWidth = currMaxWidth;
                currMaxWidth = 0;
            }
            else
            {
                currMaxWidth += f->width(txt, len, i, 1, tabWidth(), leadWidth + currMaxWidth);
                needsWordSpacing = isSpace && !previousCharacterIsSpace && i == len-1;
            }
        }
    }

    if (needsWordSpacing && len > 1) 
        currMaxWidth += wordSpacing;
    
    m_minWidth = kMax(currMinWidth, m_minWidth);
    m_maxWidth = kMax(currMaxWidth, m_maxWidth);
        
    if (!style()->autoWrap())
        m_minWidth = m_maxWidth;

    if (style()->whiteSpace() == PRE) {
        // FIXME: pre-wrap and pre-line need to be dealt with possibly?  This code won't be right
        // for them though.
        if (firstLine)
            m_beginMinWidth = m_maxWidth;
        m_endMinWidth = currMaxWidth;
    }
    
    setMinMaxKnown();
    //kdDebug( 6040 ) << "Text::calcMinMaxWidth(): min = " << m_minWidth << " max = " << m_maxWidth << endl;
}

bool RenderText::containsOnlyWhitespace(unsigned from, unsigned len) const
{
    unsigned currPos;
    for (currPos = from; 
         currPos < from+len && ((*str)[currPos] == '\n' || (*str)[currPos].unicode() == ' ' || (*str)[currPos] == '\t'); 
         currPos++);
    return currPos >= (from+len);
}

int RenderText::minXPos() const
{
    if (!m_firstTextBox) return 0;
    int retval=6666666;
    for (InlineTextBox* box = firstTextBox(); box; box = box->nextTextBox())
        retval = kMin(retval, (int)box->m_x);
    return retval;
}

int RenderText::xPos() const
{
    return m_firstTextBox ? m_firstTextBox->m_x : 0;
}

int RenderText::yPos() const
{
    return m_firstTextBox ? m_firstTextBox->m_y : 0;
}

const Font& RenderText::font()
{
    return style()->font();
}

void RenderText::setSelectionState(SelectionState s)
{
    InlineTextBox* box;
    
    m_selectionState = s;
    if (s == SelectionStart || s == SelectionEnd || s == SelectionBoth) {
        int startPos, endPos;
        selectionStartEnd(startPos, endPos);
        if(selectionState() == SelectionStart) {
            endPos = str->length();
            
            // to handle selection from end of text to end of line
            if (startPos != 0 && startPos == endPos) {
                startPos = endPos - 1;
            }
        } else if(selectionState() == SelectionEnd)
            startPos = 0;
        
        for (box = firstTextBox(); box; box = box->nextTextBox()) {
            if (box->isSelected(startPos, endPos)) {
                RootInlineBox* line = box->root();
                if (line)
                    line->setHasSelectedChildren(true);
            }
        }
    }
    else {
        for (box = firstTextBox(); box; box = box->nextTextBox()) {
            RootInlineBox* line = box->root();
            if (line)
                line->setHasSelectedChildren(s == SelectionInside);
        }
    }
    
    containingBlock()->setSelectionState(s);
}

void RenderText::setTextWithOffset(DOMStringImpl *text, uint offset, uint len, bool force)
{
    uint oldLen = str ? str->length() : 0;
    uint newLen = text ? text->length() : 0;
    int delta = newLen - oldLen;
    uint end = len ? offset+len-1 : offset;

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
                if (!dirtiedLines) { // The affected area was in between two runs. Go ahead and mark the root box of the run after the affected area as dirty.
                    firstRootBox->markDirty();
                    dirtiedLines = true;
                }
            }
            lastRootBox = root;
        }
        else if (curr->end() >= offset && curr->end() <= end) {
            curr->dirtyLineBoxes(); // Text run overlaps with the left end of the affected range.
            dirtiedLines = true;
        }
        else if (curr->start() <= offset && curr->end() >= end) {
            curr->dirtyLineBoxes(); // Text run subsumes the affected range.
            dirtiedLines = true;
        }
        else if (curr->start() <= end && curr->end() >= end) {
            curr->dirtyLineBoxes(); // Text run overlaps with right end of the affected range.
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
    }
    for (RootInlineBox* curr = firstRootBox; curr && curr != lastRootBox; curr = curr->nextRootBox()) {
        if (curr->lineBreakObj() == this && curr->lineBreakPos() > end)
            curr->setLineBreakPos(curr->lineBreakPos()+delta);
    }
    
    m_linesDirty = dirtiedLines;
    setText(text, force);
}

void RenderText::setText(DOMStringImpl *text, bool force)
{
    if (!text)
        return;
    if (!force && str == text)
        return;

    m_allAsciiChecked = false;

    str = text;
    if (str) {
        str = str->replace('\\', backslashAsCurrencySymbol());
        if (style()) {
            switch (style()->textTransform()) {
                case CAPITALIZE:
                {
                    // find previous text renderer if one exists
                    RenderObject* o;
                    bool runOnString = false;
                    for (o = previousRenderer(); o && o->isInlineFlow(); o = o->previousRenderer())
                        ;
                    if (o && o->isText()) {
                        DOMStringImpl* prevStr = static_cast<RenderText*>(o)->string();
                        QChar c = (*prevStr)[prevStr->length() - 1];
                        if (!c.isSpace())
                            runOnString = true;
                    }
                    str = str->capitalize(runOnString);
                }
                    break;
                case UPPERCASE:  str = str->upper();       break;
                case LOWERCASE:  str = str->lower();       break;
                case NONE:
                default:;
            }
        }
    }

    cacheWidths();

    // ### what should happen if we change the text of a
    // RenderBR object ?
    KHTMLAssert(!isBR() || (str->length() == 1 && (*str)[0] == '\n'));
    KHTMLAssert(!str->length() || str->unicode());

    setNeedsLayoutAndMinMaxRecalc();
}

int RenderText::height() const
{
    // FIXME: Why use line-height? Shouldn't we be adding in the height of the last text box? -dwh
    int retval = 0;
    if (firstTextBox())
        retval = lastTextBox()->m_y + lineHeight(false) - firstTextBox()->m_y;
    return retval;
}

short RenderText::lineHeight(bool firstLine, bool) const
{
    // Always use the interior line height of the parent (e.g., if our parent is an inline block).
    return parent()->lineHeight(firstLine, true);
}

void RenderText::dirtyLineBoxes(bool fullLayout, bool)
{
    if (fullLayout)
        deleteTextBoxes();
    else if (!m_linesDirty) {
        for (InlineTextBox* box = firstTextBox(); box; box = box->nextTextBox())
            box->dirtyLineBoxes();
    }
    m_linesDirty = false;
}

InlineBox* RenderText::createInlineBox(bool, bool isRootLineBox, bool)
{
    KHTMLAssert(!isRootLineBox);
    InlineTextBox* textBox = new (renderArena()) InlineTextBox(this);
    if (!m_firstTextBox)
        m_firstTextBox = m_lastTextBox = textBox;
    else {
        m_lastTextBox->setNextLineBox(textBox);
        textBox->setPreviousLineBox(m_lastTextBox);
        m_lastTextBox = textBox;
    }
    return textBox;
}

void RenderText::position(InlineBox* box, int from, int len, bool reverse, bool override)
{
    InlineTextBox *s = static_cast<InlineTextBox*>(box);
    
    // ### should not be needed!!!
    if (len == 0) {
        // We want the box to be destroyed.
        s->remove();
        s->destroy(renderArena());
        m_firstTextBox = m_lastTextBox = 0;
        return;
    }
    
    reverse = reverse && !style()->visuallyOrdered();
    m_containsReversedText |= reverse;

    s->m_reversed = reverse;
    s->m_dirOverride = override || style()->visuallyOrdered();
    s->m_start = from;
    s->m_len = len;
}

unsigned int RenderText::width(unsigned int from, unsigned int len, int xpos, bool firstLine) const
{
    if (from >= str->length())
        return 0;
    if ( from + len > str->length() ) len = str->length() - from;

    const Font *f = font(firstLine);
    return width( from, len, f, xpos );
}

unsigned int RenderText::width(unsigned int from, unsigned int len, const Font *f, int xpos) const
{
    if(!str->s || from > str->l ) return 0;
    if ( from + len > str->l ) len = str->l - from;

    int w;
    if (!style()->preserveNewline() && f == &style()->font() && from == 0 && len == str->l ) {
        w = m_maxWidth;
    } else if (f == &style()->font()) {
        w = widthFromCache(f, from, len, tabWidth(), xpos);
    } else {
        w = f->width(str->s, str->l, from, len, tabWidth(), xpos );
    }
        
    //kdDebug( 6040 ) << "RenderText::width(" << from << ", " << len << ") = " << w << endl;
    return w;
}

int RenderText::width() const
{
    int minx = 100000000;
    int maxx = 0;
    // slooow
    for (InlineTextBox* s = firstTextBox(); s; s = s->nextTextBox()) {
        if(s->m_x < minx)
            minx = s->m_x;
        if(s->m_x + s->m_width > maxx)
            maxx = s->m_x + s->m_width;
    }

    return kMax(0, maxx-minx);
}

IntRect RenderText::getAbsoluteRepaintRect()
{
    RenderObject *cb = containingBlock();
    return cb->getAbsoluteRepaintRect();
}

IntRect RenderText::selectionRect()
{
    IntRect rect;
    if (selectionState() == SelectionNone)
        return rect;
    RenderBlock* cb =  containingBlock();
    if (!cb)
        return rect;
    
    // Now calculate startPos and endPos for painting selection.
    // We include a selection while endPos > 0
    int startPos, endPos;
    if (selectionState() == SelectionInside) {
        // We are fully selected.
        startPos = 0;
        endPos = str->length();
    } else {
        selectionStartEnd(startPos, endPos);
        if (selectionState() == SelectionStart)
            endPos = str->length();
        else if (selectionState() == SelectionEnd)
            startPos = 0;
    }
    
    if (startPos == endPos)
        return rect;

    int absx, absy;
    cb->absolutePosition(absx, absy);
    RenderLayer* layer = cb->layer();
    if (layer)
       layer->subtractScrollOffset(absx, absy); 
    for (InlineTextBox* box = firstTextBox(); box; box = box->nextTextBox())
        rect.unite(box->selectionRect(absx, absy, startPos, endPos));

    return rect;
}

short RenderText::verticalPositionHint( bool firstLine ) const
{
    return parent()->verticalPositionHint( firstLine );
}

const Font *RenderText::font(bool firstLine) const
{
    return &style(firstLine)->font();
}

int RenderText::caretMinOffset() const
{
    InlineTextBox *box = firstTextBox();
    if (!box)
        return 0;
    int minOffset = box->m_start;
    for (box = box->nextTextBox(); box; box = box->nextTextBox())
        minOffset = kMin(minOffset, box->m_start);
    return minOffset;
}

int RenderText::caretMaxOffset() const
{
    InlineTextBox* box = lastTextBox();
    if (!box) 
        return str->length();
    int maxOffset = box->m_start + box->m_len;
    for (box = box->prevTextBox(); box; box = box->prevTextBox())
        maxOffset = kMax(maxOffset,box->m_start + box->m_len);
    return maxOffset;
}

unsigned RenderText::caretMaxRenderedOffset() const
{
    int l = 0;
    for (InlineTextBox* box = firstTextBox(); box; box = box->nextTextBox())
        l += box->m_len;
    return l;
}

InlineBox *RenderText::inlineBox(int offset, EAffinity affinity)
{
    for (InlineTextBox *box = firstTextBox(); box; box = box->nextTextBox()) {
        if (offset >= box->m_start && offset <= box->m_start + box->m_len) {
            if (affinity == DOWNSTREAM && atLineWrap(box, offset))
                return box->nextTextBox();
            return box;
        }
        
        if (offset < box->m_start) {
            // The offset we're looking for is before this node
            // this means the offset must be in content that is
            // not rendered.
            return box->prevTextBox() ? box->prevTextBox() : firstTextBox();
        }
    }
    
    return 0;
}

}
