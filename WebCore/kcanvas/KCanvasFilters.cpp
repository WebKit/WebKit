/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
                  2005 Eric Seidel <eric.seidel@kdemail.net>

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

#include "KCanvasFilters.h"

// Filters

void KCanvasFilter::addFilterEffect(KCanvasFilterEffect *effect)
{
	m_effects.append(effect);
}

QRect KCanvasFilterEffect::subRegion() const
{
	return m_subregion;
}

void KCanvasFilterEffect::setSubRegion(const QRect &subregion)
{
	m_subregion = subregion;
}

QString KCanvasFilterEffect::in() const
{
	return m_in;
}
void KCanvasFilterEffect::setIn(const QString &in)
{
	m_in = in;
}

QString KCanvasFilterEffect::result() const
{
	return m_result;
}

void KCanvasFilterEffect::setResult(const QString &result)
{
	m_result = result;
}



float KCanvasFEGaussianBlur::stdDeviationX() const
{
	return m_x;
}

void KCanvasFEGaussianBlur::setStdDeviationX(float x)
{
	m_x = x;
}

float KCanvasFEGaussianBlur::stdDeviationY() const
{
	return m_y;
}

void KCanvasFEGaussianBlur::setStdDeviationY(float y)
{
	m_y = y;
}

KCanvasFEImage::~KCanvasFEImage()
{
	delete m_image;
}

