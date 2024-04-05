/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2015 Igalia S.L.
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

namespace WebCore {

#if defined(BITMAP_TEXTURE_POOL_MAX_SIZE_IN_MB) && BITMAP_TEXTURE_POOL_MAX_SIZE_IN_MB > 0
static constexpr size_t poolSizeLimit = BITMAP_TEXTURE_POOL_MAX_SIZE_IN_MB * MB;
#else
static constexpr size_t poolSizeLimit = std::numeric_limits<size_t>::max();
#endif

static const Seconds releaseUnusedSecondsTolerance { 3_s };
static const Seconds releaseUnusedTexturesTimerInterval { 500_ms };
static const Seconds releaseUnusedSecondsToleranceOnLimitExceeded { 50_ms };
static const Seconds releaseUnusedTexturesTimerIntervalOnLimitExceeded { 200_ms };

BitmapTexturePool::BitmapTexturePool()
    : m_releaseUnusedTexturesTimer(RunLoop::current(), this, &BitmapTexturePool::releaseUnusedTexturesTimerFired)
    , m_releaseUnusedSecondsTolerance(releaseUnusedSecondsTolerance)
    , m_releaseUnusedTexturesTimerInterval(releaseUnusedTexturesTimerInterval)
{
}

RefPtr<BitmapTexture> BitmapTexturePool::acquireTexture(const IntSize& size, OptionSet<BitmapTexture::Flags> flags)
{
    Entry* selectedEntry = std::find_if(m_textures.begin(), m_textures.end(),
        [&](Entry& entry) {
            return entry.m_texture->refCount() == 1
                && entry.m_texture->size() == size
                && entry.m_texture->flags().contains(BitmapTexture::Flags::DepthBuffer) == flags.contains(BitmapTexture::Flags::DepthBuffer);
        });

    if (selectedEntry == m_textures.end()) {
        m_textures.append(Entry(BitmapTexture::create(size, flags)));
        selectedEntry = &m_textures.last();
        m_poolSize += size.unclampedArea();
    } else
        selectedEntry->m_texture->reset(size, flags);

    enterLimitExceededModeIfNeeded();

    scheduleReleaseUnusedTextures();

    selectedEntry->markIsInUse();
    return selectedEntry->m_texture.copyRef();
}

void BitmapTexturePool::scheduleReleaseUnusedTextures()
{
    if (m_releaseUnusedTexturesTimer.isActive())
        return;

    m_releaseUnusedTexturesTimer.startOneShot(m_releaseUnusedTexturesTimerInterval);
}

void BitmapTexturePool::releaseUnusedTexturesTimerFired()
{
    if (m_textures.isEmpty())
        return;

    // Delete entries, which have been unused in releaseUnusedSecondsTolerance.
    MonotonicTime minUsedTime = MonotonicTime::now() - m_releaseUnusedSecondsTolerance;

    m_textures.removeAllMatching([this, &minUsedTime](const Entry& entry) {
        if (entry.canBeReleased(minUsedTime)) {
            m_poolSize -= entry.m_texture->size().unclampedArea();
            return true;
        }
        return false;
    });

    exitLimitExceededModeIfNeeded();

    if (!m_textures.isEmpty())
        scheduleReleaseUnusedTextures();
}

void BitmapTexturePool::enterLimitExceededModeIfNeeded()
{
    if (m_onLimitExceededMode)
        return;

    if (m_poolSize > poolSizeLimit) {
        // If we allocated a new texture and this caused that we went over the size limit, enter limit exceeded mode,
        // set values for tolerance and interval for this mode, and trigger an immediate request to release textures.
        // While on limit exceeded mode, we are more aggressive releasing textures, by polling more often and keeping
        // the unused textures in the pool for smaller periods of time.
        m_onLimitExceededMode = true;
        m_releaseUnusedSecondsTolerance = releaseUnusedSecondsToleranceOnLimitExceeded;
        m_releaseUnusedTexturesTimerInterval = releaseUnusedTexturesTimerIntervalOnLimitExceeded;
        m_releaseUnusedTexturesTimer.startOneShot(0_s);
    }
}

void BitmapTexturePool::exitLimitExceededModeIfNeeded()
{
    if (!m_onLimitExceededMode)
        return;

    // If we're in limit exceeded mode and the pool size has become smaller than the limit,
    // exit the limit exceeded mode and set the default values for interval and tolerance again.
    if (m_poolSize <= poolSizeLimit) {
        m_onLimitExceededMode = false;
        m_releaseUnusedSecondsTolerance = releaseUnusedSecondsTolerance;
        m_releaseUnusedTexturesTimerInterval = releaseUnusedTexturesTimerInterval;
    }
}

} // namespace WebCore

#endif // USE(TEXTURE_MAPPER)
