/*
 * Copyright (C) 2025 Apple Inc.  All rights reserved.
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

#include "RemoteImageIdentifier.h"
#include <WebCore/Image.h>
#include <WebCore/NativeImage.h>
#include <wtf/TZoneMalloc.h>

namespace IPC {
class Connection;
}

namespace WebKit {

class RemoteImageProxy final : public WebCore::Image {
    WTF_MAKE_TZONE_ALLOCATED(RemoteImageProxy);
public:
    static Ref<RemoteImageProxy> create(const WebCore::FloatSize&, const WebCore::DestinationColorSpace&, IPC::Connection&);
    ~RemoteImageProxy();

    bool isRemoteImageProxy() const override { return true; }
    RefPtr<WebCore::NativeImage> nativeImage(const WebCore::DestinationColorSpace& = WebCore::DestinationColorSpace::SRGB()) override;
    WebCore::ImageDrawResult draw(WebCore::GraphicsContext&, const WebCore::FloatRect& destRect, const WebCore::FloatRect& sourceRect, WebCore::ImagePaintingOptions = { }) override;
    bool currentFrameKnownToBeOpaque() const override { return false; }
    WebCore::FloatSize size(WebCore::ImageOrientation = WebCore::ImageOrientation::Orientation::FromImage) const override;
    RemoteImageIdentifier identifier() const;
    RemoteImageReadReference newReadReference() const;
private:
    RemoteImageProxy(const WebCore::FloatSize&, const WebCore::DestinationColorSpace&, IPC::Connection&);
    void waitForInitialized() const;

    // Messages
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&);
    void didCreate();

    const Ref<IPC::Connection> m_connection;
    const WebCore::FloatSize m_size;
    mutable RefPtr<WebCore::NativeImage> m_nativeImage;
    RemoteImageReferenceTracker m_referenceTracker { RemoteImageIdentifier::generate() };
    const WebCore::DestinationColorSpace m_colorSpace;
    mutable bool m_isInitialized { false };
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::RemoteImageProxy)
    static bool isType(const WebCore::Image& image) { return image.isRemoteImageProxy(); }
SPECIALIZE_TYPE_TRAITS_END()


#endif // ENABLE(GPU_PROCESS)
