/*
 * Copyright (C) 2011-2019 Apple Inc. All rights reserved.
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

#include <WebCore/IntRect.h>
#include <wtf/Noncopyable.h>

#if USE(CAIRO)
#include <WebCore/BackingStoreBackendCairo.h>
#elif USE(DIRECT2D)
#include <WebCore/BackingStoreBackendDirect2D.h>
#endif

#if USE(DIRECT2D)
interface ID2D1RenderTarget;
interface ID3D11Device1;
interface ID3D11DeviceContext1;
interface ID3D11Texture2D;
#endif

namespace WebKit {

class ShareableBitmap;
class UpdateInfo;
class WebPageProxy;

class BackingStore {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(BackingStore);

public:
    BackingStore(const WebCore::IntSize&, float deviceScaleFactor, WebPageProxy&);
    ~BackingStore();

    const WebCore::IntSize& size() const { return m_size; }
    float deviceScaleFactor() const { return m_deviceScaleFactor; }

#if USE(CAIRO)
    typedef cairo_t* PlatformGraphicsContext;
#elif USE(DIRECT2D)
    struct DXConnections {
        ID3D11DeviceContext1* immediateContext { nullptr };
        ID3D11Texture2D* backBuffer { nullptr };
    };
    typedef DXConnections PlatformGraphicsContext;
#endif

    void paint(PlatformGraphicsContext, const WebCore::IntRect&);
    void incorporateUpdate(const UpdateInfo&);

private:
    void incorporateUpdate(ShareableBitmap*, const UpdateInfo&);
    void scroll(const WebCore::IntRect& scrollRect, const WebCore::IntSize& scrollOffset);

#if USE(CAIRO)
    std::unique_ptr<WebCore::BackingStoreBackendCairo> createBackend();
#elif USE(DIRECT2D)
    std::unique_ptr<WebCore::BackingStoreBackendDirect2D> createBackend();
#endif

    WebCore::IntSize m_size;
    float m_deviceScaleFactor;
    WebPageProxy& m_webPageProxy;
#if USE(CAIRO)
    std::unique_ptr<WebCore::BackingStoreBackendCairo> m_backend;
#elif USE(DIRECT2D)
    std::unique_ptr<WebCore::BackingStoreBackendDirect2D> m_backend;
#endif
};

} // namespace WebKit
