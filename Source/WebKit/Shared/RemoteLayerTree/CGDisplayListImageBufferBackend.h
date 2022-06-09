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

#if ENABLE(CG_DISPLAY_LIST_BACKED_IMAGE_BUFFER)

#include "ImageBufferBackendHandle.h"
#include <WebCore/ImageBufferCGBackend.h>
#include <wtf/IsoMalloc.h>

namespace WebKit {

class CGDisplayListImageBufferBackend : public WebCore::ImageBufferCGBackend {
    WTF_MAKE_ISO_ALLOCATED(CGDisplayListImageBufferBackend);
    WTF_MAKE_NONCOPYABLE(CGDisplayListImageBufferBackend);
public:
    static size_t calculateMemoryCost(const Parameters&);

    static std::unique_ptr<CGDisplayListImageBufferBackend> create(const Parameters&);
    static std::unique_ptr<CGDisplayListImageBufferBackend> create(const Parameters&, const WebCore::HostWindow*);

    ImageBufferBackendHandle createImageBufferBackendHandle() const;

    WebCore::GraphicsContext& context() const override;
    WebCore::IntSize backendSize() const override;

    // NOTE: These all ASSERT_NOT_REACHED().
    RefPtr<WebCore::NativeImage> copyNativeImage(WebCore::BackingStoreCopy = WebCore::CopyBackingStore) const override;
    std::optional<WebCore::PixelBuffer> getPixelBuffer(const WebCore::PixelBufferFormat& outputFormat, const WebCore::IntRect&) const override;
    void putPixelBuffer(const WebCore::PixelBuffer&, const WebCore::IntRect& srcRect, const WebCore::IntPoint& destPoint, WebCore::AlphaPremultiplication destFormat) override;

protected:
    CGDisplayListImageBufferBackend(const Parameters&, std::unique_ptr<WebCore::GraphicsContext>&&);

    unsigned bytesPerRow() const override;

    std::unique_ptr<WebCore::GraphicsContext> m_context;
};

}

#endif // ENABLE(CG_DISPLAY_LIST_BACKED_IMAGE_BUFFER)
