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
#if SVG_SUPPORT

#include "IntRect.h"
#include "IntPoint.h"
#include "IntRect.h"
#include <q3valuelist.h>
#include "FloatRect.h"

#include "render_object.h"

namespace WebCore {
    class FloatPoint;
}

namespace KSVG {
    class SVGStyledElementImpl;
};

class KCanvasPath;
class KCanvasContainer;
class KCanvasMatrix;
class RenderPath : public khtml::RenderObject
{
public:
    RenderPath(khtml::RenderStyle *style, KSVG::SVGStyledElementImpl *node);
    virtual ~RenderPath();

    // Hit-detection seperated for the fill and the stroke
    virtual bool fillContains(const WebCore::FloatPoint &p) const;
    virtual bool strokeContains(const WebCore::FloatPoint &p) const;

    // Returns an unscaled bounding box (not even including localTransform()) for this vector path
    virtual FloatRect relativeBBox(bool includeStroke = true) const;

    void changePath(KCanvasPath* newPath);
    KCanvasPath* path() const;

    virtual bool isRenderPath() const { return true; }
    virtual const char *renderName() const { return "KCanvasItem"; }
    
    virtual QMatrix localTransform() const;
    virtual void setLocalTransform(const QMatrix &matrix);
    
protected:
    // restricted set of args for passing to paint servers, etc.
    virtual bool hitsPath(const WebCore::FloatPoint &hitPoint, bool fill) const = 0;
    virtual FloatRect bboxForPath(bool includeStroke) const = 0;

private:
    class Private;
    Private *d;
};

// Helper data structure
typedef Q3ValueList<const RenderPath *> KCanvasItemList;

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
