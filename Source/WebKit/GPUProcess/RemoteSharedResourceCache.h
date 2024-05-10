/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include "MessageReceiver.h"
#include "RemoteSerializedImageBufferIdentifier.h"
#include "ThreadSafeObjectHeap.h"
#include <WebCore/ImageBuffer.h>
#include <WebCore/RenderingResourceIdentifier.h>
#include <atomic>
#include <wtf/FastMalloc.h>
#include <wtf/Ref.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WebKit {

// Class holding GPU process resources per WebContent process.
// Thread-safe.
class RemoteSharedResourceCache final : public ThreadSafeRefCounted<RemoteSharedResourceCache>, IPC::MessageReceiver {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<RemoteSharedResourceCache> create();
    virtual ~RemoteSharedResourceCache();

    void addSerializedImageBuffer(WebCore::RenderingResourceIdentifier, Ref<WebCore::ImageBuffer>);
    RefPtr<WebCore::ImageBuffer> takeSerializedImageBuffer(WebCore::RenderingResourceIdentifier);

    void didAddAcceleratedImageBuffer();
    void didTakeAcceleratedImageBuffer();
    WebCore::RenderingMode adjustAcceleratedImageBufferRenderingMode(WebCore::RenderingPurpose) const;

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

private:
    RemoteSharedResourceCache();

    // Messages
    void releaseSerializedImageBuffer(WebCore::RenderingResourceIdentifier);

    IPC::ThreadSafeObjectHeap<RemoteSerializedImageBufferIdentifier, RefPtr<WebCore::ImageBuffer>> m_serializedImageBuffers;
    std::atomic<size_t> m_acceleratedImageBufferCount { 0 };
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
