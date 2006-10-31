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

#ifndef KRenderingPaintServerGradient_H
#define KRenderingPaintServerGradient_H
#ifdef SVG_SUPPORT

#include "AffineTransform.h"
#include "Color.h"
#include "FloatPoint.h"
#include "KRenderingPaintServer.h"

namespace WebCore {

enum KCGradientSpreadMethod {
    SPREADMETHOD_PAD = 1,
    SPREADMETHOD_REPEAT = 2,
    SPREADMETHOD_REFLECT = 4
};

typedef std::pair<float, Color> KCGradientStop;
inline KCGradientStop makeGradientStop(float offset, const Color& color) { return std::make_pair(offset, color); }

TextStream& operator<<(TextStream&, KCGradientSpreadMethod);

class KRenderingPaintServerGradient : public KRenderingPaintServer
{
public:
    KRenderingPaintServerGradient();
    virtual ~KRenderingPaintServerGradient();

    // 'Gradient' interface
    const Vector<KCGradientStop>& gradientStops() const;
    void setGradientStops(const Vector<KCGradientStop>&);
    void setGradientStops(KRenderingPaintServerGradient*);

    KCGradientSpreadMethod spreadMethod() const;
    void setGradientSpreadMethod(const KCGradientSpreadMethod&);

    // Gradient start and end points are percentages when used in boundingBox mode.
    // For instance start point with value (0,0) is top-left and end point with value (100, 100) is 
    // bottom-right.
    // BoundingBox mode is true by default.
    bool boundingBoxMode() const;
    void setBoundingBoxMode(bool mode = true);

    AffineTransform gradientTransform() const;
    void setGradientTransform(const AffineTransform&);
    
    SVGResourceListener* listener() const;
    void setListener(SVGResourceListener*);

    TextStream& externalRepresentation(TextStream&) const;
private:
    Vector<KCGradientStop> m_stops;
    KCGradientSpreadMethod m_spreadMethod;
    bool m_boundingBoxMode;
    AffineTransform m_gradientTransform;
    SVGResourceListener* m_listener;
};

class KRenderingPaintServerLinearGradient : public KRenderingPaintServerGradient
{
public:
    KRenderingPaintServerLinearGradient();
    virtual ~KRenderingPaintServerLinearGradient();

    virtual KCPaintServerType type() const;

    // 'Linear Gradient' interface
    FloatPoint gradientStart() const;
    void setGradientStart(const FloatPoint&);

    FloatPoint gradientEnd() const;
    void setGradientEnd(const FloatPoint&);

    TextStream& externalRepresentation(TextStream&) const;
private:
    FloatPoint m_start;
    FloatPoint m_end;
};

class KRenderingPaintServerRadialGradient : public KRenderingPaintServerGradient
{
public:
    KRenderingPaintServerRadialGradient();
    virtual ~KRenderingPaintServerRadialGradient();

    virtual KCPaintServerType type() const;

    // 'Radial Gradient' interface
    FloatPoint gradientCenter() const;
    void setGradientCenter(const FloatPoint&);

    FloatPoint gradientFocal() const;
    void setGradientFocal(const FloatPoint&);

    float gradientRadius() const;
    void setGradientRadius(float);

    TextStream& externalRepresentation(TextStream&) const;
private:
    float m_radius;
    FloatPoint m_center;
    FloatPoint m_focal;
};

}

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
