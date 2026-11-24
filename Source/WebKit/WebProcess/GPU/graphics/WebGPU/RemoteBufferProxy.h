/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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

#include "RemoteDeviceProxy.h"
#include "WebGPUIdentifier.h"
#include <WebCore/WebGPUCppAPI.h>
#include <wtf/TZoneMalloc.h>

namespace WebKit::WebGPU {

static constexpr size_t maxCrossProcessResourceCopySize = 16 * MB;

class RemoteBufferProxy final : public ::WebGPU::Buffer {
    WTF_MAKE_TZONE_ALLOCATED(RemoteBufferProxy);
public:
    static Ref<RemoteBufferProxy> create(RemoteDeviceProxy& parent, WebGPUBufferIdentifier identifier, const ::WebGPU::BufferDescriptor& descriptor)
    {
        return adoptRef(*new RemoteBufferProxy(parent, identifier, descriptor));
    }

    virtual ~RemoteBufferProxy();

    RemoteDeviceProxy& parent() { return m_parent; }
    RemoteGPUProxy& root() { return m_parent->root(); }
    WebGPUBufferIdentifier backing() const { return m_backing; }

    RemoteDeviceProxy& device() const final { return m_parent; }
    void mapAsync(OptionSet<::WebGPU::MapMode>, uint64_t offset, std::optional<uint64_t> sizeForMap, CompletionHandler<void(bool)>&&) final;
    void getMappedRange(uint64_t offset, std::optional<uint64_t>, NOESCAPE const Function<void(std::span<uint8_t>)>&) final;
    std::span<uint8_t> persistentMappedRange(uint64_t offset, std::optional<uint64_t> size) final;
    std::span<uint8_t> getBufferContents() final;
    void unmap() final;
    void copyFrom(std::span<const uint8_t>, size_t offset) final;

    void destroy() final;

    uint64_t size() const final { return m_size; }
    OptionSet<::WebGPU::BufferUsage> usage() const final { return m_usage; }
    ::WebGPU::BufferMapState mapState() const final;

    void setLabel(String&&) final;
    bool isValid() const final;

private:
    RemoteBufferProxy(RemoteDeviceProxy&, WebGPUBufferIdentifier, const ::WebGPU::BufferDescriptor&);

    RemoteBufferProxy(const RemoteBufferProxy&) = delete;
    RemoteBufferProxy(RemoteBufferProxy&&) = delete;
    RemoteBufferProxy& operator=(const RemoteBufferProxy&) = delete;
    RemoteBufferProxy& operator=(RemoteBufferProxy&&) = delete;

    template<typename T>
    [[nodiscard]] IPC::Error send(T&& message)
    {
        return protect(root().streamClientConnection())->send(std::forward<T>(message), backing());
    }
    template<typename T>
    [[nodiscard]] IPC::Connection::SendSyncResult<T> sendSync(T&& message)
    {
        return protect(root().streamClientConnection())->sendSync(std::forward<T>(message), backing());
    }
    template<typename T, typename C>
    [[nodiscard]] std::optional<IPC::StreamClientConnection::AsyncReplyID> sendWithAsyncReply(T&& message, C&& completionHandler)
    {
        return protect(root().streamClientConnection())->sendWithAsyncReply(std::forward<T>(message), std::forward<C>(completionHandler), backing());
    }

    WebGPUBufferIdentifier m_backing;
    const Ref<RemoteDeviceProxy> m_parent;
    const uint64_t m_size;
    const OptionSet<::WebGPU::BufferUsage> m_usage;
    OptionSet<::WebGPU::MapMode> m_mapModeFlags;
    bool m_isMapPending { false };
    // Bumped by unmap() and destroy(), so that the reply to a mapAsync() that they aborted does not
    // map the buffer: the GPU Process completes such a call with success too.
    uint64_t m_mapGeneration { 0 };
    // The range of the current mapping, as it was requested.
    uint64_t m_mappedOffset { 0 };
    std::optional<uint64_t> m_mappedSize;
    // The contents of the mapped range, which persistentMappedRange() fetches once per mapping and
    // unmap() writes back, so that its spans stay valid until unmap().
    std::optional<Vector<uint8_t>> m_persistentMappedRange;
};

} // namespace WebKit::WebGPU

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::WebGPU::RemoteBufferProxy)
    // In the Web Process, every WebGPU::Buffer is a RemoteBufferProxy.
    static bool isType(const ::WebGPU::Buffer&) { return true; }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(GPU_PROCESS)
