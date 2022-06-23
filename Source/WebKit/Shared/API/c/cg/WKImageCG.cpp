/*
 * Copyright (C) 2010-2022 Apple Inc. All rights reserved.
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
#include "WKImageCG.h"

#include "ShareableBitmap.h"
#include "WKSharedAPICast.h"
#include "WKString.h"
#include "WebImage.h"
#include <WebCore/ColorSpace.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/ImageBufferUtilitiesCG.h>
#include <WebCore/NativeImage.h>

CGImageRef WKImageCreateCGImage(WKImageRef imageRef)
{
    WebKit::WebImage* webImage = WebKit::toImpl(imageRef);
    if (!webImage)
        return nullptr;

    auto nativeImage = webImage->copyNativeImage();
    if (!nativeImage)
        return nullptr;

    auto platformImage = nativeImage->platformImage();
    return platformImage.leakRef();
}

WKImageRef WKImageCreateFromCGImage(CGImageRef imageRef, WKImageOptions options)
{
    if (!imageRef)
        return nullptr;
    
    auto nativeImage = WebCore::NativeImage::create(imageRef);
    WebCore::IntSize imageSize = nativeImage->size();
    auto webImage = WebKit::WebImage::create(imageSize, WebKit::toImageOptions(options), WebCore::DestinationColorSpace::SRGB());

    auto& graphicsContext = webImage->context();

    WebCore::FloatRect rect(WebCore::FloatPoint(0, 0), imageSize);

    graphicsContext.clearRect(rect);
    graphicsContext.drawNativeImage(*nativeImage, imageSize, rect, rect);
    return toAPI(webImage.leakRef());
}

WKStringRef WKImageCreateDataURLFromImage(CGImageRef imageRef)
{
    String mimeType { "image/png"_s };
    auto value = WebCore::dataURL(imageRef, mimeType, { });
    return WKStringCreateWithUTF8CString(value.utf8().data());
}
