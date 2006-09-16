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

#include "config.h"
#ifdef SVG_SUPPORT
#include "IntPoint.h"

#include "KRenderingPaintServerGradient.h"

#include "AffineTransform.h"
#include "TextStream.h"
#include "KCanvasTreeDebug.h"

namespace WebCore {

//KCGradientSpreadMethod
TextStream &operator<<(TextStream &ts, KCGradientSpreadMethod m)
{
    switch (m) 
    {
    case SPREADMETHOD_PAD:
        ts << "PAD"; break;
    case SPREADMETHOD_REPEAT:
        ts << "REPEAT"; break;
    case SPREADMETHOD_REFLECT:
        ts << "REFLECT"; break;
    }
    return ts;
}

TextStream &operator<<(TextStream &ts, const Vector<KCGradientStop>& l)
{
    ts << "[";
    for (Vector<KCGradientStop>::const_iterator it = l.begin(); it != l.end(); ++it) { 
        ts << "(" << it->first << "," << it->second << ")";
        if (it + 1 != l.end())
            ts << ", ";
    }
    ts << "]";
    return ts;
}

// KRenderingPaintServerGradient
KRenderingPaintServerGradient::KRenderingPaintServerGradient()
    : KRenderingPaintServer()
    , m_spreadMethod(SPREADMETHOD_PAD)
    , m_boundingBoxMode(true)
    , m_listener(0)
{
}

KRenderingPaintServerGradient::~KRenderingPaintServerGradient()
{
}

const Vector<KCGradientStop>& KRenderingPaintServerGradient::gradientStops() const
{
    return m_stops;
}

static inline bool compareStopOffset(const KCGradientStop& first, const KCGradientStop& second)
{
    return first.first < second.first;
}

void KRenderingPaintServerGradient::setGradientStops(const Vector<KCGradientStop>& stops)
{
    m_stops = stops;
    std::sort(m_stops.begin(), m_stops.end(), compareStopOffset);
} 

void KRenderingPaintServerGradient::setGradientStops(KRenderingPaintServerGradient* server)
{
    m_stops = server->gradientStops();
}

KCGradientSpreadMethod KRenderingPaintServerGradient::spreadMethod() const
{
    return m_spreadMethod;
}

void KRenderingPaintServerGradient::setGradientSpreadMethod(const KCGradientSpreadMethod &method)
{
    m_spreadMethod = method;
}

bool KRenderingPaintServerGradient::boundingBoxMode() const
{
    return m_boundingBoxMode;
}

void KRenderingPaintServerGradient::setBoundingBoxMode(bool mode)
{
    m_boundingBoxMode = mode;
}

AffineTransform KRenderingPaintServerGradient::gradientTransform() const
{
    return m_gradientTransform;
}

void KRenderingPaintServerGradient::setGradientTransform(const AffineTransform& mat)
{
    m_gradientTransform = mat;
}

TextStream &KRenderingPaintServerGradient::externalRepresentation(TextStream &ts) const
{
    // abstract, don't stream type
    ts  << "[stops=" << gradientStops() << "]";
    if (spreadMethod() != SPREADMETHOD_PAD)
        ts << "[method=" << spreadMethod() << "]";        
    if (!boundingBoxMode())
        ts << " [bounding box mode=" << boundingBoxMode() << "]";
    if (!gradientTransform().isIdentity())
        ts << " [transform=" << gradientTransform() << "]";
    
    return ts;
}

// KRenderingPaintServerLinearGradient
KRenderingPaintServerLinearGradient::KRenderingPaintServerLinearGradient()
    : KRenderingPaintServerGradient()
{
}

KRenderingPaintServerLinearGradient::~KRenderingPaintServerLinearGradient()
{
}

FloatPoint KRenderingPaintServerLinearGradient::gradientStart() const
{
    return m_start;
}

void KRenderingPaintServerLinearGradient::setGradientStart(const FloatPoint &start)
{
    m_start = start;
}

FloatPoint KRenderingPaintServerLinearGradient::gradientEnd() const
{
    return m_end;
}

void KRenderingPaintServerLinearGradient::setGradientEnd(const FloatPoint &end)
{
    m_end = end;
}

KCPaintServerType KRenderingPaintServerLinearGradient::type() const
{
    return PS_LINEAR_GRADIENT;
}

TextStream &KRenderingPaintServerLinearGradient::externalRepresentation(TextStream &ts) const
{
    ts << "[type=LINEAR-GRADIENT] ";    
    KRenderingPaintServerGradient::externalRepresentation(ts);
    ts  << " [start=" << gradientStart() << "]"
        << " [end=" << gradientEnd() << "]";
    return ts;
}

// KRenderingPaintServerRadialGradient
KRenderingPaintServerRadialGradient::KRenderingPaintServerRadialGradient()
    : KRenderingPaintServerGradient()
{
}

KRenderingPaintServerRadialGradient::~KRenderingPaintServerRadialGradient()
{
}

FloatPoint KRenderingPaintServerRadialGradient::gradientCenter() const
{
    return m_center;
}

void KRenderingPaintServerRadialGradient::setGradientCenter(const FloatPoint &center)
{
    m_center = center;
}

FloatPoint KRenderingPaintServerRadialGradient::gradientFocal() const
{
    return m_focal;
}

void KRenderingPaintServerRadialGradient::setGradientFocal(const FloatPoint &focal)
{
    m_focal = focal;
}

float KRenderingPaintServerRadialGradient::gradientRadius() const
{
    return m_radius;
}

void KRenderingPaintServerRadialGradient::setGradientRadius(float radius)
{
    m_radius = radius;
}

KCPaintServerType KRenderingPaintServerRadialGradient::type() const
{
    return PS_RADIAL_GRADIENT;
}

KCanvasResourceListener *KRenderingPaintServerGradient::listener() const
{
    return m_listener;
}

void KRenderingPaintServerGradient::setListener(KCanvasResourceListener *listener)
{
    m_listener = listener;
}

TextStream &KRenderingPaintServerRadialGradient::externalRepresentation(TextStream &ts) const
{
    ts << "[type=RADIAL-GRADIENT] "; 
    KRenderingPaintServerGradient::externalRepresentation(ts);
    ts << " [center=" << gradientCenter() << "]"
        << " [focal=" << gradientFocal() << "]"
        << " [radius=" << gradientRadius() << "]";
    return ts;
}

}

// vim:ts=4:noet
#endif // SVG_SUPPORT

