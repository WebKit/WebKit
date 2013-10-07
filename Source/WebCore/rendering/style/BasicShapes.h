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

#ifndef BasicShapes_h
#define BasicShapes_h

#include "Length.h"
#include "WindRule.h"
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class FloatRect;
class Path;

class BasicShape : public RefCounted<BasicShape> {
public:
    virtual ~BasicShape() { }

    enum Type {
        BasicShapeRectangleType = 1,
        BasicShapeCircleType = 2,
        BasicShapeEllipseType = 3,
        BasicShapePolygonType = 4,
        BasicShapeInsetRectangleType = 5
    };

    bool canBlend(const BasicShape*) const;

    virtual void path(Path&, const FloatRect&) = 0;
    virtual WindRule windRule() const { return RULE_NONZERO; }
    virtual PassRefPtr<BasicShape> blend(const BasicShape*, double) const = 0;

    virtual Type type() const = 0;
protected:
    BasicShape() { }
};

class BasicShapeRectangle : public BasicShape {
public:
    static PassRefPtr<BasicShapeRectangle> create() { return adoptRef(new BasicShapeRectangle); }

    const Length& x() const { return m_x; }
    const Length& y() const { return m_y; }
    const Length& width() const { return m_width; }
    const Length& height() const { return m_height; }
    const Length& cornerRadiusX() const { return m_cornerRadiusX; }
    const Length& cornerRadiusY() const { return m_cornerRadiusY; }

    void setX(Length x) { m_x = std::move(x); }
    void setY(Length y) { m_y = std::move(y); }
    void setWidth(Length width) { m_width = std::move(width); }
    void setHeight(Length height) { m_height = std::move(height); }
    void setCornerRadiusX(Length radiusX)
    {
        ASSERT(!radiusX.isUndefined());
        m_cornerRadiusX = std::move(radiusX);
    }
    void setCornerRadiusY(Length radiusY)
    {
        ASSERT(!radiusY.isUndefined());
        m_cornerRadiusY = std::move(radiusY);
    }

    virtual void path(Path&, const FloatRect&) OVERRIDE;
    virtual PassRefPtr<BasicShape> blend(const BasicShape*, double) const OVERRIDE;

    virtual Type type() const OVERRIDE { return BasicShapeRectangleType; }
private:
    BasicShapeRectangle() { }

    Length m_y;
    Length m_x;
    Length m_width;
    Length m_height;
    Length m_cornerRadiusX;
    Length m_cornerRadiusY;
};

class BasicShapeCircle : public BasicShape {
public:
    static PassRefPtr<BasicShapeCircle> create() { return adoptRef(new BasicShapeCircle); }

    const Length& centerX() const { return m_centerX; }
    const Length& centerY() const { return m_centerY; }
    const Length& radius() const { return m_radius; }

    void setCenterX(Length centerX) { m_centerX = std::move(centerX); }
    void setCenterY(Length centerY) { m_centerY = std::move(centerY); }
    void setRadius(Length radius) { m_radius = std::move(radius); }

    virtual void path(Path&, const FloatRect&) OVERRIDE;
    virtual PassRefPtr<BasicShape> blend(const BasicShape*, double) const OVERRIDE;

    virtual Type type() const OVERRIDE { return BasicShapeCircleType; }
private:
    BasicShapeCircle() { }

    Length m_centerX;
    Length m_centerY;
    Length m_radius;
};

class BasicShapeEllipse : public BasicShape {
public:
    static PassRefPtr<BasicShapeEllipse> create() { return adoptRef(new BasicShapeEllipse); }

    const Length& centerX() const { return m_centerX; }
    const Length& centerY() const { return m_centerY; }
    const Length& radiusX() const { return m_radiusX; }
    const Length& radiusY() const { return m_radiusY; }

    void setCenterX(Length centerX) { m_centerX = std::move(centerX); }
    void setCenterY(Length centerY) { m_centerY = std::move(centerY); }
    void setRadiusX(Length radiusX) { m_radiusX = std::move(radiusX); }
    void setRadiusY(Length radiusY) { m_radiusY = std::move(radiusY); }

    virtual void path(Path&, const FloatRect&) OVERRIDE;
    virtual PassRefPtr<BasicShape> blend(const BasicShape*, double) const OVERRIDE;

    virtual Type type() const OVERRIDE { return BasicShapeEllipseType; }
private:
    BasicShapeEllipse() { }

    Length m_centerX;
    Length m_centerY;
    Length m_radiusX;
    Length m_radiusY;
};

class BasicShapePolygon : public BasicShape {
public:
    static PassRefPtr<BasicShapePolygon> create() { return adoptRef(new BasicShapePolygon); }

    const Vector<Length>& values() const { return m_values; }
    const Length& getXAt(unsigned i) const { return m_values[2 * i]; }
    const Length& getYAt(unsigned i) const { return m_values[2 * i + 1]; }

    void setWindRule(WindRule windRule) { m_windRule = windRule; }
    void appendPoint(Length x, Length y) { m_values.append(std::move(x)); m_values.append(std::move(y)); }

    virtual void path(Path&, const FloatRect&) OVERRIDE;
    virtual PassRefPtr<BasicShape> blend(const BasicShape*, double) const OVERRIDE;

    virtual WindRule windRule() const OVERRIDE { return m_windRule; }

    virtual Type type() const OVERRIDE { return BasicShapePolygonType; }
private:
    BasicShapePolygon()
        : m_windRule(RULE_NONZERO)
    { }

    WindRule m_windRule;
    Vector<Length> m_values;
};

class BasicShapeInsetRectangle : public BasicShape {
public:
    static PassRefPtr<BasicShapeInsetRectangle> create() { return adoptRef(new BasicShapeInsetRectangle); }

    const Length& top() const { return m_top; }
    const Length& right() const { return m_right; }
    const Length& bottom() const { return m_bottom; }
    const Length& left() const { return m_left; }
    const Length& cornerRadiusX() const { return m_cornerRadiusX; }
    const Length& cornerRadiusY() const { return m_cornerRadiusY; }

    void setTop(Length top) { m_top = std::move(top); }
    void setRight(Length right) { m_right = std::move(right); }
    void setBottom(Length bottom) { m_bottom = std::move(bottom); }
    void setLeft(Length left) { m_left = std::move(left); }
    void setCornerRadiusX(Length radiusX)
    {
        ASSERT(!radiusX.isUndefined());
        m_cornerRadiusX = std::move(radiusX);
    }
    void setCornerRadiusY(Length radiusY)
    {
        ASSERT(!radiusY.isUndefined());
        m_cornerRadiusY = std::move(radiusY);
    }

    virtual void path(Path&, const FloatRect&) OVERRIDE;
    virtual PassRefPtr<BasicShape> blend(const BasicShape*, double) const OVERRIDE;

    virtual Type type() const OVERRIDE { return BasicShapeInsetRectangleType; }
private:
    BasicShapeInsetRectangle() { }

    Length m_right;
    Length m_top;
    Length m_bottom;
    Length m_left;
    Length m_cornerRadiusX;
    Length m_cornerRadiusY;
};
}
#endif
