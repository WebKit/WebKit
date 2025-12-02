/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#if ENABLE(GPU_PROCESS) && ENABLE(VIDEO)

#include <WebCore/PlatformMediaResourceLoader.h>
#include <WebCore/ResourceRequest.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>
#include <wtf/WorkQueue.h>

namespace WebKit {

class RemoteMediaPlayerProxy;

class RemoteMediaResourceLoader final
    : public WebCore::PlatformMediaResourceLoader {
    WTF_MAKE_TZONE_ALLOCATED(RemoteMediaResourceLoader);
public:
    static Ref<RemoteMediaResourceLoader> create(RemoteMediaPlayerProxy& proxy) { return adoptRef(*new RemoteMediaResourceLoader(proxy)); }
    ~RemoteMediaResourceLoader();

    static Ref<WorkQueue> defaultQueue()
    {
        static NeverDestroyed<Ref<WorkQueue>> messageQueue = WorkQueue::create("PlatformMediaResourceLoader"_s);
        return messageQueue.get();
    }

private:
    explicit RemoteMediaResourceLoader(RemoteMediaPlayerProxy&);

    RefPtr<WebCore::PlatformMediaResource> requestResource(WebCore::ResourceRequest&&, LoadOptions) final;
    void sendH2Ping(const URL&, CompletionHandler<void(Expected<WTF::Seconds, WebCore::ResourceError>&&)>&&) final;
    Ref<GuaranteedSerialFunctionDispatcher> targetDispatcher() final { return defaultQueue(); }

    WeakPtr<RemoteMediaPlayerProxy> m_remoteMediaPlayerProxy;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && ENABLE(VIDEO)
