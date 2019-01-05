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

#include "ActiveDOMObject.h"
#include "FetchBody.h"
#include "FetchBodySource.h"
#include "FetchHeaders.h"
#include "FetchLoader.h"
#include "FetchLoaderClient.h"
#include "ResourceError.h"

namespace WebCore {

class FetchBodyOwner : public RefCounted<FetchBodyOwner>, public ActiveDOMObject {
public:
    FetchBodyOwner(ScriptExecutionContext&, Optional<FetchBody>&&, Ref<FetchHeaders>&&);
    ~FetchBodyOwner();

    bool bodyUsed() const { return isDisturbed(); }
    void arrayBuffer(Ref<DeferredPromise>&&);
    void blob(Ref<DeferredPromise>&&);
    void formData(Ref<DeferredPromise>&&);
    void json(Ref<DeferredPromise>&&);
    void text(Ref<DeferredPromise>&&);

    bool isDisturbed() const;
    bool isDisturbedOrLocked() const;

    void loadBlob(const Blob&, FetchBodyConsumer*);

    bool isActive() const { return !!m_blobLoader; }

    RefPtr<ReadableStream> readableStream(JSC::ExecState&);
    bool hasReadableStreamBody() const { return m_body && m_body->hasReadableStream(); }

#if ENABLE(STREAMS_API)
    virtual void consumeBodyAsStream();
    virtual void feedStream() { }
    virtual void cancel() { }
#endif

    bool hasLoadingError() const;
    ResourceError loadingError() const;
    Optional<Exception> loadingException() const;

protected:
    const FetchBody& body() const { return *m_body; }
    FetchBody& body() { return *m_body; }
    bool isBodyNull() const { return !m_body; }
    bool isBodyNullOrOpaque() const { return !m_body || m_isBodyOpaque; }
    void cloneBody(FetchBodyOwner&);

    void extractBody(ScriptExecutionContext&, FetchBody::Init&&);
    void updateContentType();
    void consumeOnceLoadingFinished(FetchBodyConsumer::Type, Ref<DeferredPromise>&&);

    void setBody(FetchBody&& body) { m_body = WTFMove(body); }
    void createReadableStream(JSC::ExecState&);

    // ActiveDOMObject API
    void stop() override;

    void setDisturbed() { m_isDisturbed = true; }

    void setBodyAsOpaque() { m_isBodyOpaque = true; }
    bool isBodyOpaque() const { return m_isBodyOpaque; }

    void setLoadingError(Exception&&);
    void setLoadingError(ResourceError&&);

private:
    // Blob loading routines
    void blobChunk(const char*, size_t);
    void blobLoadingSucceeded();
    void blobLoadingFailed();
    void finishBlobLoading();

    struct BlobLoader final : FetchLoaderClient {
        BlobLoader(FetchBodyOwner&);

        // FetchLoaderClient API
        void didReceiveResponse(const ResourceResponse&) final;
        void didReceiveData(const char* data, size_t size) final { owner.blobChunk(data, size); }
        void didFail(const ResourceError&) final;
        void didSucceed() final { owner.blobLoadingSucceeded(); }

        FetchBodyOwner& owner;
        std::unique_ptr<FetchLoader> loader;
    };

protected:
    Optional<FetchBody> m_body;
    String m_contentType;
    bool m_isDisturbed { false };
#if ENABLE(STREAMS_API)
    RefPtr<FetchBodySource> m_readableStreamSource;
#endif
    Ref<FetchHeaders> m_headers;

private:
    Optional<BlobLoader> m_blobLoader;
    bool m_isBodyOpaque { false };

    Variant<std::nullptr_t, Exception, ResourceError> m_loadingError;
};

} // namespace WebCore
