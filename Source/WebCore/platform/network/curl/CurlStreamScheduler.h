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

#include "CurlStream.h"
#include <wtf/Function.h>
#include <wtf/HashMap.h>

namespace WebCore {

class CurlStreamScheduler {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(CurlStreamScheduler);
public:
    CurlStreamScheduler();
    virtual ~CurlStreamScheduler();

    WEBCORE_EXPORT CurlStreamID createStream(const URL&, CurlStream::Client&);
    WEBCORE_EXPORT void destroyStream(CurlStreamID);
    WEBCORE_EXPORT void send(CurlStreamID, UniqueArray<uint8_t>&&, size_t);

    void callOnWorkerThread(Function<void()>&&);
    void callClientOnMainThread(CurlStreamID, Function<void(CurlStream::Client&)>&&);

private:
    void startThreadIfNeeded();
    void stopThreadIfNoMoreJobRunning();

    void executeTasks();

    void workerThread();

    Lock m_mutex;
    RefPtr<Thread> m_thread;
    bool m_runThread { false };

    CurlStreamID m_currentStreamID = 1;

    Vector<Function<void()>> m_taskQueue;
    HashMap<CurlStreamID, CurlStream::Client*> m_clientList;
    HashMap<CurlStreamID, std::unique_ptr<CurlStream>> m_streamList;
};

} // namespace WebCore
