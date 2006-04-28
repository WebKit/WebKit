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
    KCanvasContainer(SVGStyledElement*);
    ~KCanvasContainer();

    // Some containers do not want it's children
    // to be drawn, because they may be 'referenced'
    // Example: <marker> children in SVG
    void setDrawsContents(bool);
    bool drawsContents() const;

    virtual bool isKCanvasContainer() const { return true; }
    virtual const char* renderName() const { return "KCanvasContainer"; }
    
    virtual bool canHaveChildren() const;
    
    virtual bool requiresLayer();
    virtual short lineHeight(bool b, bool isRootLineBox = false) const;
    virtual short baselinePosition(bool b, bool isRootLineBox = false) const;
    
    virtual void calcMinMaxWidth();
    virtual void layout();
    virtual void paint(PaintInfo &paintInfo, int parentX, int parentY);
    
    virtual IntRect getAbsoluteRepaintRect();
    virtual QMatrix absoluteTransform() const;

    bool fillContains(const FloatPoint&) const;
    bool strokeContains(const FloatPoint&) const;
    FloatRect relativeBBox(bool includeStroke = true) const;
    
    virtual QMatrix localTransform() const;
    void setLocalTransform(const QMatrix&);
    
    void setViewport(const FloatRect&);
    FloatRect viewport() const;

    void setViewBox(const FloatRect&);
    FloatRect viewBox() const;

    void setAlign(KCAlign);
    KCAlign align() const;

    void setSlice(bool);
    bool slice() const;
    
private:
    KCanvasMatrix getAspectRatio(const FloatRect& logical, const FloatRect& physical) const;
    QMatrix viewportTransform() const;

    class Private;
    Private* d;
};

}

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
