/*
 * Copyright (C) 2006, 2007, 2008, 2011, 2012, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2008 Torch Mobile, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#pragma once

#include "AffineTransform.h"
#include "Color.h"
#include "FloatPoint.h"
#include "GraphicsTypes.h"
#include <wtf/RefCounted.h>
#include <wtf/Variant.h>
#include <wtf/Vector.h>

#if USE(CG)
typedef struct CGContext* CGContextRef;
typedef struct CGGradient* CGGradientRef;
typedef CGGradientRef PlatformGradient;
#elif USE(DIRECT2D)
interface ID2D1Brush;
interface ID2D1RenderTarget;
typedef ID2D1Brush* PlatformGradient;
#elif USE(CAIRO)
typedef struct _cairo_pattern cairo_pattern_t;
typedef cairo_pattern_t* PlatformGradient;
#else
typedef void* PlatformGradient;
#endif

namespace WebCore {

class Color;
class FloatRect;
class GraphicsContext;

class Gradient : public RefCounted<Gradient> {
public:
    // FIXME: ExtendedColor - A color stop needs a notion of color space.
    struct ColorStop {
        float offset { 0 };
        Color color;

        ColorStop() = default;
        ColorStop(float offset, const Color& color)
            : offset(offset)
            , color(color)
        {
        }
    };

    using ColorStopVector = Vector<ColorStop, 2>;

    struct LinearData {
        FloatPoint point0;
        FloatPoint point1;
    };

    struct RadialData {
        FloatPoint point0;
        FloatPoint point1;
        float startRadius;
        float endRadius;
        float aspectRatio; // For elliptical gradient, width / height.
    };
    
    struct ConicData {
        FloatPoint point0;
        float angleRadians;
    };

    using Data = Variant<LinearData, RadialData, ConicData>;

    enum class Type { Linear, Radial, Conic };

    static Ref<Gradient> create(LinearData&&);
    static Ref<Gradient> create(RadialData&&);
    static Ref<Gradient> create(ConicData&&);

    WEBCORE_EXPORT ~Gradient();

    Type type() const;

    bool hasAlpha() const;
    bool isZeroSize() const;

    const Data& data() const { return m_data; }

    WEBCORE_EXPORT void addColorStop(const ColorStop&);
    WEBCORE_EXPORT void addColorStop(float, const Color&);
    void setSortedColorStops(ColorStopVector&&);

    const ColorStopVector& stops() const { return m_stops; }

    void setSpreadMethod(GradientSpreadMethod);
    GradientSpreadMethod spreadMethod() const { return m_spreadMethod; }

    // CG needs to transform the gradient at draw time.
    void setGradientSpaceTransform(const AffineTransform& gradientSpaceTransformation);
    const AffineTransform& gradientSpaceTransform() const { return m_gradientSpaceTransformation; }

    void fill(GraphicsContext&, const FloatRect&);
    void adjustParametersForTiledDrawing(FloatSize&, FloatRect&, const FloatSize& spacing);

    unsigned hash() const;
    void invalidateHash() { m_cachedHash = 0; }

#if USE(CG)
    void paint(GraphicsContext&);
    void paint(CGContextRef);
#elif USE(DIRECT2D)
    PlatformGradient createPlatformGradientIfNecessary(ID2D1RenderTarget*);
#elif USE(CAIRO)
    PlatformGradient createPlatformGradient(float globalAlpha);
#endif

private:
    Gradient(LinearData&&);
    Gradient(RadialData&&);
    Gradient(ConicData&&);

    PlatformGradient platformGradient();
    void platformInit() { m_gradient = nullptr; }
    void platformDestroy();

    void sortStopsIfNecessary();

#if USE(DIRECT2D)
    void generateGradient(ID2D1RenderTarget*);
#endif

    Data m_data;

    mutable ColorStopVector m_stops;
    mutable bool m_stopsSorted { false };
    GradientSpreadMethod m_spreadMethod { SpreadMethodPad };
    AffineTransform m_gradientSpaceTransformation;

    mutable unsigned m_cachedHash { 0 };

    PlatformGradient m_gradient;
};

}
