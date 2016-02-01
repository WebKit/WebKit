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

#ifndef FetchRequest_h
#define FetchRequest_h

#if ENABLE(FETCH_API)

#include "FetchBody.h"
#include "FetchHeaders.h"
#include "FetchOptions.h"
#include "ResourceRequest.h"

namespace WebCore {

class Dictionary;
class ScriptExecutionContext;

typedef int ExceptionCode;

class FetchRequest : public RefCounted<FetchRequest> {
public:
    static RefPtr<FetchRequest> create(ScriptExecutionContext&, FetchRequest*, const Dictionary&, ExceptionCode&);
    static RefPtr<FetchRequest> create(ScriptExecutionContext&, const String&, const Dictionary&, ExceptionCode&);

    // Request API
    const String& method() const { return m_internalRequest.request.httpMethod(); }
    const String& url() const { return m_internalRequest.request.url().string(); }
    FetchHeaders& headers() { return m_headers.get(); }

    String type() const;
    String destination() const;
    String referrer() const;
    String referrerPolicy() const;
    String mode() const;
    String credentials() const;
    String cache() const;
    String redirect() const;
    const String& integrity() const { return m_internalRequest.integrity; }

    RefPtr<FetchRequest> clone(ExceptionCode&);

    // Body API
    bool isDisturbed() const { return m_body.isDisturbed(); }
    void arrayBuffer(FetchBody::ArrayBufferPromise&& promise) { m_body.arrayBuffer(WTFMove(promise)); }
    void formData(FetchBody::FormDataPromise&& promise) { m_body.formData(WTFMove(promise)); }
    void blob(FetchBody::BlobPromise&& promise) { m_body.blob(WTFMove(promise)); }
    void json(JSC::ExecState& state, FetchBody::JSONPromise&& promise) { m_body.json(state, WTFMove(promise)); }
    void text(FetchBody::TextPromise&& promise) { m_body.text(WTFMove(promise)); }

    struct InternalRequest {
        ResourceRequest request;
        FetchOptions options;
        String referrer;
        String integrity;
    };

    FetchBody& body() { return m_body; }

private:
    FetchRequest(FetchBody&&, Ref<FetchHeaders>&&, InternalRequest&&);

    FetchBody m_body;
    Ref<FetchHeaders> m_headers;
    InternalRequest m_internalRequest;
};

inline FetchRequest::FetchRequest(FetchBody&& body, Ref<FetchHeaders>&& headers, InternalRequest&& internalRequest)
    : m_body(WTFMove(body))
    , m_headers(WTFMove(headers))
    , m_internalRequest(WTFMove(internalRequest))
{
}

} // namespace WebCore

#endif // ENABLE(FETCH_API)

#endif // FetchRequest_h
