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
#include "KCanvasMatrix.h"
#include "KCanvasImage.h"
#include "KRenderingPaintServerPattern.h"

#include <qtextstream.h>
#include "KCanvasTreeDebug.h"

class KRenderingPaintServerPattern::Private
{
public:
    Private()
    {
        tile = 0;
        useBoundingBoxMode = true;
        listener = 0;
    }
    ~Private() { delete tile; }

    KCanvasImage *tile;
    KCanvasMatrix patternTransform;
    FloatRect bbox;
    bool useBoundingBoxMode;
    KCanvasResourceListener *listener;
};

KRenderingPaintServerPattern::KRenderingPaintServerPattern() : KRenderingPaintServer(), d(new Private())
{
}

KRenderingPaintServerPattern::~KRenderingPaintServerPattern()
{
    delete d;
}

void KRenderingPaintServerPattern::setBbox(const FloatRect& rect)
{
    d->bbox = rect;
}

FloatRect KRenderingPaintServerPattern::bbox() const
{
    return d->bbox;
}

bool KRenderingPaintServerPattern::boundingBoxMode() const
{
    return d->useBoundingBoxMode;
}

void KRenderingPaintServerPattern::setBoundingBoxMode(bool mode)
{
    d->useBoundingBoxMode = mode;
}

KCanvasImage *KRenderingPaintServerPattern::tile() const
{
    return d->tile;
}

void KRenderingPaintServerPattern::setTile(KCanvasImage *tile)
{
    d->tile = tile;
}

KCanvasMatrix KRenderingPaintServerPattern::patternTransform() const
{
    return d->patternTransform;
}

void KRenderingPaintServerPattern::setPatternTransform(const KCanvasMatrix &mat)
{
    d->patternTransform = mat;
}

KCPaintServerType KRenderingPaintServerPattern::type() const
{
    return PS_PATTERN;
}

KCanvasResourceListener *KRenderingPaintServerPattern::listener() const
{
    return d->listener;
}

void KRenderingPaintServerPattern::setListener(KCanvasResourceListener *listener)
{
    d->listener = listener;
}

QTextStream &KRenderingPaintServerPattern::externalRepresentation(QTextStream &ts) const
{
     ts << "[type=PATTERN]"
        << " [bbox=" << bbox() << "]";
    if (!boundingBoxMode())
        ts << " [bounding box mode=" << boundingBoxMode() << "]";        
    if (!patternTransform().qmatrix().isIdentity())
        ts << " [pattern transform=" << patternTransform().qmatrix() << "]";
    return ts;
}

// vim:ts=4:noet
#endif // SVG_SUPPORT

