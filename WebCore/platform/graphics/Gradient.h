/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef Gradient_h
#define Gradient_h

#include "FloatPoint.h"
#include "Generator.h"
#include "GraphicsTypes.h"
#include "TransformationMatrix.h"
#include <wtf/PassRefPtr.h>
#include <wtf/Vector.h>

#if PLATFORM(CG)
typedef struct CGShading* CGShadingRef;
typedef CGShadingRef PlatformGradient;
#elif PLATFORM(QT)
QT_BEGIN_NAMESPACE
class QGradient;
QT_END_NAMESPACE
typedef QGradient* PlatformGradient;
#elif PLATFORM(CAIRO)
typedef struct _cairo_pattern cairo_pattern_t;
typedef cairo_pattern_t* PlatformGradient;
#elif PLATFORM(SKIA)
class SkShader;
typedef class SkShader* PlatformGradient;
typedef class SkShader* PlatformPattern;
#else
typedef void* PlatformGradient;
#endif

namespace WebCore {

    class Color;

    class Gradient : public Generator {
    public:
        static PassRefPtr<Gradient> create(const FloatPoint& p0, const FloatPoint& p1)
        {
            return adoptRef(new Gradient(p0, p1));
        }
        static PassRefPtr<Gradient> create(const FloatPoint& p0, float r0, const FloatPoint& p1, float r1)
        {
            return adoptRef(new Gradient(p0, r0, p1, r1));
        }
        virtual ~Gradient();

        void addColorStop(float, const Color&);

        void getColor(float value, float* r, float* g, float* b, float* a) const;

        PlatformGradient platformGradient();

        struct ColorStop {
            float stop;
            float red;
            float green;
            float blue;
            float alpha;

            ColorStop() : stop(0), red(0), green(0), blue(0), alpha(0) { }
            ColorStop(float s, float r, float g, float b, float a) : stop(s), red(r), green(g), blue(b), alpha(a) { }
        };

        void setStopsSorted(bool s) { m_stopsSorted = s; }

        void setSpreadMethod(GradientSpreadMethod);
        GradientSpreadMethod spreadMethod() { return m_spreadMethod; }
        void setGradientSpaceTransform(const TransformationMatrix& gradientSpaceTransformation) { m_gradientSpaceTransformation = gradientSpaceTransformation; }
        // Qt and CG transform the gradient at draw time
        TransformationMatrix gradientSpaceTransform() { return m_gradientSpaceTransformation; }

        virtual void fill(GraphicsContext*, const FloatRect&);

    private:
        Gradient(const FloatPoint& p0, const FloatPoint& p1);
        Gradient(const FloatPoint& p0, float r0, const FloatPoint& p1, float r1);

        void platformInit() { m_gradient = 0; }
        void platformDestroy();

        int findStop(float value) const;

        bool m_radial;
        FloatPoint m_p0;
        FloatPoint m_p1;
        float m_r0;
        float m_r1;
        mutable Vector<ColorStop> m_stops;
        mutable bool m_stopsSorted;
        mutable int m_lastStop;
        GradientSpreadMethod m_spreadMethod;
        TransformationMatrix m_gradientSpaceTransformation;

        PlatformGradient m_gradient;
    };

} //namespace

#endif
