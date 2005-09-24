/*
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004 Apple Computer, Inc.
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
#ifndef RENDERTEXT_H
#define RENDERTEXT_H

#include "dom/dom_string.h"
#include "xml/dom_stringimpl.h"
#include "xml/dom_textimpl.h"
#include "rendering/render_object.h"
#include "rendering/render_flow.h"

#include <qptrvector.h>
#include <assert.h>
#include "KWQKHTMLPart.h"

class QPainter;
class QFontMetrics;

namespace DOM {
    class Position;
    class DocumentMarker;
};

// Define a constant for soft hyphen's unicode value.
#define SOFT_HYPHEN 173

const int cNoTruncation = -1;
const int cFullTruncation = -2;

namespace khtml
{
    class RenderText;
    class RenderStyle;

class InlineTextBox : public InlineRunBox
{
public:
    InlineTextBox(RenderObject* obj)
    :InlineRunBox(obj)
    {
        m_start = 0;
        m_len = 0;
        m_reversed = false;
        m_treatAsText = true;
        m_toAdd = 0;
        m_truncation = cNoTruncation;
    }
    
    InlineTextBox* nextTextBox() const { return static_cast<InlineTextBox*>(nextLineBox()); }
    InlineTextBox* prevTextBox() const { return static_cast<InlineTextBox*>(prevLineBox()); }
    
    uint start() const { return m_start; }
    uint end() const { return m_len ? m_start+m_len-1 : m_start; }
    uint len() const { return m_len; }
  
    void setStart(uint start) { m_start = start; }
    void setLen(uint len) { m_len = len; }

    void offsetRun(int d) { m_start += d; }

    void detach(RenderArena* arena);
    
    QRect selectionRect(int absx, int absy, int startPos, int endPos);
    bool isSelected(int startPos, int endPos) const;
    void selectionStartEnd(int& sPos, int& ePos);
    
    virtual void paint(RenderObject::PaintInfo& i, int tx, int ty);
    virtual bool nodeAtPoint(RenderObject::NodeInfo& i, int x, int y, int tx, int ty);

    RenderText* textObject() const;
    
    virtual void deleteLine(RenderArena* arena);
    virtual void extractLine();
    virtual void attachLine();

    virtual RenderObject::SelectionState selectionState();

    virtual void clearTruncation() { m_truncation = cNoTruncation; }
    virtual int placeEllipsisBox(bool ltr, int blockEdge, int ellipsisWidth, bool& foundBox);

    // Overloaded new operator.  Derived classes must override operator new
    // in order to allocate out of the RenderArena.
    void* operator new(size_t sz, RenderArena* renderArena) throw();    

    // Overridden to prevent the normal delete from being called.
    void operator delete(void* ptr, size_t sz);
    
    // Current used only for sorting, == could be made more robust if needed
    bool operator ==(const InlineTextBox &b) { return m_start == b.m_start; }
    bool operator <(const InlineTextBox &b) { return m_start < b.m_start; }
        
private:
    // The normal operator new is disallowed.
    void* operator new(size_t sz) throw();

public:
    void setSpaceAdd(int add) { m_width -= m_toAdd; m_toAdd = add; m_width += m_toAdd; }
    int spaceAdd() { return m_toAdd; }

    virtual bool isInlineTextBox() { return true; }    
    virtual bool isText() const { return m_treatAsText; }
    void setIsText(bool b) { m_treatAsText = b; }
    
    void paintDecoration(QPainter* p, int _tx, int _ty, int decoration);
    void paintSelection(QPainter* p, int tx, int ty, RenderStyle* style, const Font* font);
    void paintMarkedTextBackground(QPainter* p, int tx, int ty, RenderStyle* style, const Font* font, int startPos, int endPos);
    void paintMarker(QPainter* p, int _tx, int _ty, DOM::DocumentMarker marker);
    void paintMarkedTextUnderline(QPainter *pt, int _tx, int _ty, KWQKHTMLPart::MarkedTextUnderline underline);
    virtual int caretMinOffset() const;
    virtual int caretMaxOffset() const;
    virtual unsigned caretMaxRenderedOffset() const;
    
    int textPos() const;
    int offsetForPosition(int _x, bool includePartialGlyphs = true) const;
    int positionForOffset(int offset) const;
    
    /**
     * if this text run was rendered @ref _ty pixels below the upper edge
     * of a view, would the @ref _y -coordinate be inside the vertical range
     * of this object's representation?
     */
    bool checkVerticalPoint(int _y, int _ty, int _h);

    int m_start;
    unsigned short m_len;
    
    int m_truncation; // Where to truncate when text overflow is applied.  We use special constants to
                      // denote no truncation (the whole run paints) and full truncation (nothing paints at all).

    bool m_reversed : 1;
    bool m_dirOverride : 1;
    bool m_treatAsText : 1; // Whether or not to treat a <br> as text for the purposes of line height.
    int m_toAdd : 13; // for justified text

private:
    friend class RenderText;
};

class RenderText : public RenderObject
{
    friend class InlineTextBox;

public:
    RenderText(DOM::NodeImpl* node, DOM::DOMStringImpl *_str);
    virtual ~RenderText();

    virtual bool isTextFragment() const;
    virtual SharedPtr<DOM::DOMStringImpl> originalString() const;
    
    virtual const char *renderName() const { return "RenderText"; }

    virtual void setStyle(RenderStyle *style);

    void extractTextBox(InlineTextBox* textBox);
    void attachTextBox(InlineTextBox* textBox);
    void removeTextBox(InlineTextBox* textBox);
    void deleteTextBoxes();
    virtual void detach();
    
    DOM::DOMString data() const { return str; }
    DOM::DOMStringImpl *string() const { return str; }

    virtual InlineBox* createInlineBox(bool,bool, bool isOnlyRun = false);
    virtual void dirtyLineBoxes(bool fullLayout, bool isRootInlineBox = false);
    
    virtual void paint(PaintInfo& i, int tx, int ty) { assert(false); }
    virtual void layout() { assert(false); }

    virtual bool nodeAtPoint(NodeInfo& info, int x, int y, int tx, int ty,
                             HitTestAction hitTestAction) { assert(false); return false; }

    virtual void absoluteRects(QValueList<QRect>& rects, int _tx, int _ty);

    virtual VisiblePosition positionForCoordinates(int x, int y);

    unsigned int length() const { return str->l; }
    QChar *text() const { return str->s; }
    unsigned int stringLength() const { return str->l; } // non virtual implementation of length()
    virtual void position(InlineBox* box, int from, int len, bool reverse, bool override);

    virtual unsigned int width(unsigned int from, unsigned int len, const Font *f, int xpos) const;
    virtual unsigned int width(unsigned int from, unsigned int len, int xpos, bool firstLine = false) const;
    virtual int width() const;
    virtual int height() const;

    // height of the contents (without paddings, margins and borders)
    virtual short lineHeight( bool firstLine, bool isRootLineBox=false ) const;
    virtual short baselinePosition( bool firstLine, bool isRootLineBox=false ) const;

    // overrides
    virtual void calcMinMaxWidth();
    virtual int minWidth() const { return m_minWidth; }
    virtual int maxWidth() const { return m_maxWidth; }

    // widths
    void calcMinMaxWidth(int leadWidth);
    virtual void trimmedMinMaxWidth(int leadWidth,
                                    int& beginMinW, bool& beginWS, 
                                    int& endMinW, bool& endWS,
                                    bool& hasBreakableChar, bool& hasBreak,
                                    int& beginMaxW, int& endMaxW,
                                    int& minW, int& maxW, bool& stripFrontSpaces);
    
    bool containsOnlyWhitespace(unsigned int from, unsigned int len) const;
    
    // returns the minimum x position of all runs relative to the parent.
    // defaults to 0.
    int minXPos() const;

    virtual int xPos() const;
    virtual int yPos() const;

    virtual const QFont &font();
    virtual short verticalPositionHint( bool firstLine ) const;

    void setText(DOM::DOMStringImpl *text, bool force=false);
    void setTextWithOffset(DOM::DOMStringImpl *text, uint offset, uint len, bool force=false);

    virtual bool canBeSelectionLeaf() const { return true; }
    virtual SelectionState selectionState() const {return m_selectionState;}
    virtual void setSelectionState(SelectionState s);
    virtual QRect selectionRect();
    virtual QRect caretRect(int offset, EAffinity affinity, int *extraWidthToEndOfLine = 0);
    void posOfChar(int ch, int &x, int &y);

    virtual int marginLeft() const { return style()->marginLeft().minWidth(0); }
    virtual int marginRight() const { return style()->marginRight().minWidth(0); }

    virtual QRect getAbsoluteRepaintRect();

    const QFontMetrics &metrics(bool firstLine) const;
    const Font *htmlFont(bool firstLine) const;

    DOM::TextImpl *element() const
    { return static_cast<DOM::TextImpl*>(RenderObject::element()); }

    InlineTextBox* firstTextBox() const { return m_firstTextBox; }
    InlineTextBox* lastTextBox() const { return m_lastTextBox; }
    
    virtual InlineBox *inlineBox(int offset, EAffinity affinity = UPSTREAM);

#if APPLE_CHANGES
    int widthFromCache(const Font *, int start, int len, int tabWidth, int xpos) const;
    bool shouldUseMonospaceCache(const Font *) const;
    void cacheWidths();
    bool allAscii() const;
#endif

    virtual int caretMinOffset() const;
    virtual int caretMaxOffset() const;
    virtual unsigned caretMaxRenderedOffset() const;

    virtual int previousOffset (int current) const;
    virtual int nextOffset (int current) const;
    
    bool containsReversedText() { return m_containsReversedText; }
    
#if APPLE_CHANGES
public:
#endif
    InlineTextBox * findNextInlineTextBox( int offset, int &pos ) const;

protected: // members
    DOM::DOMStringImpl *str;
    
    InlineTextBox* m_firstTextBox;
    InlineTextBox* m_lastTextBox;
    
    int m_minWidth;
    int m_maxWidth;
    int m_beginMinWidth;
    int m_endMinWidth;
    
    SelectionState m_selectionState : 3 ;
    bool m_hasBreakableChar : 1; // Whether or not we can be broken into multiple lines.
    bool m_hasBreak : 1; // Whether or not we have a hard break (e.g., <pre> with '\n').
    bool m_hasTab : 1; // Whether or not we have a variable width tab character (e.g., <pre> with '\t').
    bool m_hasBeginWS : 1; // Whether or not we begin with WS (only true if we aren't pre)
    bool m_hasEndWS : 1; // Whether or not we end with WS (only true if we aren't pre)
    
    bool m_linesDirty : 1; // This bit indicates that the text run has already dirtied specific
                           // line boxes, and this hint will enable layoutInlineChildren to avoid
                           // just dirtying everything when character data is modified (e.g., appended/inserted
                           // or removed).
    bool m_containsReversedText : 1;

    // 22 bits left
#if APPLE_CHANGES
    mutable bool m_allAsciiChecked:1;
    mutable bool m_allAscii:1;
    int m_monospaceCharacterWidth;
#endif
};

// Used to represent a text substring of an element, e.g., for text runs that are split because of
// first letter and that must therefore have different styles (and positions in the render tree).
// We cache offsets so that text transformations can be applied in such a way that we can recover
// the original unaltered string from our corresponding DOM node.
class RenderTextFragment : public RenderText
{
public:
    RenderTextFragment(DOM::NodeImpl* _node, DOM::DOMStringImpl* _str,
                       int startOffset, int length);
    RenderTextFragment(DOM::NodeImpl* _node, DOM::DOMStringImpl* _str);
    ~RenderTextFragment();
    
    virtual bool isTextFragment() const;
    
    uint start() const { return m_start; }
    uint end() const { return m_end; }
    
    DOM::DOMStringImpl* contentString() const { return m_generatedContentStr; }
    virtual SharedPtr<DOM::DOMStringImpl> originalString() const;
    
private:
    uint m_start;
    uint m_end;
    DOM::DOMStringImpl* m_generatedContentStr;
};

inline RenderText *InlineTextBox::textObject() const
{
    return static_cast<RenderText *>(m_object);
}

}

#endif
