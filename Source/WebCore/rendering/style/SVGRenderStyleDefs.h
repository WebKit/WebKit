/*
    Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
    Copyright (C) Research In Motion Limited 2010. All rights reserved.
    Copyright (C) 2014 Adobe Systems Incorporated. All rights reserved.
    Copyright (C) 2025 Samuel Weinig <sam@webkit.org>

    Based on khtml code by:
    Copyright (C) 2000-2003 Lars Knoll (knoll@kde.org)
              (C) 2000 Antti Koivisto (koivisto@kde.org)
              (C) 2000-2003 Dirk Mueller (mueller@kde.org)
              (C) 2002-2003 Apple Inc. All rights reserved.

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

#include <WebCore/RenderStyleConstants.h>
#include <WebCore/SVGLengthValue.h>
#include <WebCore/StyleBoxShadow.h>
#include <WebCore/StyleColor.h>
#include <WebCore/StyleOpacity.h>
#include <WebCore/StyleSVGBaselineShift.h>
#include <WebCore/StyleSVGCenterCoordinateComponent.h>
#include <WebCore/StyleSVGCoordinateComponent.h>
#include <WebCore/StyleSVGMarkerResource.h>
#include <WebCore/StyleSVGPaint.h>
#include <WebCore/StyleSVGPathData.h>
#include <WebCore/StyleSVGRadius.h>
#include <WebCore/StyleSVGRadiusComponent.h>
#include <WebCore/StyleSVGStrokeDasharray.h>
#include <WebCore/StyleSVGStrokeDashoffset.h>
#include <wtf/FixedVector.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WTF {
class TextStream;
}

namespace WebCore {

class CSSValue;
class CSSValueList;

// Inherited/Non-Inherited Style Datastructures
DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(StyleFillData);
class StyleFillData : public RefCounted<StyleFillData> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(StyleFillData, StyleFillData);
public:
    static Ref<StyleFillData> create() { return adoptRef(*new StyleFillData); }
    Ref<StyleFillData> copy() const;

    bool operator==(const StyleFillData&) const;

#if !LOG_DISABLED
    void dumpDifferences(TextStream&, const StyleFillData&) const;
#endif

    Style::Opacity fillOpacity;
    Style::SVGPaint fill;
    Style::SVGPaint visitedLinkFill;

private:
    StyleFillData();
    StyleFillData(const StyleFillData&);
};

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(StyleStrokeData);
class StyleStrokeData : public RefCounted<StyleStrokeData> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(StyleStrokeData, StyleStrokeData);
public:
    static Ref<StyleStrokeData> create() { return adoptRef(*new StyleStrokeData); }
    Ref<StyleStrokeData> copy() const;

    bool operator==(const StyleStrokeData&) const;

#if !LOG_DISABLED
    void dumpDifferences(TextStream&, const StyleStrokeData&) const;
#endif

    Style::Opacity strokeOpacity;
    Style::SVGPaint stroke;
    Style::SVGPaint visitedLinkStroke;
    Style::SVGStrokeDashoffset strokeDashOffset;
    Style::SVGStrokeDasharray strokeDashArray;

private:
    StyleStrokeData();
    StyleStrokeData(const StyleStrokeData&);
};

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(StyleStopData);
class StyleStopData : public RefCounted<StyleStopData> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(StyleStopData, StyleStopData);
public:
    static Ref<StyleStopData> create() { return adoptRef(*new StyleStopData); }
    Ref<StyleStopData> copy() const;

    bool operator==(const StyleStopData&) const;

#if !LOG_DISABLED
    void dumpDifferences(TextStream&, const StyleStopData&) const;
#endif

    Style::Opacity stopOpacity;
    Style::Color stopColor;

private:
    StyleStopData();
    StyleStopData(const StyleStopData&);
};

// Note: the rule for this class is, *no inheritance* of these props
DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(StyleMiscData);
class StyleMiscData : public RefCounted<StyleMiscData> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(StyleMiscData, StyleMiscData);
public:
    static Ref<StyleMiscData> create() { return adoptRef(*new StyleMiscData); }
    Ref<StyleMiscData> copy() const;

    bool operator==(const StyleMiscData&) const;

#if !LOG_DISABLED
    void dumpDifferences(TextStream&, const StyleMiscData&) const;
#endif

    Style::Opacity floodOpacity;
    Style::Color floodColor;
    Style::Color lightingColor;

    Style::SVGBaselineShift baselineShift;

private:
    StyleMiscData();
    StyleMiscData(const StyleMiscData&);
};

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(StyleShadowSVGData);
class StyleShadowSVGData : public RefCounted<StyleShadowSVGData> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(StyleShadowSVGData, StyleShadowSVGData);
public:
    static Ref<StyleShadowSVGData> create() { return adoptRef(*new StyleShadowSVGData); }
    Ref<StyleShadowSVGData> copy() const;

    bool operator==(const StyleShadowSVGData&) const;

#if !LOG_DISABLED
    void dumpDifferences(TextStream&, const StyleShadowSVGData&) const;
#endif

    Style::BoxShadows shadow;

private:
    StyleShadowSVGData();
    StyleShadowSVGData(const StyleShadowSVGData&);
};

// Inherited resources
DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(StyleInheritedResourceData);
class StyleInheritedResourceData : public RefCounted<StyleInheritedResourceData> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(StyleInheritedResourceData, StyleInheritedResourceData);
public:
    static Ref<StyleInheritedResourceData> create() { return adoptRef(*new StyleInheritedResourceData); }
    Ref<StyleInheritedResourceData> copy() const;

    bool operator==(const StyleInheritedResourceData&) const;

#if !LOG_DISABLED
    void dumpDifferences(TextStream&, const StyleInheritedResourceData&) const;
#endif

    Style::SVGMarkerResource markerStart;
    Style::SVGMarkerResource markerMid;
    Style::SVGMarkerResource markerEnd;

private:
    StyleInheritedResourceData();
    StyleInheritedResourceData(const StyleInheritedResourceData&);
};

// Positioning and sizing properties.
DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(StyleLayoutData);
class StyleLayoutData : public RefCounted<StyleLayoutData> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(StyleLayoutData, StyleLayoutData);
public:
    static Ref<StyleLayoutData> create() { return adoptRef(*new StyleLayoutData); }
    Ref<StyleLayoutData> copy() const;

    bool operator==(const StyleLayoutData&) const;

#if !LOG_DISABLED
    void dumpDifferences(TextStream&, const StyleLayoutData&) const;
#endif

    Style::SVGCenterCoordinateComponent cx;
    Style::SVGCenterCoordinateComponent cy;
    Style::SVGRadius r;
    Style::SVGRadiusComponent rx;
    Style::SVGRadiusComponent ry;
    Style::SVGCoordinateComponent x;
    Style::SVGCoordinateComponent y;
    Style::SVGPathData d;

private:
    StyleLayoutData();
    StyleLayoutData(const StyleLayoutData&);
};

WTF::TextStream& operator<<(WTF::TextStream&, const StyleFillData&);
WTF::TextStream& operator<<(WTF::TextStream&, const StyleStrokeData&);
WTF::TextStream& operator<<(WTF::TextStream&, const StyleStopData&);
WTF::TextStream& operator<<(WTF::TextStream&, const StyleMiscData&);
WTF::TextStream& operator<<(WTF::TextStream&, const StyleShadowSVGData&);
WTF::TextStream& operator<<(WTF::TextStream&, const StyleInheritedResourceData&);
WTF::TextStream& operator<<(WTF::TextStream&, const StyleLayoutData&);

} // namespace WebCore
