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

#include "config.h"
#include "RemoteImageDecoderAVFProxy.h"

#if ENABLE(GPU_PROCESS) && HAVE(AVASSETREADER)

#include "GPUConnectionToWebProcess.h"
#include "GPUProcess.h"
#include "RemoteImageDecoderAVFManagerMessages.h"
#include "RemoteImageDecoderAVFProxyMessages.h"
#include "SharedBufferReference.h"
#include "WebCoreArgumentCoders.h"
#include <CoreGraphics/CGImage.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/ImageDecoderAVFObjC.h>
#include <wtf/Scope.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

using namespace WebCore;

RemoteImageDecoderAVFProxy::RemoteImageDecoderAVFProxy(GPUConnectionToWebProcess& connectionToWebProcess)
    : m_connectionToWebProcess(connectionToWebProcess)
    , m_resourceOwner(connectionToWebProcess.webProcessIdentity())
{
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteImageDecoderAVFProxy);

void RemoteImageDecoderAVFProxy::createDecoder(const IPC::SharedBufferReference& data, const String& mimeType, CompletionHandler<void(std::optional<ImageDecoderIdentifier>&&)>&& completionHandler)
{
    auto imageDecoder = ImageDecoderAVFObjC::create(data.isNull() ? SharedBuffer::create() : data.unsafeBuffer().releaseNonNull(), mimeType, AlphaOption::Premultiplied, GammaAndColorProfileOption::Ignored, m_resourceOwner);

    std::optional<ImageDecoderIdentifier> imageDecoderIdentifier;
    if (!imageDecoder)
        return completionHandler(WTFMove(imageDecoderIdentifier));

    auto identifier = ImageDecoderIdentifier::generate();
    m_imageDecoders.add(identifier, imageDecoder.copyRef());

    imageDecoder->setEncodedDataStatusChangeCallback([proxy = WeakPtr<MessageReceiver> { *this },  identifier](auto) mutable {
        if (proxy)
            static_cast<RemoteImageDecoderAVFProxy*>(proxy.get())->encodedDataStatusChanged(identifier);
    });

    imageDecoderIdentifier = identifier;
    completionHandler(WTFMove(imageDecoderIdentifier));
}

void RemoteImageDecoderAVFProxy::deleteDecoder(ImageDecoderIdentifier identifier)
{
    ASSERT(m_imageDecoders.contains(identifier));
    if (!m_imageDecoders.contains(identifier))
        return;

    m_imageDecoders.take(identifier);
    RefPtr connection = m_connectionToWebProcess.get();
    if (!connection)
        return;
    if (allowsExitUnderMemoryPressure())
        connection->gpuProcess().tryExitIfUnusedAndUnderMemoryPressure();
}

void RemoteImageDecoderAVFProxy::encodedDataStatusChanged(ImageDecoderIdentifier identifier)
{
    RefPtr connection = m_connectionToWebProcess.get();
    if (!connection)
        return;

    RefPtr imageDecoder = m_imageDecoders.get(identifier);
    if (!imageDecoder)
        return;

    connection->protectedConnection()->send(Messages::RemoteImageDecoderAVFManager::EncodedDataStatusChanged(identifier, imageDecoder->frameCount(), imageDecoder->size(), imageDecoder->hasTrack()), 0);
}

void RemoteImageDecoderAVFProxy::setExpectedContentSize(ImageDecoderIdentifier identifier, long long expectedContentSize)
{
    ASSERT(m_imageDecoders.contains(identifier));
    if (!m_imageDecoders.contains(identifier))
        return;

    RefPtr { m_imageDecoders.get(identifier) }->setExpectedContentSize(expectedContentSize);
}

void RemoteImageDecoderAVFProxy::setData(ImageDecoderIdentifier identifier, const IPC::SharedBufferReference& data, bool allDataReceived, CompletionHandler<void(size_t frameCount, const IntSize& size, bool hasTrack, std::optional<Vector<ImageDecoder::FrameInfo>>&&)>&& completionHandler)
{
    ASSERT(m_imageDecoders.contains(identifier));
    if (!m_imageDecoders.contains(identifier)) {
        completionHandler(0, { }, false, std::nullopt);
        return;
    }

    RefPtr imageDecoder = m_imageDecoders.get(identifier);
    imageDecoder->setData(data.isNull() ? SharedBuffer::create() : data.unsafeBuffer().releaseNonNull(), allDataReceived);

    auto frameCount = imageDecoder->frameCount();

    std::optional<Vector<ImageDecoder::FrameInfo>> frameInfos;
    if (frameCount)
        frameInfos = imageDecoder->frameInfos();

    completionHandler(frameCount, imageDecoder->size(), imageDecoder->hasTrack(), WTFMove(frameInfos));
}

void RemoteImageDecoderAVFProxy::createFrameImageAtIndex(ImageDecoderIdentifier identifier, size_t index, CompletionHandler<void(std::optional<WebCore::ShareableBitmap::Handle>&&)>&& completionHandler)
{
    ASSERT(m_imageDecoders.contains(identifier));

    std::optional<ShareableBitmap::Handle> imageHandle;

    auto invokeCallbackAtScopeExit = makeScopeExit([&] {
        completionHandler(WTFMove(imageHandle));
    });

    RefPtr imageDecoder = m_imageDecoders.get(identifier);
    if (!imageDecoder)
        return;

    auto nativeImage = NativeImage::createTransient(imageDecoder->createFrameImageAtIndex(index));
    if (!nativeImage)
        return;
    bool isOpaque = false;
    auto imageSize = nativeImage->size();
    auto bitmap = ShareableBitmap::create({ imageSize, nativeImage->colorSpace(), isOpaque });
    if (!bitmap)
        return;
    auto context = bitmap->createGraphicsContext();
    if (!context)
        return;

    FloatRect imageRect { { }, imageSize };
    context->drawNativeImage(*nativeImage, imageRect, imageRect, { CompositeOperator::Copy });
    imageHandle = bitmap->createHandle();
}

void RemoteImageDecoderAVFProxy::clearFrameBufferCache(ImageDecoderIdentifier identifier, size_t index)
{
    ASSERT(m_imageDecoders.contains(identifier));
    if (RefPtr imageDecoder = m_imageDecoders.get(identifier))
        imageDecoder->clearFrameBufferCache(std::min(index, imageDecoder->frameCount() - 1));
}

bool RemoteImageDecoderAVFProxy::allowsExitUnderMemoryPressure() const
{
    return m_imageDecoders.isEmpty();
}

}

#endif
