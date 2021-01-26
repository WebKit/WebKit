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

#include "GraphicsChecksMac.h"
#include "SwitchingGPUClient.h"

namespace WebCore {

class ScopedHighPerformanceGPURequest {
public:
    ScopedHighPerformanceGPURequest() = default;
    ScopedHighPerformanceGPURequest(ScopedHighPerformanceGPURequest&& other)
        : m_requested(std::exchange(other.m_requested, false))
    {
    }
    ~ScopedHighPerformanceGPURequest()
    {
        if (!m_requested)
            return;
        SwitchingGPUClient::singletonIfExists()->releaseHighPerformanceGPU();
    }
    ScopedHighPerformanceGPURequest& operator=(ScopedHighPerformanceGPURequest&& other)
    {
        if (m_requested)
            SwitchingGPUClient::singletonIfExists()->releaseHighPerformanceGPU();
        m_requested = std::exchange(other.m_requested, false);
        return *this;
    }
    static ScopedHighPerformanceGPURequest acquire()
    {
        if (!hasLowAndHighPowerGPUs())
            return { };
        auto client = SwitchingGPUClient::singletonIfExists();
        if (!client)
            return { };
        client->requestHighPerformanceGPU();
        return { DidRequest };
    }
private:
    enum RequestState { DidRequest };
    ScopedHighPerformanceGPURequest(RequestState)
        : m_requested(true)
    {
    }
    bool m_requested { false };
};

}
