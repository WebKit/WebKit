/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

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

#ifndef KCanvasContainer_H
#define KCanvasContainer_H
#if SVG_SUPPORT

#include "kcanvas/RenderPath.h"
#include "RenderContainer.h"

namespace WebCore {

enum KCAlign {
    ALIGN_NONE = 0,
    ALIGN_XMINYMIN = 1,
    ALIGN_XMIDYMIN = 2,
    ALIGN_XMAXYMIN = 3,
    ALIGN_XMINYMID = 4,
    ALIGN_XMIDYMID = 5,
    ALIGN_XMAXYMID = 6,
    ALIGN_XMINYMAX = 7,
    ALIGN_XMIDYMAX = 8,
    ALIGN_XMAXYMAX = 9
};

class KCanvasRenderingStyle;

class KCanvasContainer : public RenderContainer
{
public:
    KCanvasContainer(SVGStyledElementImpl *node);
    virtual ~KCanvasContainer();

    // Some containers do not want it's children
    // to be drawn, because they may be 'referenced'
    // Example: <marker> children in SVG
    void setDrawsContents(bool drawsContents);
    bool drawsContents() const;

    virtual bool isKCanvasContainer() const { return true; }
    virtual const char *renderName() const { return "KCanvasContainer"; }

    virtual bool fillContains(const FloatPoint &p) const;
    virtual bool strokeContains(const FloatPoint &p) const;
    virtual FloatRect relativeBBox(bool includeStroke = true) const;
    
    virtual QMatrix localTransform() const;
    virtual void setLocalTransform(const QMatrix &matrix);
    
    virtual void setViewport(const FloatRect& viewport) = 0;
    virtual FloatRect viewport() const = 0;

    virtual void setViewBox(const FloatRect& viewBox) = 0;
    virtual FloatRect viewBox() const = 0;

    virtual void setAlign(KCAlign align) = 0;
    virtual KCAlign align() const = 0;

    void setSlice(bool slice);
    bool slice() const;
    
protected:
    KCanvasMatrix getAspectRatio(const FloatRect logical, const FloatRect physical) const;

private:
    class Private;
    Private *d;
};

}

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
