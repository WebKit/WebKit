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

#if ENABLE(FETCH_API)

#include "FetchBodyOwner.h"
#include "ResourceResponse.h"
#include <runtime/TypedArrays.h>

namespace JSC {
class ExecState;
class JSValue;
};

namespace WebCore {

class FetchRequest;
class ReadableStreamSource;

class FetchResponse final : public FetchBodyOwner {
public:
    using Type = ResourceResponse::Type;

    static Ref<FetchResponse> create(ScriptExecutionContext& context) { return adoptRef(*new FetchResponse(context, std::nullopt, FetchHeaders::create(FetchHeaders::Guard::Response), ResourceResponse())); }
    static Ref<FetchResponse> error(ScriptExecutionContext&);
    static ExceptionOr<Ref<FetchResponse>> redirect(ScriptExecutionContext&, const String& url, int status);

    using FetchPromise = DOMPromise<IDLInterface<FetchResponse>>;
    static void fetch(ScriptExecutionContext&, FetchRequest&, FetchPromise&&);

    void consume(unsigned, Ref<DeferredPromise>&&);
#if ENABLE(READABLE_STREAM_API)
    void startConsumingStream(unsigned);
    void consumeChunk(Ref<JSC::Uint8Array>&&);
    void finishConsumingStream(Ref<DeferredPromise>&&);
#endif

    ExceptionOr<void> setStatus(int status, const String& statusText);
    void initializeWith(JSC::ExecState&, JSC::JSValue);

    Type type() const { return m_response.type(); }
    const String& url() const;
    bool redirected() const { return m_response.isRedirected(); }
    int status() const { return m_response.httpStatusCode(); }
    bool ok() const { return m_response.isSuccessful(); }
    const String& statusText() const { return m_response.httpStatusText(); }

    FetchHeaders& headers() { return m_headers; }
    Ref<FetchResponse> cloneForJS();

#if ENABLE(READABLE_STREAM_API)
    ReadableStreamSource* createReadableStreamSource();
    void consumeBodyAsStream();
    void feedStream();
    void cancel();
#endif

    bool isLoading() const { return !!m_bodyLoader; }

private:
    FetchResponse(ScriptExecutionContext&, std::optional<FetchBody>&&, Ref<FetchHeaders>&&, ResourceResponse&&);

    static void startFetching(ScriptExecutionContext&, const FetchRequest&, FetchPromise&&);

    void stop() final;
    const char* activeDOMObjectName() const final;
    bool canSuspendForDocumentSuspension() const final;

#if ENABLE(READABLE_STREAM_API)
    void closeStream();
#endif

    class BodyLoader final : public FetchLoaderClient {
    public:
        BodyLoader(FetchResponse&, FetchPromise&&);
        ~BodyLoader();

        bool start(ScriptExecutionContext&, const FetchRequest&);
        void stop();

#if ENABLE(READABLE_STREAM_API)
        RefPtr<SharedBuffer> startStreaming();
#endif

    private:
        // FetchLoaderClient API
        void didSucceed() final;
        void didFail() final;
        void didReceiveResponse(const ResourceResponse&) final;
        void didReceiveData(const char*, size_t) final;

        FetchResponse& m_response;
        std::optional<FetchPromise> m_promise;
        std::unique_ptr<FetchLoader> m_loader;
    };

    ResourceResponse m_response;
    std::optional<BodyLoader> m_bodyLoader;
    mutable String m_responseURL;

    FetchBodyConsumer m_consumer { FetchBodyConsumer::Type::ArrayBuffer  };
};

} // namespace WebCore

#endif // ENABLE(FETCH_API)
