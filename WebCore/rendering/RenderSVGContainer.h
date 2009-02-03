/*
    Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2007 Rob Buis <buis@kde.org>

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
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef RenderSVGContainer_h
#define RenderSVGContainer_h

#if ENABLE(SVG)

#include "RenderPath.h"
#include "SVGPreserveAspectRatio.h"

namespace WebCore {

class SVGElement;

class RenderSVGContainer : public RenderObject {
public:
    RenderSVGContainer(SVGStyledElement*);
    ~RenderSVGContainer();

    virtual RenderObjectChildList* virtualChildren() { return children(); }
    virtual const RenderObjectChildList* virtualChildren() const { return children(); }
    const RenderObjectChildList* children() const { return &m_children; }
    RenderObjectChildList* children() { return &m_children; }

    int width() const { return m_width; }
    int height() const { return m_height; }

    virtual void addChild(RenderObject* newChild, RenderObject* beforeChild = 0);
    virtual void removeChild(RenderObject*);

    virtual void destroy();

    // Some containers do not want it's children
    // to be drawn, because they may be 'referenced'
    // Example: <marker> children in SVG
    void setDrawsContents(bool);
    bool drawsContents() const;

    virtual bool isSVGContainer() const { return true; }
    virtual const char* renderName() const { return "RenderSVGContainer"; }

    virtual bool requiresLayer() const { return false; }
    virtual int lineHeight(bool b, bool isRootLineBox = false) const;
    virtual int baselinePosition(bool b, bool isRootLineBox = false) const;

    virtual void layout();
    virtual void paint(PaintInfo&, int parentX, int parentY);

    virtual IntRect clippedOverflowRectForRepaint(RenderBox* repaintContainer);
    virtual void absoluteRects(Vector<IntRect>& rects, int tx, int ty, bool topLevel = true);
    virtual void absoluteQuads(Vector<FloatQuad>&, bool topLevel = true);
    virtual void addFocusRingRects(GraphicsContext*, int tx, int ty);

    FloatRect relativeBBox(bool includeStroke = true) const;

    virtual bool calculateLocalTransform();
    virtual TransformationMatrix localTransform() const;
    virtual TransformationMatrix viewportTransform() const;

    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, int x, int y, int tx, int ty, HitTestAction);

protected:
    virtual void applyContentTransforms(PaintInfo&);
    virtual void applyAdditionalTransforms(PaintInfo&);

    void calcBounds();

    virtual IntRect outlineBoundsForRepaint(RenderBox* /*repaintContainer*/) const;

private:
    int calcReplacedWidth() const;
    int calcReplacedHeight() const;

    RenderObjectChildList m_children;

    int m_width;
    int m_height;
    
    bool selfWillPaint() const;

    bool m_drawsContents : 1;
    
protected:    
    IntRect m_absoluteBounds;
    TransformationMatrix m_localTransform;
};
  
} // namespace WebCore

#endif // ENABLE(SVG)
#endif // RenderSVGContainer_h

// vim:ts=4:noet
