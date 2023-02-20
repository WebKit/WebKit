/*
 * Copyright (C) 2016 Canon Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted, provided that the following conditions
 * are required to be met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Canon Inc. nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY CANON INC. AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL CANON INC. AND ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "ExceptionOr.h"
#include "SharedBuffer.h"
#include <wtf/Function.h>
#include <wtf/Span.h>

namespace WebCore {

class FragmentedSharedBuffer;

class FetchResponseBodyLoader {
    WTF_MAKE_FAST_ALLOCATED;
public:
    virtual ~FetchResponseBodyLoader() = default;

    virtual void stop() = 0;
    virtual bool isActive() const = 0;
    virtual RefPtr<FragmentedSharedBuffer> startStreaming() = 0;

    using ConsumeDataByChunkCallback = Function<void(ExceptionOr<Span<const uint8_t>*>&&)>;
    void consumeDataByChunk(ConsumeDataByChunkCallback&&);
    ConsumeDataByChunkCallback takeConsumeDataCallback() { return WTFMove(m_consumeDataCallback); }
    bool hasConsumeDataCallback() const { return !!m_consumeDataCallback; }
    ConsumeDataByChunkCallback& consumeDataCallback() { return m_consumeDataCallback; }

private:
    ConsumeDataByChunkCallback m_consumeDataCallback;
};

inline void FetchResponseBodyLoader::consumeDataByChunk(ConsumeDataByChunkCallback&& callback)
{
    ASSERT(!m_consumeDataCallback);
    m_consumeDataCallback = WTFMove(callback);
    auto data = startStreaming();
    if (!data)
        return;

    auto contiguousBuffer = data->makeContiguous();
    Span chunk { contiguousBuffer->data(), data->size() };
    m_consumeDataCallback(&chunk);
}

} // namespace WebCore
