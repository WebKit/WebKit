/*
    Copyright (C) 2004, 2005, 2006 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005, 2006 Rob Buis <buis@kde.org>
                  2005 Apple Computer, Inc.

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
#include "KCanvasPathQt.h"

namespace WebCore {

KCanvasPathQt::KCanvasPathQt()
{
}

KCanvasPathQt::~KCanvasPathQt()
{
}

bool KCanvasPathQt::isEmpty() const
{
    return m_path.isEmpty();
}

void KCanvasPathQt::moveTo(float x, float y)
{
    m_path.moveTo(x, y);
}

void KCanvasPathQt::lineTo(float x, float y)
{
    m_path.lineTo(x, y);
}

void KCanvasPathQt::curveTo(float x1, float y1, float x2, float y2, float x3, float y3)
{
    m_path.cubicTo(x1, y1, x2, y2, x3, y3);
}

void KCanvasPathQt::closeSubpath()
{
    m_path.closeSubpath();
}

FloatRect KCanvasPathQt::boundingBox()
{
    return FloatRect(m_path.boundingRect());
}

FloatRect KCanvasPathQt::strokeBoundingBox(const KRenderingStrokePainter& strokePainter)
{
    qDebug("KCanvasPathQt::strokeBoundingBox() TODO!");
    return boundingBox();
}

bool KCanvasPathQt::strokeContainsPoint(const FloatPoint& point)
{
    qDebug("KCanvasPathQt::strokeContainsPoint() TODO!");    
    return containsPoint(point, RULE_EVENODD);
}

bool KCanvasPathQt::containsPoint(const FloatPoint& point, KCWindRule rule)
{
    Qt::FillRule savedRule = m_path.fillRule();
    m_path.setFillRule(rule == RULE_EVENODD ? Qt::OddEvenFill : Qt::WindingFill);

    bool contains = m_path.contains(point);

    m_path.setFillRule(savedRule);
    return contains;
}

}

// vim:ts=4:noet
