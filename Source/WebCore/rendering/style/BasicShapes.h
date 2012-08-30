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

class BasicShape : public WTF::RefCountedBase {
public:
    enum Type {
        BASIC_SHAPE_RECTANGLE = 1,
        BASIC_SHAPE_CIRCLE = 2,
        BASIC_SHAPE_ELLIPSE = 3,
        BASIC_SHAPE_POLYGON = 4
    };
    
    void deref() 
    {
        if (derefBase())
            destroy();
    }

    Type type() const { return m_type; }

protected:
    BasicShape(Type type)
        : m_type(type)
    { }
    
private:
    void destroy();
    Type m_type;
};

class BasicShapeRectangle : public BasicShape {
public:
    static PassRefPtr<BasicShapeRectangle> create() { return adoptRef(new BasicShapeRectangle); }

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
    
private:
    BasicShapeRectangle()
        : BasicShape(BASIC_SHAPE_RECTANGLE)
        , m_cornerRadiusX(Undefined)
        , m_cornerRadiusY(Undefined)
    { }

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

    Length centerX() const { return m_centerX; }
    Length centerY() const { return m_centerY; }
    Length radius() const { return m_radius; }

    void setCenterX(Length centerX) { m_centerX = centerX; }
    void setCenterY(Length centerY) { m_centerY = centerY; }
    void setRadius(Length radius) { m_radius = radius; }

private:
    BasicShapeCircle() 
        : BasicShape(BASIC_SHAPE_CIRCLE)
    { }

    Length m_centerX;
    Length m_centerY;
    Length m_radius;
};

class BasicShapeEllipse : public BasicShape {
public:
    static PassRefPtr<BasicShapeEllipse> create() { return adoptRef(new BasicShapeEllipse); }

    Length centerX() const { return m_centerX; }
    Length centerY() const { return m_centerY; }
    Length radiusX() const { return m_radiusX; }
    Length radiusY() const { return m_radiusY; }

    void setCenterX(Length centerX) { m_centerX = centerX; }
    void setCenterY(Length centerY) { m_centerY = centerY; }
    void setRadiusX(Length radiusX) { m_radiusX = radiusX; }
    void setRadiusY(Length radiusY) { m_radiusY = radiusY; }

private:
    BasicShapeEllipse() 
        : BasicShape(BASIC_SHAPE_ELLIPSE)
    { }

    Length m_centerX;
    Length m_centerY;
    Length m_radiusX;
    Length m_radiusY;
};

class BasicShapePolygon : public BasicShape {
public:
    static PassRefPtr<BasicShapePolygon> create() { return adoptRef(new BasicShapePolygon); }

    WindRule windRule() const { return m_windRule; }
    const Vector<Length>& values() const { return m_values; }
    Length getXAt(unsigned i) const { return m_values.at(2 * i); }
    Length getYAt(unsigned i) const { return m_values.at(2 * i + 1); }

    void setWindRule(WindRule windRule) { m_windRule = windRule; }
    void appendPoint(Length x, Length y) { m_values.append(x); m_values.append(y); }

private:
    BasicShapePolygon()
        : BasicShape(BASIC_SHAPE_POLYGON)
        , m_windRule(RULE_NONZERO)
    { }

    WindRule m_windRule;
    Vector<Length> m_values;
};
}
#endif
