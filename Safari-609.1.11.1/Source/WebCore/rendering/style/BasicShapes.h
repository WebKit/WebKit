/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#pragma once

#include "Length.h"
#include "LengthSize.h"
#include "WindRule.h"
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/TypeCasts.h>
#include <wtf/Vector.h>

namespace WTF {
class TextStream;
}

namespace WebCore {

class FloatRect;
class Path;
class RenderBox;
class SVGPathByteStream;

class BasicShape : public RefCounted<BasicShape> {
public:
    virtual ~BasicShape() = default;

    enum class Type {
        Polygon,
        Path,
        Circle,
        Ellipse,
        Inset
    };

    virtual Type type() const = 0;

    virtual const Path& path(const FloatRect&) = 0;
    virtual WindRule windRule() const { return WindRule::NonZero; }

    virtual bool canBlend(const BasicShape&) const = 0;
    virtual Ref<BasicShape> blend(const BasicShape& from, double) const = 0;

    virtual bool operator==(const BasicShape&) const = 0;
    
    virtual void dump(TextStream&) const = 0;
};

class BasicShapeCenterCoordinate {
public:
    enum Direction {
        TopLeft,
        BottomRight
    };

    BasicShapeCenterCoordinate()
    {
        updateComputedLength();
    }

    BasicShapeCenterCoordinate(Direction direction, Length length)
        : m_direction(direction)
        , m_length(length)
    {
        updateComputedLength();
    }

    Direction direction() const { return m_direction; }
    const Length& length() const { return m_length; }
    const Length& computedLength() const { return m_computedLength; }

    BasicShapeCenterCoordinate blend(const BasicShapeCenterCoordinate& from, double progress) const
    {
        return BasicShapeCenterCoordinate(TopLeft, WebCore::blend(from.m_computedLength, m_computedLength, progress));
    }
    
    bool operator==(const BasicShapeCenterCoordinate& other) const
    {
        return m_direction == other.m_direction
            && m_length == other.m_length
            && m_computedLength == other.m_computedLength;
    }

private:
    void updateComputedLength();

    Direction m_direction { TopLeft };
    Length m_length { Undefined };
    Length m_computedLength;
};

class BasicShapeRadius {
public:
    enum Type {
        Value,
        ClosestSide,
        FarthestSide
    };

    BasicShapeRadius() = default;

    explicit BasicShapeRadius(Length v)
        : m_value(v)
        , m_type(Value)
    { }
    explicit BasicShapeRadius(Type t)
        : m_value(Undefined)
        , m_type(t)
    { }

    const Length& value() const { return m_value; }
    Type type() const { return m_type; }

    bool canBlend(const BasicShapeRadius& other) const
    {
        // FIXME determine how to interpolate between keywords. See bug 125108.
        return m_type == Value && other.type() == Value;
    }

    BasicShapeRadius blend(const BasicShapeRadius& from, double progress) const
    {
        if (m_type != Value || from.type() != Value)
            return BasicShapeRadius(from);

        return BasicShapeRadius(WebCore::blend(from.value(), value(), progress));
    }
    
    bool operator==(const BasicShapeRadius& other) const
    {
        return m_value == other.m_value && m_type == other.m_type;
    }

private:
    Length m_value { Undefined };
    Type m_type { ClosestSide };
};

class BasicShapeCircle final : public BasicShape {
public:
    static Ref<BasicShapeCircle> create() { return adoptRef(*new BasicShapeCircle); }

    const BasicShapeCenterCoordinate& centerX() const { return m_centerX; }
    const BasicShapeCenterCoordinate& centerY() const { return m_centerY; }
    const BasicShapeRadius& radius() const { return m_radius; }
    float floatValueForRadiusInBox(float boxWidth, float boxHeight) const;

    void setCenterX(BasicShapeCenterCoordinate centerX) { m_centerX = WTFMove(centerX); }
    void setCenterY(BasicShapeCenterCoordinate centerY) { m_centerY = WTFMove(centerY); }
    void setRadius(BasicShapeRadius radius) { m_radius = WTFMove(radius); }

private:
    BasicShapeCircle() = default;

    Type type() const override { return Type::Circle; }

    const Path& path(const FloatRect&) override;

    bool canBlend(const BasicShape&) const override;
    Ref<BasicShape> blend(const BasicShape& from, double) const override;

    bool operator==(const BasicShape&) const override;

    void dump(TextStream&) const final;

    BasicShapeCenterCoordinate m_centerX;
    BasicShapeCenterCoordinate m_centerY;
    BasicShapeRadius m_radius;
};

class BasicShapeEllipse final : public BasicShape {
public:
    static Ref<BasicShapeEllipse> create() { return adoptRef(*new BasicShapeEllipse); }

    const BasicShapeCenterCoordinate& centerX() const { return m_centerX; }
    const BasicShapeCenterCoordinate& centerY() const { return m_centerY; }
    const BasicShapeRadius& radiusX() const { return m_radiusX; }
    const BasicShapeRadius& radiusY() const { return m_radiusY; }
    float floatValueForRadiusInBox(const BasicShapeRadius&, float center, float boxWidthOrHeight) const;

    void setCenterX(BasicShapeCenterCoordinate centerX) { m_centerX = WTFMove(centerX); }
    void setCenterY(BasicShapeCenterCoordinate centerY) { m_centerY = WTFMove(centerY); }
    void setRadiusX(BasicShapeRadius radiusX) { m_radiusX = WTFMove(radiusX); }
    void setRadiusY(BasicShapeRadius radiusY) { m_radiusY = WTFMove(radiusY); }

private:
    BasicShapeEllipse() = default;

    Type type() const override { return Type::Ellipse; }

    const Path& path(const FloatRect&) override;

    bool canBlend(const BasicShape&) const override;
    Ref<BasicShape> blend(const BasicShape& from, double) const override;

    bool operator==(const BasicShape&) const override;

    void dump(TextStream&) const final;

    BasicShapeCenterCoordinate m_centerX;
    BasicShapeCenterCoordinate m_centerY;
    BasicShapeRadius m_radiusX;
    BasicShapeRadius m_radiusY;
};

class BasicShapePolygon final : public BasicShape {
public:
    static Ref<BasicShapePolygon> create() { return adoptRef(*new BasicShapePolygon); }

    const Vector<Length>& values() const { return m_values; }
    const Length& getXAt(unsigned i) const { return m_values[2 * i]; }
    const Length& getYAt(unsigned i) const { return m_values[2 * i + 1]; }

    void setWindRule(WindRule windRule) { m_windRule = windRule; }
    void appendPoint(Length x, Length y) { m_values.append(WTFMove(x)); m_values.append(WTFMove(y)); }

    WindRule windRule() const override { return m_windRule; }

private:
    BasicShapePolygon() = default;

    Type type() const override { return Type::Polygon; }

    const Path& path(const FloatRect&) override;

    bool canBlend(const BasicShape&) const override;
    Ref<BasicShape> blend(const BasicShape& from, double) const override;

    bool operator==(const BasicShape&) const override;

    void dump(TextStream&) const final;

    WindRule m_windRule { WindRule::NonZero };
    Vector<Length> m_values;
};

class BasicShapePath final : public BasicShape {
public:
    static Ref<BasicShapePath> create(std::unique_ptr<SVGPathByteStream>&& byteStream)
    {
        return adoptRef(*new BasicShapePath(WTFMove(byteStream)));
    }

    void setWindRule(WindRule windRule) { m_windRule = windRule; }
    WindRule windRule() const override { return m_windRule; }

    const SVGPathByteStream* pathData() const { return m_byteStream.get(); }

private:
    BasicShapePath(std::unique_ptr<SVGPathByteStream>&&);

    Type type() const override { return Type::Path; }

    const Path& path(const FloatRect&) override;

    bool canBlend(const BasicShape&) const override;
    Ref<BasicShape> blend(const BasicShape& from, double) const override;

    bool operator==(const BasicShape&) const override;

    void dump(TextStream&) const final;

    std::unique_ptr<SVGPathByteStream> m_byteStream;
    WindRule m_windRule { WindRule::NonZero };
};

class BasicShapeInset final : public BasicShape {
public:
    static Ref<BasicShapeInset> create() { return adoptRef(*new BasicShapeInset); }

    const Length& top() const { return m_top; }
    const Length& right() const { return m_right; }
    const Length& bottom() const { return m_bottom; }
    const Length& left() const { return m_left; }

    const LengthSize& topLeftRadius() const { return m_topLeftRadius; }
    const LengthSize& topRightRadius() const { return m_topRightRadius; }
    const LengthSize& bottomRightRadius() const { return m_bottomRightRadius; }
    const LengthSize& bottomLeftRadius() const { return m_bottomLeftRadius; }

    void setTop(Length top) { m_top = WTFMove(top); }
    void setRight(Length right) { m_right = WTFMove(right); }
    void setBottom(Length bottom) { m_bottom = WTFMove(bottom); }
    void setLeft(Length left) { m_left = WTFMove(left); }

    void setTopLeftRadius(LengthSize radius) { m_topLeftRadius = WTFMove(radius); }
    void setTopRightRadius(LengthSize radius) { m_topRightRadius = WTFMove(radius); }
    void setBottomRightRadius(LengthSize radius) { m_bottomRightRadius = WTFMove(radius); }
    void setBottomLeftRadius(LengthSize radius) { m_bottomLeftRadius = WTFMove(radius); }

private:
    BasicShapeInset() = default;

    Type type() const override { return Type::Inset; }

    const Path& path(const FloatRect&) override;

    bool canBlend(const BasicShape&) const override;
    Ref<BasicShape> blend(const BasicShape& from, double) const override;

    bool operator==(const BasicShape&) const override;

    void dump(TextStream&) const final;

    Length m_right;
    Length m_top;
    Length m_bottom;
    Length m_left;

    LengthSize m_topLeftRadius;
    LengthSize m_topRightRadius;
    LengthSize m_bottomRightRadius;
    LengthSize m_bottomLeftRadius;
};

WTF::TextStream& operator<<(WTF::TextStream&, const BasicShapeRadius&);
WTF::TextStream& operator<<(WTF::TextStream&, const BasicShapeCenterCoordinate&);
WTF::TextStream& operator<<(WTF::TextStream&, const BasicShape&);

} // namespace WebCore

#define SPECIALIZE_TYPE_TRAITS_BASIC_SHAPE(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ToValueTypeName) \
    static bool isType(const WebCore::BasicShape& basicShape) { return basicShape.type() == WebCore::predicate; } \
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BASIC_SHAPE(BasicShapeCircle, BasicShape::Type::Circle)
SPECIALIZE_TYPE_TRAITS_BASIC_SHAPE(BasicShapeEllipse, BasicShape::Type::Ellipse)
SPECIALIZE_TYPE_TRAITS_BASIC_SHAPE(BasicShapePolygon, BasicShape::Type::Polygon)
SPECIALIZE_TYPE_TRAITS_BASIC_SHAPE(BasicShapePath, BasicShape::Type::Path)
SPECIALIZE_TYPE_TRAITS_BASIC_SHAPE(BasicShapeInset, BasicShape::Type::Inset)
