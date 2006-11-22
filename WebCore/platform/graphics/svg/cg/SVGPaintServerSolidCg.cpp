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

#include "RenderObject.h"
#include "KRenderingDeviceQuartz.h"
#include "QuartzSupport.h"

namespace WebCore {

bool SVGPaintServerSolid::setup(KRenderingDeviceContext* context, const RenderObject* object, SVGPaintTargetType type) const
{
    KRenderingDeviceContextQuartz* quartzContext = static_cast<KRenderingDeviceContextQuartz*>(context);
    CGContextRef contextRef = quartzContext->cgContext();
    RenderStyle* style = object->style();

    CGContextSetAlpha(contextRef, style->opacity());

    static CGColorSpaceRef deviceRGBColorSpace = CGColorSpaceCreateDeviceRGB(); // This should be shared from GraphicsContext, or some other central location

    if ((type & ApplyToFillTargetType) && style->svgStyle()->hasFill()) {
        CGFloat colorComponents[4];
        color().getRGBA(colorComponents[0], colorComponents[1], colorComponents[2], colorComponents[3]);
        ASSERT(!color().hasAlpha());
        colorComponents[3] = style->svgStyle()->fillOpacity(); // SVG/CSS colors are not specified w/o alpha
        CGContextSetFillColorSpace(contextRef, deviceRGBColorSpace);
        CGContextSetFillColor(contextRef, colorComponents);
        if (isPaintingText()) {
            const_cast<RenderObject*>(object)->style()->setColor(color());
            CGContextSetTextDrawingMode(contextRef, kCGTextFill);
        }
    }

    if ((type & ApplyToStrokeTargetType) && style->svgStyle()->hasStroke()) {
        CGFloat colorComponents[4];
        color().getRGBA(colorComponents[0], colorComponents[1], colorComponents[2], colorComponents[3]);
        ASSERT(!color().hasAlpha());
        colorComponents[3] = style->svgStyle()->strokeOpacity(); // SVG/CSS colors are not specified w/o alpha
        CGContextSetStrokeColorSpace(contextRef, deviceRGBColorSpace);
        CGContextSetStrokeColor(contextRef, colorComponents);
        applyStrokeStyleToContext(contextRef, style, object);
        if (isPaintingText()) {
            const_cast<RenderObject*>(object)->style()->setColor(color());
            CGContextSetTextDrawingMode(contextRef, kCGTextStroke);
        }
    }

    return true;
}

} // namespace WebCore

#endif

// vim:ts=4:noet
