/*
    Copyright (C) 2004, 2005, 2006 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005, 2006 Rob Buis <buis@kde.org>

    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; see the file COPYING.LIB. If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"

#include <QPen>
#include <QVector>

#include "KRenderingStrokePainter.h"
#include "KRenderingPaintServerQt.h"

namespace WebCore {

KRenderingPaintServerQt::KRenderingPaintServerQt()
{
}

KRenderingPaintServerQt::~KRenderingPaintServerQt()
{
}

void KRenderingPaintServerQt::setPenProperties(const KRenderingStrokePainter& strokePainter, QPen& pen) const
{
    pen.setWidthF(strokePainter.strokeWidth());
    if (strokePainter.strokeCapStyle() == CAP_BUTT) {
        pen.setCapStyle(Qt::FlatCap);
    } else if (strokePainter.strokeCapStyle() == CAP_ROUND) {
        pen.setCapStyle(Qt::RoundCap);
    }

    if(strokePainter.strokeJoinStyle() == JOIN_MITER) {
        pen.setJoinStyle(Qt::MiterJoin);
        pen.setMiterLimit((qreal)strokePainter.strokeMiterLimit());
    } else if(strokePainter.strokeJoinStyle() == JOIN_ROUND) {
        pen.setJoinStyle(Qt::RoundJoin);
    }

    KCDashArray dashes = strokePainter.dashArray();
    unsigned int dashLength = !dashes.isEmpty() ? dashes.count() : 0;
    if(dashLength) {
        QVector<qreal> pattern;
        unsigned int count = (dashLength % 2) == 0 ? dashLength : dashLength * 2;

        for(unsigned int i = 0; i < count; i++)
            pattern.append(dashes[i % dashLength] / (float)pen.widthF());

        pen.setDashPattern(pattern);
        // TODO: dash-offset, does/will qt4 API allow it? (Rob)
    }
}

}

// vim:ts=4:noet
