/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2015, 2025, 2026 Igalia S.L.
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
#include "BitmapTexturePool.h"

#if USE(TEXTURE_MAPPER)
#include "GLContext.h"
#include "GLContextWrapper.h"
#include "PlatformDisplay.h"
#include <wtf/TZoneMallocInlines.h>

#if USE(GLIB)
#include <wtf/glib/RunLoopSourcePriority.h>
#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(BitmapTexturePool);

#if defined(BITMAP_TEXTURE_POOL_MAX_SIZE_IN_MB) && BITMAP_TEXTURE_POOL_MAX_SIZE_IN_MB > 0
static constexpr size_t s_poolSizeLimitInBytes = BITMAP_TEXTURE_POOL_MAX_SIZE_IN_MB * MB;
#else
static constexpr size_t s_poolSizeLimitInBytes = std::numeric_limits<size_t>::max();
#endif

static constexpr Seconds s_releaseUnusedSecondsTolerance { 3_s };
static constexpr Seconds s_releaseUnusedTexturesTimerInterval { 500_ms };
static constexpr Seconds s_releaseUnusedSecondsToleranceOnLimitExceeded { 50_ms };
static constexpr Seconds s_releaseUnusedTexturesTimerIntervalOnLimitExceeded { 200_ms };

BitmapTexturePool& BitmapTexturePool::singleton()
{
    static NeverDestroyed<BitmapTexturePool> pool;
    return pool;
}

BitmapTexturePool::BitmapTexturePool()
    : m_releaseUnusedTexturesTimer(RunLoop::mainSingleton(), "BitmapTexturePool::ReleaseUnusedTexturesTimer"_s, this, &BitmapTexturePool::releaseUnusedTexturesTimerFired)
    , m_releaseUnusedSecondsTolerance(s_releaseUnusedSecondsTolerance)
    , m_releaseUnusedTexturesTimerInterval(s_releaseUnusedTexturesTimerInterval)
{
#if USE(GLIB)
    m_releaseUnusedTexturesTimer.setPriority(RunLoopSourcePriority::ReleaseUnusedResourcesTimer);
#endif
}

Ref<BitmapTexture> BitmapTexturePool::acquireTexture(const IntSize& size, OptionSet<BitmapTexture::Flags> flags)
{
    ASSERT(GLContextWrapper::currentContext());
    Locker locker { m_lock };
    Entry* selectedEntry = std::find_if(m_textures.begin(), m_textures.end(), [&](Entry& entry) {
        return entry.texture->refCount() == 1
            && entry.texture->size() == size
#if USE(GBM)
            && entry.texture->flags().contains(BitmapTexture::Flags::BackedByDMABuf) == flags.contains(BitmapTexture::Flags::BackedByDMABuf)
            && entry.texture->flags().contains(BitmapTexture::Flags::ForceLinearBuffer) == flags.contains(BitmapTexture::Flags::ForceLinearBuffer)
            && entry.texture->flags().contains(BitmapTexture::Flags::ForceVivanteSuperTiledBuffer) == flags.contains(BitmapTexture::Flags::ForceVivanteSuperTiledBuffer)
#endif
            && entry.texture->flags().contains(BitmapTexture::Flags::UseBGRALayout) == flags.contains(BitmapTexture::Flags::UseBGRALayout)
            && entry.texture->flags().contains(BitmapTexture::Flags::DepthBuffer) == flags.contains(BitmapTexture::Flags::DepthBuffer)
            && entry.texture->flags().contains(BitmapTexture::Flags::NearestFiltering) == flags.contains(BitmapTexture::Flags::NearestFiltering);
    });

    if (selectedEntry == m_textures.end()) {
        m_textures.append(Entry(BitmapTexture::create(size, flags)));
        selectedEntry = &m_textures.last();
        m_poolSizeInBytes += selectedEntry->texture->sizeInBytes();
    } else {
        RELEASE_ASSERT(size == selectedEntry->texture->size());
        selectedEntry->texture->reset(size, flags);
    }

    enterLimitExceededModeIfNeeded();

    scheduleReleaseUnusedTextures();

    selectedEntry->markIsInUse();
    return selectedEntry->texture;
}

#if USE(GBM)
Ref<BitmapTexture> BitmapTexturePool::createTextureForImage(EGLImage image, OptionSet<BitmapTexture::Flags> flags)
{
    ASSERT(GLContextWrapper::currentContext());
    Locker locker { m_lock };
    auto texture = BitmapTexture::create(image, flags);
    m_imageTextures.append(texture.copyRef());
    scheduleReleaseUnusedTextures();
    return texture;
}
#endif

void BitmapTexturePool::scheduleReleaseUnusedTextures()
{
    ASSERT(m_lock.isHeld());
    if (m_releaseUnusedTexturesTimer.isActive())
        return;

    m_releaseUnusedTexturesTimer.startOneShot(m_releaseUnusedTexturesTimerInterval);
}

void BitmapTexturePool::releaseUnusedTexturesTimerFired()
{
    Locker locker { m_lock };

    auto hasTextures = [this] -> bool {
        if (!m_textures.isEmpty())
            return true;
#if USE(GBM)
        if (!m_imageTextures.isEmpty())
            return true;
#endif
        return false;
    };

    if (!hasTextures())
        return;

    auto releaseTexturesIfNeeded = [&] {
        if (!m_textures.isEmpty()) {
            // Delete entries, which have been unused in releaseUnusedSecondsTolerance.
            MonotonicTime minUsedTime = MonotonicTime::now() - m_releaseUnusedSecondsTolerance;

            auto matchCount = m_textures.removeAllMatching([this, &minUsedTime](const Entry& entry) {
                if (entry.canBeReleased(minUsedTime)) {
                    m_poolSizeInBytes -= entry.texture->sizeInBytes();
                    return true;
                }
                return false;
            });

            if (matchCount)
                exitLimitExceededModeIfNeeded();
        }

#if USE(GBM)
        if (!m_imageTextures.isEmpty()) {
            m_imageTextures.removeAllMatching([](const BitmapTexture& texture) {
                return texture.refCount() == 1;
            });
        }
#endif
    };

    if (GLContextWrapper::currentContext())
        releaseTexturesIfNeeded();
#if !PLATFORM(WIN)
    else if (auto* context = PlatformDisplay::sharedDisplay().sharingGLContext()) {
        GLContext::ScopedGLContextCurrent scopedCurrent(*context);
        releaseTexturesIfNeeded();
    }
#endif

    if (hasTextures())
        scheduleReleaseUnusedTextures();
}

void BitmapTexturePool::enterLimitExceededModeIfNeeded()
{
    ASSERT(m_lock.isHeld());
    if (m_onLimitExceededMode)
        return;

    if (m_poolSizeInBytes > s_poolSizeLimitInBytes) {
        // If we allocated a new texture and this caused that we went over the size limit, enter limit exceeded mode,
        // set values for tolerance and interval for this mode, and trigger an immediate request to release textures.
        // While on limit exceeded mode, we are more aggressive releasing textures, by polling more often and keeping
        // the unused textures in the pool for smaller periods of time.
        m_onLimitExceededMode = true;
        m_releaseUnusedSecondsTolerance = s_releaseUnusedSecondsToleranceOnLimitExceeded;
        m_releaseUnusedTexturesTimerInterval = s_releaseUnusedTexturesTimerIntervalOnLimitExceeded;
        m_releaseUnusedTexturesTimer.startOneShot(0_s);
    }
}

void BitmapTexturePool::exitLimitExceededModeIfNeeded()
{
    ASSERT(m_lock.isHeld());
    if (!m_onLimitExceededMode)
        return;

    // If we're in limit exceeded mode and the pool size has become smaller than the limit,
    // exit the limit exceeded mode and set the default values for interval and tolerance again.
    if (m_poolSizeInBytes <= s_poolSizeLimitInBytes) {
        m_onLimitExceededMode = false;
        m_releaseUnusedSecondsTolerance = s_releaseUnusedSecondsTolerance;
        m_releaseUnusedTexturesTimerInterval = s_releaseUnusedTexturesTimerInterval;
    }
}

} // namespace WebCore

#endif // USE(TEXTURE_MAPPER)
