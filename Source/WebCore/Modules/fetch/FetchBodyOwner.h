/*
 * Copyright (C) 2016 Canon Inc.
 * Copyright (C) 2020-2024 Apple Inc. All rights reserved.
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
#include "ExceptionOr.h"
#include "FetchBody.h"
#include "FetchBodySource.h"
#include "FetchHeaders.h"
#include "FetchLoader.h"
#include "FetchLoaderClient.h"
#include "ResourceError.h"
#include "SharedBuffer.h"
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(FetchBodyOwner);

class FetchBodyOwner : public RefCountedAndCanMakeWeakPtr<FetchBodyOwner>, public ActiveDOMObject {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(FetchBodyOwner);
public:
    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    ~FetchBodyOwner();

    bool bodyUsed() const { return isDisturbed(); }
    void arrayBuffer(Ref<DeferredPromise>&&);
    void blob(Ref<DeferredPromise>&&);
    void bytes(Ref<DeferredPromise>&&);
    void formData(Ref<DeferredPromise>&&);
    void json(Ref<DeferredPromise>&&);
    void text(Ref<DeferredPromise>&&);

    bool isDisturbed() const;
    bool isDisturbedOrLocked() const;

    void loadBlob(const Blob&, FetchBodyConsumer*);

    ExceptionOr<RefPtr<ReadableStream>> readableStream(JSC::JSGlobalObject&);
    bool hasReadableStreamBody() const { return m_body && m_body->hasReadableStream(); }
    bool isReadableStreamBody() const { return m_body && m_body->isReadableStream(); }

    virtual void consumeBodyAsStream();
    virtual void feedStream() { }
    virtual void cancel() { }
    virtual void loadBody() { }

    bool hasLoadingError() const;
    ResourceError loadingError() const;
    std::optional<Exception> loadingException() const;

    String contentType() const { return m_headers->fastGet(HTTPHeaderName::ContentType); }

protected:
    FetchBodyOwner(ScriptExecutionContext*, std::optional<FetchBody>&&, Ref<FetchHeaders>&&);

    const FetchBody& body() const { return *m_body; }
    FetchBody& body() { return *m_body; }
    bool isBodyNull() const { return !m_body; }
    bool isBodyNullOrOpaque() const { return !m_body || m_isBodyOpaque; }
    void cloneBody(FetchBodyOwner&);

    ExceptionOr<void> extractBody(FetchBody::Init&&);
    void consumeOnceLoadingFinished(FetchBodyConsumer::Type, Ref<DeferredPromise>&&);

    void setBody(FetchBody&& body) { m_body = WTFMove(body); }
    ExceptionOr<void> createReadableStream(JSC::JSGlobalObject&);

    // ActiveDOMObject.
    void stop() override;

    void setDisturbed() { m_isDisturbed = true; }

    void setBodyAsOpaque() { m_isBodyOpaque = true; }
    bool isBodyOpaque() const { return m_isBodyOpaque; }

    void setLoadingError(Exception&&);
    void setLoadingError(ResourceError&&);

private:
    // Blob loading routines
    void blobChunk(const SharedBuffer&);
    void blobLoadingSucceeded();
    void blobLoadingFailed();
    void finishBlobLoading();

    // ActiveDOMObject API
    bool virtualHasPendingActivity() const final;

    struct BlobLoader final : FetchLoaderClient {
        WTF_MAKE_TZONE_ALLOCATED(BlobLoader);
        WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(BlobLoader);
    public:
        BlobLoader(FetchBodyOwner&);

        // FetchLoaderClient API
        void didReceiveResponse(const ResourceResponse&) final;
        void didReceiveData(const SharedBuffer& buffer) final { owner.blobChunk(buffer); }
        void didFail(const ResourceError&) final;
        void didSucceed(const NetworkLoadMetrics&) final;

        FetchBodyOwner& owner;
        std::unique_ptr<FetchLoader> loader;
    };

protected:
    std::optional<FetchBody> m_body;
    bool m_isDisturbed { false };
    RefPtr<FetchBodySource> m_readableStreamSource;
    Ref<FetchHeaders> m_headers;

private:
    std::optional<BlobLoader> m_blobLoader;
    bool m_isBodyOpaque { false };

    std::variant<std::nullptr_t, Exception, ResourceError> m_loadingError;
};

} // namespace WebCore
