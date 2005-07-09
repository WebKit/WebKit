/*
	Copyright (C) 2004 Nikolas Zimmermann <wildfox@kde.org>
				  2004 Rob Buis <buis@kde.org>

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
#include "KCanvasImage.h"
#include "KRenderingPaintServerPattern.h"

class KRenderingPaintServerPattern::Private
{
public:
	Private()
	{
		tile = 0;
		x = y = width = height = 0;
		useBoundingBoxMode = true;
	}
	~Private() { delete tile; }

	KCanvasImage *tile;
	KCanvasMatrix patternTransform;
	float x, y, width, height;
	bool useBoundingBoxMode;
};

KRenderingPaintServerPattern::KRenderingPaintServerPattern() : KRenderingPaintServer(), KCanvasResource(), d(new Private())
{
}

KRenderingPaintServerPattern::~KRenderingPaintServerPattern()
{
	delete d;
}

float KRenderingPaintServerPattern::x() const
{
	return d->x;
}

void KRenderingPaintServerPattern::setX(float x)
{
	d->x = x;
}
	
float KRenderingPaintServerPattern::y() const
{
	return d->y;
}

void KRenderingPaintServerPattern::setY(float y)
{
	d->y = y;
}

bool KRenderingPaintServerPattern::boundingBoxMode() const
{
	return d->useBoundingBoxMode;
}

void KRenderingPaintServerPattern::setBoundingBoxMode(bool mode)
{
	d->useBoundingBoxMode = mode;
}

float KRenderingPaintServerPattern::width() const
{
	return d->width;
}

void KRenderingPaintServerPattern::setWidth(float width)
{
	d->width = width;
}

float KRenderingPaintServerPattern::height() const
{
	return d->height;
}

void KRenderingPaintServerPattern::setHeight(float height)
{
	d->height = height;
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

// vim:ts=4:noet
