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
#if SVG_SUPPORT
#include "IntPoint.h"

#include "KRenderingPaintServerGradient.h"
#include "KCanvasMatrix.h"

#include <qtextstream.h>
#include "KCanvasTreeDebug.h"

namespace WebCore {

//KCGradientSpreadMethod
QTextStream &operator<<(QTextStream &ts, KCGradientSpreadMethod m)
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

QTextStream &operator<<(QTextStream &ts, const Vector<KCGradientStop>& l)
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
class KRenderingPaintServerGradient::Private
{
public:
    Private() { boundingBoxMode = true; spreadMethod = SPREADMETHOD_PAD; listener = 0; }
    ~Private() { }

    Vector<KCGradientStop> stops;
    KCGradientSpreadMethod spreadMethod;
    bool boundingBoxMode;
    KCanvasMatrix gradientTransform;
    KCanvasResourceListener *listener;
};

KRenderingPaintServerGradient::KRenderingPaintServerGradient() : KRenderingPaintServer(), d(new Private())
{
}

KRenderingPaintServerGradient::~KRenderingPaintServerGradient()
{
    delete d;
}

const Vector<KCGradientStop>& KRenderingPaintServerGradient::gradientStops() const
{
    return d->stops;
}

static inline bool compareStopOffset(const KCGradientStop& first, const KCGradientStop& second)
{
    return first.first < second.first;
}

void KRenderingPaintServerGradient::setGradientStops(const Vector<KCGradientStop>& stops)
{
    d->stops = stops;
    std::sort(d->stops.begin(), d->stops.end(), compareStopOffset);
} 

void KRenderingPaintServerGradient::setGradientStops(KRenderingPaintServerGradient* server)
{
    d->stops = server->gradientStops();
}

KCGradientSpreadMethod KRenderingPaintServerGradient::spreadMethod() const
{
    return d->spreadMethod;
}

void KRenderingPaintServerGradient::setGradientSpreadMethod(const KCGradientSpreadMethod &method)
{
    d->spreadMethod = method;
}

bool KRenderingPaintServerGradient::boundingBoxMode() const
{
    return d->boundingBoxMode;
}

void KRenderingPaintServerGradient::setBoundingBoxMode(bool mode)
{
    d->boundingBoxMode = mode;
}

KCanvasMatrix KRenderingPaintServerGradient::gradientTransform() const
{
    return d->gradientTransform;
}

void KRenderingPaintServerGradient::setGradientTransform(const KCanvasMatrix &mat)
{
    d->gradientTransform = mat;
}

QTextStream &KRenderingPaintServerGradient::externalRepresentation(QTextStream &ts) const
{
    // abstract, don't stream type
    ts  << "[stops=" << gradientStops() << "]";
    if (spreadMethod() != SPREADMETHOD_PAD)
        ts << "[method=" << spreadMethod() << "]";        
    if (!boundingBoxMode())
        ts << " [bounding box mode=" << boundingBoxMode() << "]";
    if (!gradientTransform().qmatrix().isIdentity())
        ts << " [transform=" << gradientTransform().qmatrix() << "]";
    
    return ts;
}

// KRenderingPaintServerLinearGradient
class KRenderingPaintServerLinearGradient::Private
{
public:
    Private() { }
    ~Private() { }

    FloatPoint start, end;
};

KRenderingPaintServerLinearGradient::KRenderingPaintServerLinearGradient() : KRenderingPaintServerGradient(), d(new Private())
{
}

KRenderingPaintServerLinearGradient::~KRenderingPaintServerLinearGradient()
{
    delete d;
}

FloatPoint KRenderingPaintServerLinearGradient::gradientStart() const
{
    return d->start;
}

void KRenderingPaintServerLinearGradient::setGradientStart(const FloatPoint &start)
{
    d->start = start;
}

FloatPoint KRenderingPaintServerLinearGradient::gradientEnd() const
{
    return d->end;
}

void KRenderingPaintServerLinearGradient::setGradientEnd(const FloatPoint &end)
{
    d->end = end;
}

KCPaintServerType KRenderingPaintServerLinearGradient::type() const
{
    return PS_LINEAR_GRADIENT;
}

QTextStream &KRenderingPaintServerLinearGradient::externalRepresentation(QTextStream &ts) const
{
    ts << "[type=LINEAR-GRADIENT] ";    
    KRenderingPaintServerGradient::externalRepresentation(ts);
    ts  << " [start=" << gradientStart() << "]"
        << " [end=" << gradientEnd() << "]";
    return ts;
}

// KRenderingPaintServerRadialGradient
class KRenderingPaintServerRadialGradient::Private
{
public:
    Private() { }
    ~Private() { }

    float radius;
    FloatPoint center, focal;
};

KRenderingPaintServerRadialGradient::KRenderingPaintServerRadialGradient() : KRenderingPaintServerGradient(), d(new Private())
{
}

KRenderingPaintServerRadialGradient::~KRenderingPaintServerRadialGradient()
{
    delete d;
}

FloatPoint KRenderingPaintServerRadialGradient::gradientCenter() const
{
    return d->center;
}

void KRenderingPaintServerRadialGradient::setGradientCenter(const FloatPoint &center)
{
    d->center = center;
}

FloatPoint KRenderingPaintServerRadialGradient::gradientFocal() const
{
    return d->focal;
}

void KRenderingPaintServerRadialGradient::setGradientFocal(const FloatPoint &focal)
{
    d->focal = focal;
}

float KRenderingPaintServerRadialGradient::gradientRadius() const
{
    return d->radius;
}

void KRenderingPaintServerRadialGradient::setGradientRadius(float radius)
{
    d->radius = radius;
}

KCPaintServerType KRenderingPaintServerRadialGradient::type() const
{
    return PS_RADIAL_GRADIENT;
}

KCanvasResourceListener *KRenderingPaintServerGradient::listener() const
{
    return d->listener;
}

void KRenderingPaintServerGradient::setListener(KCanvasResourceListener *listener)
{
    d->listener = listener;
}

QTextStream &KRenderingPaintServerRadialGradient::externalRepresentation(QTextStream &ts) const
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

