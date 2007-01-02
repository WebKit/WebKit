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
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include "config.h"

#ifdef SVG_SUPPORT
#include "SVGPaintServerLinearGradient.h"

#include "GraphicsContext.h"
#include "RenderPath.h"

#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>

namespace WebCore {

bool SVGPaintServerLinearGradient::setup(GraphicsContext*& context, const RenderObject* object, SVGPaintTargetType type, bool isPaintingText) const
{
    QPainter* painter(context ? context->platformContext() : 0);
    Q_ASSERT(painter);

    QPainterPath* path(context ? context->currentPath() : 0);
    Q_ASSERT(path);

    RenderStyle* renderStyle = object->style();

    double x1, x2, y1, y2;
    if (boundingBoxMode()) {
        QRectF bbox = path->boundingRect();
        x1 = double(bbox.left()) + (double(gradientStart().x() / 100.0) * double(bbox.width()));
        y1 = double(bbox.top()) + (double(gradientStart().y() / 100.0) * double(bbox.height()));
        x2 = double(bbox.left()) + (double(gradientEnd().x() / 100.0)  * double(bbox.width()));
        y2 = double(bbox.top()) + (double(gradientEnd().y() / 100.0) * double(bbox.height()));
    } else {
        x1 = gradientStart().x();
        y1 = gradientStart().y();
        x2 = gradientEnd().x();
        y2 = gradientEnd().y();
    }

    painter->setPen(Qt::NoPen);
    painter->setBrush(Qt::NoBrush);

    QLinearGradient gradient(QPointF(x1, y1), QPointF(x2, y2));
    if (spreadMethod() == SPREADMETHOD_REPEAT)
        gradient.setSpread(QGradient::RepeatSpread);
    else if (spreadMethod() == SPREADMETHOD_REFLECT)
        gradient.setSpread(QGradient::ReflectSpread);
    else
        gradient.setSpread(QGradient::PadSpread);

    double opacity = 1.0;

    // TODO: opacity fixes!

    if ((type & ApplyToFillTargetType) && renderStyle->svgStyle()->hasFill()) {
        fillColorArray(gradient, gradientStops(), opacity);

        QBrush brush(gradient);
        brush.setMatrix(gradientTransform());

        painter->setBrush(brush);
        context->setFillRule(renderStyle->svgStyle()->fillRule());
    }

    if ((type & ApplyToStrokeTargetType) && renderStyle->svgStyle()->hasStroke()) {
        fillColorArray(gradient, gradientStops(), opacity);

        QPen pen;
        QBrush brush(gradient);
        brush.setMatrix(gradientTransform());

        setPenProperties(object, renderStyle, pen);
        pen.setBrush(brush);

        painter->setPen(pen);
    }

    return true;
}

} // namespace WebCore

#endif

// vim:ts=4:noet
