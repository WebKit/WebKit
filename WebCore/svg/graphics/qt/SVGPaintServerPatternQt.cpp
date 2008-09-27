/*
    Copyright (C) 2006 Nikolas Zimmermann <wildfox@kde.org>
    Copyright (C) 2008 Dirk Schulze <vbs85@gmx.de>

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
#include "SVGPaintServerPattern.h"

#include "AffineTransform.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "Pattern.h"
#include "RenderObject.h"
#include "SVGPatternElement.h"

#include <QPainter>
#include <QPainterPath>

namespace WebCore {

bool SVGPaintServerPattern::setup(GraphicsContext*& context, const RenderObject* object, SVGPaintTargetType type, bool isPaintingText) const
{
    QPainter* painter(context ? context->platformContext() : 0);
    Q_ASSERT(painter);

    QPainterPath* path(context ? context->currentPath() : 0);
    Q_ASSERT(path);

    RenderStyle* style = object ? object->style() : 0;
    const SVGRenderStyle* svgStyle = object ? object->style()->svgStyle() : 0;

    FloatRect targetRect = object->relativeBBox(false);
    m_ownerElement->buildPattern(targetRect);

    if (!tile())
        return false;

    RefPtr<Pattern> pattern = Pattern::create(tile()->image(), true, true);

    painter->setPen(Qt::NoPen);
    painter->setBrush(Qt::NoBrush);

    AffineTransform affine;
    affine.translate(patternBoundaries().x(), patternBoundaries().y());
    affine.multiply(patternTransform());

    QBrush brush(pattern->createPlatformPattern(affine));
    if ((type & ApplyToFillTargetType) && svgStyle->hasFill()) {
        painter->setBrush(brush);
        context->setFillRule(svgStyle->fillRule());
    }

    if ((type & ApplyToStrokeTargetType) && svgStyle->hasStroke()) {
        QPen pen;
        pen.setBrush(brush);
        painter->setPen(pen);
        applyStrokeStyleToContext(context, style, object);
    }

    return true;
}

} // namespace WebCore

#endif

// vim:ts=4:noet
