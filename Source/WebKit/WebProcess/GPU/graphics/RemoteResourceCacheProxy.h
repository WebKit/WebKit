/*
 * Copyright (C) 2020-2023 Apple Inc.  All rights reserved.
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
#include <WebCore/DecomposedGlyphs.h>
#include <WebCore/FilterFunction.h>
#include <WebCore/Gradient.h>
#include <WebCore/NativeImage.h>
#include <wtf/CheckedRef.h>
#include <wtf/HashMap.h>

namespace WebCore {
class Filter;
class Font;
class ImageBuffer;
struct FontCustomPlatformData;
}

namespace WebKit {

class RemoteImageBufferProxy;
class RemoteRenderingBackendProxy;

class RemoteResourceCacheProxy final : public WebCore::RenderingResourceObserver {
public:
    using WeakValueType = WebCore::RenderingResourceObserver;
    RemoteResourceCacheProxy(RemoteRenderingBackendProxy&);
    ~RemoteResourceCacheProxy();

    void recordNativeImageUse(WebCore::NativeImage&, const WebCore::DestinationColorSpace&);
    void recordFontUse(WebCore::Font&);
    void recordDecomposedGlyphsUse(WebCore::DecomposedGlyphs&);
    void recordGradientUse(WebCore::Gradient&);
    void recordFilterUse(WebCore::Filter&);
    void recordFontCustomPlatformDataUse(const WebCore::FontCustomPlatformData&);

    void didPaintLayers();

    void releaseMemory();
    void releaseNativeImages();
    
    unsigned nativeImageCountForTesting() const { return m_nativeImages.size(); }

private:
    // WebCore::RenderingResourceObserver.
    void willDestroyNativeImage(WebCore::RenderingResourceIdentifier) override;
    void willDestroyGradient(WebCore::RenderingResourceIdentifier) override;
    void willDestroyDecomposedGlyphs(WebCore::RenderingResourceIdentifier) override;
    void willDestroyFilter(WebCore::RenderingResourceIdentifier) override;

    void finalizeRenderingUpdateForFonts();
    void prepareForNextRenderingUpdate();
    void releaseFonts();
    void releaseFontCustomPlatformDatas();

    HashSet<WebCore::RenderingResourceIdentifier> m_nativeImages;
    HashSet<WebCore::RenderingResourceIdentifier> m_gradients;
    HashSet<WebCore::RenderingResourceIdentifier> m_decomposedGlyphs;
    HashSet<WebCore::RenderingResourceIdentifier> m_filters;

    WeakPtrFactory<WebCore::RenderingResourceObserver> m_resourceObserverWeakFactory;
    WeakPtrFactory<WebCore::RenderingResourceObserver> m_nativeImageResourceObserverWeakFactory;

    using FontHashMap = HashMap<WebCore::RenderingResourceIdentifier, uint64_t>;
    FontHashMap m_fonts;
    FontHashMap m_fontCustomPlatformDatas;

    unsigned m_numberOfFontsUsedInCurrentRenderingUpdate { 0 };
    unsigned m_numberOfFontCustomPlatformDatasUsedInCurrentRenderingUpdate { 0 };

    const CheckedRef<RemoteRenderingBackendProxy> m_remoteRenderingBackendProxy;
    uint64_t m_renderingUpdateID;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
