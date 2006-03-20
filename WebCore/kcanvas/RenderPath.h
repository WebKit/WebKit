/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
                  2005 Eric Seidel <eric.seidel@kdemail.net>
                  2006 Apple Computer, Inc

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
#if SVG_SUPPORT

#include "IntRect.h"
#include "IntPoint.h"
#include "IntRect.h"
#include <q3valuelist.h>
#include "FloatRect.h"

#include "RenderObject.h"

namespace WebCore {

class FloatPoint;
class SVGStyledElement;

class KCanvasPath;
class KCanvasContainer;
class KCanvasMatrix;

class RenderPath : public RenderObject
{
public:
    RenderPath(RenderStyle *style, SVGStyledElement *node);
    virtual ~RenderPath();

    // Hit-detection seperated for the fill and the stroke
    virtual bool fillContains(const FloatPoint &p) const;
    virtual bool strokeContains(const FloatPoint &p) const;

    // Returns an unscaled bounding box (not even including localTransform()) for this vector path
    virtual FloatRect relativeBBox(bool includeStroke = true) const;

    void setPath(KCanvasPath* newPath);
    KCanvasPath* path() const;

    virtual bool isRenderPath() const { return true; }
    virtual const char *renderName() const { return "KCanvasItem"; }
    
    virtual QMatrix localTransform() const;
    virtual void setLocalTransform(const QMatrix &matrix);
    
    virtual void layout();
    virtual IntRect getAbsoluteRepaintRect();
    virtual bool requiresLayer();
    virtual short lineHeight(bool b, bool isRootLineBox = false) const;
    virtual short baselinePosition(bool b, bool isRootLineBox = false) const;
    virtual void paint(PaintInfo&, int parentX, int parentY);
    
    virtual bool nodeAtPoint(NodeInfo&, int x, int y, int tx, int ty, HitTestAction);

protected:
    virtual void drawMarkersIfNeeded(const FloatRect&, const KCanvasPath*) const = 0;

private:
    FloatRect strokeBBox() const;
    FloatPoint mapAbsolutePointToLocal(const FloatPoint& point) const;

    class Private;
    Private *d;
};

// Helper data structure
typedef Q3ValueList<const RenderPath *> KCanvasItemList;

}

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
