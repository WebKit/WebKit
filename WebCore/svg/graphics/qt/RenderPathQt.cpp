/*
    Copyright (C) 2004, 2005, 2006 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005, 2006 Rob Buis <buis@kde.org>
                  2005 Eric Seidel <eric.seidel@kdemail.net>

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
#include "RenderPath.h"
#include "SVGRenderStyle.h"
#include "SVGPaintServer.h"

#include <QDebug>
#include <QPainterPathStroker>

namespace WebCore {
    
bool RenderPath::strokeContains(const FloatPoint& point, bool requiresStroke) const
{
    if (path().isEmpty())
        return false;

    if (requiresStroke && !SVGPaintServer::strokePaintServer(style(), this))
        return false;

    return false;
}

static QPainterPath getPathStroke(const QPainterPath &path, const RenderObject* object, const RenderStyle* style)
{ 
    QPainterPathStroker s;
    s.setWidth(SVGRenderStyle::cssPrimitiveToLength(object, style->svgStyle()->strokeWidth(), 1.0));

    if (style->svgStyle()->capStyle() == ButtCap)
        s.setCapStyle(Qt::FlatCap);
    else if (style->svgStyle()->capStyle() == RoundCap)
        s.setCapStyle(Qt::RoundCap);

    if (style->svgStyle()->joinStyle() == MiterJoin) {
        s.setJoinStyle(Qt::MiterJoin);
        s.setMiterLimit((qreal) style->svgStyle()->strokeMiterLimit());
    } else if(style->svgStyle()->joinStyle() == RoundJoin)
        s.setJoinStyle(Qt::RoundJoin);

    const DashArray& dashes = WebCore::dashArrayFromRenderingStyle(style);
    double dashOffset = SVGRenderStyle::cssPrimitiveToLength(object, style->svgStyle()->strokeDashOffset(), 0.0);

    unsigned int dashLength = !dashes.isEmpty() ? dashes.size() : 0;
    if(dashLength) {
        QVector<qreal> pattern;
        unsigned int count = (dashLength % 2) == 0 ? dashLength : dashLength * 2;

        for(unsigned int i = 0; i < count; i++)
            pattern.append(dashes[i % dashLength] / (float)s.width());

        s.setDashPattern(pattern);
    
        Q_UNUSED(dashOffset);
        // TODO: dash-offset, does/will qt4 API allow it? (Rob)
    }

    return s.createStroke(path);
}

FloatRect RenderPath::strokeBBox() const
{
    QPainterPath outline = getPathStroke(*(path().platformPath()), this, style());
    return outline.boundingRect();
}

}

// vim:ts=4:noet
