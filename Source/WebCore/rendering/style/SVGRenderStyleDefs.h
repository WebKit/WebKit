/*
    Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
    Copyright (C) Research In Motion Limited 2010. All rights reserved.
    Copyright (C) 2014 Adobe Systems Incorporated. All rights reserved.

    Based on khtml code by:
    Copyright (C) 2000-2003 Lars Knoll (knoll@kde.org)
              (C) 2000 Antti Koivisto (koivisto@kde.org)
              (C) 2000-2003 Dirk Mueller (mueller@kde.org)
              (C) 2002-2003 Apple Inc.

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

#pragma once

#include "Length.h"
#include "SVGLengthValue.h"
#include "ShadowData.h"
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class CSSValue;
class CSSValueList;
class SVGPaint;

enum class SVGPaintType : uint8_t {
    RGBColor,
    None,
    CurrentColor,
    URINone,
    URICurrentColor,
    URIRGBColor,
    URI
};

enum class BaselineShift : uint8_t {
    Baseline,
    Sub,
    Super,
    Length
};

enum class TextAnchor : uint8_t {
    Start,
    Middle,
    End
};

enum class ColorInterpolation : uint8_t {
    Auto,
    SRGB,
    LinearRGB
};

enum class ColorRendering : uint8_t {
    Auto,
    OptimizeSpeed,
    OptimizeQuality
};

enum class ShapeRendering : uint8_t {
    Auto,
    OptimizeSpeed,
    CrispEdges,
    GeometricPrecision
};

enum class GlyphOrientation : uint8_t {
    Degrees0,
    Degrees90,
    Degrees180,
    Degrees270,
    Auto
};

enum class AlignmentBaseline : uint8_t {
    Auto,
    Baseline,
    BeforeEdge,
    TextBeforeEdge,
    Middle,
    Central,
    AfterEdge,
    TextAfterEdge,
    Ideographic,
    Alphabetic,
    Hanging,
    Mathematical
};

enum class DominantBaseline : uint8_t {
    Auto,
    UseScript,
    NoChange,
    ResetSize,
    Ideographic,
    Alphabetic,
    Hanging,
    Mathematical,
    Central,
    Middle,
    TextAfterEdge,
    TextBeforeEdge
};

enum class VectorEffect : uint8_t {
    None,
    NonScalingStroke
};

enum class BufferedRendering : uint8_t {
    Auto,
    Dynamic,
    Static
};

enum class MaskType : uint8_t {
    Luminance,
    Alpha
};

// Inherited/Non-Inherited Style Datastructures
class StyleFillData : public RefCounted<StyleFillData> {
public:
    static Ref<StyleFillData> create() { return adoptRef(*new StyleFillData); }
    Ref<StyleFillData> copy() const;

    bool operator==(const StyleFillData&) const;
    bool operator!=(const StyleFillData& other) const
    {
        return !(*this == other);
    }

    float opacity;
    Color paintColor;
    Color visitedLinkPaintColor;
    String paintUri;
    String visitedLinkPaintUri;
    SVGPaintType paintType;
    SVGPaintType visitedLinkPaintType;

private:
    StyleFillData();
    StyleFillData(const StyleFillData&);
};

class StyleStrokeData : public RefCounted<StyleStrokeData> {
public:
    static Ref<StyleStrokeData> create() { return adoptRef(*new StyleStrokeData); }
    Ref<StyleStrokeData> copy() const;

    bool operator==(const StyleStrokeData&) const;
    bool operator!=(const StyleStrokeData& other) const
    {
        return !(*this == other);
    }

    float opacity;

    Color paintColor;
    Color visitedLinkPaintColor;

    String paintUri;
    String visitedLinkPaintUri;

    Length dashOffset;
    Vector<SVGLengthValue> dashArray;

    SVGPaintType paintType;
    SVGPaintType visitedLinkPaintType;

private:
    StyleStrokeData();
    StyleStrokeData(const StyleStrokeData&);
};

class StyleStopData : public RefCounted<StyleStopData> {
public:
    static Ref<StyleStopData> create() { return adoptRef(*new StyleStopData); }
    Ref<StyleStopData> copy() const;

    bool operator==(const StyleStopData&) const;
    bool operator!=(const StyleStopData& other) const
    {
        return !(*this == other);
    }

    float opacity;
    Color color;

private:
    StyleStopData();
    StyleStopData(const StyleStopData&);
};

class StyleTextData : public RefCounted<StyleTextData> {
public:
    static Ref<StyleTextData> create() { return adoptRef(*new StyleTextData); }
    Ref<StyleTextData> copy() const;
    
    bool operator==(const StyleTextData& other) const;
    bool operator!=(const StyleTextData& other) const
    {
        return !(*this == other);
    }

    SVGLengthValue kerning;

private:
    StyleTextData();
    StyleTextData(const StyleTextData&);
};

// Note: the rule for this class is, *no inheritance* of these props
class StyleMiscData : public RefCounted<StyleMiscData> {
public:
    static Ref<StyleMiscData> create() { return adoptRef(*new StyleMiscData); }
    Ref<StyleMiscData> copy() const;

    bool operator==(const StyleMiscData&) const;
    bool operator!=(const StyleMiscData& other) const
    {
        return !(*this == other);
    }

    float floodOpacity;
    Color floodColor;
    Color lightingColor;

    // non-inherited text stuff lives here not in StyleTextData.
    SVGLengthValue baselineShiftValue;

private:
    StyleMiscData();
    StyleMiscData(const StyleMiscData&);
};

class StyleShadowSVGData : public RefCounted<StyleShadowSVGData> {
public:
    static Ref<StyleShadowSVGData> create() { return adoptRef(*new StyleShadowSVGData); }
    Ref<StyleShadowSVGData> copy() const;

    bool operator==(const StyleShadowSVGData&) const;
    bool operator!=(const StyleShadowSVGData& other) const
    {
        return !(*this == other);
    }

    std::unique_ptr<ShadowData> shadow;

private:
    StyleShadowSVGData();
    StyleShadowSVGData(const StyleShadowSVGData&);
};

// Non-inherited resources
class StyleResourceData : public RefCounted<StyleResourceData> {
public:
    static Ref<StyleResourceData> create() { return adoptRef(*new StyleResourceData); }
    Ref<StyleResourceData> copy() const;

    bool operator==(const StyleResourceData&) const;
    bool operator!=(const StyleResourceData& other) const
    {
        return !(*this == other);
    }

    String clipper;
    String masker;

private:
    StyleResourceData();
    StyleResourceData(const StyleResourceData&);
};

// Inherited resources
class StyleInheritedResourceData : public RefCounted<StyleInheritedResourceData> {
public:
    static Ref<StyleInheritedResourceData> create() { return adoptRef(*new StyleInheritedResourceData); }
    Ref<StyleInheritedResourceData> copy() const;

    bool operator==(const StyleInheritedResourceData&) const;
    bool operator!=(const StyleInheritedResourceData& other) const
    {
        return !(*this == other);
    }

    String markerStart;
    String markerMid;
    String markerEnd;

private:
    StyleInheritedResourceData();
    StyleInheritedResourceData(const StyleInheritedResourceData&);
};

// Positioning and sizing properties.
class StyleLayoutData : public RefCounted<StyleLayoutData> {
public:
    static Ref<StyleLayoutData> create() { return adoptRef(*new StyleLayoutData); }
    Ref<StyleLayoutData> copy() const;

    bool operator==(const StyleLayoutData&) const;
    bool operator!=(const StyleLayoutData& other) const
    {
        return !(*this == other);
    }

    Length cx;
    Length cy;
    Length r;
    Length rx;
    Length ry;
    Length x;
    Length y;

private:
    StyleLayoutData();
    StyleLayoutData(const StyleLayoutData&);
};

} // namespace WebCore
