/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include <WebCore/NativeImage.h>
#include <WebCore/ShareableBitmap.h>

namespace WebKit {
class RemoteResourceCacheProxy;

class RemoteNativeImageBackendProxy final : public WebCore::NativeImageBackend {
public:
    static std::unique_ptr<RemoteNativeImageBackendProxy> create(WebCore::NativeImage&);
    ~RemoteNativeImageBackendProxy() final;

    const WebCore::PlatformImagePtr& platformImage() const final;
    WebCore::IntSize size() const final;
    bool hasAlpha() const final;
    WebCore::DestinationColorSpace colorSpace() const final;
    bool isRemoteNativeImageBackendProxy() const final;

    std::optional<WebCore::ShareableBitmap::Handle> createHandle();
private:
    RemoteNativeImageBackendProxy(Ref<WebCore::ShareableBitmap>, WebCore::PlatformImagePtr);

    Ref<WebCore::ShareableBitmap> m_bitmap;
    WebCore::PlatformImageNativeImageBackend m_platformBackend;
};

}
SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::RemoteNativeImageBackendProxy)
    static bool isType(const WebCore::NativeImageBackend& backend) { return backend.isRemoteNativeImageBackendProxy(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif
