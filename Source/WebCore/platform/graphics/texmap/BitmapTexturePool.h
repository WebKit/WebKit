/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2014 Igalia S.L.
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

#ifndef BitmapTexturePool_h
#define BitmapTexturePool_h

#include "BitmapTexture.h"
#include "Timer.h"
#include <wtf/CurrentTime.h>

#if USE(TEXTURE_MAPPER_GL)
#include "GraphicsContext3D.h"
#endif

namespace WebCore {

class GraphicsContext3D;
class IntSize;

class BitmapTexturePool {
    WTF_MAKE_NONCOPYABLE(BitmapTexturePool);
    WTF_MAKE_FAST_ALLOCATED;
public:
#if USE(TEXTURE_MAPPER_GL)
    explicit BitmapTexturePool(RefPtr<GraphicsContext3D>&&);
#endif

    RefPtr<BitmapTexture> acquireTexture(const IntSize&);

private:
    struct Entry {
        explicit Entry(RefPtr<BitmapTexture>&& texture)
            : m_texture(WTF::move(texture))
        { }

        void markIsInUse() { m_lastUsedTime = monotonicallyIncreasingTime(); }

        RefPtr<BitmapTexture> m_texture;
        double m_lastUsedTime;
    };

    void scheduleReleaseUnusedTextures();
    void releaseUnusedTexturesTimerFired();
    RefPtr<BitmapTexture> createTexture();

#if USE(TEXTURE_MAPPER_GL)
    RefPtr<GraphicsContext3D> m_context3D;
#endif

    Vector<Entry> m_textures;
    Timer m_releaseUnusedTexturesTimer;
};

} // namespace WebCore

#endif // BitmapTexturePool_h
