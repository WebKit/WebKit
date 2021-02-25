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
#include "RemoteImageDecoderAVFManagerMessages.h"
#include "RemoteImageDecoderAVFProxyMessages.h"
#include "WebCoreArgumentCoders.h"
#include <WebCore/IOSurface.h>
#include <WebCore/ImageDecoderAVFObjC.h>

namespace WebKit {

using namespace WebCore;

RemoteImageDecoderAVFProxy::RemoteImageDecoderAVFProxy(GPUConnectionToWebProcess& connectionToWebProcess)
    : m_connectionToWebProcess(makeWeakPtr(connectionToWebProcess))
{
}

void RemoteImageDecoderAVFProxy::createDecoder(const IPC::DataReference& data, const String& mimeType, CompletionHandler<void(Optional<ImageDecoderIdentifier>&&)>&& completionHandler)
{
    auto imageDecoder = ImageDecoderAVFObjC::create(SharedBuffer::create(data.data(), data.size()), mimeType, AlphaOption::Premultiplied, GammaAndColorProfileOption::Ignored);

    Optional<ImageDecoderIdentifier> imageDecoderIdentifier;
    if (!imageDecoder)
        return completionHandler(WTFMove(imageDecoderIdentifier));

    auto identifier = ImageDecoderIdentifier::generate();
    m_imageDecoders.add(identifier, imageDecoder.copyRef());

    imageDecoder->setEncodedDataStatusChangeCallback([this, identifier](auto) mutable {
        encodedDataStatusChanged(identifier);
    });

    imageDecoderIdentifier = identifier;
    completionHandler(WTFMove(imageDecoderIdentifier));
}

void RemoteImageDecoderAVFProxy::deleteDecoder(const ImageDecoderIdentifier& identifier)
{
    ASSERT(m_imageDecoders.contains(identifier));
    if (!m_imageDecoders.contains(identifier))
        return;

    m_imageDecoders.take(identifier);
}

void RemoteImageDecoderAVFProxy::encodedDataStatusChanged(const ImageDecoderIdentifier& identifier)
{
    if (!m_connectionToWebProcess || !m_imageDecoders.contains(identifier))
        return;

    auto imageDecoder = m_imageDecoders.get(identifier);
    m_connectionToWebProcess->connection().send(Messages::RemoteImageDecoderAVFManager::EncodedDataStatusChanged(identifier, imageDecoder->frameCount(), imageDecoder->size(), imageDecoder->hasTrack()), 0);
}

void RemoteImageDecoderAVFProxy::setExpectedContentSize(const ImageDecoderIdentifier& identifier, long long expectedContentSize)
{
    ASSERT(m_imageDecoders.contains(identifier));
    if (!m_imageDecoders.contains(identifier))
        return;

    m_imageDecoders.get(identifier)->setExpectedContentSize(expectedContentSize);
}

void RemoteImageDecoderAVFProxy::setData(const ImageDecoderIdentifier& identifier, const IPC::DataReference& data, bool allDataReceived, CompletionHandler<void(size_t frameCount, const IntSize& size, bool hasTrack, Optional<Vector<ImageDecoder::FrameInfo>>&&)>&& completionHandler)
{
    ASSERT(m_imageDecoders.contains(identifier));
    if (!m_imageDecoders.contains(identifier)) {
        completionHandler(0, { }, false, WTF::nullopt);
        return;
    }

    auto imageDecoder = m_imageDecoders.get(identifier);
    imageDecoder->setData(SharedBuffer::create(data.data(), data.size()), allDataReceived);

    auto frameCount = imageDecoder->frameCount();

    Optional<Vector<ImageDecoder::FrameInfo>> frameInfos;
    if (frameCount)
        frameInfos = imageDecoder->frameInfos();

    completionHandler(frameCount, imageDecoder->size(), imageDecoder->hasTrack(), WTFMove(frameInfos));
}

void RemoteImageDecoderAVFProxy::createFrameImageAtIndex(const ImageDecoderIdentifier& identifier, size_t index, CompletionHandler<void(Optional<WTF::MachSendRight>&&)>&& completionHandler)
{
    ASSERT(m_imageDecoders.contains(identifier));
    Optional<WTF::MachSendRight> sendRight;
    if (!m_imageDecoders.contains(identifier)) {
        completionHandler(WTFMove(sendRight));
        return;
    }

    auto frameImage = m_imageDecoders.get(identifier)->createFrameImageAtIndex(index);
    if (!frameImage) {
        completionHandler(WTFMove(sendRight));
        return;
    }

    if (auto surface = IOSurface::createFromImage(frameImage.get()))
        sendRight = surface->createSendRight();

    completionHandler(WTFMove(sendRight));
}

}

#endif
