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
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#ifndef RenderSVGRoot_h
#define RenderSVGRoot_h

#if ENABLE(SVG)
#include "RenderContainer.h"
#include "RenderPath.h"
#include "SVGPreserveAspectRatio.h"

namespace WebCore {

class SVGElement;

class RenderSVGRoot : public RenderContainer {
public:
    RenderSVGRoot(SVGStyledElement*);
    ~RenderSVGRoot();

    virtual bool isSVGRoot() const { return true; }
    virtual const char* renderName() const { return "RenderSVGRoot"; }

    virtual short lineHeight(bool b, bool isRootLineBox = false) const;
    virtual short baselinePosition(bool b, bool isRootLineBox = false) const;
    
    virtual void layout();
    virtual void paint(PaintInfo&, int parentX, int parentY);
    
    virtual IntRect absoluteClippedOverflowRect();
    virtual void absoluteRects(Vector<IntRect>& rects, int tx, int ty);
    virtual void addFocusRingRects(GraphicsContext*, int tx, int ty);

    virtual AffineTransform absoluteTransform() const;

    bool fillContains(const FloatPoint&) const;
    bool strokeContains(const FloatPoint&) const;
    FloatRect relativeBBox(bool includeStroke = true) const;
    
    virtual AffineTransform localTransform() const;
    void setLocalTransform(const AffineTransform&);
   
    FloatRect viewport() const;

    void setViewBox(const FloatRect&);
    FloatRect viewBox() const;

    void setAlign(SVGPreserveAspectRatio::SVGPreserveAspectRatioType);
    SVGPreserveAspectRatio::SVGPreserveAspectRatioType align() const;

    void setSlice(bool);
    bool slice() const;
    
    AffineTransform viewportTransform() const;
    
    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, int x, int y, int tx, int ty, HitTestAction);
    
private:
    void calcViewport(); 
    AffineTransform getAspectRatio(const FloatRect& logical, const FloatRect& physical) const;
    void applyContentTransforms(PaintInfo&, int parentX, int parentY);

    bool m_slice : 1;
    AffineTransform m_matrix;
    
    FloatRect m_viewport;
    FloatRect m_viewBox;
    SVGPreserveAspectRatio::SVGPreserveAspectRatioType m_align;
    IntRect m_absoluteBounds;
};

} // namespace WebCore

#endif // ENABLE(SVG)
#endif // RenderSVGRoot_h

// vim:ts=4:noet
