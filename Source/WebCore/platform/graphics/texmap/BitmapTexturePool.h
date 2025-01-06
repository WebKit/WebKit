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

#pragma once

#if USE(TEXTURE_MAPPER)

#include "BitmapTexture.h"
#include <wtf/RunLoop.h>

namespace WebCore {
class BitmapTexturePool;
}

namespace WTF {
template<typename T> struct IsDeprecatedTimerSmartPointerException;
template<> struct IsDeprecatedTimerSmartPointerException<WebCore::BitmapTexturePool> : std::true_type { };
}

namespace WebCore {

class IntSize;

class BitmapTexturePool {
    WTF_MAKE_NONCOPYABLE(BitmapTexturePool);
    WTF_MAKE_FAST_ALLOCATED();
public:
    BitmapTexturePool();

    Ref<BitmapTexture> acquireTexture(const IntSize&, OptionSet<BitmapTexture::Flags>);
    void releaseUnusedTexturesTimerFired();

private:
    struct Entry {
        explicit Entry(Ref<BitmapTexture>&& texture)
            : m_texture(WTFMove(texture))
        { }

        void markIsInUse() { m_lastUsedTime = MonotonicTime::now(); }
        bool canBeReleased (MonotonicTime minUsedTime) const { return m_lastUsedTime < minUsedTime && m_texture->refCount() == 1; }

        Ref<BitmapTexture> m_texture;
        MonotonicTime m_lastUsedTime;
    };

    void scheduleReleaseUnusedTextures();
    void enterLimitExceededModeIfNeeded();
    void exitLimitExceededModeIfNeeded();

    Vector<Entry> m_textures;
    RunLoop::Timer m_releaseUnusedTexturesTimer;
    uint64_t m_poolSize { 0 };
    bool m_onLimitExceededMode { false };
    Seconds m_releaseUnusedSecondsTolerance;
    Seconds m_releaseUnusedTexturesTimerInterval;
};

} // namespace WebCore

#endif // USE(TEXTURE_MAPPER)
