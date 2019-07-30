/*
 * Copyright (C) 2016-2018 Apple Inc.  All rights reserved.
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

#include "config.h"
#include "Gradient.h"

#include "FloatPoint.h"
#include "GraphicsContext.h"
#include "PlatformContextDirect2D.h"
#include <d2d1.h>
#include <wtf/RetainPtr.h>

#define GRADIENT_DRAWING 3

namespace WebCore {

void Gradient::platformDestroy()
{
    if (m_gradient)
        m_gradient->Release();
    m_gradient = nullptr;
}

ID2D1Brush* Gradient::platformGradient()
{
    ASSERT(m_gradient);
    return m_gradient;
}

ID2D1Brush* Gradient::createPlatformGradientIfNecessary(ID2D1RenderTarget* context)
{
    generateGradient(context);
    return m_gradient;
}

void Gradient::generateGradient(ID2D1RenderTarget* renderTarget)
{
    sortStopsIfNecessary();

    Vector<D2D1_GRADIENT_STOP> gradientStops;
    // FIXME: Add support for ExtendedColor.
    for (auto stop : m_stops) {
        float r;
        float g;
        float b;
        float a;
        stop.color.getRGBA(r, g, b, a);
        gradientStops.append(D2D1::GradientStop(stop.offset, D2D1::ColorF(r, g, b, a)));
    }

    COMPtr<ID2D1GradientStopCollection> gradientStopCollection;
    HRESULT hr = renderTarget->CreateGradientStopCollection(gradientStops.data(), gradientStops.size(), &gradientStopCollection);
    RELEASE_ASSERT(SUCCEEDED(hr));

    if (m_gradient) {
        m_gradient->Release();
        m_gradient = nullptr;
    }

    WTF::switchOn(m_data,
        [&] (const LinearData& data) {
            ID2D1LinearGradientBrush* linearGradient = nullptr;
            hr = renderTarget->CreateLinearGradientBrush(
                D2D1::LinearGradientBrushProperties(data.point0, data.point1),
                D2D1::BrushProperties(), gradientStopCollection.get(),
                &linearGradient);
            RELEASE_ASSERT(SUCCEEDED(hr));
            m_gradient = linearGradient;
        },
        [&] (const RadialData& data) {
            FloatSize offset = data.point1 - data.point0;
            ID2D1RadialGradientBrush* radialGradient = nullptr;
            float radiusX = data.endRadius + offset.width();
            float radiusY = radiusX / data.aspectRatio;
            hr = renderTarget->CreateRadialGradientBrush(
                D2D1::RadialGradientBrushProperties(data.point0, D2D1::Point2F(offset.width(), offset.height()), radiusX, radiusY),
                D2D1::BrushProperties(), gradientStopCollection.get(),
                &radialGradient);
            RELEASE_ASSERT(SUCCEEDED(hr));
            m_gradient = radialGradient;
        },
        [&] (const ConicData&) {
            // FIXME: implement conic gradient rendering.
        }
    );

    hash();
}

void Gradient::fill(GraphicsContext& context, const FloatRect& rect)
{
    auto d2dContext = context.platformContext()->renderTarget();

    WTF::switchOn(m_data,
        [&] (const LinearData& data) {
            if (!m_cachedHash || !m_gradient)
                generateGradient(d2dContext);

            d2dContext->SetTags(GRADIENT_DRAWING, __LINE__);

            const D2D1_RECT_F d2dRect = rect;
            d2dContext->FillRectangle(&d2dRect, m_gradient);
        },
        [&] (const RadialData& data) {
            bool needScaling = data.aspectRatio != 1;
            if (needScaling) {
                context.save();
                // Scale from the center of the gradient. We only ever scale non-deprecated gradients,
                // for which m_p0 == m_p1.
                ASSERT(data.point0 == data.point1);

                D2D1_MATRIX_3X2_F ctm = { };
                d2dContext->GetTransform(&ctm);

                AffineTransform transform(ctm);
                transform.translate(data.point0);
                transform.scaleNonUniform(1.0, 1.0 / data.aspectRatio);
                transform.translate(-data.point0);

                d2dContext->SetTransform(ctm);
            }

            if (!m_cachedHash || !m_gradient)
                generateGradient(d2dContext);

            d2dContext->SetTags(GRADIENT_DRAWING, __LINE__);

            const D2D1_RECT_F d2dRect = rect;
            d2dContext->FillRectangle(&d2dRect, m_gradient);

            if (needScaling)
                context.restore();
        },
        [&] (const ConicData&) {
            // FIXME: implement conic gradient rendering.
        }
    );
}

}
