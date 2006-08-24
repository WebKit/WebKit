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
#include <QBrush>
#include <QPainter>

#include "RenderStyle.h"
#include "KRenderingDeviceQt.h"
#include "KCanvasRenderingStyle.h"
#include "KRenderingFillPainter.h"
#include "KRenderingStrokePainter.h"
#include "KRenderingPaintServerSolidQt.h"

namespace WebCore {

KRenderingPaintServerSolidQt::KRenderingPaintServerSolidQt()
    : KRenderingPaintServerSolid()
    , KRenderingPaintServerQt()
{
}

KRenderingPaintServerSolidQt::~KRenderingPaintServerSolidQt()
{
}

// 'Solid' interface
void KRenderingPaintServerSolidQt::draw(KRenderingDeviceContext* context, const RenderPath* path, KCPaintTargetType type) const
{
    if (!setup(context, path, type))
        return;

    renderPath(context, path, type);
    teardown(context, path, type);
}

bool KRenderingPaintServerSolidQt::setup(KRenderingDeviceContext* context, const RenderObject* object, KCPaintTargetType type) const
{
    KRenderingDeviceContextQt* qtContext = static_cast<KRenderingDeviceContextQt*>(context);

    RenderStyle* renderStyle = object->style();
    // TODO? qtContext->painter().setOpacity(renderStyle->opacity());

    QColor c = color();

    if ((type & APPLY_TO_FILL) && KSVGPainterFactory::isFilled(renderStyle)) {
        KRenderingFillPainter fillPainter = KSVGPainterFactory::fillPainter(renderStyle, object);
        c.setAlphaF(fillPainter.opacity());

        QBrush brush(c);
        qtContext->painter().setBrush(brush);
        qtContext->setFillRule(fillPainter.fillRule());

        /* if(isPaintingText()) ... */
    }

    if((type & APPLY_TO_STROKE) && KSVGPainterFactory::isStroked(renderStyle)) {
        KRenderingStrokePainter strokePainter = KSVGPainterFactory::strokePainter(renderStyle, object);
        c.setAlphaF(strokePainter.opacity());

        QPen pen(c);
        setPenProperties(strokePainter, pen);
        qtContext->painter().setPen(pen);

        /* if(isPaintingText()) ... */
    }

    return true;
}

void KRenderingPaintServerSolidQt::teardown(KRenderingDeviceContext*, const RenderObject*, KCPaintTargetType) const
{
}

void KRenderingPaintServerSolidQt::renderPath(KRenderingDeviceContext* context, const RenderPath* path, KCPaintTargetType type) const
{
    RenderStyle* renderStyle = path->style();
    KRenderingDeviceContextQt* qtContext = static_cast<KRenderingDeviceContextQt*>(context);

    if ((type & APPLY_TO_FILL) && KSVGPainterFactory::isFilled(renderStyle))
        qtContext->fillPath();

    if ((type & APPLY_TO_STROKE) && KSVGPainterFactory::isStroked(renderStyle))
        qtContext->strokePath();
}

}

// vim:ts=4:noet
