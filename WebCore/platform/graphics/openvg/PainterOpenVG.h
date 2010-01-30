/*
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef PainterOpenVG_h
#define PainterOpenVG_h

#include "Color.h"
#include "GraphicsContext.h"

#include <openvg.h>

#include <wtf/Noncopyable.h>
#include <wtf/Platform.h>
#include <wtf/Vector.h>

namespace WebCore {

class FloatPoint;
class FloatRect;
class IntRect;
class IntSize;
class SurfaceOpenVG;
class TransformationMatrix;

struct PlatformPainterState;

class PainterOpenVG : public Noncopyable {
public:
    friend class SurfaceOpenVG;
    friend struct PlatformPainterState;

    enum SaveMode {
        CreateNewState,
        CreateNewStateWithPaintStateOnly // internal usage only, do not use outside PainterOpenVG
    };

    PainterOpenVG();
    PainterOpenVG(SurfaceOpenVG*);
    ~PainterOpenVG();

    void begin(SurfaceOpenVG*);
    void end();

    TransformationMatrix transformationMatrix() const;
    void setTransformationMatrix(const TransformationMatrix&);
    void concatTransformationMatrix(const TransformationMatrix&);

    CompositeOperator compositeOperation() const;
    void setCompositeOperation(CompositeOperator);
    float opacity() const;
    void setOpacity(float);

    float strokeThickness() const;
    void setStrokeThickness(float);
    StrokeStyle strokeStyle() const;
    void setStrokeStyle(const StrokeStyle&);

    void setLineDash(const DashArray&, float dashOffset);
    void setLineCap(LineCap);
    void setLineJoin(LineJoin);
    void setMiterLimit(float);

    Color strokeColor() const;
    void setStrokeColor(const Color&);

    Color fillColor() const;
    void setFillColor(const Color&);

    bool antialiasingEnabled() const;
    void setAntialiasingEnabled(bool);

    void drawRect(const FloatRect&, VGbitfield paintModes = (VG_STROKE_PATH | VG_FILL_PATH));
    void drawRoundedRect(const FloatRect&, const IntSize& topLeft, const IntSize& topRight, const IntSize& bottomLeft, const IntSize& bottomRight, VGbitfield paintModes = (VG_STROKE_PATH | VG_FILL_PATH));
    void drawLine(const IntPoint& from, const IntPoint& to);
    void drawArc(const IntRect& ellipseBounds, int startAngle, int angleSpan, VGbitfield paintModes = (VG_STROKE_PATH | VG_FILL_PATH));
    void drawEllipse(const IntRect& bounds, VGbitfield paintModes = (VG_STROKE_PATH | VG_FILL_PATH));
    void drawPolygon(size_t numPoints, const FloatPoint* points, VGbitfield paintModes = (VG_STROKE_PATH | VG_FILL_PATH));

    void scale(const FloatSize& scaleFactors);
    void rotate(float radians);
    void translate(float dx, float dy);

    void intersectClipRect(const FloatRect&);

    void save(PainterOpenVG::SaveMode saveMode = CreateNewState);
    void restore();

    SurfaceOpenVG* surface() { return m_surface; }
    void blitToSurface();

private:
    void destroyPainterStates();
    void applyState();

    void intersectScissorRect(const FloatRect&);

private:
    Vector<PlatformPainterState*> m_stateStack;
    PlatformPainterState* m_state;
    SurfaceOpenVG* m_surface;
};

}

#endif
