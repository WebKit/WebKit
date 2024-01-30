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

#include "ShareableBitmap.h"
#include <WebCore/NativeImage.h>

namespace WebKit {
class RemoteResourceCacheProxy;

class RemoteNativeImageBackendProxy final : public NativeImageBackend {
public:
    static std::unique_ptr<RemoteNativeImageBackendProxy> create(NativeImage&);
    ~RemoteNativeImageBackendProxy() final;

    const PlatformImagePtr& platformImage() const final;
    IntSize size() const final;
    bool hasAlpha() const final;
    DestinationColorSpace colorSpace() const final;
    bool isRemoteNativeImageBackendProxy() const final;

    std::optional<ShareableBitmap::Handle> createHandle();
private:
    RemoteNativeImageBackendProxy(Ref<ShareableBitmap>, PlatformImagePtr);

    Ref<ShareableBitmap> m_bitmap;
    PlatformImageNativeImageBackend m_platformBackend;
};

}
SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::RemoteNativeImageBackendProxy)
    static bool isType(const WebCore::NativeImageBackend& backend) { return backend.isRemoteNativeImageBackendProxy(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif
