/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include <WebCore/PendingStreamIdentifier.h>
#include <span>
#include <wtf/Deque.h>
#include <wtf/Forward.h>
#include <wtf/Function.h>
#include <wtf/Lock.h>
#include <wtf/Ref.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {

class SharedBuffer;

class PendingStreamState : public ThreadSafeRefCounted<PendingStreamState> {
public:
    enum class HTTPVersion : uint8_t {
        Unknown,
        HTTP1,
        HTTP2OrLater,
    };

    using HTTPVersionProbe = Function<HTTPVersion()>;
    WEBCORE_EXPORT static Ref<PendingStreamState> create();
    WEBCORE_EXPORT ~PendingStreamState();

    WEBCORE_EXPORT void appendData(Ref<SharedBuffer>&&);
    WEBCORE_EXPORT void endStream();
    WEBCORE_EXPORT void errorStream(int errorCode);

    WEBCORE_EXPORT void setHTTPVersionProbe(HTTPVersionProbe&&);

    WEBCORE_EXPORT void setDataAvailableHandler(Function<void()>&&);
    WEBCORE_EXPORT void clearDataAvailableHandler();

    HTTPVersion currentHTTPVersion();

    size_t read(std::span<uint8_t> outBuffer, bool& atEOF, int& errorCode);
    WEBCORE_EXPORT RefPtr<SharedBuffer> takeNextChunk(bool& atEOF, int& errorCode);
    bool hasReadyData();

private:
    class DataAvailableHandler;
    PendingStreamState();

    static void invokeHandlerIfNeeded(NOESCAPE const std::function<RefPtr<DataAvailableHandler>()>&);

    Lock m_lock;
    Deque<Ref<SharedBuffer>> m_chunks WTF_GUARDED_BY_LOCK(m_lock);
    size_t m_frontChunkOffset WTF_GUARDED_BY_LOCK(m_lock) { 0 };
    bool m_ended WTF_GUARDED_BY_LOCK(m_lock) { false };
    int m_errorCode WTF_GUARDED_BY_LOCK(m_lock) { 0 };
    RefPtr<DataAvailableHandler> m_dataAvailableHandler WTF_GUARDED_BY_LOCK(m_lock);
    HTTPVersionProbe m_httpVersionProbe WTF_GUARDED_BY_LOCK(m_lock);
};

} // namespace WebCore
