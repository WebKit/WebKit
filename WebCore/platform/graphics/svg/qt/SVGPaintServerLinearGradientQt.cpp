/*
    Copyright (C) 2006 Nikolas Zimmermann <wildfox@kde.org>

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
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"

#if ENABLE(SVG)
#include "SVGPaintServerLinearGradient.h"
#include "SVGGradientElement.h"

#include "GraphicsContext.h"
#include "RenderPath.h"

#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>

namespace WebCore {

QGradient SVGPaintServerLinearGradient::setupGradient(GraphicsContext*& context, const RenderObject* object) const
{
    QPainterPath* path(context ? context->currentPath() : 0);
    Q_ASSERT(path);

    double x1, x2, y1, y2;
    if (boundingBoxMode()) {
        QRectF bbox = path->boundingRect();
        x1 = bbox.x();
        y1 = bbox.y();
        x2 = bbox.x() + bbox.width();
        y2 = bbox.y() + bbox.height();
    } else {
        x1 = gradientStart().x();
        y1 = gradientStart().y();
        x2 = gradientEnd().x();
        y2 = gradientEnd().y();
    }

    QLinearGradient gradient(QPointF(x1, y1), QPointF(x2, y2));

    return gradient;
}

} // namespace WebCore

#endif

// vim:ts=4:noet
