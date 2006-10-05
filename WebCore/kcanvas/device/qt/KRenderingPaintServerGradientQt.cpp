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

#include <math.h>
#include <QPointF>

#include "RenderStyle.h"
#include "AffineTransform.h"
#include "KRenderingDeviceQt.h"
#include "KCanvasRenderingStyle.h"
#include "KRenderingFillPainter.h"
#include "KRenderingStrokePainter.h"
#include "KRenderingPaintServerGradientQt.h"

namespace WebCore {

void fill_color_array(QGradient& gradient, const Vector<KCGradientStop>& stops, float opacity)
{
    for (unsigned i = 0; i < stops.size(); ++i) {
        float offset = stops[i].first;
        Color color = stops[i].second;
        
        QColor c(color.red(), color.green(), color.blue());
        c.setAlpha(int(color.alpha() * opacity));

        gradient.setColorAt(offset, c);
    }
}

// KRenderingPaintServerLinearGradientQt
KRenderingPaintServerLinearGradientQt::KRenderingPaintServerLinearGradientQt()
    : KRenderingPaintServerLinearGradient()
    , KRenderingPaintServerQt()
{
}

void KRenderingPaintServerLinearGradientQt::renderPath(KRenderingDeviceContext* context, const RenderPath* path, KCPaintTargetType type) const
{
    RenderStyle* renderStyle = path->style();
    KRenderingDeviceContextQt* qtContext = static_cast<KRenderingDeviceContextQt*>(context);

    if ((type & APPLY_TO_FILL) && renderStyle->svgStyle()->hasFill())
        qtContext->fillPath();

    if ((type & APPLY_TO_STROKE) && renderStyle->svgStyle()->hasStroke())
        qtContext->strokePath();
}

bool KRenderingPaintServerLinearGradientQt::setup(KRenderingDeviceContext* context, const RenderObject* object, KCPaintTargetType type) const
{
    KRenderingDeviceContextQt* qtContext = static_cast<KRenderingDeviceContextQt*>(context);
    Q_ASSERT(qtContext != 0);

    if (listener())
        listener()->resourceNotification();

    RenderStyle* renderStyle = object->style();

    double x1, x2, y1, y2;
    if (boundingBoxMode()) {
        QRectF bbox = qtContext->pathBBox();
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

    qtContext->painter().setPen(Qt::NoPen);
    qtContext->painter().setBrush(Qt::NoBrush);

    QLinearGradient gradient(QPointF(x1, y1), QPointF(x2, y2));
    if (spreadMethod() == SPREADMETHOD_REPEAT)
        gradient.setSpread(QGradient::RepeatSpread);
    else if (spreadMethod() == SPREADMETHOD_REFLECT)
        gradient.setSpread(QGradient::ReflectSpread);
    else
        gradient.setSpread(QGradient::PadSpread);

    double opacity = 1.0;

    // TODO: Gradient transform + opacity fixes! 

    if ((type & APPLY_TO_FILL) && renderStyle->svgStyle()->hasFill()) {
        KRenderingFillPainter fillPainter = KSVGPainterFactory::fillPainter(renderStyle, object);
        fill_color_array(gradient, gradientStops(), opacity);

        QBrush brush(gradient);

        qtContext->painter().setBrush(brush);
        qtContext->setFillRule(fillPainter.fillRule());
    }

    if ((type & APPLY_TO_STROKE) && renderStyle->svgStyle()->hasStroke()) {
        KRenderingStrokePainter strokePainter = KSVGPainterFactory::strokePainter(renderStyle, object);
        fill_color_array(gradient, gradientStops(), opacity);

        QPen pen;
        QBrush brush(gradient);

        setPenProperties(strokePainter, pen);
        pen.setBrush(brush);

        qtContext->painter().setPen(pen);
    }

    return true;
}

void KRenderingPaintServerLinearGradientQt::teardown(KRenderingDeviceContext*, const RenderObject*, KCPaintTargetType) const
{
}

void KRenderingPaintServerLinearGradientQt::draw(KRenderingDeviceContext* context, const RenderPath* path, KCPaintTargetType type) const
{
    if (!setup(context, path, type))
        return;

    renderPath(context, path, type);
    teardown(context, path, type);
}

// KRenderingPaintServerRadialGradientQt
KRenderingPaintServerRadialGradientQt::KRenderingPaintServerRadialGradientQt()
    : KRenderingPaintServerRadialGradient()
    , KRenderingPaintServerQt()
{
}

bool KRenderingPaintServerRadialGradientQt::setup(KRenderingDeviceContext* context, const RenderObject* object, KCPaintTargetType type) const
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

    if ((type & APPLY_TO_FILL) && renderStyle->svgStyle()->hasFill()) {
        KRenderingFillPainter fillPainter = KSVGPainterFactory::fillPainter(renderStyle, object);
        fill_color_array(gradient, gradientStops(), opacity);

        QBrush brush(gradient);

        qtContext->painter().setBrush(brush);
        qtContext->setFillRule(fillPainter.fillRule());
    }

    if ((type & APPLY_TO_STROKE) && renderStyle->svgStyle()->hasStroke()) {
        KRenderingStrokePainter strokePainter = KSVGPainterFactory::strokePainter(renderStyle, object);
        fill_color_array(gradient, gradientStops(), opacity);

        QPen pen;
        QBrush brush(gradient);

        setPenProperties(strokePainter, pen);
        pen.setBrush(brush);

        qtContext->painter().setPen(pen);
    }

    return true;
}

void KRenderingPaintServerRadialGradientQt::draw(KRenderingDeviceContext* context, const RenderPath* path, KCPaintTargetType type) const
{
    if (!setup(context, path, type))
        return;

    renderPath(context, path, type);
    teardown(context, path, type);
}

void KRenderingPaintServerRadialGradientQt::teardown(KRenderingDeviceContext*, const RenderObject*, KCPaintTargetType) const
{
}

void KRenderingPaintServerRadialGradientQt::renderPath(KRenderingDeviceContext* context, const RenderPath* path, KCPaintTargetType type) const
{
    RenderStyle* renderStyle = path->style();
    KRenderingDeviceContextQt* qtContext = static_cast<KRenderingDeviceContextQt*>(context);

    if ((type & APPLY_TO_FILL) && renderStyle->svgStyle()->hasFill())
        qtContext->fillPath();

    if ((type & APPLY_TO_STROKE) && renderStyle->svgStyle()->hasStroke())
        qtContext->strokePath();
}

}

// vim:ts=4:noet
