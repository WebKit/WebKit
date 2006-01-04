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
#include <qpoint.h>

#include "KRenderingPaintServerGradient.h"
#include "KCanvasMatrix.h"

#include <qtextstream.h>
#include "KCanvasTreeDebug.h"

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

//KCSortedGradientStopList
KCSortedGradientStopList::KCSortedGradientStopList()
{
    setAutoDelete(true);
}

void KCSortedGradientStopList::addStop(float offset, const QColor &color)
{
    KCGradientOffsetPair *pair = new KCGradientOffsetPair;
    pair->offset = offset;
    pair->color = color;

    append(pair);
    sort();
}

int KCSortedGradientStopList::compareItems(Q3PtrCollection::Item item1, Q3PtrCollection::Item item2)
{
    KCGradientOffsetPair *pair1 = static_cast<KCGradientOffsetPair *>(item1);
    KCGradientOffsetPair *pair2 = static_cast<KCGradientOffsetPair *>(item2);

    if(pair1->offset == pair2->offset)
        return 0;
    else if(pair1->offset < pair2->offset)
        return -1;

    return 1;
}

QTextStream &operator<<(QTextStream &ts, const KCSortedGradientStopList &l)
{
    ts << "[";
    KCSortedGradientStopList::Iterator it(l); 
    while(*it)
    {
        ts << "(" << (*it)->offset << "," << (*it)->color << ")";
        ++it;
        if (*it)
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

    KCSortedGradientStopList stops;
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

KCSortedGradientStopList &KRenderingPaintServerGradient::gradientStops() const{

    return d->stops;
}

void KRenderingPaintServerGradient::setGradientStops(const KCSortedGradientStopList &stops)
{
    d->stops = stops;
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

    QPointF start, end;
};

KRenderingPaintServerLinearGradient::KRenderingPaintServerLinearGradient() : KRenderingPaintServerGradient(), d(new Private())
{
}

KRenderingPaintServerLinearGradient::~KRenderingPaintServerLinearGradient()
{
    delete d;
}

QPointF KRenderingPaintServerLinearGradient::gradientStart() const
{
    return d->start;
}

void KRenderingPaintServerLinearGradient::setGradientStart(const QPointF &start)
{
    d->start = start;
}

QPointF KRenderingPaintServerLinearGradient::gradientEnd() const
{
    return d->end;
}

void KRenderingPaintServerLinearGradient::setGradientEnd(const QPointF &end)
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
    QPointF center, focal;
};

KRenderingPaintServerRadialGradient::KRenderingPaintServerRadialGradient() : KRenderingPaintServerGradient(), d(new Private())
{
}

KRenderingPaintServerRadialGradient::~KRenderingPaintServerRadialGradient()
{
    delete d;
}

QPointF KRenderingPaintServerRadialGradient::gradientCenter() const
{
    return d->center;
}

void KRenderingPaintServerRadialGradient::setGradientCenter(const QPointF &center)
{
    d->center = center;
}

QPointF KRenderingPaintServerRadialGradient::gradientFocal() const
{
    return d->focal;
}

void KRenderingPaintServerRadialGradient::setGradientFocal(const QPointF &focal)
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

// vim:ts=4:noet
