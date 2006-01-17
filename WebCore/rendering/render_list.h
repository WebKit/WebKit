/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
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
#ifndef RENDER_LIST_H
#define RENDER_LIST_H

#include "render_block.h"

// ### list-style-position, list-style-image is still missing

namespace khtml
{

class RenderListItem;

/* used to render the lists marker.
     This class always has to be a direct child of a RenderListItem!
*/
class RenderListMarker : public RenderBox
{
public:
    RenderListMarker(DOM::DocumentImpl* document);
    ~RenderListMarker();

    virtual void setStyle(RenderStyle *style);

    virtual const char *renderName() const { return "RenderListMarker"; }
    // so the marker gets to layout itself. Only needed for
    // list-style-position: inside

    virtual void paint(PaintInfo& i, int xoff, int yoff);
    virtual void layout( );
    virtual void calcMinMaxWidth();

    virtual void setPixmap( const QPixmap &, const IntRect&, CachedImage *);

    virtual void calcWidth();

    virtual InlineBox* createInlineBox(bool, bool, bool);

    virtual short lineHeight(bool b, bool isRootLineBox=false) const;
    virtual short baselinePosition(bool b, bool isRootLineBox=false) const;
    
    virtual bool isListMarker() const { return true; }

    CachedImage* listImage() const { return m_listImage; }
    
    RenderListItem* listItem() { return m_listItem; }
    void setListItem(RenderListItem* listItem) { m_listItem = listItem; }
    
    const QString& text() const { return m_item; }

protected:
    friend class RenderListItem;
    
    bool isInside() const;

    QString m_item;
    CachedImage *m_listImage;
    QPixmap m_listPixmap;
    int m_value;
    RenderListItem* m_listItem;
};

class ListMarkerBox : public InlineBox
{
public:
    ListMarkerBox(RenderObject* obj) :InlineBox(obj) {}
    virtual bool isText() const { return !static_cast<RenderListMarker*>(object())->listImage(); }
};

class RenderListItem : public RenderBlock
{
public:
    RenderListItem(DOM::NodeImpl*);
    virtual ~RenderListItem();
    
    virtual void destroy();

    virtual const char *renderName() const { return "RenderListItem"; }

    virtual void setStyle(RenderStyle *style);

    virtual bool isListItem() const { return true; }
    
    int value() const { return m_marker->m_value; }
    void setValue( int v ) { predefVal = v; }
    void calcListValue();
    
    virtual void paint(PaintInfo& i, int xoff, int yoff);

    virtual void layout( );
    virtual void calcMinMaxWidth();

    virtual IntRect getAbsoluteRepaintRect();
    
    void updateMarkerLocation();
    
    void setNotInList(bool notInList) { _notInList = notInList; }
    bool notInList() const { return _notInList; }

    QString markerStringValue() { if (m_marker) return m_marker->m_item; return ""; }

protected:
    int predefVal;
    RenderListMarker *m_marker;
    bool _notInList;
};

}; //namespace

#endif
