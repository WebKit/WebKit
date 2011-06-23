/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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


#ifndef LayerTextureSubImage_h
#define LayerTextureSubImage_h

#if USE(ACCELERATED_COMPOSITING)

#include "GraphicsTypes3D.h"
#include "IntRect.h"
#include "IntSize.h"
#include <wtf/OwnArrayPtr.h>

namespace WebCore {

class GraphicsContext3D;

class LayerTextureSubImage {
public:
    explicit LayerTextureSubImage(bool useMapSubForUpload);
    ~LayerTextureSubImage();

    void setSubImageSize(const IntSize&);
    void upload(const uint8_t* image, const IntRect& imageRect,
                const IntRect& sourceRect, const IntRect& destRect,
                GC3Denum format, GraphicsContext3D*);

private:
    void uploadWithTexSubImage(const uint8_t* image, const IntRect& imageRect,
                               const IntRect& sourceRect, const IntRect& destRect,
                               GC3Denum format, GraphicsContext3D*);
    void uploadWithMapTexSubImage(const uint8_t* image, const IntRect& imageRect,
                                  const IntRect& sourceRect, const IntRect& destRect,
                                  GC3Denum format, GraphicsContext3D*);

    bool m_useMapTexSubImage;
    IntSize m_subImageSize;
    OwnArrayPtr<uint8_t> m_subImage;
};

} // namespace WebCore
#endif // USE(ACCELERATED_COMPOSITING)
#endif // LayerTextureSubImage_h

