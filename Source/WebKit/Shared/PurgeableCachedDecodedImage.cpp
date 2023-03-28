/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "PurgeableCachedDecodedImage.h"

#if ENABLE(PURGEABLE_DECODED_IMAGE_DATA_CACHE)

#include "CacheableDecodedImage.h"
#include "WebProcess.h"
#include <WebCore/MemoryCache.h>
#include <WebCore/Timer.h>
#include <wtf/NeverDestroyed.h>

namespace WebKit {

static constexpr Seconds cleanupDelay { 2_s };

class PurgeableCachedDecodedImageTimer {
public:
    static PurgeableCachedDecodedImageTimer& singleton();

    void start();
    void defer();
    void stop();

private:
    static void timerFired();

    std::optional<DeferrableOneShotTimer> m_timer;
};

PurgeableCachedDecodedImageTimer& PurgeableCachedDecodedImageTimer::singleton()
{
    static LazyNeverDestroyed<PurgeableCachedDecodedImageTimer> timer;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        timer.construct();
    });
    return timer;
}

void PurgeableCachedDecodedImageTimer::start()
{
    if (!m_timer)
        m_timer.emplace(&PurgeableCachedDecodedImageTimer::timerFired, cleanupDelay);

    m_timer->restart();
}

void PurgeableCachedDecodedImageTimer::defer()
{
    if (m_timer)
        m_timer->restart();
}

void PurgeableCachedDecodedImageTimer::stop()
{
    m_timer.reset();
}

void PurgeableCachedDecodedImageTimer::timerFired()
{
    WebProcess::singleton().releaseAllRemoteImageResources();
    MemoryCache::singleton().destroyAndCacheDecodedDataForAllImages();
}

std::unique_ptr<PurgeableCachedDecodedImage> PurgeableCachedDecodedImage::create(ShareableBitmap& bitmap)
{
    auto memory = createVolatileCopyOfBitmap(bitmap);
    if (!memory)
        return nullptr;

    return makeUnique<PurgeableCachedDecodedImage>(memory.releaseNonNull(), bitmap.size(), bitmap.configuration());
}

RefPtr<SharedMemory> PurgeableCachedDecodedImage::createVolatileCopyOfBitmap(ShareableBitmap& bitmap)
{
    auto memory = SharedMemory::allocate(bitmap.sizeInBytes());
    if (!memory)
        return nullptr;

    memcpy(memory->data(), bitmap.data(), memory->size());
    memory->setVolatile();
    return memory;
}

PurgeableCachedDecodedImage::PurgeableCachedDecodedImage(Ref<SharedMemory>&& memory, const WebCore::IntSize& size, const ShareableBitmapConfiguration& configuration)
    : m_memory(WTFMove(memory))
    , m_size(size)
    , m_configuration(configuration)
{
    PurgeableCachedDecodedImageTimer::singleton().defer();
}

PurgeableCachedDecodedImage::~PurgeableCachedDecodedImage() = default;

RefPtr<NativeImage> PurgeableCachedDecodedImage::createNativeImage()
{
    if (!m_memory)
        return nullptr;

    if (m_memory->setNonVolatile() == WebCore::SetNonVolatileResult::Empty) {
        m_memory = nullptr;
        return nullptr;
    }

    auto bitmap = ShareableBitmap::create(m_size, m_configuration, m_memory.releaseNonNull());
    if (!bitmap)
        return nullptr;

    auto decodedImage = CacheableDecodedImage::create(WTFMove(bitmap));
    if (!decodedImage)
        return nullptr;

    return NativeImage::create(makeUniqueRefFromNonNullUniquePtr(WTFMove(decodedImage)));
}

void PurgeableCachedDecodedImage::startCleanup()
{
    PurgeableCachedDecodedImageTimer::singleton().start();
}

void PurgeableCachedDecodedImage::stopCleanup()
{
    PurgeableCachedDecodedImageTimer::singleton().stop();
}

} // namespace WebKit

#endif // ENABLE(PURGEABLE_DECODED_IMAGE_DATA_CACHE)
