/*
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Dirk Mueller (mueller@kde.org)
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

#include <qptrvector.h>
#include <assert.h>

class QPainter;
class QFontMetrics;

namespace khtml
{
    class RenderText;
    class RenderStyle;

class TextSlave
{
public:
    TextSlave(int x, int y, int start, int len,
	      int baseline, int width,
              bool reversed = false, int toAdd = 0, bool firstLine = false)
    {
        m_x = x;
        m_y = y;
        m_start = start;
        m_len = len;
        m_baseline = baseline;
        m_width = width;
        m_reversed = reversed;
        m_firstLine = firstLine;
        m_toAdd = toAdd;
    }
    void printDecoration( QPainter *pt, RenderText* p, int _tx, int _ty, int decoration, bool begin, bool end);
    void printBoxDecorations(QPainter *p, RenderStyle* style, RenderText *parent, int _tx, int _ty, bool begin, bool end);
    void printSelection(const Font *f, RenderText *text, QPainter *p, RenderStyle* style, int tx, int ty, int startPos, int endPos);

    // Return before, after (offset set to max), or inside the text, at @p offset
    FindSelectionResult checkSelectionPoint(int _x, int _y, int _tx, int _ty, const Font *f, RenderText *text, int & offset, short lineheight);

    /**
     * if this textslave was rendered @ref _ty pixels below the upper edge
     * of a view, would the @ref _y -coordinate be inside the vertical range
     * of this object's representation?
     */
    bool checkVerticalPoint(int _y, int _ty, int _h, int height)
    { if((_ty + m_y > _y + _h) || (_ty + m_y + m_baseline + height < _y)) return false; return true; }

    int m_start;
    int m_y;
    unsigned short m_len;
    short m_x;
    unsigned short m_baseline;
    unsigned short m_width;

    bool m_reversed : 1;
    bool m_firstLine : 1;
    int m_toAdd : 14; // for justified text
private:
    // this is just for QVector::bsearch. Don't use it otherwise
    TextSlave(int _x, int _y)
    {
        m_x = _x;
        m_y = _y;
        m_reversed = false;
    };
    friend class RenderText;
};

class TextSlaveArray : public QPtrVector<TextSlave>
{
public:
    TextSlaveArray();

    TextSlave* first();

    int	  findFirstMatching( Item ) const;
    virtual int compareItems( Item, Item );
};

class RenderText : public RenderObject
{
    friend class TextSlave;

public:
    RenderText(DOM::NodeImpl* node, DOM::DOMStringImpl *_str);
    virtual ~RenderText();

    virtual const char *renderName() const { return "RenderText"; }

    virtual void setStyle(RenderStyle *style);

    virtual bool isRendered() const { return true; }

    virtual void print( QPainter *, int x, int y, int w, int h,
                        int tx, int ty);
    virtual void printObject( QPainter *, int x, int y, int w, int h,
                        int tx, int ty);

    void deleteSlaves();

    DOM::DOMString data() const { return str; }
    DOM::DOMStringImpl *string() const { return str; }

    virtual void layout() {assert(false);}

    virtual bool nodeAtPoint(NodeInfo& info, int x, int y, int tx, int ty);

    // Return before, after (offset set to max), or inside the text, at @p offset
    virtual FindSelectionResult checkSelectionPoint( int _x, int _y, int _tx, int _ty,
                                                     DOM::NodeImpl*& node, int & offset );

    unsigned int length() const { return str->l; }
    QChar *text() const { return str->s; }
    unsigned int stringLength() const { return str->l; } // non virtual implementation of length()
    virtual void position(int x, int y, int from, int len, int width, bool reverse, bool firstLine, int spaceAdd);

    virtual unsigned int width(unsigned int from, unsigned int len, const Font *f) const;
    virtual unsigned int width(unsigned int from, unsigned int len, bool firstLine = false) const;
    virtual short width() const;
    virtual int height() const;

    // height of the contents (without paddings, margins and borders)
    virtual short lineHeight( bool firstLine ) const;
    virtual short baselinePosition( bool firstLine ) const;

    // overrides
    virtual void calcMinMaxWidth();
    virtual short minWidth() const { return m_minWidth; }
    virtual short maxWidth() const { return m_maxWidth; }

    // returns the minimum x position of all slaves relative to the parent.
    // defaults to 0.
    int minXPos() const;

    virtual int xPos() const;
    virtual int yPos() const;

    bool hasReturn() const { return m_hasReturn; }

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

    virtual void repaint();

    bool hasBreakableChar() const { return m_hasBreakableChar; }
    const QFontMetrics &metrics(bool firstLine) const;
    const Font *htmlFont(bool firstLine) const;

    DOM::TextImpl *element() const
    { return static_cast<DOM::TextImpl*>(RenderObject::element()); }

protected:
    void printTextOutline(QPainter *p, int tx, int ty, const QRect &prevLine, const QRect &thisLine, const QRect &nextLine);

    TextSlave * findTextSlave( int offset, int &pos );

protected: // members
    TextSlaveArray m_lines;
    DOM::DOMStringImpl *str; //

    short m_lineHeight;
    short m_minWidth;
    short m_maxWidth;

    SelectionState m_selectionState : 3 ;
    bool m_hasReturn : 1;
    bool m_hasBreakableChar : 1;

    // 19 bits left
};


};
#endif
