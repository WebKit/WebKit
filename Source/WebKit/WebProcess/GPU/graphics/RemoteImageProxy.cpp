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

#include "config.h"

#if ENABLE(GPU_PROCESS)
#include "RemoteImageProxy.h"

#include "RemoteSharedResourceCacheMessages.h"

namespace WebKit {
using namespace WebCore;
WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteImageProxy);

Ref<RemoteImageProxy> RemoteImageProxy::create(const WebCore::FloatSize& size, const WebCore::DestinationColorSpace& colorSpace, IPC::Connection& connection)
{
    return adoptRef(*new RemoteImageProxy(size, colorSpace, connection));
}

RemoteImageProxy::RemoteImageProxy(const WebCore::FloatSize& size, const WebCore::DestinationColorSpace& colorSpace, IPC::Connection& connection)
    : m_connection(connection)
    , m_size(size)
    , m_colorSpace(colorSpace)
{
}

RemoteImageProxy::~RemoteImageProxy()
{
    m_connection->send(Messages::RemoteSharedResourceCache::ReleaseImage(m_referenceTracker.write()), 0);
}

RefPtr<NativeImage> RemoteImageProxy::nativeImage(const DestinationColorSpace& colorSpace)
{
    waitForInitialized();
    return m_nativeImage;
}

FloatSize RemoteImageProxy::size(ImageOrientation) const
{
    return m_size;
}

void RemoteImageProxy::waitForInitialized() const
{
    if (m_isInitialized)
        return;
    m_isInitialized = true;
    auto result = m_connection->sendSync(Messages::RemoteSharedResourceCache::GetShareableBitmap(newReadReference()), 0);
    if (UNLIKELY(!result.succeeded()))
        return; // FIXME: Did become unresponsive.
    auto [handle] = result.takeReply();
    if (!handle)
        return;
    RefPtr bitmap = ShareableBitmap::create(WTFMove(*handle));
    if (!bitmap)
        return;
    m_nativeImage = NativeImage::create(bitmap->createPlatformImage(DontCopyBackingStore));
}


ImageDrawResult RemoteImageProxy::draw(GraphicsContext& context, const FloatRect& destRect, const FloatRect& sourceRect, ImagePaintingOptions options)
{
    if (RefPtr image = nativeImage()) {
        context.drawNativeImage(*image, destRect, sourceRect, options);
        return ImageDrawResult::DidDraw;
    }
    return ImageDrawResult::DidNothing;
}

RemoteImageIdentifier RemoteImageProxy::identifier() const
{
    return m_referenceTracker.identifier();
}

RemoteImageReadReference RemoteImageProxy::newReadReference() const
{
    return m_referenceTracker.read();
}

void RemoteImageProxy::didCreate()
{
}

}

#endif
