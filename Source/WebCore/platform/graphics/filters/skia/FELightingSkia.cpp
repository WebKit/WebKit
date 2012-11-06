/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#if USE(SKIA)
#include "FELighting.h"

#include "DistantLightSource.h"
#include "NativeImageSkia.h"
#include "PointLightSource.h"
#include "SkLightingImageFilter.h"
#include "SkiaImageFilterBuilder.h"
#include "SpotLightSource.h"

namespace WebCore {

SkImageFilter* FELighting::createImageFilter(SkiaImageFilterBuilder* builder)
{
    SkImageFilter* input = builder ? builder->build(inputEffect(0)) : 0;
    switch (m_lightSource->type()) {
    case LS_DISTANT: {
        DistantLightSource* distantLightSource = static_cast<DistantLightSource*>(m_lightSource.get());
        float azimuthRad = deg2rad(distantLightSource->azimuth());
        float elevationRad = deg2rad(distantLightSource->elevation());
        SkPoint3 direction(cosf(azimuthRad) * cosf(elevationRad),
                           sinf(azimuthRad) * cosf(elevationRad),
                           sinf(elevationRad));
        if (m_specularConstant > 0)
            return SkLightingImageFilter::CreateDistantLitSpecular(direction, m_lightingColor.rgb(), m_surfaceScale, m_specularConstant, m_specularExponent, input);
        else
            return SkLightingImageFilter::CreateDistantLitDiffuse(direction, m_lightingColor.rgb(), m_surfaceScale, m_diffuseConstant, input);
    }
    case LS_POINT: {
        PointLightSource* pointLightSource = static_cast<PointLightSource*>(m_lightSource.get());
        FloatPoint3D position = pointLightSource->position();
        SkPoint3 skPosition(position.x(), position.y(), position.z());
        if (m_specularConstant > 0)
            return SkLightingImageFilter::CreatePointLitSpecular(skPosition, m_lightingColor.rgb(), m_surfaceScale, m_specularConstant, m_specularExponent, input);
        else
            return SkLightingImageFilter::CreatePointLitDiffuse(skPosition, m_lightingColor.rgb(), m_surfaceScale, m_diffuseConstant, input);
    }
    case LS_SPOT: {
        SpotLightSource* spotLightSource = static_cast<SpotLightSource*>(m_lightSource.get());
        SkPoint3 location(spotLightSource->position().x(), spotLightSource->position().y(), spotLightSource->position().z());
        SkPoint3 target(spotLightSource->direction().x(), spotLightSource->direction().y(), spotLightSource->direction().z());
        float specularExponent = spotLightSource->specularExponent();
        float limitingConeAngle = spotLightSource->limitingConeAngle();
        if (!limitingConeAngle || limitingConeAngle > 90 || limitingConeAngle < -90)
            limitingConeAngle = 90;
        if (m_specularConstant > 0)
            return SkLightingImageFilter::CreateSpotLitSpecular(location, target, specularExponent, limitingConeAngle, m_lightingColor.rgb(), m_surfaceScale, m_specularConstant, m_specularExponent, input);
        else
            return SkLightingImageFilter::CreateSpotLitDiffuse(location, target, specularExponent, limitingConeAngle, m_lightingColor.rgb(), m_surfaceScale, m_diffuseConstant, input);
    }
    default:
        ASSERT_NOT_REACHED();
        return 0;
    }
}

bool FELighting::platformApplySkia()
{
    // For now, only use the skia implementation for accelerated rendering.
    if (filter()->renderingMode() != Accelerated)
        return false;

    ImageBuffer* resultImage = createImageBufferResult();
    if (!resultImage)
        return false;

    FilterEffect* in = inputEffect(0);

    IntRect drawingRegion = drawingRegionOfInputImage(in->absolutePaintRect());

    setIsAlphaImage(in->isAlphaImage());

    RefPtr<Image> image = in->asImageBuffer()->copyImage(DontCopyBackingStore);
    NativeImageSkia* nativeImage = image->nativeImageForCurrentFrame();

    GraphicsContext* dstContext = resultImage->context();

    SkPaint paint;
    paint.setImageFilter(createImageFilter(0))->unref();
    dstContext->platformContext()->canvas()->drawBitmap(nativeImage->bitmap(), drawingRegion.location().x(), drawingRegion.location().y(), &paint);
    return true;
}

};
#endif
