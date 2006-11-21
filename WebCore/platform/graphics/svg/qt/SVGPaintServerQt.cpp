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
#include "SVGPaintServer.h"

#include "KCanvasRenderingStyle.h"
#include "KRenderingDeviceQt.h"
#include "RenderPath.h"

#include <QPen>
#include <QVector>

namespace WebCore {

void SVGPaintServer::setPenProperties(const RenderObject* object, const RenderStyle* style, QPen& pen) const
{
    pen.setWidthF(KSVGPainterFactory::cssPrimitiveToLength(object, style->svgStyle()->strokeWidth(), 1.0));

    if (style->svgStyle()->capStyle() == ButtCap)
        pen.setCapStyle(Qt::FlatCap);
    else if (style->svgStyle()->capStyle() == RoundCap)
        pen.setCapStyle(Qt::RoundCap);

    if (style->svgStyle()->joinStyle() == MiterJoin) {
        pen.setJoinStyle(Qt::MiterJoin);
        pen.setMiterLimit((qreal) style->svgStyle()->strokeMiterLimit());
    } else if(style->svgStyle()->joinStyle() == RoundJoin)
        pen.setJoinStyle(Qt::RoundJoin);

    const KCDashArray& dashes = KSVGPainterFactory::dashArrayFromRenderingStyle(style);
    double dashOffset = KSVGPainterFactory::cssPrimitiveToLength(object, style->svgStyle()->strokeDashOffset(), 0.0);

    unsigned int dashLength = !dashes.isEmpty() ? dashes.size() : 0;
    if(dashLength) {
        QVector<qreal> pattern;
        unsigned int count = (dashLength % 2) == 0 ? dashLength : dashLength * 2;

        for(unsigned int i = 0; i < count; i++)
            pattern.append(dashes[i % dashLength] / (float)pen.widthF());

        pen.setDashPattern(pattern);

        Q_UNUSED(dashOffset);
        // TODO: dash-offset, does/will qt4 API allow it? (Rob)
    }
}

void SVGPaintServer::draw(KRenderingDeviceContext* context, const RenderPath* path, SVGPaintTargetType type) const
{
    if (!setup(context, path, type))
        return;

    renderPath(context, path, type);
    teardown(context, path, type);
}

void SVGPaintServer::teardown(KRenderingDeviceContext*, const RenderObject*, SVGPaintTargetType) const
{
    // no-op
}

void SVGPaintServer::renderPath(KRenderingDeviceContext* context, const RenderPath* path, SVGPaintTargetType type) const
{
    RenderStyle* renderStyle = path->style();
    KRenderingDeviceContextQt* qtContext = static_cast<KRenderingDeviceContextQt*>(context);

    if ((type & APPLY_TO_FILL) && renderStyle->svgStyle()->hasFill())
        qtContext->fillPath();

    if ((type & APPLY_TO_STROKE) && renderStyle->svgStyle()->hasStroke())
        qtContext->strokePath();
}

} // namespace WebCore

#endif

// vim:ts=4:noet
