/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)

#include "ImageBufferBackendHandleSharing.h"
#include <WebCore/ImageBuffer.h>
#include <WebCore/ImageBufferCGBackend.h>
#include <wtf/IsoMalloc.h>

namespace WebKit {

class DynamicContentScalingImageBufferBackend : public WebCore::ImageBufferCGBackend, public ImageBufferBackendHandleSharing {
    WTF_MAKE_ISO_ALLOCATED(DynamicContentScalingImageBufferBackend);
    WTF_MAKE_NONCOPYABLE(DynamicContentScalingImageBufferBackend);
public:
    static size_t calculateMemoryCost(const Parameters&);

    static std::unique_ptr<DynamicContentScalingImageBufferBackend> create(const Parameters&, const WebCore::ImageBufferCreationContext&);

    DynamicContentScalingImageBufferBackend(const Parameters&, const WebCore::ImageBufferCreationContext&, WebCore::RenderingMode);

    WebCore::GraphicsContext& context() final;
    std::optional<ImageBufferBackendHandle> createBackendHandle(WebCore::SharedMemory::Protection = WebCore::SharedMemory::Protection::ReadWrite) const final;

    void releaseGraphicsContext() final;

    // NOTE: These all ASSERT_NOT_REACHED().
    RefPtr<WebCore::NativeImage> copyNativeImage() final;
    RefPtr<WebCore::NativeImage> createNativeImageReference() final;
    void getPixelBuffer(const WebCore::IntRect&, WebCore::PixelBuffer&) final;
    void putPixelBuffer(const WebCore::PixelBuffer&, const WebCore::IntRect& srcRect, const WebCore::IntPoint& destPoint, WebCore::AlphaPremultiplication destFormat) final;

protected:
    unsigned bytesPerRow() const final;
    String debugDescription() const final;

    // ImageBufferBackendSharing
    ImageBufferBackendSharing* toBackendSharing() final { return this; }

    mutable std::unique_ptr<WebCore::GraphicsContextCG> m_context;
    RetainPtr<id> m_resourceCache;
    WebCore::RenderingMode m_renderingMode;
};

}

#endif // ENABLE(RE_DYNAMIC_CONTENT_SCALING)
