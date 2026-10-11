/*
 * Copyright (C) 2021-2025 Apple Inc. All rights reserved.
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

#if ENABLE(GPU_PROCESS)

#include "RemoteAdapterProxy.h"
#include "RemoteVideoFrameObjectHeapProxy.h"
#include "SharedVideoFrame.h"
#include "WebGPUIdentifier.h"
#include <WebCore/WebGPUCppAPI.h>
#include <WebCore/WebGPUImageCopyExternalImage.h>
#include <WebCore/WebGPUImageCopyTextureTagged.h>
#include <wtf/TZoneMalloc.h>

namespace WebKit::WebGPU {

class ConvertToBackingContext;

class RemoteQueueProxy final : public ::WebGPU::Queue {
    WTF_MAKE_TZONE_ALLOCATED(RemoteQueueProxy);
public:
    static Ref<RemoteQueueProxy> create(RemoteAdapterProxy& parent, ConvertToBackingContext& convertToBackingContext, WebGPUIdentifier identifier)
    {
        return adoptRef(*new RemoteQueueProxy(parent, convertToBackingContext, identifier));
    }

    virtual ~RemoteQueueProxy();

    // Called by RemoteGPUProxy, which implements the queue commands that take WebCore sources.
    void copyExternalImageToTexture(const WebCore::WebGPUExternalImageSource&, const WebCore::WebGPUImageCopyTextureTagged& destination, const ::WebGPU::Extent3D& copySize);
    RefPtr<WebCore::NativeImage> getNativeImage(WebCore::VideoFrame&);

    RemoteAdapterProxy& parent() const { return m_parent; }
    RemoteGPUProxy& root() { return m_parent->root(); }

    void submit(Vector<Ref<::WebGPU::CommandBuffer>>&&) final;
    void onSubmittedWorkDone(CompletionHandler<void()>&&) final;
    void writeBuffer(const ::WebGPU::Buffer&, uint64_t bufferOffset, std::span<const uint8_t> data) final;
    void writeTexture(const ::WebGPU::TexelCopyTextureInfo& destination, std::span<const uint8_t> data, const ::WebGPU::TexelCopyBufferLayout&, const ::WebGPU::Extent3D& writeSize) final;
#if PLATFORM(COCOA)
    void copyExternalImageToTexture(const ::WebGPU::ImageCopyExternalImage& source, const ::WebGPU::ImageCopyTextureTagged& destination, const ::WebGPU::Extent3D& copySize) final;
#endif
    void setLabel(String&&) final;
    bool isValid() const final;

private:
    friend class DowncastConvertToBackingContext;

    RemoteQueueProxy(RemoteAdapterProxy&, ConvertToBackingContext&, WebGPUIdentifier);

    RemoteQueueProxy(const RemoteQueueProxy&) = delete;
    RemoteQueueProxy(RemoteQueueProxy&&) = delete;
    RemoteQueueProxy& operator=(const RemoteQueueProxy&) = delete;
    RemoteQueueProxy& operator=(RemoteQueueProxy&&) = delete;

    WebGPUIdentifier backing() const { return m_backing; }

    template<typename T>
    [[nodiscard]] IPC::Error send(T&& message)
    {
        return protect(root().streamClientConnection())->send(std::forward<T>(message), backing());
    }
    template<typename T, typename C>
    [[nodiscard]] std::optional<IPC::StreamClientConnection::AsyncReplyID> sendWithAsyncReply(T&& message, C&& completionHandler)
    {
        return protect(root().streamClientConnection())->sendWithAsyncReply(std::forward<T>(message), std::forward<C>(completionHandler), backing());
    }
    template<typename T>
    [[nodiscard]] IPC::Connection::SendSyncResult<T> sendSync(T&& message)
    {
        return protect(root().streamClientConnection())->sendSync(std::forward<T>(message), backing());
    }

#if PLATFORM(COCOA) && ENABLE(VIDEO)
    void copyExternalImageFromVideoFrameToTexture(
        const WebCore::WebGPUExternalImageSource&,
        const WebCore::WebGPUImageCopyTextureTagged& destination,
        const ::WebGPU::Extent3D& copySize);
#endif

    WebGPUIdentifier m_backing;
    const Ref<ConvertToBackingContext> m_convertToBackingContext;
    const Ref<RemoteAdapterProxy> m_parent;
#if ENABLE(VIDEO)
    const RefPtr<RemoteVideoFrameObjectHeapProxy> m_videoFrameObjectHeapProxy;
#endif
#if PLATFORM(COCOA) && ENABLE(VIDEO)
    WebKit::SharedVideoFrameWriter m_sharedVideoFrameWriter;
#endif
};

} // namespace WebKit::WebGPU

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::WebGPU::RemoteQueueProxy)
    // In the Web Process, every WebGPU::Queue is a RemoteQueueProxy.
    static bool isType(const ::WebGPU::Queue&) { return true; }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(GPU_PROCESS)
