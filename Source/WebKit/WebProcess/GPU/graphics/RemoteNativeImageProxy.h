/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(GPU_PROCESS)

#include "RemoteNativeImageIdentifier.h"
#include <WebCore/IntSize.h>
#include <WebCore/NativeImage.h>
#include <WebCore/PlatformColorSpace.h>
#include <optional>

namespace WebKit {

class RemoteResourceCacheProxy;
class RemoteSharedResourceCacheProxy;

class RemoteNativeImageProxy final : public WebCore::NativeImage {
    WTF_MAKE_TZONE_ALLOCATED(RemoteNativeImageProxy);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RemoteNativeImageProxy);
public:
    static Ref<RemoteNativeImageProxy> create(const WebCore::IntSize&, WebCore::PlatformColorSpace&&, bool hasAlpha, WeakRef<RemoteResourceCacheProxy>&&);

    // Creates an image whose contents live in the GPU process's RemoteSharedResourceCache. It is not
    // cached in any rendering backend until RemoteResourceCacheProxy::recordSharedNativeImageUse()
    // adopts it, and its shared cache entry is released via `sharedResourceCache` when destroyed.
    static Ref<RemoteNativeImageProxy> create(const WebCore::IntSize&, WebCore::PlatformColorSpace&&, bool hasAlpha, Ref<RemoteSharedResourceCacheProxy>&&);

    ~RemoteNativeImageProxy() override;
    WebCore::PlatformImagePtr platformImage() const override;
    WebCore::IntSize size() const override;
    bool hasAlpha() const override;
    WebCore::DestinationColorSpace colorSpace() const override;

    // Reference tracking for images held in the RemoteSharedResourceCache. The tracker's lifetime is
    // this image's lifetime, so the shared cache does not need to track it separately.
    bool isSharedNativeImage() const { return m_referenceTracker.has_value(); }
    RemoteNativeImageReference reference() const { return m_referenceTracker->add(); }
    RemoteNativeImageReadReference newReadReference() const { return m_referenceTracker->read(); }
    RemoteNativeImageWriteReference writeReference() const { return m_referenceTracker->write(); }

    // Called by the RemoteResourceCacheProxy that adopts a shared image into its rendering backend, so
    // that the image's destruction is reported to it and pixel read-back is routed through it.
    void attachResourceCache(WeakRef<RemoteResourceCacheProxy>&&);

private:
    RemoteNativeImageProxy(const WebCore::IntSize&, WebCore::PlatformColorSpace&&, bool hasAlpha, WeakPtr<RemoteResourceCacheProxy>&&, RefPtr<RemoteSharedResourceCacheProxy>&&);

    WeakPtr<RemoteResourceCacheProxy> m_resourceCache;
    // Set only for images held in the RemoteSharedResourceCache, which releases the shared entry.
    const RefPtr<RemoteSharedResourceCacheProxy> m_sharedResourceCache;
    std::optional<RemoteNativeImageReferenceTracker> m_referenceTracker;
    const WebCore::IntSize m_size;
    const WebCore::PlatformColorSpace m_colorSpace;
    const bool m_hasAlpha;
};

}

#endif
