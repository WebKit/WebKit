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
#include "SVGPaintServerGradient.h"

#include "GraphicsContext.h"
#include "RenderObject.h"
#include "RenderStyle.h"
#include "SVGGradientElement.h"

#include <QPainter>
#include <QPainterPath>
#include <QColor>
#include <QGradient>

namespace WebCore {

// Helper function used by linear & radial gradient
void SVGPaintServerGradient::fillColorArray(QGradient& gradient, const Vector<SVGGradientStop>& stops,
                                            float opacity) const
{
    for (unsigned i = 0; i < stops.size(); ++i) {
        float offset = stops[i].first;
        Color color = stops[i].second;

        QColor c(color.red(), color.green(), color.blue());
        c.setAlpha(int(color.alpha() * opacity));

        gradient.setColorAt(offset, c);
    }
}

bool SVGPaintServerGradient::setup(GraphicsContext*& context, const RenderObject* object,
                                   SVGPaintTargetType type, bool isPaintingText) const
{
    m_ownerElement->buildGradient();

    QPainter* painter(context ? context->platformContext() : 0);
    Q_ASSERT(painter);

    QPainterPath* path(context ? context->currentPath() : 0);
    Q_ASSERT(path);

    const SVGRenderStyle* svgStyle = object->style()->svgStyle();
    RenderStyle* style = object->style();

    QGradient gradient = setupGradient(context, object);

    painter->setPen(Qt::NoPen);
    painter->setBrush(Qt::NoBrush);

    if (spreadMethod() == SpreadMethodRepeat)
        gradient.setSpread(QGradient::RepeatSpread);
    else if (spreadMethod() == SpreadMethodReflect)
        gradient.setSpread(QGradient::ReflectSpread);
    else
        gradient.setSpread(QGradient::PadSpread);    
    double opacity = 1.0;

    if ((type & ApplyToFillTargetType) && svgStyle->hasFill()) {
        fillColorArray(gradient, gradientStops(), opacity);

        QBrush brush(gradient);
        brush.setMatrix(gradientTransform());

        painter->setBrush(brush);
        context->setFillRule(svgStyle->fillRule());
    }

    if ((type & ApplyToStrokeTargetType) && svgStyle->hasStroke()) {
        fillColorArray(gradient, gradientStops(), opacity);

        QPen pen;
        QBrush brush(gradient);
        brush.setMatrix(gradientTransform());
        pen.setBrush(brush);
        painter->setPen(pen);

        applyStrokeStyleToContext(context, style, object);
    }

    return true;
}

} // namespace WebCore

#endif

// vim:ts=4:noet
