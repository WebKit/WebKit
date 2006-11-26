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
#include "SVGPaintServerSolid.h"

#include "KRenderingDeviceQt.h"
#include "RenderPath.h"

namespace WebCore {

bool SVGPaintServerSolid::setup(KRenderingDeviceContext* context, const RenderObject* object, SVGPaintTargetType type) const
{
    KRenderingDeviceContextQt* qtContext = static_cast<KRenderingDeviceContextQt*>(context);

    RenderStyle* renderStyle = object->style();
    // TODO? qtContext->painter().setOpacity(renderStyle->opacity());

    QColor c = color();

    if ((type & ApplyToFillTargetType) && renderStyle->svgStyle()->hasFill()) {
        c.setAlphaF(renderStyle->svgStyle()->fillOpacity());

        QBrush brush(c);
        qtContext->painter().setBrush(brush);
        qtContext->setFillRule(renderStyle->svgStyle()->fillRule());

        /* if(isPaintingText()) ... */
    }

    if ((type & ApplyToStrokeTargetType) && renderStyle->svgStyle()->hasStroke()) {
        c.setAlphaF(renderStyle->svgStyle()->strokeOpacity());

        QPen pen(c);
        setPenProperties(object, renderStyle, pen);
        qtContext->painter().setPen(pen);

        /* if(isPaintingText()) ... */
    }

    return true;
}

} // namespace WebCore

#endif

// vim:ts=4:noet
