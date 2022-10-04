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

#if ENABLE(WEB_CODECS)

#include "BufferSource.h"
#include "ExceptionOr.h"
#include "WebCodecsEncodedVideoChunkType.h"
#include <wtf/Vector.h>

namespace WebCore {

class WebCodecsEncodedVideoChunk : public RefCounted<WebCodecsEncodedVideoChunk> {
public:
    ~WebCodecsEncodedVideoChunk() = default;

    struct Init {
        WebCodecsEncodedVideoChunkType type { WebCodecsEncodedVideoChunkType::Key };
        int64_t timestamp { 0 };
        std::optional<uint64_t> duration;
        BufferSource data;
    };

    static Ref<WebCodecsEncodedVideoChunk> create(Init&& init) { return adoptRef(*new WebCodecsEncodedVideoChunk(WTFMove(init))); }

    WebCodecsEncodedVideoChunkType type() const { return m_type; };
    int64_t timestamp() const { return m_timestamp; }
    std::optional<uint64_t> duration() const { return m_duration; }
    size_t byteLength() const { return m_data.size(); }

    ExceptionOr<void> copyTo(BufferSource&&);

    const uint8_t* data() const { return m_data.data(); }

private:
    explicit WebCodecsEncodedVideoChunk(Init&&);

    WebCodecsEncodedVideoChunkType m_type { WebCodecsEncodedVideoChunkType::Key };
    int64_t m_timestamp { 0 };
    std::optional<uint64_t> m_duration { 0 };
    Vector<uint8_t> m_data;
};

}

#endif // ENABLE(WEB_CODECS)
