/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "NativeImage.h"

#include "Direct2DOperations.h"
#include "GeometryUtilities.h"
#include "NotImplemented.h"
#include "PlatformContextDirect2D.h"
#include <d2d1.h>
#include <wincodec.h>

namespace WebCore {

static IWICImagingFactory* imagingFactory()
{
    static IWICImagingFactory* imagingFactory = nullptr;
    if (!imagingFactory) {
        HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&imagingFactory));
        RELEASE_ASSERT(SUCCEEDED(hr));
    }

    return imagingFactory;
}

IntSize NativeImage::size() const
{
    return m_platformImage->GetPixelSize();
}

bool NativeImage::hasAlpha() const
{
    D2D1_PIXEL_FORMAT pixelFormat = m_platformImage->GetPixelFormat();
    return pixelFormat.alphaMode != D2D1_ALPHA_MODE_IGNORE;
}

Color NativeImage::singlePixelSolidColor() const
{
    if (size() != IntSize(1, 1))
        return Color();

    notImplemented();
    return Color();
}

void NativeImage::clearSubimages()
{
    notImplemented();
}

}
