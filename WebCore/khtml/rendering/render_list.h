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
    RenderListMarker();
    ~RenderListMarker();

    virtual void setStyle(RenderStyle *style);

    virtual const char *renderName() const { return "RenderListMarker"; }
    // so the marker gets to layout itself. Only needed for
    // list-style-position: inside

    virtual void paint(QPainter *p, int x, int y, int w, int h,
                       int xoff, int yoff, PaintAction paintAction);
    virtual void paintObject(QPainter *p, int x, int y, int w, int h,
                             int xoff, int yoff, PaintAction paintAction);
    virtual void layout( );
    virtual void calcMinMaxWidth();

    virtual void setPixmap( const QPixmap &, const QRect&, CachedImage *);

    virtual void calcWidth();

    virtual short lineHeight(bool b) const;
    virtual short baselinePosition(bool b) const;
    
    virtual bool isListMarker() const { return true; }
    
    RenderListItem* listItem() { return m_listItem; }
    void setListItem(RenderListItem* listItem) { m_listItem = listItem; }
    
protected:
    friend class RenderListItem;
    
    bool isInside() const;

    QString m_item;
    CachedImage *m_listImage;
    long m_value;
    RenderListItem* m_listItem;
};

class RenderListItem : public RenderBlock
{
public:
    RenderListItem(DOM::NodeImpl*);
    virtual ~RenderListItem();
    
    virtual void detach(RenderArena *);

    virtual const char *renderName() const { return "RenderListItem"; }

    virtual void setStyle(RenderStyle *style);

    virtual bool isListItem() const { return true; }
    
    long value() const { return m_marker->m_value; }
    void setValue( long v ) { predefVal = v; }
    void calcListValue();
    
    virtual void paint(QPainter *p, int x, int y, int w, int h,
                       int xoff, int yoff, PaintAction paintAction);
    virtual void paintObject(QPainter *p, int x, int y, int w, int h,
                       int xoff, int yoff, PaintAction paintAction);

    virtual void layout( );
    virtual void calcMinMaxWidth();
    
    void updateMarkerLocation();
    
    void setNotInList(bool notInList) { _notInList = notInList; }
    bool notInList() const { return _notInList; }

protected:
    long int predefVal;
    RenderListMarker *m_marker;
    bool _notInList;
    bool _markerInstalledInParent;
};

}; //namespace

#endif
