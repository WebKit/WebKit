/*
 * Copyright (C) 2024 Igalia S.L.
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
#include "CoordinatedImageBackingStore.h"

#if USE(COORDINATED_GRAPHICS)
#include "CoordinatedPlatformLayerBufferNativeImage.h"
#include "NativeImage.h"

#if USE(CAIRO)
#include "CairoUtilities.h"
#endif

namespace WebCore {

Ref<CoordinatedImageBackingStore> CoordinatedImageBackingStore::create(Ref<NativeImage>&& nativeImage)
{
    return adoptRef(*new CoordinatedImageBackingStore(WTFMove(nativeImage)));
}

CoordinatedImageBackingStore::CoordinatedImageBackingStore(Ref<NativeImage>&& nativeImage)
    : m_buffer(CoordinatedPlatformLayerBufferNativeImage::create(WTFMove(nativeImage), nullptr))
{
}

CoordinatedImageBackingStore::~CoordinatedImageBackingStore() = default;

uint64_t CoordinatedImageBackingStore::uniqueIDForNativeImage(const NativeImage& nativeImage)
{
#if USE(CAIRO)
    return getSurfaceUniqueID(nativeImage.platformImage().get());
#elif USE(SKIA)
    return nativeImage.platformImage()->uniqueID();
#endif
}

bool CoordinatedImageBackingStore::isSameNativeImage(const NativeImage& nativeImage)
{
    return uniqueIDForNativeImage(nativeImage) == uniqueIDForNativeImage(downcast<CoordinatedPlatformLayerBufferNativeImage>(*m_buffer).image());
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS)
