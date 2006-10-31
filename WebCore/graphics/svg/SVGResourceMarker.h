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

#ifndef KCanvasMarker_H
#define KCanvasMarker_H
#ifdef SVG_SUPPORT

#include "KCanvasResource.h"

namespace WebCore {

class KCanvasMarker : public KCanvasResource
{
public:
    KCanvasMarker(RenderSVGContainer* = 0);
    virtual ~KCanvasMarker();
    
    virtual bool isMarker() const { return true; }

    void setMarker(RenderSVGContainer*);
    
    void setRef(double refX, double refY);
    double refX() const;    
    double refY() const;
    
    void setAngle(float angle);
    void setAutoAngle();
    float angle() const;

    void setUseStrokeWidth(bool useStrokeWidth = true);
    bool useStrokeWidth() const;

    void draw(GraphicsContext*, const FloatRect&, double x, double y, double strokeWidth = 1, double angle = 0);

    TextStream& externalRepresentation(TextStream&) const; 

private:
    double m_refX, m_refY;
    float m_angle;
    RenderSVGContainer* m_marker;
    bool m_useStrokeWidth;
};

KCanvasMarker* getMarkerById(Document*, const AtomicString&);

}

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
