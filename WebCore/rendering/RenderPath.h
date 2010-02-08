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
#include "AffineTransform.h"
#include "FloatRect.h"
#include "RenderSVGModelObject.h"
#include "SVGMarkerLayoutInfo.h"

namespace WebCore {

class FloatPoint;
class RenderSVGContainer;
class SVGStyledTransformableElement;

class RenderPath : public RenderSVGModelObject {
public:
    RenderPath(SVGStyledTransformableElement*);

    const Path& path() const { return m_path; }

private:
    // Hit-detection seperated for the fill and the stroke
    bool fillContains(const FloatPoint&, bool requiresFill = true) const;
    bool strokeContains(const FloatPoint&, bool requiresStroke = true) const;

    virtual FloatRect objectBoundingBox() const;
    virtual FloatRect strokeBoundingBox() const;
    virtual FloatRect markerBoundingBox() const;
    virtual FloatRect repaintRectInLocalCoordinates() const;

    virtual const AffineTransform& localToParentTransform() const;

    void setPath(const Path&);

    virtual bool isRenderPath() const { return true; }
    virtual const char* renderName() const { return "RenderPath"; }

    virtual void layout();
    virtual void paint(PaintInfo&, int parentX, int parentY);
    virtual void addFocusRingRects(Vector<IntRect>&, int tx, int ty);

    virtual bool nodeAtFloatPoint(const HitTestRequest&, HitTestResult&, const FloatPoint& pointInParent, HitTestAction);

    void calculateMarkerBoundsIfNeeded() const;

private:
    virtual AffineTransform localTransform() const;

    mutable Path m_path;
    mutable FloatRect m_cachedLocalFillBBox;
    mutable FloatRect m_cachedLocalStrokeBBox;
    mutable FloatRect m_cachedLocalRepaintRect;
    mutable FloatRect m_cachedLocalMarkerBBox;
    mutable SVGMarkerLayoutInfo m_markerLayoutInfo;
    AffineTransform m_localTransform;
};

inline RenderPath* toRenderPath(RenderObject* object)
{
    ASSERT(!object || object->isRenderPath());
    return static_cast<RenderPath*>(object);
}

inline const RenderPath* toRenderPath(const RenderObject* object)
{
    ASSERT(!object || object->isRenderPath());
    return static_cast<const RenderPath*>(object);
}

// This will catch anyone doing an unnecessary cast.
void toRenderPath(const RenderPath*);

}

#endif // ENABLE(SVG)
#endif
