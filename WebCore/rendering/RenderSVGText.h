/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2006 Apple Computer, Inc.
 *           (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef RenderSVGText_h
#define RenderSVGText_h

#if ENABLE(SVG)

#include "AffineTransform.h"
#include "RenderSVGBlock.h"

namespace WebCore {

class SVGTextElement;

class RenderSVGText : public RenderSVGBlock {
public:
    RenderSVGText(SVGTextElement* node);

private:
    virtual const char* renderName() const { return "RenderSVGText"; }

    virtual const SVGRenderBase* toSVGRenderBase() const { return this; }

    virtual bool isSVGText() const { return true; }

    virtual const AffineTransform& localToParentTransform() const { return m_localTransform; }

    virtual void paint(PaintInfo&, int tx, int ty);
    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, int x, int y, int tx, int ty, HitTestAction);
    virtual bool nodeAtFloatPoint(const HitTestRequest&, HitTestResult&, const FloatPoint& pointInParent, HitTestAction);

    virtual bool requiresLayer() const { return false; }
    virtual void layout();

    virtual void absoluteRects(Vector<IntRect>&, int tx, int ty);
    virtual void absoluteQuads(Vector<FloatQuad>&);

    virtual IntRect clippedOverflowRectForRepaint(RenderBoxModelObject* repaintContainer);
    virtual void computeRectForRepaint(RenderBoxModelObject* repaintContainer, IntRect&, bool fixed = false);

    virtual void mapLocalToContainer(RenderBoxModelObject* repaintContainer, bool useTransforms, bool fixed, TransformState&) const;

    virtual FloatRect objectBoundingBox() const;
    virtual FloatRect strokeBoundingBox() const;
    virtual FloatRect repaintRectInLocalCoordinates() const;

    // FIXME: This can be removed when localTransform() is removed from RenderObject
    virtual AffineTransform localTransform() const { return m_localTransform; }

    virtual RootInlineBox* createRootInlineBox();

    AffineTransform m_localTransform;
};

}

#endif // ENABLE(SVG)
#endif

// vim:ts=4:noet
