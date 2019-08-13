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

#include "FetchBodyConsumer.h"
#include "ThreadableLoader.h"
#include "ThreadableLoaderClient.h"
#include <wtf/URL.h>

namespace WebCore {

class Blob;
class FetchLoaderClient;
class FetchRequest;
class ScriptExecutionContext;

class FetchLoader final : public ThreadableLoaderClient {
public:
    FetchLoader(FetchLoaderClient&, FetchBodyConsumer*);
    ~FetchLoader();

    RefPtr<SharedBuffer> startStreaming();

    void start(ScriptExecutionContext&, const FetchRequest&);
    void start(ScriptExecutionContext&, const Blob&);
    void startLoadingBlobURL(ScriptExecutionContext&, const URL& blobURL);
    void stop();

    bool isStarted() const { return m_isStarted; }

private:
    // ThreadableLoaderClient API.
    void didReceiveResponse(unsigned long, const ResourceResponse&) final;
    void didReceiveData(const char*, int) final;
    void didFinishLoading(unsigned long) final;
    void didFail(const ResourceError&) final;

private:
    FetchLoaderClient& m_client;
    RefPtr<ThreadableLoader> m_loader;
    FetchBodyConsumer* m_consumer;
    bool m_isStarted { false };
    URL m_urlForReading;
    Optional<PAL::SessionID> m_sessionID;
};

} // namespace WebCore
