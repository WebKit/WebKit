/*
    Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2007 Rob Buis <buis@kde.org>

    Based on khtml code by:
    Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
    Copyright (C) 1999-2003 Lars Knoll (knoll@kde.org)
    Copyright (C) 2002-2003 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Apple Computer, Inc.

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
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#if ENABLE(SVG)
#include "SVGRenderStyleDefs.h"

#include "RenderStyle.h"
#include "SVGRenderStyle.h"

using namespace WebCore;

StyleFillData::StyleFillData()
{
    paint = SVGRenderStyle::initialFillPaint();
    opacity = SVGRenderStyle::initialFillOpacity();
}

StyleFillData::StyleFillData(const StyleFillData& other)
    : RefCounted<StyleFillData>()
{
    paint = other.paint;
    opacity = other.opacity;
}

bool StyleFillData::operator==(const StyleFillData &other) const
{
    if (opacity != other.opacity)
        return false;

    if (!paint || !other.paint)
        return paint == other.paint;

    if (paint->paintType() != other.paint->paintType())
        return false;

    if (paint->paintType() == SVGPaint::SVG_PAINTTYPE_URI)
        return paint->uri() == other.paint->uri();

    if (paint->paintType() == SVGPaint::SVG_PAINTTYPE_RGBCOLOR)
        return paint->color() == other.paint->color();

    return paint == other.paint;
}

StyleStrokeData::StyleStrokeData()
{
    width = SVGRenderStyle::initialStrokeWidth();
    paint = SVGRenderStyle::initialStrokePaint();
    opacity = SVGRenderStyle::initialStrokeOpacity();
    miterLimit = SVGRenderStyle::initialStrokeMiterLimit();
    dashOffset = SVGRenderStyle::initialStrokeDashOffset();
    dashArray = SVGRenderStyle::initialStrokeDashArray();
}

StyleStrokeData::StyleStrokeData(const StyleStrokeData& other)
    : RefCounted<StyleStrokeData>()
{
    width = other.width;
    paint = other.paint;
    opacity = other.opacity;
    miterLimit = other.miterLimit;
    dashOffset = other.dashOffset;
    dashArray = other.dashArray;
}

bool StyleStrokeData::operator==(const StyleStrokeData &other) const
{
    return (paint == other.paint) &&
           (width == other.width) &&
           (opacity == other.opacity) &&
           (miterLimit == other.miterLimit) &&
           (dashOffset == other.dashOffset) &&
           (dashArray == other.dashArray);
}

StyleStopData::StyleStopData()
{
    color = SVGRenderStyle::initialStopColor();
    opacity = SVGRenderStyle::initialStopOpacity();
}

StyleStopData::StyleStopData(const StyleStopData& other)
    : RefCounted<StyleStopData>()
{
    color = other.color;
    opacity = other.opacity;
}

bool StyleStopData::operator==(const StyleStopData &other) const
{
    return (color == other.color) &&
           (opacity == other.opacity);
}

StyleTextData::StyleTextData()
{
    kerning = SVGRenderStyle::initialKerning();
}

StyleTextData::StyleTextData(const StyleTextData& other)
    : RefCounted<StyleTextData>()
{
    kerning = other.kerning;
}

bool StyleTextData::operator==(const StyleTextData& other) const
{
    return kerning == other.kerning;
}

StyleClipData::StyleClipData()
{
    clipPath = SVGRenderStyle::initialClipPath();
}

StyleClipData::StyleClipData(const StyleClipData& other)
    : RefCounted<StyleClipData>()
{
    clipPath = other.clipPath;
}

bool StyleClipData::operator==(const StyleClipData &other) const
{
    return (clipPath == other.clipPath);
}

StyleMaskData::StyleMaskData()
{
    maskElement = SVGRenderStyle::initialMaskElement();
}

StyleMaskData::StyleMaskData(const StyleMaskData& other)
    : RefCounted<StyleMaskData>()
{
    maskElement = other.maskElement;
}

bool StyleMaskData::operator==(const StyleMaskData &other) const
{
    return (maskElement == other.maskElement);
}

StyleMarkerData::StyleMarkerData()
{
    startMarker = SVGRenderStyle::initialStartMarker();
    midMarker = SVGRenderStyle::initialMidMarker();
    endMarker = SVGRenderStyle::initialEndMarker();
}

StyleMarkerData::StyleMarkerData(const StyleMarkerData& other)
    : RefCounted<StyleMarkerData>()
{
    startMarker = other.startMarker;
    midMarker = other.midMarker;
    endMarker = other.endMarker;
}

bool StyleMarkerData::operator==(const StyleMarkerData &other) const
{
    return (startMarker == other.startMarker && midMarker == other.midMarker && endMarker == other.endMarker);
}

StyleMiscData::StyleMiscData()
{
    floodColor = SVGRenderStyle::initialFloodColor();
    floodOpacity = SVGRenderStyle::initialFloodOpacity();
    lightingColor = SVGRenderStyle::initialLightingColor();
    baselineShiftValue = SVGRenderStyle::initialBaselineShiftValue();
}

StyleMiscData::StyleMiscData(const StyleMiscData& other)
    : RefCounted<StyleMiscData>()
{
    filter = other.filter;
    floodColor = other.floodColor;
    floodOpacity = other.floodOpacity;
    lightingColor = other.lightingColor;
    baselineShiftValue = other.baselineShiftValue;
}

bool StyleMiscData::operator==(const StyleMiscData &other) const
{
    return filter == other.filter
           && floodOpacity == other.floodOpacity
           && floodColor == other.floodColor
           && lightingColor == other.lightingColor
           && baselineShiftValue == other.baselineShiftValue;
}

StyleShadowSVGData::StyleShadowSVGData()
{
}

StyleShadowSVGData::StyleShadowSVGData(const StyleShadowSVGData& other)
    : RefCounted<StyleShadowSVGData>()
    , shadow(other.shadow ? new ShadowData(*other.shadow) : 0)
{
}

bool StyleShadowSVGData::operator==(const StyleShadowSVGData& other) const
{
    if ((!shadow && other.shadow) || (shadow && !other.shadow))
        return false;
    if (shadow && other.shadow && (*shadow != *other.shadow))
        return false;
    return true;
}

#endif // ENABLE(SVG)

// vim:ts=4:noet
