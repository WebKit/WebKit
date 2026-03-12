/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2014, 2026 Igalia S.L.
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
#include <wtf/CheckedPtr.h>
#include <wtf/Lock.h>
#include <wtf/MonotonicTime.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RunLoop.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>

typedef void *EGLImage;

namespace WebCore {
class IntSize;

// Thread safe singleton to create textures for any GL context created with the shared PlatformDisplay sharing context.
// It should be used with a current GL context to be able to create the textures.
// Unused textures are automatically deleted in the main thread using a timer.
class BitmapTexturePool final : public CanMakeThreadSafeCheckedPtr<BitmapTexturePool> {
    WTF_MAKE_TZONE_ALLOCATED(BitmapTexturePool);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(BitmapTexturePool);
public:
    WEBCORE_EXPORT static BitmapTexturePool& singleton();
    ~BitmapTexturePool() = default;

    WEBCORE_EXPORT Ref<BitmapTexture> acquireTexture(const IntSize&, OptionSet<BitmapTexture::Flags>);
#if USE(GBM)
    Ref<BitmapTexture> createTextureForImage(EGLImage, const IntSize&, OptionSet<BitmapTexture::Flags>);
#endif

#if USE(GRAPHICS_LAYER_WC)
    void releaseUnusedTexturesNow() { releaseUnusedTexturesTimerFired(); }
#endif

private:
    friend class NeverDestroyed<BitmapTexturePool>;
    BitmapTexturePool();

    struct Entry {
        explicit Entry(Ref<BitmapTexture>&& texture)
            : texture(WTF::move(texture))
        { }

        void markIsInUse() { lastUsedTime = MonotonicTime::now(); }
        bool canBeReleased (MonotonicTime minUsedTime) const { return lastUsedTime < minUsedTime && texture->refCount() == 1; }

        const Ref<BitmapTexture> texture;
        MonotonicTime lastUsedTime;
    };

    void scheduleReleaseUnusedTextures();
    void enterLimitExceededModeIfNeeded();
    void exitLimitExceededModeIfNeeded();
    void releaseUnusedTexturesTimerFired();

    Lock m_lock;
    Vector<Entry> m_textures WTF_GUARDED_BY_LOCK(m_lock);
#if USE(GBM)
    Vector<Ref<BitmapTexture>> m_imageTextures WTF_GUARDED_BY_LOCK(m_lock);
#endif
    RunLoop::Timer m_releaseUnusedTexturesTimer WTF_GUARDED_BY_LOCK(m_lock);
    uint64_t m_poolSizeInBytes WTF_GUARDED_BY_LOCK(m_lock) { 0 };
    bool m_onLimitExceededMode WTF_GUARDED_BY_LOCK(m_lock) { false };
    Seconds m_releaseUnusedSecondsTolerance WTF_GUARDED_BY_LOCK(m_lock);
    Seconds m_releaseUnusedTexturesTimerInterval WTF_GUARDED_BY_LOCK(m_lock);
};

} // namespace WebCore

#endif // USE(TEXTURE_MAPPER)
