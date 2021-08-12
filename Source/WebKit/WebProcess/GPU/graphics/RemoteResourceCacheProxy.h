/*
 * Copyright (C) 2020 Apple Inc.  All rights reserved.
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

#include "RenderingUpdateID.h"
#include <WebCore/NativeImage.h>
#include <WebCore/RenderingResourceIdentifier.h>
#include <wtf/HashMap.h>

namespace WebCore {
class Font;
class ImageBuffer;
}

namespace WebKit {

class RemoteRenderingBackendProxy;

class RemoteResourceCacheProxy : public WebCore::NativeImage::Observer {
public:
    RemoteResourceCacheProxy(RemoteRenderingBackendProxy&);
    ~RemoteResourceCacheProxy();

    void cacheImageBuffer(WebCore::ImageBuffer&);
    WebCore::ImageBuffer* cachedImageBuffer(WebCore::RenderingResourceIdentifier);
    void releaseImageBuffer(WebCore::RenderingResourceIdentifier);

    void recordNativeImageUse(WebCore::NativeImage&);
    void recordFontUse(WebCore::Font&);
    void recordImageBufferUse(WebCore::ImageBuffer&);

    void finalizeRenderingUpdate();

    void remoteResourceCacheWasDestroyed();
    void releaseAllRemoteFonts();
    void releaseMemory();

private:
    struct ImageBufferState {
        WeakPtr<WebCore::ImageBuffer> imageBuffer;
        uint64_t useCount;
    };
    using ImageBufferHashMap = HashMap<WebCore::RenderingResourceIdentifier, ImageBufferState>;

    struct NativeImageState {
        WeakPtr<WebCore::NativeImage> image;
        uint64_t useCount;
    };
    using NativeImageHashMap = HashMap<WebCore::RenderingResourceIdentifier, NativeImageState>;

    struct FontState {
        RenderingUpdateID lastRenderingUpdateVersionUsedWithin;
        uint64_t useCount;
    };
    using FontHashMap = HashMap<WebCore::RenderingResourceIdentifier, FontState>;
    
    void releaseNativeImage(WebCore::RenderingResourceIdentifier) override;
    void clearNativeImageMap();

    void finalizeRenderingUpdateForFonts();
    void prepareForNextRenderingUpdate();
    void clearFontMap();

    ImageBufferHashMap m_imageBuffers;
    NativeImageHashMap m_nativeImages;
    FontHashMap m_fonts;

    unsigned m_numberOfFontsUsedInCurrentRenderingUpdate { 0 };

    RemoteRenderingBackendProxy& m_remoteRenderingBackendProxy;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
