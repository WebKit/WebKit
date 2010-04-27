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

#if ENABLE(3D_CANVAS)

#include "GraphicsContext3D.h"

#include "Image.h"
#include "NativeImageSkia.h"

#include <algorithm>

namespace WebCore {

bool GraphicsContext3D::getImageData(Image* image,
                                     Vector<uint8_t>& outputVector,
                                     bool premultiplyAlpha,
                                     bool* hasAlphaChannel,
                                     AlphaOp* neededAlphaOp,
                                     unsigned int* format)
{
    if (!image)
        return false;
    NativeImageSkia* skiaImage = image->nativeImageForCurrentFrame();
    if (!skiaImage)
        return false;
    SkBitmap::Config skiaConfig = skiaImage->config();
    // FIXME: must support more image configurations.
    if (skiaConfig != SkBitmap::kARGB_8888_Config)
        return false;
    SkBitmap& skiaImageRef = *skiaImage;
    SkAutoLockPixels lock(skiaImageRef);
    int height = skiaImage->height();
    int rowBytes = skiaImage->rowBytes();
    ASSERT(rowBytes == skiaImage->width() * 4);
    uint8_t* pixels = reinterpret_cast<uint8_t*>(skiaImage->getPixels());
    outputVector.resize(rowBytes * height);
    int size = rowBytes * height;
    memcpy(outputVector.data(), pixels, size);
    *hasAlphaChannel = true;
    if (!premultiplyAlpha)
        // FIXME: must fetch the image data before the premultiplication step
        *neededAlphaOp = kAlphaDoUnmultiply;
    // Convert from BGRA to RGBA. FIXME: add GL_BGRA extension support
    // to all underlying OpenGL implementations.
    for (int i = 0; i < size; i += 4)
        std::swap(outputVector[i], outputVector[i + 2]);
    *format = RGBA;
    return true;
}

} // namespace WebCore

#endif // ENABLE(3D_CANVAS)
