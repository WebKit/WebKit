/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "config.h"

#if ENABLE(WEBGL)

#include "GraphicsContext3D.h"

#include "BitmapImage.h"
#include "Image.h"
#include "ImageSource.h"
#include "NativeImageSkia.h"
#include "SkColorPriv.h"
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>

#include <algorithm>

namespace WebCore {

bool GraphicsContext3D::getImageData(Image* image,
                                     GC3Denum format,
                                     GC3Denum type,
                                     bool premultiplyAlpha,
                                     bool ignoreGammaAndColorProfile,
                                     Vector<uint8_t>& outputVector)
{
    if (!image)
        return false;
    OwnPtr<NativeImageSkia> pixels;
    NativeImageSkia* skiaImage = image->nativeImageForCurrentFrame();
    AlphaOp neededAlphaOp = AlphaDoNothing;
    bool hasAlpha = skiaImage ? !skiaImage->isOpaque() : true;
    if ((!skiaImage || ignoreGammaAndColorProfile || (hasAlpha && !premultiplyAlpha)) && image->data()) {
        ImageSource decoder(ImageSource::AlphaNotPremultiplied,
                            ignoreGammaAndColorProfile ? ImageSource::GammaAndColorProfileIgnored : ImageSource::GammaAndColorProfileApplied);
        // Attempt to get raw unpremultiplied image data 
        decoder.setData(image->data(), true);
        if (!decoder.frameCount() || !decoder.frameIsCompleteAtIndex(0))
            return false;
        hasAlpha = decoder.frameHasAlphaAtIndex(0);
        pixels = adoptPtr(decoder.createFrameAtIndex(0));
        if (!pixels.get() || !pixels->isDataComplete() || !pixels->width() || !pixels->height())
            return false;
        SkBitmap::Config skiaConfig = pixels->config();
        if (skiaConfig != SkBitmap::kARGB_8888_Config)
            return false;
        skiaImage = pixels.get();
        if (hasAlpha && premultiplyAlpha)
            neededAlphaOp = AlphaDoPremultiply;
    } else if (!premultiplyAlpha && hasAlpha)
        neededAlphaOp = AlphaDoUnmultiply;
    if (!skiaImage)
        return false;
    SkBitmap& skiaImageRef = *skiaImage;
    SkAutoLockPixels lock(skiaImageRef);
    ASSERT(skiaImage->rowBytes() == skiaImage->width() * 4);
    outputVector.resize(skiaImage->rowBytes() * skiaImage->height());
    return packPixels(reinterpret_cast<const uint8_t*>(skiaImage->getPixels()),
                      SK_B32_SHIFT ? SourceFormatRGBA8 : SourceFormatBGRA8,
                      skiaImage->width(), skiaImage->height(), 0,
                      format, type, neededAlphaOp, outputVector.data());
}

} // namespace WebCore

#endif // ENABLE(WEBGL)
