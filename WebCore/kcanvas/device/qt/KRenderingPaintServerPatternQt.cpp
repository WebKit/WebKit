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

#include <qrect.h>
#include <qimage.h>

#include <math.h>
#include <QPointF>
#include <QPainterPath>

#include "RenderStyle.h"
#include "KRenderingDeviceQt.h"
#include "KCanvasRenderingStyle.h"
#include "KRenderingFillPainter.h"
#include "KRenderingStrokePainter.h"
#include "KRenderingPaintServerPatternQt.h"
#include "KCanvasImage.h"

namespace WebCore {

// KRenderingPaintServerPatternQt
KRenderingPaintServerPatternQt::KRenderingPaintServerPatternQt()
    : KRenderingPaintServerPattern()
    , KRenderingPaintServerQt()
{
}

KRenderingPaintServerPatternQt::~KRenderingPaintServerPatternQt()
{
}

void KRenderingPaintServerPatternQt::renderPath(KRenderingDeviceContext* context, const RenderPath* path, KCPaintTargetType type) const
{
    RenderStyle* renderStyle = path->style();
    KRenderingDeviceContextQt* qtContext = static_cast<KRenderingDeviceContextQt*>(context);

    if ((type & APPLY_TO_FILL) && renderStyle->svgStyle()->hasFill())
        qtContext->fillPath();

    if ((type & APPLY_TO_STROKE) && renderStyle->svgStyle()->hasStroke())
        qtContext->strokePath();
}

bool KRenderingPaintServerPatternQt::setup(KRenderingDeviceContext* context, const RenderObject* object, KCPaintTargetType type) const
{
/*
    KRenderingDeviceContextQt* qtContext = static_cast<KRenderingDeviceContextQt*>(context);
    Q_ASSERT(qtContext != 0);

    QPainterPath* _path = static_cast<QPainterPath*>(qtContext->path());
    Q_ASSERT(_path != 0);
 
    if (listener()) {
        listener()->resourceNotification();
    }

    RenderStyle* renderStyle = object->style();

    qtContext->painter().setPen(Qt::NoPen);
    qtContext->painter().setBrush(Qt::NoBrush);
    QImage* patternimage = new QImage(tile()->bits(), tile()->width(), tile()->height(), QImage::Format_ARGB32_Premultiplied);
    patternimage->setAlphaBuffer(true);
    if (type & APPLY_TO_FILL) {
        //QColor c = color();
        //c.setAlphaF(style->fillPainter()->opacity() * style->opacity() * opacity());
        KRenderingFillPainter fillPainter = KSVGPainterFactory::fillPainter(renderStyle, object);
        QBrush brush(QPixmap::fromImage(*patternimage));
        _path->setFillRule(fillPainter.fillRule() == RULE_EVENODD ? Qt::OddEvenFill : Qt::WindingFill);
        qtContext->painter().setBrush(brush);
    }
    if (type & APPLY_TO_STROKE) {
        //QColor c = color();
        //c.setAlphaF(style->strokePainter()->opacity() * style->opacity() * opacity());
        KRenderingStrokePainter strokePainter = KSVGPainterFactory::strokePainter(renderStyle, object);

        QPen pen;
        QBrush brush(QPixmap::fromImage(*patternimage));

        setPenProperties(strokePainter, pen);
        pen.setBrush(brush);
        qtContext->painter().setPen(pen);
    }

    qtContext->painter().drawPath(*_path);

    delete patternimage;
*/

    return true;
}

void KRenderingPaintServerPatternQt::teardown(KRenderingDeviceContext*, const RenderObject*, KCPaintTargetType) const
{
}

void KRenderingPaintServerPatternQt::draw(KRenderingDeviceContext* context, const RenderPath* path, KCPaintTargetType type) const
{
    if (!setup(context, path, type))
        return;

    renderPath(context, path, type);
    teardown(context, path, type);
}

}

// vim:ts=4:noet

