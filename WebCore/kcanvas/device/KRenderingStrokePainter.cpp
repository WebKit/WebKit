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
#include "KRenderingPaintServer.h"
#include "KRenderingStrokePainter.h"

namespace WebCore {

KRenderingStrokePainter::KRenderingStrokePainter()
    : m_dirty(true)
    , m_opacity(1.0)
    , m_strokeWidth(1.0)
    , m_miterLimit(4)
    , m_capStyle(CAP_BUTT)
    , m_joinStyle(JOIN_MITER)
    , m_dashOffset(0.0)
{
}

KRenderingStrokePainter::~KRenderingStrokePainter()
{
}

float KRenderingStrokePainter::strokeWidth() const
{
    return m_strokeWidth;
}

void KRenderingStrokePainter::setStrokeWidth(float width)
{
    setDirty();
    m_strokeWidth = width;
}

unsigned KRenderingStrokePainter::strokeMiterLimit() const
{
    return m_miterLimit;
}

void KRenderingStrokePainter::setStrokeMiterLimit(unsigned limit)
{
    setDirty();
    m_miterLimit = limit;
}

KCCapStyle KRenderingStrokePainter::strokeCapStyle() const
{
    return m_capStyle;
}

void KRenderingStrokePainter::setStrokeCapStyle(KCCapStyle style)
{
    setDirty();
    m_capStyle = style;
}

KCJoinStyle KRenderingStrokePainter::strokeJoinStyle() const
{
    return m_joinStyle;
}

void KRenderingStrokePainter::setStrokeJoinStyle(KCJoinStyle style)
{
    setDirty();
    m_joinStyle = style;
}

float KRenderingStrokePainter::dashOffset() const
{
    return m_dashOffset;
}

void KRenderingStrokePainter::setDashOffset(float offset)
{
    setDirty();
    m_dashOffset = offset;
}

const KCDashArray& KRenderingStrokePainter::dashArray() const
{
    return m_dashArray;
}

void KRenderingStrokePainter::setDashArray(const KCDashArray& dashArray)
{
    setDirty();
    m_dashArray = dashArray;
}

float KRenderingStrokePainter::opacity() const
{
    return m_opacity;
}

void KRenderingStrokePainter::setOpacity(float opacity)
{
    setDirty();
    m_opacity = opacity;
}

bool KRenderingStrokePainter::dirty() const
{
    return m_dirty;
}

void KRenderingStrokePainter::setDirty(bool dirty)
{
    m_dirty = dirty;
}

}

// vim:ts=4:noet
#endif // SVG_SUPPORT

