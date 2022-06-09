/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007 Holger Hans Peter Freyther <zecke@selfish.org>
 * Copyright (C) 2008, 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2010 Torch Mobile (Beijing) Co. Ltd. All rights reserved.
 * Copyright (C) 2020 Apple Inc.  All rights reserved.
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
#include "ImageBufferCairoBackend.h"

#include "BitmapImage.h"
#include "CairoOperations.h"
#include "Color.h"
#include "ColorTransferFunctions.h"
#include "GraphicsContext.h"
#include "GraphicsContextCairo.h"
#include "ImageBufferUtilitiesCairo.h"
#include "MIMETypeRegistry.h"
#include <cairo.h>
#include <wtf/text/Base64.h>

#if USE(CAIRO)

namespace WebCore {

RefPtr<Image> ImageBufferCairoBackend::copyImage(BackingStoreCopy copyBehavior, PreserveResolution) const
{
    // BitmapImage will release the passed in surface on destruction
    return BitmapImage::create(copyNativeImage(copyBehavior));
}

void ImageBufferCairoBackend::draw(GraphicsContext& destContext, const FloatRect& destRect, const FloatRect& srcRect, const ImagePaintingOptions& options)
{
    if (!destContext.hasPlatformContext()) {
        // If there's no platformContext, we're using threaded rendering, and all the operations must be done
        // through the GraphicsContext.
        auto image = copyImage(&destContext == &context() ? CopyBackingStore : DontCopyBackingStore);
        destContext.drawImage(*image, destRect, srcRect, options);
        return;
    }

    InterpolationQualityMaintainer interpolationQualityForThisScope(destContext, options.interpolationQuality());
    const auto& destinationContextState = destContext.state();

    if (auto image = copyNativeImage(&destContext == &context() ? CopyBackingStore : DontCopyBackingStore))
        drawPlatformImage(*destContext.platformContext(), image->platformImage().get(), destRect, srcRect, { options, destinationContextState.imageInterpolationQuality }, destinationContextState.alpha, WebCore::Cairo::ShadowState(destinationContextState));
}

void ImageBufferCairoBackend::drawPattern(GraphicsContext& destContext, const FloatRect& destRect, const FloatRect& srcRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, const ImagePaintingOptions& options)
{
    if (!destContext.hasPlatformContext()) {
        // If there's no platformContext, we're using threaded rendering, and all the operations must be done
        // through the GraphicsContext.
        auto image = copyImage(DontCopyBackingStore);
        image->drawPattern(destContext, destRect, srcRect, patternTransform, phase, spacing, options);
        return;
    }

    if (auto image = copyNativeImage(&destContext == &context() ? CopyBackingStore : DontCopyBackingStore))
        Cairo::drawPattern(*destContext.platformContext(), image->platformImage().get(), logicalSize(), destRect, srcRect, patternTransform, phase, options);
}

void ImageBufferCairoBackend::clipToMask(GraphicsContext& destContext, const FloatRect& destRect)
{
    if (auto image = copyNativeImage(DontCopyBackingStore))
        Cairo::clipToImageBuffer(*destContext.platformContext(), image->platformImage().get(), destRect);
}

void ImageBufferCairoBackend::transformToColorSpace(const DestinationColorSpace& newColorSpace)
{
    if (m_parameters.colorSpace == newColorSpace)
        return;

    // only sRGB <-> linearRGB are supported at the moment
    if ((m_parameters.colorSpace != DestinationColorSpace::LinearSRGB() && m_parameters.colorSpace != DestinationColorSpace::SRGB())
        || (newColorSpace != DestinationColorSpace::LinearSRGB() && newColorSpace != DestinationColorSpace::SRGB()))
        return;

    m_parameters.colorSpace = newColorSpace;

    if (newColorSpace == DestinationColorSpace::LinearSRGB()) {
        static const std::array<uint8_t, 256> linearRgbLUT = [] {
            std::array<uint8_t, 256> array;
            for (unsigned i = 0; i < 256; i++) {
                float color = i / 255.0f;
                color = SRGBTransferFunction<float, TransferFunctionMode::Clamped>::toLinear(color);
                array[i] = static_cast<uint8_t>(round(color * 255));
            }
            return array;
        }();
        platformTransformColorSpace(linearRgbLUT);
    } else {
        static const std::array<uint8_t, 256> deviceRgbLUT= [] {
            std::array<uint8_t, 256> array;
            for (unsigned i = 0; i < 256; i++) {
                float color = i / 255.0f;
                color = SRGBTransferFunction<float, TransferFunctionMode::Clamped>::toGammaEncoded(color);
                array[i] = static_cast<uint8_t>(round(color * 255));
            }
            return array;
        }();
        platformTransformColorSpace(deviceRgbLUT);
    }
}

String ImageBufferCairoBackend::toDataURL(const String& mimeType, std::optional<double> quality, PreserveResolution) const
{
    Vector<uint8_t> encodedImage = toData(mimeType, quality);
    if (encodedImage.isEmpty())
        return "data:,";

    return makeString("data:", mimeType, ";base64,", base64Encoded(encodedImage.data(), encodedImage.size()));
}

Vector<uint8_t> ImageBufferCairoBackend::toData(const String& mimeType, std::optional<double> quality) const
{
    ASSERT(MIMETypeRegistry::isSupportedImageMIMETypeForEncoding(mimeType));
    cairo_surface_t* image = cairo_get_target(context().platformContext()->cr());
    return data(image, mimeType, quality);
}

} // namespace WebCore

#endif // USE(CAIRO)
