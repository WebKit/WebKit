/*
 * Copyright (C) 2012, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TextureUploader_h
#define TextureUploader_h

#include "LayerTextureUpdater.h"

namespace WebCore {

class TextureUploader {
public:
    virtual void uploadTexture(GraphicsContext3D*, LayerTextureUpdater::Texture*, TextureAllocator*, const IntRect sourceRect, const IntRect destRect) = 0;

protected:
    virtual ~TextureUploader() { }
};

class AcceleratedTextureUploader : public TextureUploader {
    WTF_MAKE_NONCOPYABLE(AcceleratedTextureUploader);
public:
    static PassOwnPtr<AcceleratedTextureUploader> create(PassRefPtr<GraphicsContext3D> context)
    {
        return adoptPtr(new AcceleratedTextureUploader(context));
    }
    virtual ~AcceleratedTextureUploader();

    virtual void uploadTexture(GraphicsContext3D*, LayerTextureUpdater::Texture*, TextureAllocator*, const IntRect sourceRect, const IntRect destRect);

protected:
    explicit AcceleratedTextureUploader(PassRefPtr<GraphicsContext3D>);

    RefPtr<GraphicsContext3D> m_context;
};

}

#endif
