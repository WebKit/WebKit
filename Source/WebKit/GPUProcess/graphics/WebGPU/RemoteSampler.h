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

#if ENABLE(GPU_PROCESS)

#include "StreamMessageReceiver.h"
#include "WebGPUIdentifier.h"
#include <wtf/Ref.h>
#include <wtf/text/WTFString.h>

namespace PAL::WebGPU {
class Sampler;
}

namespace WebKit {

namespace WebGPU {
class ObjectHeap;
}

class RemoteSampler final : public IPC::StreamMessageReceiver {
public:
    static Ref<RemoteSampler> create(PAL::WebGPU::Sampler& sampler, WebGPU::ObjectHeap& objectHeap)
    {
        return adoptRef(*new RemoteSampler(sampler, objectHeap));
    }

    virtual ~RemoteSampler();

private:
    friend class ObjectHeap;

    RemoteSampler(PAL::WebGPU::Sampler&, WebGPU::ObjectHeap&);

    RemoteSampler(const RemoteSampler&) = delete;
    RemoteSampler(RemoteSampler&&) = delete;
    RemoteSampler& operator=(const RemoteSampler&) = delete;
    RemoteSampler& operator=(RemoteSampler&&) = delete;

    void didReceiveStreamMessage(IPC::StreamServerConnectionBase&, IPC::Decoder&) final;

    void setLabel(String&&);

    Ref<PAL::WebGPU::Sampler> m_backing;
    Ref<WebGPU::ObjectHeap> m_objectHeap;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
