/*
 *  Copyright (C) 2003, 2006, 2008 Apple Inc. All rights reserved.
 *  Copyright (C) 2005, 2006 Alexey Proskuryakov <ap@nypop.com>
 *  Copyright (C) 2011 Google Inc. All rights reserved.
 *  Copyright (C) 2012 Intel Corporation
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once

#include "ActiveDOMObject.h"
#include "ExceptionOr.h"
#include "FormData.h"
#include "ResourceResponse.h"
#include "ThreadableLoaderClient.h"
#include "URL.h"
#include "XMLHttpRequestEventTarget.h"
#include "XMLHttpRequestProgressEventThrottle.h"
#include <wtf/Variant.h>
#include <wtf/text/StringBuilder.h>

namespace JSC {
class ArrayBuffer;
class ArrayBufferView;
}

namespace WebCore {

class Blob;
class Document;
class DOMFormData;
class SecurityOrigin;
class SharedBuffer;
class TextResourceDecoder;
class ThreadableLoader;
class XMLHttpRequestUpload;
struct OwnedString;

class XMLHttpRequest final : public RefCounted<XMLHttpRequest>, public XMLHttpRequestEventTarget, private ThreadableLoaderClient, public ActiveDOMObject {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<XMLHttpRequest> create(ScriptExecutionContext&);
    WEBCORE_EXPORT ~XMLHttpRequest();

    enum State {
        UNSENT = 0,
        OPENED = 1,
        HEADERS_RECEIVED = 2,
        LOADING = 3,
        DONE = 4
    };

    virtual void didReachTimeout();

    EventTargetInterface eventTargetInterface() const override { return XMLHttpRequestEventTargetInterfaceType; }
    ScriptExecutionContext* scriptExecutionContext() const override { return ActiveDOMObject::scriptExecutionContext(); }

    using SendTypes = Variant<RefPtr<Document>, RefPtr<Blob>, RefPtr<JSC::ArrayBufferView>, RefPtr<JSC::ArrayBuffer>, RefPtr<DOMFormData>, String>;

    const URL& url() const { return m_url; }
    String statusText() const;
    int status() const;
    State readyState() const;
    bool withCredentials() const { return m_includeCredentials; }
    ExceptionOr<void> setWithCredentials(bool);
    ExceptionOr<void> open(const String& method, const String& url);
    ExceptionOr<void> open(const String& method, const URL&, bool async);
    ExceptionOr<void> open(const String& method, const String&, bool async, const String& user, const String& password);
    ExceptionOr<void> send(std::optional<SendTypes>&&);
    void abort();
    ExceptionOr<void> setRequestHeader(const String& name, const String& value);
    ExceptionOr<void> overrideMimeType(const String& override);
    bool doneWithoutErrors() const { return !m_error && m_state == DONE; }
    String getAllResponseHeaders() const;
    String getResponseHeader(const String& name) const;
    ExceptionOr<OwnedString> responseText();
    String responseTextIgnoringResponseType() const { return m_responseBuilder.toStringPreserveCapacity(); }
    String responseMIMEType() const;

    Document* optionalResponseXML() const { return m_responseDocument.get(); }
    ExceptionOr<Document*> responseXML();

    Ref<Blob> createResponseBlob();
    RefPtr<JSC::ArrayBuffer> createResponseArrayBuffer();

    unsigned timeout() const { return m_timeoutMilliseconds; }
    ExceptionOr<void> setTimeout(unsigned);

    bool responseCacheIsValid() const { return m_responseCacheIsValid; }
    void didCacheResponse();

    enum class ResponseType { EmptyString, Arraybuffer, Blob, Document, Json, Text };
    ExceptionOr<void> setResponseType(ResponseType);
    ResponseType responseType() const;

    String responseURL() const;

    XMLHttpRequestUpload& upload();
    XMLHttpRequestUpload* optionalUpload() const { return m_upload.get(); }

    const ResourceResponse& resourceResponse() const { return m_response; }

    using RefCounted<XMLHttpRequest>::ref;
    using RefCounted<XMLHttpRequest>::deref;

private:
    explicit XMLHttpRequest(ScriptExecutionContext&);

    // ActiveDOMObject
    void contextDestroyed() override;
    bool canSuspendForDocumentSuspension() const override;
    void suspend(ReasonForSuspension) override;
    void resume() override;
    void stop() override;
    const char* activeDOMObjectName() const override;

    void refEventTarget() override { ref(); }
    void derefEventTarget() override { deref(); }

    Document* document() const;
    SecurityOrigin* securityOrigin() const;

#if ENABLE(DASHBOARD_SUPPORT)
    bool usesDashboardBackwardCompatibilityMode() const;
#endif

    // ThreadableLoaderClient
    void didSendData(unsigned long long bytesSent, unsigned long long totalBytesToBeSent) override;
    void didReceiveResponse(unsigned long identifier, const ResourceResponse&) override;
    void didReceiveData(const char* data, int dataLength) override;
    void didFinishLoading(unsigned long identifier) override;
    void didFail(const ResourceError&) override;

    bool responseIsXML() const;

    std::optional<ExceptionOr<void>> prepareToSend();
    ExceptionOr<void> send(Document&);
    ExceptionOr<void> send(const String& = { });
    ExceptionOr<void> send(Blob&);
    ExceptionOr<void> send(DOMFormData&);
    ExceptionOr<void> send(JSC::ArrayBuffer&);
    ExceptionOr<void> send(JSC::ArrayBufferView&);
    ExceptionOr<void> sendBytesData(const void*, size_t);

    void changeState(State);
    void callReadyStateChangeListener();
    void dropProtection();

    // Returns false when cancelling the loader within internalAbort() triggers an event whose callback creates a new loader. 
    // In that case, the function calling internalAbort should exit.
    bool internalAbort();

    void clearResponse();
    void clearResponseBuffers();
    void clearRequest();

    ExceptionOr<void> createRequest();

    void genericError();
    void networkError();
    void abortError();

    void dispatchErrorEvents(const AtomicString&);

    void resumeTimerFired();
    Ref<TextResourceDecoder> createDecoder() const;

    std::unique_ptr<XMLHttpRequestUpload> m_upload;

    URL m_url;
    String m_method;
    HTTPHeaderMap m_requestHeaders;
    RefPtr<FormData> m_requestEntityBody;
    String m_mimeTypeOverride;
    bool m_async { true };
    bool m_includeCredentials { false };

    RefPtr<ThreadableLoader> m_loader;
    State m_state { UNSENT };
    bool m_sendFlag { false };

    ResourceResponse m_response;
    String m_responseEncoding;

    RefPtr<TextResourceDecoder> m_decoder;

    StringBuilder m_responseBuilder;
    bool m_createdDocument { false };
    RefPtr<Document> m_responseDocument;

    RefPtr<SharedBuffer> m_binaryResponseBuilder;

    bool m_error { false };

    bool m_uploadListenerFlag { false };
    bool m_uploadComplete { false };

    bool m_wasAbortedByClient { false };

    // Used for progress event tracking.
    long long m_receivedLength { 0 };

    std::optional<ExceptionCode> m_exceptionCode;

    XMLHttpRequestProgressEventThrottle m_progressEventThrottle;

    ResponseType m_responseType { ResponseType::EmptyString };
    bool m_responseCacheIsValid { false };
    mutable String m_allResponseHeaders;

    Timer m_resumeTimer;
    bool m_dispatchErrorOnResuming { false };

    Timer m_networkErrorTimer;
    void networkErrorTimerFired();

    unsigned m_timeoutMilliseconds { 0 };
    MonotonicTime m_sendingTime;
    Timer m_timeoutTimer;
};

inline auto XMLHttpRequest::responseType() const -> ResponseType
{
    return m_responseType;
}

} // namespace WebCore
