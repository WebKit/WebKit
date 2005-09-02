/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
                  2005 Eric Seidel <eric.seidel@kdemail.net>

    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    aint with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#ifndef KCanvasItem_H
#define KCanvasItem_H

#include <qrect.h>
#include <qpoint.h>
#include <qrect.h>
#include <q3valuelist.h>

#include <kcanvas/KCanvasTypes.h>

class KCanvas;
class KRenderingStyle;
class KCanvasContainer;
class KCanvasItem
{
public:
    KCanvasItem(KCanvas *canvas, KRenderingStyle *style, KCanvasUserData path);
    virtual ~KCanvasItem();

    int zIndex() const;
    bool isVisible() const;

    // Hit-detection seperated for the fill and the stroke
    virtual bool fillContains(const QPoint &p) const;
    virtual bool strokeContains(const QPoint &p) const;

    // Returns an unscaled bounding box for this vector path
    virtual QRect bbox(bool includeStroke = true) const;

    // Drawing
    virtual void draw(const QRect &rect) const;

    // Update
    void changePath(KCanvasUserData newPath);

    KCanvas *canvas() const;
    KCanvasUserData path() const;
    KRenderingStyle *style() const;

    // Convenience function
    virtual void invalidate() const;

    // Returns pointer to a self-specified data structure
    // ie. to "link" a KCanvasItem to a KDOM::NodeImpl *
    KCanvasUserData userData() const;
    void setUserData(KCanvasUserData userData);

    // Container handling...
    virtual bool isContainer() const { return false; }
    
    virtual void appendItem(KCanvasItem *) { }
    virtual void insertItemBefore(KCanvasItem *, KCanvasItem *) { }
    virtual void removeItem(const KCanvasItem *) { }

    virtual bool raise();
    virtual bool lower();
    
    KCanvasContainer *parent() const;
    void setParent(KCanvasContainer *parent);

    KCanvasItem *prev() const;
    void setPrev(KCanvasItem *prev);

    KCanvasItem *next() const;
    void setNext(KCanvasItem *next);
    
protected:
    // restricted set of args for passing to paint servers, etc.
    const KCanvasCommonArgs commonArgs() const;
    virtual bool hitsPath(const QPoint &hitPoint, bool fill) const;
    virtual QRect bboxPath(bool includeStroke, bool includeTransforms = true) const;

private:
    class Private;
    Private *d;
};

// Helper data structure
typedef Q3ValueList<const KCanvasItem *> KCanvasItemList;

#endif

// vim:ts=4:noet
