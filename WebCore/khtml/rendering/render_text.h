/*
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003 Apple Computer, Inc.
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

class QPainter;
class QFontMetrics;

namespace khtml
{
    class RenderText;
    class RenderStyle;

class TextRun : public InlineBox
{
public:
    TextRun(RenderObject* obj)
    :InlineBox(obj)
    {
        m_start = 0;
        m_len = 0;
        m_reversed = false;
        m_toAdd = 0;
    }
    
    void detach(RenderArena* renderArena);
    
    // Overloaded new operator.  Derived classes must override operator new
    // in order to allocate out of the RenderArena.
    void* operator new(size_t sz, RenderArena* renderArena) throw();    

    // Overridden to prevent the normal delete from being called.
    void operator delete(void* ptr, size_t sz);
        
private:
    // The normal operator new is disallowed.
    void* operator new(size_t sz) throw();

public:
    void setSpaceAdd(int add) { m_width -= m_toAdd; m_toAdd = add; m_width += m_toAdd; }
    int spaceAdd() { return m_toAdd; }

    virtual bool isTextRun() { return true; }
    
    void paintDecoration( QPainter *pt, int _tx, int _ty, int decoration);
    void paintSelection(const Font *f, RenderText *text, QPainter *p, RenderStyle* style, int tx, int ty, int startPos, int endPos);

    // Return before, after (offset set to max), or inside the text, at @p offset
    FindSelectionResult checkSelectionPoint(int _x, int _y, int _tx, int _ty, const Font *f, RenderText *text, int & offset, short lineheight);

    /**
     * if this text run was rendered @ref _ty pixels below the upper edge
     * of a view, would the @ref _y -coordinate be inside the vertical range
     * of this object's representation?
     */
    bool checkVerticalPoint(int _y, int _ty, int _h)
    { if((_ty + m_y > _y + _h) || (_ty + m_y + m_baseline + height() < _y)) return false; return true; }

    int m_start;
    unsigned short m_len;
    
    bool m_reversed : 1;
    int m_toAdd : 14; // for justified text
private:
    // this is just for QVector::bsearch. Don't use it otherwise
    TextRun(int _x, int _y)
        :InlineBox(0)
    {
        m_x = _x;
        m_y = _y;
        m_reversed = false;
    };
    friend class RenderText;
};

class TextRunArray : public QPtrVector<TextRun>
{
public:
    TextRunArray();

    TextRun* first();

    int	  findFirstMatching( Item ) const;
    virtual int compareItems( Item, Item );
};

class RenderText : public RenderObject
{
    friend class TextRun;

public:
    RenderText(DOM::NodeImpl* node, DOM::DOMStringImpl *_str);
    virtual ~RenderText();

    virtual const char *renderName() const { return "RenderText"; }

    virtual void setStyle(RenderStyle *style);

    virtual void paint(QPainter *, int x, int y, int w, int h,
                       int tx, int ty, PaintAction paintAction);
    virtual void paintObject(QPainter *, int x, int y, int w, int h,
                             int tx, int ty, PaintAction paintAction);

    void deleteRuns(RenderArena *renderArena = 0);
    virtual void detach(RenderArena* renderArena);
    
    DOM::DOMString data() const { return str; }
    DOM::DOMStringImpl *string() const { return str; }

    virtual InlineBox* createInlineBox(bool,bool);
    
    virtual void layout() {assert(false);}

    virtual bool nodeAtPoint(NodeInfo& info, int x, int y, int tx, int ty, bool inside = false);

    // Return before, after (offset set to max), or inside the text, at @p offset
    virtual FindSelectionResult checkSelectionPointIgnoringContinuations
        (int _x, int _y, int _tx, int _ty, DOM::NodeImpl*& node, int & offset);

    unsigned int length() const { return str->l; }
    QChar *text() const { return str->s; }
    unsigned int stringLength() const { return str->l; } // non virtual implementation of length()
    virtual void position(InlineBox* box, int from, int len, bool reverse);

    virtual unsigned int width(unsigned int from, unsigned int len, const Font *f) const;
    virtual unsigned int width(unsigned int from, unsigned int len, bool firstLine = false) const;
    virtual short width() const;
    virtual int height() const;

    // height of the contents (without paddings, margins and borders)
    virtual short lineHeight( bool firstLine, bool isRootLineBox=false ) const;
    virtual short baselinePosition( bool firstLine, bool isRootLineBox=false ) const;

    // overrides
    virtual void calcMinMaxWidth();
    virtual short minWidth() const { return m_minWidth; }
    virtual short maxWidth() const { return m_maxWidth; }
    virtual void trimmedMinMaxWidth(short& beginMinW, bool& beginWS, 
                                    short& endMinW, bool& endWS,
                                    bool& hasBreakableChar, bool& hasBreak,
                                    short& beginMaxW, short& endMaxW,
                                    short& minW, short& maxW, bool& stripFrontSpaces);
    
    bool containsOnlyWhitespace(unsigned int from, unsigned int len) const;
    
    // returns the minimum x position of all runs relative to the parent.
    // defaults to 0.
    int minXPos() const;

    virtual int xPos() const;
    virtual int yPos() const;

    virtual const QFont &font();
    virtual short verticalPositionHint( bool firstLine ) const;

    bool isFixedWidthFont() const;

    void setText(DOM::DOMStringImpl *text, bool force=false);

    virtual SelectionState selectionState() const {return m_selectionState;}
    virtual void setSelectionState(SelectionState s) {m_selectionState = s; }
    virtual void cursorPos(int offset, int &_x, int &_y, int &height);
    virtual bool absolutePosition(int &/*xPos*/, int &/*yPos*/, bool f = false);
    void posOfChar(int ch, int &x, int &y);

    virtual short marginLeft() const { return style()->marginLeft().minWidth(0); }
    virtual short marginRight() const { return style()->marginRight().minWidth(0); }

    virtual int rightmostPosition() const;

    virtual void repaint(bool immediate=false);

    const QFontMetrics &metrics(bool firstLine) const;
    const Font *htmlFont(bool firstLine) const;

    DOM::TextImpl *element() const
    { return static_cast<DOM::TextImpl*>(RenderObject::element()); }

#if APPLE_CHANGES
    TextRunArray textRuns() const { return m_lines; }
    int widthFromCache(const Font *, int start, int len) const;
    bool shouldUseMonospaceCache(const Font *) const;
    void cacheWidths();
    bool allAscii() const;
#endif

protected:
    void paintTextOutline(QPainter *p, int tx, int ty, const QRect &prevLine, const QRect &thisLine, const QRect &nextLine);

#if APPLE_CHANGES
public:
#endif
    TextRun * findTextRun( int offset, int &pos );

protected: // members
    TextRunArray m_lines;
    DOM::DOMStringImpl *str; //

    short m_lineHeight;
    short m_minWidth;
    short m_maxWidth;
    short m_beginMinWidth;
    short m_endMinWidth;
    
    SelectionState m_selectionState : 3 ;
    bool m_hasBreakableChar : 1; // Whether or not we can be broken into multiple lines.
    bool m_hasBreak : 1; // Whether or not we have a hard break (e.g., <pre> with '\n').
    bool m_hasBeginWS : 1; // Whether or not we begin with WS (only true if we aren't pre)
    bool m_hasEndWS : 1; // Whether or not we end with WS (only true if we aren't pre)
    
    // 19 bits left
#if APPLE_CHANGES
    mutable bool m_allAsciiChecked:1;
    mutable bool m_allAscii:1;
    int m_monospaceCharacterWidth;
#endif
};


};
#endif
