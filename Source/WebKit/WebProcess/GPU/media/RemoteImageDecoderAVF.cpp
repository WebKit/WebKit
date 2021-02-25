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
#include "RemoteImageDecoderAVF.h"

#if ENABLE(GPU_PROCESS) && HAVE(AVASSETREADER)

#include "GPUProcessConnection.h"
#include "RemoteImageDecoderAVFProxyMessages.h"
#include "SharedBufferDataReference.h"
#include "WebProcess.h"
#include <WebCore/AVAssetMIMETypeCache.h>
#include <WebCore/ColorSpaceCG.h>
#include <WebCore/IOSurface.h>
#include <WebCore/ImageTypes.h>
#include <WebCore/MIMETypeRegistry.h>
#include <WebCore/SharedBuffer.h>
#include <WebCore/UTIUtilities.h>
#include <wtf/MachSendRight.h>

namespace WebKit {

using namespace WebCore;

RemoteImageDecoderAVF::RemoteImageDecoderAVF(RemoteImageDecoderAVFManager& manager, const WebCore::ImageDecoderIdentifier& identifier, const String& mimeType)
    : ImageDecoder()
    , m_manager(manager)
    , m_identifier(identifier)
    , m_mimeType(mimeType)
    , m_uti(WebCore::UTIFromMIMEType(mimeType))
{
}

RemoteImageDecoderAVF::~RemoteImageDecoderAVF()
{
    m_manager.deleteRemoteImageDecoder(m_identifier);
}

GPUProcessConnection& RemoteImageDecoderAVF::gpuProcessConnection() const
{
    return m_manager.gpuProcessConnection();
}

bool RemoteImageDecoderAVF::canDecodeType(const String& mimeType)
{
    // TODO: We may need to remove AVAssetMIMETypeCache in the Web process
    return AVAssetMIMETypeCache::singleton().canDecodeType(mimeType) != MediaPlayerEnums::SupportsType::IsNotSupported;
}

bool RemoteImageDecoderAVF::supportsMediaType(MediaType type)
{
    // TODO: We may need to remove AVAssetMIMETypeCache in the Web process
    return type == MediaType::Video && AVAssetMIMETypeCache::singleton().isAvailable();
}

EncodedDataStatus RemoteImageDecoderAVF::encodedDataStatus() const
{
    if (m_frameCount)
        return EncodedDataStatus::Complete;
    if (m_size)
        return EncodedDataStatus::SizeAvailable;
    if (m_hasTrack)
        return EncodedDataStatus::TypeAvailable;

    return EncodedDataStatus::Unknown;
}

void RemoteImageDecoderAVF::setEncodedDataStatusChangeCallback(WTF::Function<void(EncodedDataStatus)>&& callback)
{
    m_encodedDataStatusChangedCallback = WTFMove(callback);
}

IntSize RemoteImageDecoderAVF::size() const
{
    if (m_size)
        return m_size.value();
    return IntSize();
}

size_t RemoteImageDecoderAVF::frameCount() const
{
    return m_frameCount;
}

RepetitionCount RemoteImageDecoderAVF::repetitionCount() const
{
    return m_frameCount > 1 ? RepetitionCountInfinite : RepetitionCountNone;
}

String RemoteImageDecoderAVF::uti() const
{
    return m_uti;
}

String RemoteImageDecoderAVF::filenameExtension() const
{
    return MIMETypeRegistry::preferredExtensionForMIMEType(m_mimeType);
}

IntSize RemoteImageDecoderAVF::frameSizeAtIndex(size_t, SubsamplingLevel) const
{
    return size();
}

bool RemoteImageDecoderAVF::frameIsCompleteAtIndex(size_t index) const
{
    return index < m_frameCount;
}

ImageDecoder::FrameMetadata RemoteImageDecoderAVF::frameMetadataAtIndex(size_t) const
{
    return { };
}

Seconds RemoteImageDecoderAVF::frameDurationAtIndex(size_t index) const
{
    if (m_frameInfos.isEmpty() || index >= m_frameInfos.size())
        return { };

    return m_frameInfos[index].duration;
}

bool RemoteImageDecoderAVF::frameHasAlphaAtIndex(size_t index) const
{
    if (m_frameInfos.isEmpty() || index >= m_frameInfos.size())
        return false;

    return m_frameInfos[index].hasAlpha;
}

bool RemoteImageDecoderAVF::frameAllowSubsamplingAtIndex(size_t index) const
{
    return index < m_frameCount;
}

unsigned RemoteImageDecoderAVF::frameBytesAtIndex(size_t, SubsamplingLevel) const
{
    return (size().area() * 4).unsafeGet();
}

PlatformImagePtr RemoteImageDecoderAVF::createFrameImageAtIndex(size_t index, SubsamplingLevel, const DecodingOptions&)
{
    if (m_frameImages.contains(index))
        return m_frameImages.get(index);

    // This function might be called in the main thread to run the decoder synchronously
    if (isMainThread())
        return nullptr;

    callOnMainThreadAndWait([this, protectedThis = makeRef(*this), index] {
        Optional<MachSendRight> sendRight;
        if (!gpuProcessConnection().connection().sendSync(Messages::RemoteImageDecoderAVFProxy::CreateFrameImageAtIndex(m_identifier, index), Messages::RemoteImageDecoderAVFProxy::CreateFrameImageAtIndex::Reply(sendRight), 0))
            return;

        if (!sendRight)
            return;

        auto surface = WebCore::IOSurface::createFromSendRight(WTFMove(*sendRight), sRGBColorSpaceRef());
        if (!surface)
            return;

        if (auto image = IOSurface::sinkIntoImage(WTFMove(surface)))
            m_frameImages.add(index, image);
    });

    if (m_frameImages.contains(index))
        return m_frameImages.get(index);

    return nullptr;
}

void RemoteImageDecoderAVF::setExpectedContentSize(long long expectedContentSize)
{
    gpuProcessConnection().connection().send(Messages::RemoteImageDecoderAVFProxy::SetExpectedContentSize(m_identifier, expectedContentSize), 0);
}

// If allDataReceived is true, the caller expects encodedDataStatus() to be >= EncodedDataStatus::SizeAvailable
// after this function returns (in the same run loop).
void RemoteImageDecoderAVF::setData(SharedBuffer& data, bool allDataReceived)
{
    IPC::SharedBufferDataReference dataReference { data };

    uint32_t frameCount;
    IntSize size;
    bool hasTrack;
    Optional<Vector<ImageDecoder::FrameInfo>> frameInfos;
    if (!gpuProcessConnection().connection().sendSync(Messages::RemoteImageDecoderAVFProxy::SetData(m_identifier, dataReference, allDataReceived), Messages::RemoteImageDecoderAVFProxy::SetData::Reply(frameCount, size, hasTrack, frameInfos), 0))
        return;

    m_isAllDataReceived = allDataReceived;
    m_frameCount = frameCount;
    if (!size.isEmpty())
        m_size = size;
    m_hasTrack = hasTrack;

    if (frameInfos)
        m_frameInfos.swap(*frameInfos);
}

void RemoteImageDecoderAVF::clearFrameBufferCache(size_t)
{
    m_frameImages.clear();
}

void RemoteImageDecoderAVF::encodedDataStatusChanged(size_t frameCount, const WebCore::IntSize& size, bool hasTrack)
{
    m_frameCount = frameCount;
    if (!size.isEmpty())
        m_size = size;
    m_hasTrack = hasTrack;

    if (m_encodedDataStatusChangedCallback)
        m_encodedDataStatusChangedCallback(encodedDataStatus());
}

}

#endif
