/*
    Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
                  2005 Eric Seidel <eric@webkit.org>
                  2006 Apple Computer, Inc
                  2009 Google, Inc.

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
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef RenderPath_h
#define RenderPath_h

#if ENABLE(SVG)

#include "FloatRect.h"
#include "RenderSVGModelObject.h"
#include "TransformationMatrix.h"

namespace WebCore {

class FloatPoint;
class RenderSVGContainer;
class SVGStyledTransformableElement;

class RenderPath : public RenderSVGModelObject {
public:
    RenderPath(SVGStyledTransformableElement*);

    // Hit-detection seperated for the fill and the stroke
    bool fillContains(const FloatPoint&, bool requiresFill = true) const;
    bool strokeContains(const FloatPoint&, bool requiresStroke = true) const;

    virtual FloatRect objectBoundingBox() const;
    virtual FloatRect repaintRectInLocalCoordinates() const;

    virtual TransformationMatrix localToParentTransform() const;

    const Path& path() const;
    void setPath(const Path&);

    virtual bool isRenderPath() const { return true; }
    virtual const char* renderName() const { return "RenderPath"; }

    virtual void layout();
    virtual void paint(PaintInfo&, int parentX, int parentY);
    virtual void addFocusRingRects(GraphicsContext*, int tx, int ty);

    virtual bool nodeAtFloatPoint(const HitTestRequest&, HitTestResult&, const FloatPoint& pointInParent, HitTestAction);

    FloatRect drawMarkersIfNeeded(GraphicsContext*, const FloatRect&, const Path&) const;

private:
    virtual TransformationMatrix localTransform() const;

    mutable Path m_path;
    mutable FloatRect m_cachedLocalFillBBox;
    mutable FloatRect m_cachedLocalRepaintRect;
    FloatRect m_markerBounds;
    TransformationMatrix m_localTransform;
};

}

#endif // ENABLE(SVG)
#endif

// vim:ts=4:noet
