/*
 * Copyright (C) 2020-2022 Apple Inc.  All rights reserved.
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

#if ENABLE(GPU_PROCESS) && HAVE(IOSURFACE)

#include "ImageBufferBackendHandleSharing.h"
#include <WebCore/ImageBufferIOSurfaceBackend.h>
#include <wtf/IsoMalloc.h>

namespace WebCore {
class ProcessIdentity;
}

namespace WebKit {

class ImageBufferShareableMappedIOSurfaceBackend final : public WebCore::ImageBufferIOSurfaceBackend, public ImageBufferBackendHandleSharing {
    WTF_MAKE_ISO_ALLOCATED(ImageBufferShareableMappedIOSurfaceBackend);
    WTF_MAKE_NONCOPYABLE(ImageBufferShareableMappedIOSurfaceBackend);
public:
    static std::unique_ptr<ImageBufferShareableMappedIOSurfaceBackend> create(const Parameters&, const WebCore::ImageBufferCreationContext&);
    static std::unique_ptr<ImageBufferShareableMappedIOSurfaceBackend> create(const Parameters&, ImageBufferBackendHandle);

    using WebCore::ImageBufferIOSurfaceBackend::ImageBufferIOSurfaceBackend;

    void setOwnershipIdentity(const WebCore::ProcessIdentity&);

    ImageBufferBackendHandle createBackendHandle(SharedMemory::Protection = SharedMemory::Protection::ReadWrite) const final;

private:
    // ImageBufferBackendSharing
    ImageBufferBackendSharing* toBackendSharing() final { return this; }

    RefPtr<WebCore::NativeImage> copyNativeImage(WebCore::BackingStoreCopy = WebCore::CopyBackingStore) const final;

    mutable WebCore::IOSurfaceSeed m_lastSeedWhenCopyingImage { 0 };
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && HAVE(IOSURFACE)
