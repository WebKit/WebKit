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
#include "khtml/rendering/render_object.h"

namespace KSVG {
    class SVGStyledElementImpl;
};

namespace KSVG {
    class KCanvasRenderingStyle;
}
class KCanvasContainer;
class KCanvasMatrix;
class RenderPath : public khtml::RenderObject
{
public:
    RenderPath(khtml::RenderStyle *style, KSVG::SVGStyledElementImpl *node);
    virtual ~RenderPath();

    // Hit-detection seperated for the fill and the stroke
    virtual bool fillContains(const QPoint &p) const;
    virtual bool strokeContains(const QPoint &p) const;

    // Returns an unscaled bounding box for this vector path
    virtual QRect bbox(bool includeStroke = true) const;

    // Drawing
    void setupForDraw() const;

    // Update
    void changePath(KCanvasUserData newPath);

    KCanvasUserData path() const;
    virtual void setStyle(khtml::RenderStyle *style);
    KSVG::KCanvasRenderingStyle *canvasStyle() const;

    virtual bool isRenderPath() const { return true; }
    virtual const char *renderName() const { return "KCanvasItem"; }
    
    virtual QMatrix localTransform() const;
    virtual void setLocalTransform(const QMatrix &matrix);
    
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
typedef Q3ValueList<const RenderPath *> KCanvasItemList;

#endif

// vim:ts=4:noet
