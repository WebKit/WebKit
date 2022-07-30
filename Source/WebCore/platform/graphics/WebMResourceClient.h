/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#if ENABLE(ALTERNATE_WEBM_PLAYER)

#include "PlatformMediaResourceLoader.h"
#include <wtf/WeakPtr.h>

namespace WebCore {

class WebMResourceClientParent : public CanMakeWeakPtr<WebMResourceClientParent> {
public:
    virtual ~WebMResourceClientParent() = default;
    virtual void responseReceived(PlatformMediaResource&, const ResourceResponse&) = 0;
    virtual void dataReceived(const SharedBuffer&) = 0;
    virtual void loadFailed(const ResourceError&) = 0;
    virtual void loadFinished(const FragmentedSharedBuffer&) = 0;
};

class WebMResourceClient final
    : public PlatformMediaResourceClient
    , public CanMakeWeakPtr<WebMResourceClient> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static RefPtr<WebMResourceClient> create(WebMResourceClientParent&, PlatformMediaResourceLoader&, ResourceRequest&&);
    ~WebMResourceClient() { stop(); }

    void stop();

private:
    WebMResourceClient(WebMResourceClientParent&, Ref<PlatformMediaResource>&&);

    void responseReceived(PlatformMediaResource&, const ResourceResponse&, CompletionHandler<void(ShouldContinuePolicyCheck)>&&) final;
    void dataReceived(PlatformMediaResource&, const SharedBuffer&) final;
    void loadFailed(PlatformMediaResource&, const ResourceError&) final;
    void loadFinished(PlatformMediaResource&, const NetworkLoadMetrics&) final;

    WeakPtr<WebMResourceClientParent> m_parent;
    RefPtr<PlatformMediaResource> m_resource;
    SharedBufferBuilder m_buffer;
};

} // namespace WebCore

#endif // ENABLE(ALTERNATE_WEBM_PLAYER)
