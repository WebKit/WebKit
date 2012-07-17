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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER “AS IS” AND ANY
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

#ifndef WrapShapes_h
#define WrapShapes_h

#include "Length.h"
#include "WindRule.h"

namespace WebCore {

class WrapShape : public RefCounted<WrapShape> {
public:
    enum Type {
        WRAP_SHAPE_RECTANGLE = 1,
        WRAP_SHAPE_CIRCLE = 2,
        WRAP_SHAPE_ELLIPSE = 3,
        WRAP_SHAPE_POLYGON = 4
    };

    virtual Type type() const = 0;
    virtual ~WrapShape() { }

protected:
    WrapShape() { }
};

class WrapShapeRectangle : public WrapShape {
public:
    static PassRefPtr<WrapShapeRectangle> create() { return adoptRef(new WrapShapeRectangle); }

    Length x() const { return m_x; }
    Length y() const { return m_y; }
    Length width() const { return m_width; }
    Length height() const { return m_height; }
    Length cornerRadiusX() const { return m_cornerRadiusX; }
    Length cornerRadiusY() const { return m_cornerRadiusY; }

    void setX(Length x) { m_x = x; }
    void setY(Length y) { m_y = y; }
    void setWidth(Length width) { m_width = width; }
    void setHeight(Length height) { m_height = height; }
    void setCornerRadiusX(Length radiusX) { m_cornerRadiusX = radiusX; }
    void setCornerRadiusY(Length radiusY) { m_cornerRadiusY = radiusY; }

    virtual Type type() const { return WRAP_SHAPE_RECTANGLE; }

private:
    WrapShapeRectangle()
        : m_cornerRadiusX(Undefined)
        , m_cornerRadiusY(Undefined)
    { }

    Length m_y;
    Length m_x;
    Length m_width;
    Length m_height;
    Length m_cornerRadiusX;
    Length m_cornerRadiusY;
};

class WrapShapeCircle : public WrapShape {
public:
    static PassRefPtr<WrapShapeCircle> create() { return adoptRef(new WrapShapeCircle); }

    Length centerX() const { return m_centerX; }
    Length centerY() const { return m_centerY; }
    Length radius() const { return m_radius; }

    void setCenterX(Length centerX) { m_centerX = centerX; }
    void setCenterY(Length centerY) { m_centerY = centerY; }
    void setRadius(Length radius) { m_radius = radius; }

    virtual Type type() const { return WRAP_SHAPE_CIRCLE; }
private:
    WrapShapeCircle() { }

    Length m_centerX;
    Length m_centerY;
    Length m_radius;
};

class WrapShapeEllipse : public WrapShape {
public:
    static PassRefPtr<WrapShapeEllipse> create() { return adoptRef(new WrapShapeEllipse); }

    Length centerX() const { return m_centerX; }
    Length centerY() const { return m_centerY; }
    Length radiusX() const { return m_radiusX; }
    Length radiusY() const { return m_radiusY; }

    void setCenterX(Length centerX) { m_centerX = centerX; }
    void setCenterY(Length centerY) { m_centerY = centerY; }
    void setRadiusX(Length radiusX) { m_radiusX = radiusX; }
    void setRadiusY(Length radiusY) { m_radiusY = radiusY; }

    virtual Type type() const { return WRAP_SHAPE_ELLIPSE; }
private:
    WrapShapeEllipse() { }

    Length m_centerX;
    Length m_centerY;
    Length m_radiusX;
    Length m_radiusY;
};

class WrapShapePolygon : public WrapShape {
public:
    static PassRefPtr<WrapShapePolygon> create() { return adoptRef(new WrapShapePolygon); }

    WindRule windRule() const { return m_windRule; }
    const Vector<Length>& values() const { return m_values; }
    Length getXAt(unsigned i) const { return m_values.at(2 * i); }
    Length getYAt(unsigned i) const { return m_values.at(2 * i + 1); }

    void setWindRule(WindRule windRule) { m_windRule = windRule; }
    void appendPoint(Length x, Length y) { m_values.append(x); m_values.append(y); }

    virtual Type type() const { return WRAP_SHAPE_POLYGON; }
private:
    WrapShapePolygon()
        : m_windRule(RULE_NONZERO)
    { }

    WindRule m_windRule;
    Vector<Length> m_values;
};
}
#endif
