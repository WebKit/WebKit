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

#pragma once

#include "ScopedRenderingResourcesRequest.h"
#include <atomic>
#include <wtf/StdLibExtras.h>

namespace WebKit {

class ScopedWebGLRenderingResourcesRequest {
public:
    ScopedWebGLRenderingResourcesRequest() = default;
    ScopedWebGLRenderingResourcesRequest(ScopedWebGLRenderingResourcesRequest&& other)
        : m_requested(std::exchange(other.m_requested, false))
        , m_renderingResourcesRequest(WTFMove(other.m_renderingResourcesRequest))
    {
    }
    ~ScopedWebGLRenderingResourcesRequest()
    {
        reset();
    }
    ScopedWebGLRenderingResourcesRequest& operator=(ScopedWebGLRenderingResourcesRequest&& other)
    {
        if (this != &other) {
            reset();
            m_requested = std::exchange(other.m_requested, false);
            m_renderingResourcesRequest = WTFMove(other.m_renderingResourcesRequest);
        }
        return *this;
    }
    bool isRequested() const { return m_requested; }
    void reset()
    {
        if (!m_requested)
            return;
        ASSERT(s_requests);
        --s_requests;
        if (!s_requests)
            scheduleFreeWebGLRenderingResources();
    }
    static ScopedWebGLRenderingResourcesRequest acquire()
    {
        return { DidRequest };
    }
private:
    static void scheduleFreeWebGLRenderingResources();
    static void freeWebGLRenderingResources();
    enum RequestState { DidRequest };
    ScopedWebGLRenderingResourcesRequest(RequestState)
        : m_requested(true)
        , m_renderingResourcesRequest(ScopedRenderingResourcesRequest::acquire())
    {
        ++s_requests;
    }
    bool m_requested { false };
    ScopedRenderingResourcesRequest m_renderingResourcesRequest;
    static std::atomic<unsigned> s_requests;
};

}
