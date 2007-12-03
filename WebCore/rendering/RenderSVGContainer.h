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

    virtual RenderObject* firstChild() const { return m_firstChild; }
    virtual RenderObject* lastChild() const { return m_lastChild; }

    virtual int width() const { return m_width; }
    virtual int height() const { return m_height; }

    virtual bool canHaveChildren() const;
    virtual void addChild(RenderObject* newChild, RenderObject* beforeChild = 0);
    virtual void removeChild(RenderObject*);

    virtual void destroy();
    void destroyLeftoverChildren();

    virtual RenderObject* removeChildNode(RenderObject*, bool fullRemove = true);
    virtual void appendChildNode(RenderObject*, bool fullAppend = true);
    virtual void insertChildNode(RenderObject* child, RenderObject* before, bool fullInsert = true);

    // Designed for speed.  Don't waste time doing a bunch of work like layer updating and repainting when we know that our
    // change in parentage is not going to affect anything.
    virtual void moveChildNode(RenderObject* child) { appendChildNode(child->parent()->removeChildNode(child, false), false); }

    virtual void calcPrefWidths() { setPrefWidthsDirty(false); }

    // Some containers do not want it's children
    // to be drawn, because they may be 'referenced'
    // Example: <marker> children in SVG
    void setDrawsContents(bool);
    bool drawsContents() const;

    virtual bool isSVGContainer() const { return true; }
    virtual const char* renderName() const { return "RenderSVGContainer"; }

    virtual bool requiresLayer();
    virtual short lineHeight(bool b, bool isRootLineBox = false) const;
    virtual short baselinePosition(bool b, bool isRootLineBox = false) const;

    virtual void layout();
    virtual void paint(PaintInfo&, int parentX, int parentY);

    virtual IntRect absoluteClippedOverflowRect();
    virtual void absoluteRects(Vector<IntRect>& rects, int tx, int ty, bool topLevel = true);
    virtual void addFocusRingRects(GraphicsContext*, int tx, int ty);

    FloatRect relativeBBox(bool includeStroke = true) const;

    virtual bool calculateLocalTransform();
    virtual AffineTransform localTransform() const;
    virtual AffineTransform viewportTransform() const;

    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, int x, int y, int tx, int ty, HitTestAction);

protected:
    virtual void applyContentTransforms(PaintInfo&);
    virtual void applyAdditionalTransforms(PaintInfo&);

    void calcBounds();

private:
    int calcReplacedWidth() const;
    int calcReplacedHeight() const;

    RenderObject* m_firstChild;
    RenderObject* m_lastChild;

    int m_width;
    int m_height;
    
    bool selfWillPaint() const;

    bool m_drawsContents : 1;
    
protected:    
    IntRect m_absoluteBounds;
    AffineTransform m_localTransform;
};
  
} // namespace WebCore

#endif // ENABLE(SVG)
#endif // RenderSVGContainer_h

// vim:ts=4:noet
