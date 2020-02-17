/*
 * Copyright (C) 2020 Sony Interactive Entertainment Inc.
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

#include "CurlContext.h"
#include <wtf/URL.h>
#include <wtf/UniqueArray.h>
#include <wtf/Vector.h>

namespace WebCore {

class CurlStreamScheduler;
class SocketStreamError;

using CurlStreamID = uint16_t;
const CurlStreamID invalidCurlStreamID = 0;

class CurlStream {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(CurlStream);
public:
    class Client {
    public:
        virtual void didOpen(CurlStreamID) = 0;
        virtual void didSendData(CurlStreamID, size_t) = 0;
        virtual void didReceiveData(CurlStreamID, const char*, size_t) = 0;
        virtual void didFail(CurlStreamID, CURLcode) = 0;
    };

    static std::unique_ptr<CurlStream> create(CurlStreamScheduler& scheduler, CurlStreamID streamID, URL&& url)
    {
        return WTF::makeUnique<CurlStream>(scheduler, streamID, WTFMove(url));
    }

    CurlStream(CurlStreamScheduler&, CurlStreamID, URL&&);
    virtual ~CurlStream();

    void send(UniqueArray<uint8_t>&&, size_t);

    void appendMonitoringFd(fd_set&, fd_set&, fd_set&, int&);
    void tryToTransfer(const fd_set&, const fd_set&, const fd_set&);

private:
    void destroyHandle();

    void tryToReceive();
    void tryToSend();

    void notifyFailure(CURLcode);

    static const size_t kReceiveBufferSize = 16 * 1024;

    CurlStreamScheduler& m_scheduler;
    CurlStreamID m_streamID;

    std::unique_ptr<CurlHandle> m_curlHandle;

    WTF::Vector<std::pair<UniqueArray<uint8_t>, size_t>> m_sendBuffers;
    size_t m_sendBufferOffset { 0 };
};

} // namespace WebCore
