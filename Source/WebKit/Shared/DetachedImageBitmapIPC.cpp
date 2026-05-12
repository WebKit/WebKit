/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "DetachedImageBitmapIPC.h"

#include "Decoder.h"
#include "Encoder.h"
#include "ImageBufferBackendHandleSharing.h"
#include "RemoteImageBufferProxy.h"
#include "RemoteRenderingBackendProxy.h"
#include "SendableSerializedImageBuffer.h"
#include <WebCore/ImageBitmap.h>
#include <WebCore/NativeImage.h>
#include <WebCore/ShareableBitmap.h>
#include <wtf/NeverDestroyed.h>

namespace WebKit {

static WeakPtr<RemoteRenderingBackendProxy>& currentRenderingBackendForEncodingSlot()
{
    static thread_local NeverDestroyed<WeakPtr<RemoteRenderingBackendProxy>> slot;
    return slot.get();
}

ScopedCrossProcessImageBitmapEncoding::ScopedCrossProcessImageBitmapEncoding(RemoteRenderingBackendProxy* renderingBackend)
    : m_previous(WTF::move(currentRenderingBackendForEncodingSlot()))
{
    currentRenderingBackendForEncodingSlot() = renderingBackend;
}

ScopedCrossProcessImageBitmapEncoding::~ScopedCrossProcessImageBitmapEncoding()
{
    currentRenderingBackendForEncodingSlot() = WTF::move(m_previous);
}

RefPtr<RemoteRenderingBackendProxy> ScopedCrossProcessImageBitmapEncoding::currentForThread()
{
    return currentRenderingBackendForEncodingSlot().get();
}

static std::optional<ImageBufferBackendHandle> handleForCrossProcessTransfer(const WebCore::SerializedImageBuffer& buffer)
{
    if (auto* sendable = dynamicDowncast<SendableSerializedImageBuffer>(&buffer)) {
        // The same DetachedImageBitmap is being re-encoded (e.g. broadcast). Duplicate
        // the existing handle so the source remains usable for a subsequent send.
        return WTF::switchOn(sendable->handle(),
            [](const WebCore::ShareableBitmap::Handle& shareableHandle) -> std::optional<ImageBufferBackendHandle> {
                return ImageBufferBackendHandle { WebCore::ShareableBitmap::Handle { shareableHandle } };
            }
#if PLATFORM(COCOA)
            , [](const MachSendRight& sendRight) -> std::optional<ImageBufferBackendHandle> {
                return ImageBufferBackendHandle { MachSendRight { sendRight } };
            }
#endif
#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
            , [](const WebCore::DynamicContentScalingDisplayList&) -> std::optional<ImageBufferBackendHandle> {
                return std::nullopt;
            }
#endif
        );
    }

    if (auto* remoteBuffer = dynamicDowncast<RemoteSerializedImageBufferProxy>(&buffer)) {
        if (RefPtr renderingBackend = ScopedCrossProcessImageBitmapEncoding::currentForThread())
            return renderingBackend->createSerializedImageBufferHandle(remoteBuffer->identifier());
        return std::nullopt;
    }

    RefPtr localBuffer = buffer.imageBufferForCrossProcessSerialization();
    if (!localBuffer)
        return std::nullopt;
    if (auto* sharing = dynamicDowncast<ImageBufferBackendHandleSharing>(localBuffer->toBackendSharing()))
        return sharing->createBackendHandle();

    // Local backend doesn't expose a sharable handle (e.g. plain CGBitmap or local IOSurface
    // backend that doesn't implement the WebKit-side sharing interface). Copy the pixels into
    // a SharedMemory-backed ShareableBitmap so the receiver can materialize it without IOKit.
    RefPtr nativeImage = localBuffer->copyNativeImage();
    if (!nativeImage)
        return std::nullopt;
    RefPtr<WebCore::ShareableBitmap> bitmap;
#if USE(CG)
    bitmap = WebCore::ShareableBitmap::createFromImagePixels(*nativeImage);
#endif
    if (!bitmap)
        bitmap = WebCore::ShareableBitmap::createFromImageDraw(*nativeImage, WebCore::DestinationColorSpace { nativeImage->colorSpace() });
    if (!bitmap)
        return std::nullopt;
    auto bitmapHandle = bitmap->createHandle();
    if (!bitmapHandle)
        return std::nullopt;
    return ImageBufferBackendHandle { WTF::move(*bitmapHandle) };
}

static WebCore::ImageBuffer::Parameters parametersForBuffer(const WebCore::SerializedImageBuffer& buffer)
{
    if (auto* sendable = dynamicDowncast<SendableSerializedImageBuffer>(&buffer))
        return sendable->parameters();
    if (auto* remoteBuffer = dynamicDowncast<RemoteSerializedImageBufferProxy>(&buffer))
        return remoteBuffer->parameters();
    if (RefPtr localBuffer = buffer.imageBufferForCrossProcessSerialization())
        return localBuffer->parameters();
    return { { }, 1, WebCore::DestinationColorSpace::SRGB(), { WebCore::PixelFormat::BGRA8 }, WebCore::RenderingPurpose::Unspecified };
}

static WebCore::ImageBufferBackend::Info infoForBuffer(const WebCore::SerializedImageBuffer& buffer)
{
    if (auto* sendable = dynamicDowncast<SendableSerializedImageBuffer>(&buffer))
        return sendable->backendInfo();
    if (auto* remoteBuffer = dynamicDowncast<RemoteSerializedImageBufferProxy>(&buffer))
        return remoteBuffer->info();
    if (RefPtr localBuffer = buffer.imageBufferForCrossProcessSerialization())
        return localBuffer->backendInfo();
    return { WebCore::RenderingMode::Unaccelerated, { }, 0 };
}

} // namespace WebKit

namespace IPC {

void ArgumentCoder<std::optional<WebCore::DetachedImageBitmap>>::encode(Encoder& encoder, const std::optional<WebCore::DetachedImageBitmap>& opt)
{
    if (!opt) {
        encoder << false;
        return;
    }
    auto& detached = *opt;
    auto handle = WebKit::handleForCrossProcessTransfer(detached.serializedImageBuffer());
    if (!handle) {
        // Extraction failed (e.g. send site missing ScopedCrossProcessImageBitmapEncoding).
        // Drop the slot so the receiver matches the original [NotSerialized] semantic:
        // CloneDeserializer::readTransferredImageBitmap fails for this index, and the
        // recipient sees a messageerror event rather than an invalid ImageBitmap.
        encoder << false;
        return;
    }
    auto info = WebKit::infoForBuffer(detached.serializedImageBuffer());
    encoder << true;
    encoder << WebKit::DetachedImageBitmapIPCData {
        WebKit::parametersForBuffer(detached.serializedImageBuffer()),
        info.renderingMode,
        info.baseTransform,
        static_cast<uint64_t>(info.memoryCost),
        WTF::move(*handle),
        detached.originClean(),
        detached.premultiplyAlpha(),
        detached.forciblyPremultiplyAlpha(),
    };
}

std::optional<std::optional<WebCore::DetachedImageBitmap>> ArgumentCoder<std::optional<WebCore::DetachedImageBitmap>>::decode(Decoder& decoder)
{
    auto isEngaged = decoder.decode<bool>();
    if (!isEngaged)
        return std::nullopt;
    if (!*isEngaged)
        return std::optional<std::optional<WebCore::DetachedImageBitmap>>(std::optional<WebCore::DetachedImageBitmap>(std::nullopt));
    auto data = decoder.decode<WebKit::DetachedImageBitmapIPCData>();
    if (!data)
        return std::nullopt;
    WebCore::ImageBufferBackend::Info info { data->renderingMode, data->baseTransform, static_cast<size_t>(data->memoryCost) };
    std::unique_ptr<WebCore::SerializedImageBuffer> sendable = makeUnique<WebKit::SendableSerializedImageBuffer>(data->parameters, info, WTF::move(data->handle));
    WebCore::DetachedImageBitmap detached { makeUniqueRefFromNonNullUniquePtr(WTF::move(sendable)), data->originClean, data->premultiplyAlpha, data->forciblyPremultiplyAlpha };
    return std::optional<std::optional<WebCore::DetachedImageBitmap>>(std::optional<WebCore::DetachedImageBitmap>(WTF::move(detached)));
}

} // namespace IPC
