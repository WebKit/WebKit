/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ImageDecoder.h"

#include <wtf/NeverDestroyed.h>

#if USE(CG)
#include "ImageDecoderCG.h"
#elif USE(DIRECT2D)
#include "ImageDecoderDirect2D.h"
#else
#include "ScalableImageDecoder.h"
#endif

#if HAVE(AVASSETREADER)
#include "ImageDecoderAVFObjC.h"
#endif

#if USE(GSTREAMER) && ENABLE(VIDEO)
#include "ImageDecoderGStreamer.h"
#endif

namespace WebCore {

#if ENABLE(GPU_PROCESS) && HAVE(AVASSETREADER)
using FactoryVector = Vector<ImageDecoder::ImageDecoderFactory>;

static void platformRegisterFactories(FactoryVector& factories)
{
    factories.append({ ImageDecoderAVFObjC::supportsMediaType, ImageDecoderAVFObjC::canDecodeType, ImageDecoderAVFObjC::create });
}

static FactoryVector& installedFactories()
{
    static auto factories = makeNeverDestroyed<FactoryVector>({ });
    static std::once_flag registerDefaults;
    std::call_once(registerDefaults, [&] {
        platformRegisterFactories(factories);
    });

    return factories;
}

void ImageDecoder::installFactory(ImageDecoder::ImageDecoderFactory&& factory)
{
    installedFactories().append(WTFMove(factory));
}

void ImageDecoder::resetFactories()
{
    installedFactories().clear();
    platformRegisterFactories(installedFactories());
}

void ImageDecoder::clearFactories()
{
    installedFactories().clear();
}
#endif

RefPtr<ImageDecoder> ImageDecoder::create(SharedBuffer& data, const String& mimeType, AlphaOption alphaOption, GammaAndColorProfileOption gammaAndColorProfileOption)
{
    UNUSED_PARAM(mimeType);

#if HAVE(AVASSETREADER)
    if (!ImageDecoderCG::canDecodeType(mimeType)) {
#if ENABLE(GPU_PROCESS)
        for (auto& factory : installedFactories()) {
            if (factory.canDecodeType(mimeType))
                return factory.createImageDecoder(data, mimeType, alphaOption, gammaAndColorProfileOption);
        }
#else
        if (ImageDecoderAVFObjC::canDecodeType(mimeType))
            return ImageDecoderAVFObjC::create(data, mimeType, alphaOption, gammaAndColorProfileOption);
#endif
    }
#endif

#if USE(GSTREAMER) && ENABLE(VIDEO)
    if (ImageDecoderGStreamer::canDecodeType(mimeType))
        return ImageDecoderGStreamer::create(data, mimeType, alphaOption, gammaAndColorProfileOption);
#endif

#if USE(CG)
    return ImageDecoderCG::create(data, alphaOption, gammaAndColorProfileOption);
#elif USE(DIRECT2D)
    return ImageDecoderDirect2D::create(data, alphaOption, gammaAndColorProfileOption);
#else
    return ScalableImageDecoder::create(data, alphaOption, gammaAndColorProfileOption);
#endif
}

bool ImageDecoder::supportsMediaType(MediaType type)
{
#if USE(CG)
    if (ImageDecoderCG::supportsMediaType(type))
        return true;
#elif USE(DIRECT2D)
    if (ImageDecoderDirect2D::supportsMediaType(type))
        return true;
#else
    if (ScalableImageDecoder::supportsMediaType(type))
        return true;
#endif

#if HAVE(AVASSETREADER)
#if ENABLE(GPU_PROCESS)
    for (auto& factory : installedFactories()) {
        if (factory.supportsMediaType(type))
            return true;
    }
#else
    if (ImageDecoderAVFObjC::supportsMediaType(type))
        return true;
#endif
#endif

#if USE(GSTREAMER) && ENABLE(VIDEO)
    if (ImageDecoderGStreamer::supportsMediaType(type))
        return true;
#endif

    return false;
}

}
