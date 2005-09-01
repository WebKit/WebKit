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

#include "KCanvasMatrix.h"
#include "KRenderingStyle.h"
#include "KCanvasResources.h"
#include "KRenderingFillPainter.h"
#include "KRenderingStrokePainter.h"

class KRenderingStyle::Private
{
public:
    Private()
    {
        fillPainter = 0;
        strokePainter = 0;

        visible = true;

        endMarker = 0;
        midMarker = 0;
        startMarker = 0;
        
        filter = 0;
    
        imageRendering = IR_OPTIMIZE_QUALITY;

        opacity = 1.0f;
    }

    ~Private()
    {
        delete fillPainter;
        delete strokePainter;

        delete endMarker;
        delete midMarker;
        delete startMarker;
    }

    KCanvasMatrix objectMatrix;

    KRenderingFillPainter *fillPainter;
    KRenderingStrokePainter *strokePainter;

    bool visible : 1;

    KCImageRendering imageRendering : 1;

    QStringList clipPaths;
    
    KCanvasFilter *filter;

    KCanvasMarker *startMarker;
    KCanvasMarker *midMarker;
    KCanvasMarker *endMarker;

    float opacity;
};

KRenderingStyle::KRenderingStyle() : d(new Private())
{
}

KRenderingStyle::~KRenderingStyle()
{
    delete d;
}

// World matrix property
KCanvasMatrix KRenderingStyle::objectMatrix() const
{
    return d->objectMatrix;
}

void KRenderingStyle::setObjectMatrix(const KCanvasMatrix &objectMatrix)
{
    d->objectMatrix = objectMatrix;
}

// Stroke (aka Pen) properties
bool KRenderingStyle::isStroked() const
{
    return (d->strokePainter != 0);
}

KRenderingStrokePainter *KRenderingStyle::strokePainter()
{
    if(!d->strokePainter)
        d->strokePainter = new KRenderingStrokePainter();
        
    return d->strokePainter;
}

void KRenderingStyle::disableStrokePainter()
{
    if(d->strokePainter)
    {
        delete d->strokePainter;
        d->strokePainter = 0;
    }
}

// Fill (aka Bush) properties
bool KRenderingStyle::isFilled() const
{
    return (d->fillPainter != 0);
}

KRenderingFillPainter *KRenderingStyle::fillPainter()
{
    if(!d->fillPainter)
        d->fillPainter = new KRenderingFillPainter();
        
    return d->fillPainter;
}

void KRenderingStyle::disableFillPainter()
{
    if(d->fillPainter)
    {
        delete d->fillPainter;
        d->fillPainter = 0;
    }
}

// Display states
bool KRenderingStyle::visible() const
{
    return d->visible;
}

void KRenderingStyle::setVisible(bool visible)
{
    d->visible = visible;
}

// Color interpolation
KCColorInterpolation KRenderingStyle::colorInterpolation() const
{
    // TODO!
    return KCColorInterpolation();
}

void KRenderingStyle::setColorInterpolation(KCColorInterpolation /*interpolation*/)
{
    // TODO!
}

// Quality vs. speed control
KCImageRendering KRenderingStyle::imageRendering() const
{
    return d->imageRendering;
}

void KRenderingStyle::setImageRendering(KCImageRendering ir)
{
    d->imageRendering = ir;
}

// Overall opacity
float KRenderingStyle::opacity() const
{
    return d->opacity;
}

void KRenderingStyle::setOpacity(float opacity)
{
    d->opacity = opacity;
}

// Clipping
QStringList KRenderingStyle::clipPaths() const
{
    return d->clipPaths;
}

void KRenderingStyle::setClipPaths(const QStringList &data) const
{
    d->clipPaths = data;
}

// Filters
KCanvasFilter *KRenderingStyle::filter() const
{
    return d->filter;
}

void KRenderingStyle::setFilter(KCanvasFilter *newFilter) const
{
    d->filter = newFilter;
}

// Markers
KCanvasMarker *KRenderingStyle::startMarker() const
{
    return d->startMarker;
}

void KRenderingStyle::setStartMarker(KCanvasMarker *marker)
{
    d->startMarker = marker;
}

KCanvasMarker *KRenderingStyle::midMarker() const
{
    return d->midMarker;
}

void KRenderingStyle::setMidMarker(KCanvasMarker *marker)
{
    d->midMarker = marker;
}

KCanvasMarker *KRenderingStyle::endMarker() const
{
    return d->endMarker;
}

void KRenderingStyle::setEndMarker(KCanvasMarker *marker)
{
    d->endMarker = marker;
}

bool KRenderingStyle::hasMarkers() const
{
    return d->startMarker || d->midMarker || d->endMarker;
}

// vim:ts=4:noet
