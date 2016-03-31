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

#include "config.h"
#include "FetchBodyOwner.h"

#if ENABLE(FETCH_API)

#include "ExceptionCode.h"
#include "FetchLoader.h"
#include "JSBlob.h"
#include "ResourceResponse.h"

namespace WebCore {

FetchBodyOwner::FetchBodyOwner(ScriptExecutionContext& context, FetchBody&& body)
    : ActiveDOMObject(&context)
    , m_body(WTFMove(body))
{
    suspendIfNeeded();
}

void FetchBodyOwner::stop()
{
    if (m_blobLoader) {
        if (m_blobLoader->loader)
            m_blobLoader->loader->stop();
        finishBlobLoading();
    }
    ASSERT(!m_blobLoader);
}

void FetchBodyOwner::arrayBuffer(DeferredWrapper&& promise)
{
    if (m_body.isEmpty()) {
        fulfillPromiseWithArrayBuffer(promise, nullptr, 0);
        return;
    }
    if (isDisturbed()) {
        promise.reject<ExceptionCode>(TypeError);
        return;
    }
    m_isDisturbed = true;
    m_body.arrayBuffer(*this, WTFMove(promise));
}

void FetchBodyOwner::blob(DeferredWrapper&& promise)
{
    if (m_body.isEmpty()) {
        promise.resolve<RefPtr<Blob>>(Blob::create());
        return;
    }
    if (isDisturbed()) {
        promise.reject<ExceptionCode>(TypeError);
        return;
    }
    m_isDisturbed = true;
    m_body.blob(*this, WTFMove(promise));
}

void FetchBodyOwner::formData(DeferredWrapper&& promise)
{
    if (m_body.isEmpty()) {
        promise.reject<ExceptionCode>(0);
        return;
    }
    if (isDisturbed()) {
        promise.reject<ExceptionCode>(TypeError);
        return;
    }
    m_isDisturbed = true;
    m_body.formData(*this, WTFMove(promise));
}

void FetchBodyOwner::json(DeferredWrapper&& promise)
{
    if (m_body.isEmpty()) {
        promise.reject<ExceptionCode>(SYNTAX_ERR);
        return;
    }
    if (isDisturbed()) {
        promise.reject<ExceptionCode>(TypeError);
        return;
    }
    m_isDisturbed = true;
    m_body.json(*this, WTFMove(promise));
}

void FetchBodyOwner::text(DeferredWrapper&& promise)
{
    if (m_body.isEmpty()) {
        promise.resolve(String());
        return;
    }
    if (isDisturbed()) {
        promise.reject<ExceptionCode>(TypeError);
        return;
    }
    m_isDisturbed = true;
    m_body.text(*this, WTFMove(promise));
}

void FetchBodyOwner::loadBlob(Blob& blob, FetchLoader::Type type)
{
    // Can only be called once for a body instance.
    ASSERT(isDisturbed());
    ASSERT(!m_blobLoader);

    if (!scriptExecutionContext()) {
        m_body.loadingFailed();
        return;
    }

    m_blobLoader = { *this };
    m_blobLoader->loader = std::make_unique<FetchLoader>(type, *m_blobLoader);

    m_blobLoader->loader->start(*scriptExecutionContext(), blob);
    if (!m_blobLoader->loader->isStarted()) {
        m_body.loadingFailed();
        m_blobLoader = Nullopt;
        return;
    }
    setPendingActivity(this);
}

void FetchBodyOwner::finishBlobLoading()
{
    ASSERT(m_blobLoader);

    m_blobLoader = Nullopt;
    unsetPendingActivity(this);
}

void FetchBodyOwner::loadedBlobAsText(String&& text)
{
    m_body.loadedAsText(WTFMove(text));
}

void FetchBodyOwner::blobLoadingFailed()
{
    m_body.loadingFailed();
    finishBlobLoading();
}

FetchBodyOwner::BlobLoader::BlobLoader(FetchBodyOwner& owner)
    : owner(owner)
{
}

void FetchBodyOwner::BlobLoader::didReceiveResponse(const ResourceResponse& response)
{
    if (response.httpStatusCode() != 200)
        didFail();
}

void FetchBodyOwner::BlobLoader::didFail()
{
    // didFail might be called within FetchLoader::start call.
    if (loader->isStarted())
        owner.blobLoadingFailed();
}

} // namespace WebCore

#endif // ENABLE(FETCH_API)
