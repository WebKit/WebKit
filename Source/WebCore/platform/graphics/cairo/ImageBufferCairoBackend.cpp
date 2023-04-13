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
#include <cairo.h>

#if USE(CAIRO)

namespace WebCore {

void ImageBufferCairoBackend::transformToColorSpace(const DestinationColorSpace& newColorSpace)
{
#if ENABLE(DESTINATION_COLOR_SPACE_LINEAR_SRGB)
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
#else
    ASSERT(newColorSpace == DestinationColorSpace::SRGB());
    ASSERT(m_parameters.colorSpace == DestinationColorSpace::SRGB());
    UNUSED_PARAM(newColorSpace);
#endif
}

} // namespace WebCore

#endif // USE(CAIRO)
