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

#include <WebCore/IntSize.h>
#include <WebCore/NativeImage.h>
#include <WebCore/PlatformColorSpace.h>

namespace WebKit {

class RemoteResourceCacheProxy;

class RemoteNativeImageProxy final : public WebCore::NativeImage {
    WTF_MAKE_TZONE_ALLOCATED(RemoteNativeImageProxy);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RemoteNativeImageProxy);
public:
    static Ref<RemoteNativeImageProxy> create(const WebCore::IntSize&, WebCore::PlatformColorSpace&&, bool hasAlpha, WeakRef<RemoteResourceCacheProxy>&&);
    ~RemoteNativeImageProxy() override;
    const WebCore::PlatformImagePtr& platformImage() const override;
    WebCore::IntSize size() const override;
    bool hasAlpha() const override;
    WebCore::DestinationColorSpace colorSpace() const override;

private:
    RemoteNativeImageProxy(const WebCore::IntSize&, WebCore::PlatformColorSpace&&, bool hasAlpha, WeakRef<RemoteResourceCacheProxy>&&);

    WeakPtr<RemoteResourceCacheProxy> m_resourceCache;
    const WebCore::IntSize m_size;
    const WebCore::PlatformColorSpace m_colorSpace;
    const bool m_hasAlpha;
};

}

#endif
