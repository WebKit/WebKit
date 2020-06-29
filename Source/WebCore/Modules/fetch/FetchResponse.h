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

#include "FetchBodyOwner.h"
#include "FetchHeaders.h"
#include "ReadableStreamSink.h"
#include "ResourceResponse.h"
#include <JavaScriptCore/TypedArrays.h>
#include <wtf/WeakPtr.h>

namespace JSC {
class CallFrame;
class JSValue;
};

namespace WebCore {

class AbortSignal;
class FetchRequest;
struct ReadableStreamChunk;
class ReadableStreamSource;

class FetchResponse final : public FetchBodyOwner, public CanMakeWeakPtr<FetchResponse> {
public:
    using Type = ResourceResponse::Type;

    struct Init {
        unsigned short status { 200 };
        String statusText;
        Optional<FetchHeaders::Init> headers;
    };

    WEBCORE_EXPORT static Ref<FetchResponse> create(ScriptExecutionContext&, Optional<FetchBody>&&, FetchHeaders::Guard, ResourceResponse&&);

    static ExceptionOr<Ref<FetchResponse>> create(ScriptExecutionContext&, Optional<FetchBody::Init>&&, Init&&);
    static Ref<FetchResponse> error(ScriptExecutionContext&);
    static ExceptionOr<Ref<FetchResponse>> redirect(ScriptExecutionContext&, const String& url, int status);

    using NotificationCallback = WTF::Function<void(ExceptionOr<FetchResponse&>&&)>;
    static void fetch(ScriptExecutionContext&, FetchRequest&, NotificationCallback&&);

    void startConsumingStream(unsigned);
    void consumeChunk(Ref<JSC::Uint8Array>&&);
    void finishConsumingStream(Ref<DeferredPromise>&&);

    Type type() const { return filteredResponse().type(); }
    const String& url() const;
    bool redirected() const { return filteredResponse().isRedirected(); }
    int status() const { return filteredResponse().httpStatusCode(); }
    bool ok() const { return filteredResponse().isSuccessful(); }
    const String& statusText() const { return filteredResponse().httpStatusText(); }

    const FetchHeaders& headers() const { return m_headers; }
    FetchHeaders& headers() { return m_headers; }
    ExceptionOr<Ref<FetchResponse>> clone(ScriptExecutionContext&);

    void consumeBodyAsStream() final;
    void feedStream() final;
    void cancel() final;

    using ResponseData = Variant<std::nullptr_t, Ref<FormData>, Ref<SharedBuffer>>;
    ResponseData consumeBody();
    void setBodyData(ResponseData&&, uint64_t bodySizeWithPadding);

    bool isLoading() const { return !!m_bodyLoader; }
    bool isBodyReceivedByChunk() const { return isLoading() || hasReadableStreamBody(); }
    bool isBlobBody() const { return !isBodyNull() && body().isBlob(); }
    bool isBlobFormData() const { return !isBodyNull() && body().isFormData(); }

    using ConsumeDataByChunkCallback = WTF::Function<void(ExceptionOr<ReadableStreamChunk*>&&)>;
    void consumeBodyReceivedByChunk(ConsumeDataByChunkCallback&&);

    WEBCORE_EXPORT ResourceResponse resourceResponse() const;
    ResourceResponse::Tainting tainting() const { return m_internalResponse.tainting(); }

    uint64_t bodySizeWithPadding() const { return m_bodySizeWithPadding; }
    void setBodySizeWithPadding(uint64_t size) { m_bodySizeWithPadding = size; }
    uint64_t opaqueLoadIdentifier() const { return m_opaqueLoadIdentifier; }

    void initializeOpaqueLoadIdentifierForTesting() { m_opaqueLoadIdentifier = 1; }

    const HTTPHeaderMap& internalResponseHeaders() const { return m_internalResponse.httpHeaderFields(); }

private:
    FetchResponse(ScriptExecutionContext&, Optional<FetchBody>&&, Ref<FetchHeaders>&&, ResourceResponse&&);

    void stop() final;
    const char* activeDOMObjectName() const final;

    const ResourceResponse& filteredResponse() const;

    void closeStream();

    void addAbortSteps(Ref<AbortSignal>&&);

    class BodyLoader final : public FetchLoaderClient {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        BodyLoader(FetchResponse&, NotificationCallback&&);
        ~BodyLoader();

        bool start(ScriptExecutionContext&, const FetchRequest&);
        void stop();

        void consumeDataByChunk(ConsumeDataByChunkCallback&&);

        RefPtr<SharedBuffer> startStreaming();
        NotificationCallback takeNotificationCallback() { return WTFMove(m_responseCallback); }
        ConsumeDataByChunkCallback takeConsumeDataCallback() { return WTFMove(m_consumeDataCallback); }

    private:
        // FetchLoaderClient API
        void didSucceed() final;
        void didFail(const ResourceError&) final;
        void didReceiveResponse(const ResourceResponse&) final;
        void didReceiveData(const char*, size_t) final;

        FetchResponse& m_response;
        NotificationCallback m_responseCallback;
        ConsumeDataByChunkCallback m_consumeDataCallback;
        std::unique_ptr<FetchLoader> m_loader;
        Ref<PendingActivity<FetchResponse>> m_pendingActivity;
        FetchOptions::Credentials m_credentials;
    };

    mutable Optional<ResourceResponse> m_filteredResponse;
    ResourceResponse m_internalResponse;
    std::unique_ptr<BodyLoader> m_bodyLoader;
    mutable String m_responseURL;
    // Opaque responses will padd their body size when used with Cache API.
    uint64_t m_bodySizeWithPadding { 0 };
    uint64_t m_opaqueLoadIdentifier { 0 };
    RefPtr<AbortSignal> m_abortSignal;
};

} // namespace WebCore
