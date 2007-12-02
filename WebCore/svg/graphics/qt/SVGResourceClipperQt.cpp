/*
    Copyright (C) 2004, 2005, 2006 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005, 2006 Rob Buis <buis@kde.org>
                  2005 Apple Computer, Inc.

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
#include "SVGResourceClipper.h"

#include "GraphicsContext.h"

#include <QPainter>
#include <QPainterPath>

namespace WebCore {

void SVGResourceClipper::applyClip(GraphicsContext* context, const FloatRect& boundingBox) const
{
    if (m_clipData.clipData().size() < 1)
        return;

    context->beginPath();

    QPainterPath newPath;

    bool heterogenousClipRules = false;
    WindRule clipRule = m_clipData.clipData()[0].windRule;

    unsigned int clipDataCount = m_clipData.clipData().size();
    for (unsigned int x = 0; x < clipDataCount; x++) {
        ClipData clipData = m_clipData.clipData()[x];
        if (clipData.windRule != clipRule)
            heterogenousClipRules = true;

        QPainterPath path = *(clipData.path.platformPath());
        if (path.isEmpty())
            continue;

        if (!newPath.isEmpty())
            newPath.closeSubpath();

        // Respect clipping units...
        QMatrix transform;

        if (clipData.bboxUnits) {
            transform.translate(boundingBox.x(), boundingBox.y());
            transform.scale(boundingBox.width(), boundingBox.height());
        }

        // TODO: support heterogenous clip rules!
        //clipRule = (clipData.windRule() == RULE_EVENODD ? Qt::OddEvenFill : Qt::WindingFill);

        for (int i = 0; i < path.elementCount(); ++i) {
            const QPainterPath::Element &cur = path.elementAt(i);

            switch (cur.type) {
                case QPainterPath::MoveToElement:
                    newPath.moveTo(QPointF(cur.x, cur.y) * transform);
                    break;
                case QPainterPath::LineToElement:
                    newPath.lineTo(QPointF(cur.x, cur.y) * transform);
                    break;
                case QPainterPath::CurveToElement:
                {
                    const QPainterPath::Element &c1 = path.elementAt(i + 1);
                    const QPainterPath::Element &c2 = path.elementAt(i + 2);

                    Q_ASSERT(c1.type == QPainterPath::CurveToDataElement);
                    Q_ASSERT(c2.type == QPainterPath::CurveToDataElement);

                    newPath.cubicTo(QPointF(cur.x, cur.y) * transform,
                                    QPointF(c1.x, c1.y) * transform,
                                    QPointF(c2.x, c2.y) * transform);

                    i += 2;
                    break;
                }
                case QPainterPath::CurveToDataElement:
                    Q_ASSERT(false);
                    break;
            }
        }
    }

    if (m_clipData.clipData().size()) {
        // FIXME!
        // We don't currently allow for heterogenous clip rules.
        // we would have to detect such, draw to a mask, and then clip
        // to that mask
        // if (!CGContextIsPathEmpty(cgContext)) {
            if (clipRule == RULE_EVENODD)
                newPath.setFillRule(Qt::OddEvenFill);
            else
                newPath.setFillRule(Qt::WindingFill);
        // }
    }

    QPainter* painter(context ? context->platformContext() : 0);
    Q_ASSERT(painter);

    painter->setClipPath(newPath);
}

} // namespace WebCore

#endif

// vim:ts=4:noet
