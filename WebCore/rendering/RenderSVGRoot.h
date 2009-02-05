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

#ifndef RenderSVGRoot_h
#define RenderSVGRoot_h

#if ENABLE(SVG)
#include "RenderBox.h"
#include "FloatRect.h"

namespace WebCore {

class SVGStyledElement;
class TransformationMatrix;

class RenderSVGRoot : public RenderBox {
public:
    RenderSVGRoot(SVGStyledElement*);
    ~RenderSVGRoot();

    virtual RenderObjectChildList* virtualChildren() { return children(); }
    virtual const RenderObjectChildList* virtualChildren() const { return children(); }
    const RenderObjectChildList* children() const { return &m_children; }
    RenderObjectChildList* children() { return &m_children; }

    virtual bool isSVGRoot() const { return true; }
    virtual const char* renderName() const { return "RenderSVGRoot"; }

    virtual int lineHeight(bool b, bool isRootLineBox = false) const;
    virtual int baselinePosition(bool b, bool isRootLineBox = false) const;
    virtual void calcPrefWidths();
    
    virtual void layout();
    virtual void paint(PaintInfo&, int parentX, int parentY);
    
    virtual IntRect clippedOverflowRectForRepaint(RenderBox* repaintContainer);
    virtual void absoluteRects(Vector<IntRect>& rects, int tx, int ty);
    virtual void absoluteQuads(Vector<FloatQuad>&, bool topLevel = true);
    virtual void addFocusRingRects(GraphicsContext*, int tx, int ty);

    virtual TransformationMatrix absoluteTransform() const;

    bool fillContains(const FloatPoint&) const;
    bool strokeContains(const FloatPoint&) const;
    FloatRect relativeBBox(bool includeStroke = true) const;
    
    virtual TransformationMatrix localTransform() const;
   
    FloatRect viewport() const;

    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, int x, int y, int tx, int ty, HitTestAction);

    virtual void position(InlineBox*);
    
private:
    void calcViewport(); 
    void applyContentTransforms(PaintInfo&, int parentX, int parentY);

    RenderObjectChildList m_children;
    FloatRect m_viewport;
    IntRect m_absoluteBounds;
};

} // namespace WebCore

#endif // ENABLE(SVG)
#endif // RenderSVGRoot_h

// vim:ts=4:noet
