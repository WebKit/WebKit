/*
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
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

#ifndef KHTML_RenderText_H
#define KHTML_RenderText_H

#include "RenderObject.h"
#include "Text.h"

typedef void UBreakIterator;

namespace WebCore {

// Define a constant for soft hyphen's unicode value.
const unsigned short SOFT_HYPHEN = 173;

class DocumentMarker;
class InlineBox;
class Position;
class String;
class StringImpl;

class RenderText : public RenderObject {
    friend class InlineTextBox;

public:
    RenderText(Node*, StringImpl*);

    virtual bool isTextFragment() const;
    virtual PassRefPtr<StringImpl> originalString() const;
    
    virtual const char *renderName() const { return "RenderText"; }

    virtual void setStyle(RenderStyle *style);

    void extractTextBox(InlineTextBox* textBox);
    void attachTextBox(InlineTextBox* textBox);
    void removeTextBox(InlineTextBox* textBox);
    void deleteTextBoxes();
    virtual void destroy();
    
    String data() const { return str.get(); }
    StringImpl* string() const { return str.get(); }

    virtual InlineBox* createInlineBox(bool,bool, bool isOnlyRun = false);
    virtual void dirtyLineBoxes(bool fullLayout, bool isRootInlineBox = false);
    
    virtual void paint(PaintInfo& i, int tx, int ty) { assert(false); }
    virtual void layout() { assert(false); }

    virtual bool nodeAtPoint(NodeInfo& info, int x, int y, int tx, int ty,
                             HitTestAction hitTestAction) { assert(false); return false; }

    virtual void absoluteRects(DeprecatedValueList<IntRect>& rects, int _tx, int _ty);
    virtual DeprecatedValueList<IntRect> RenderText::lineBoxRects();

    virtual VisiblePosition positionForCoordinates(int x, int y);

    unsigned int length() const { return str->length(); }
    const QChar* text() const { return str->unicode(); }
    unsigned int stringLength() const { return str->length(); } // non virtual implementation of length()
    virtual void position(InlineBox* box, int from, int len, bool reverse, bool override);

    virtual unsigned int width(unsigned int from, unsigned int len, const Font *f, int xpos) const;
    virtual unsigned int width(unsigned int from, unsigned int len, int xpos, bool firstLine = false) const;
    virtual int width() const;
    virtual int height() const;

    virtual short lineHeight(bool firstLine, bool isRootLineBox = false) const;

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

    virtual const Font& font();
    virtual short verticalPositionHint( bool firstLine ) const;

    void setText(StringImpl*, bool force = false);
    void setTextWithOffset(StringImpl*, unsigned offset, unsigned len, bool force = false);

    virtual bool canBeSelectionLeaf() const { return true; }
    virtual SelectionState selectionState() const { return m_selectionState; }
    virtual void setSelectionState(SelectionState s);
    virtual IntRect selectionRect();
    virtual IntRect caretRect(int offset, EAffinity affinity, int *extraWidthToEndOfLine = 0);
    void posOfChar(int ch, int &x, int &y);

    virtual int marginLeft() const { return style()->marginLeft().calcMinValue(0); }
    virtual int marginRight() const { return style()->marginRight().calcMinValue(0); }

    virtual IntRect getAbsoluteRepaintRect();

    const Font* font(bool firstLine) const;

    Text *element() const { return static_cast<Text*>(RenderObject::element()); }

    InlineTextBox* firstTextBox() const { return m_firstTextBox; }
    InlineTextBox* lastTextBox() const { return m_lastTextBox; }
    
    virtual InlineBox *inlineBox(int offset, EAffinity affinity = UPSTREAM);

    int widthFromCache(const Font*, int start, int len, int tabWidth, int xpos) const;
    bool shouldUseMonospaceCache(const Font *) const;
    void cacheWidths();
    bool allAscii() const;

    virtual int caretMinOffset() const;
    virtual int caretMaxOffset() const;
    virtual unsigned caretMaxRenderedOffset() const;

    virtual int previousOffset (int current) const;
    virtual int nextOffset (int current) const;
    
    bool atLineWrap(InlineTextBox *box, int offset);
    bool containsReversedText() { return m_containsReversedText; }
    
public:
    InlineTextBox * findNextInlineTextBox( int offset, int &pos ) const;

protected: // members
    RefPtr<StringImpl> str;
    
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
    mutable bool m_allAsciiChecked:1;
    mutable bool m_allAscii:1;
    int m_monospaceCharacterWidth;
};

UBreakIterator* characterBreakIterator(const StringImpl*);

}

#endif
