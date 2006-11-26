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
#include "SVGPaintServerRadialGradient.h"

#include "KRenderingDeviceQt.h"
#include "RenderPath.h"

#include <math.h>
#include <QRadialGradient>

namespace WebCore {

bool SVGPaintServerRadialGradient:: setup(KRenderingDeviceContext* context, const RenderObject* object, SVGPaintTargetType type) const
{
    KRenderingDeviceContextQt* qtContext = static_cast<KRenderingDeviceContextQt*>(context);
    Q_ASSERT(qtContext != 0);

    if (listener())
        listener()->resourceNotification();

    RenderStyle* renderStyle = object->style();

    qtContext->painter().setPen(Qt::NoPen);
    qtContext->painter().setBrush(Qt::NoBrush);
    QMatrix mat = qtContext->ctm();

    double cx, fx, cy, fy, r;
    if (boundingBoxMode()) {
        QRectF bbox = qtContext->pathBBox();
        cx = double(bbox.left()) + (double(gradientCenter().x() / 100.0) * double(bbox.width()));
        cy = double(bbox.top()) + (double(gradientCenter().y() / 100.0) * double(bbox.height()));
        fx = double(bbox.left()) + (double(gradientFocal().x() / 100.0) * double(bbox.width())) - cx;
        fy = double(bbox.top()) + (double(gradientFocal().y() / 100.0) * double(bbox.height())) - cy;
        r = double(gradientRadius() / 100.0) * (sqrt(pow(bbox.width(), 2) + pow(bbox.height(), 2)));

        float width = bbox.width();
        float height = bbox.height();

        int diff = int(width - height); // allow slight tolerance
        if (!(diff > -2 && diff < 2)) {
            // make elliptical or circular depending on bbox aspect ratio
            float ratioX = (width / height);
            float ratioY = (height / width);
            mat.scale((width > height) ? 1 : ratioX, (width > height) ? ratioY : 1);
        }
    } else {
        cx = gradientCenter().x();
        cy = gradientCenter().y();

        fx = gradientFocal().x();
        fy = gradientFocal().y();

        fx -= cx;
        fy -= cy;

        r = gradientRadius();
    }

    if (sqrt(fx * fx + fy * fy) > r) {
        // Spec: If (fx, fy) lies outside the circle defined by (cx, cy) and r, set (fx, fy)
        // to the point of intersection of the line through (fx, fy) and the circle.
        double angle = atan2(fy, fx);
        fx = int(cos(angle) * r) - 1;
        fy = int(sin(angle) * r) - 1;
    }

    QRadialGradient gradient(QPointF(cx, cy), gradientRadius(), QPointF(fx + cx, fy + cy));
    if (spreadMethod() == SPREADMETHOD_REPEAT)
        gradient.setSpread(QGradient::RepeatSpread);
    else if (spreadMethod() == SPREADMETHOD_REFLECT)
        gradient.setSpread(QGradient::ReflectSpread);
    else
        gradient.setSpread(QGradient::PadSpread);

    double opacity = 1.0;

    // TODO: Gradient transform + opacity fixes!

    // AffineTransform gradientTrans = gradientTransform();
    // gradientTrans.map(cx, cy, &cx, &cy);
    // qtContext->painter().setMatrix(mat);

    if ((type & ApplyToFillTargetType) && renderStyle->svgStyle()->hasFill()) {
        fillColorArray(gradient, gradientStops(), opacity);

        QBrush brush(gradient);

        qtContext->painter().setBrush(brush);
        qtContext->setFillRule(renderStyle->svgStyle()->fillRule());
    }

    if ((type & ApplyToStrokeTargetType) && renderStyle->svgStyle()->hasStroke()) {
        fillColorArray(gradient, gradientStops(), opacity);

        QPen pen;
        QBrush brush(gradient);

        setPenProperties(object, renderStyle, pen);
        pen.setBrush(brush);

        qtContext->painter().setPen(pen);
    }

    return true;
}

} // namespace WebCore

#endif

// vim:ts=4:noet
