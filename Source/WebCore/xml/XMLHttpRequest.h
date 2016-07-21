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
#include "FormData.h"
#include "ResourceResponse.h"
#include "ThreadableLoaderClient.h"
#include "XMLHttpRequestEventTarget.h"
#include "XMLHttpRequestProgressEventThrottle.h"
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

    const URL& url() const { return m_url; }
    String statusText() const;
    int status() const;
    State readyState() const;
    bool withCredentials() const { return m_includeCredentials; }
    void setWithCredentials(bool, ExceptionCode&);
    void open(const String& method, const String& url, ExceptionCode&);
    void open(const String& method, const URL&, bool async, ExceptionCode&);
    void open(const String& method, const String&, bool async, const String& user, const String& password, ExceptionCode&);
    void send(ExceptionCode&);
    void send(Document*, ExceptionCode&);
    void send(const String&, ExceptionCode&);
    void send(Blob*, ExceptionCode&);
    void send(DOMFormData*, ExceptionCode&);
    void send(JSC::ArrayBuffer*, ExceptionCode&);
    void send(JSC::ArrayBufferView*, ExceptionCode&);
    void abort();
    void setRequestHeader(const String& name, const String& value, ExceptionCode&);
    void overrideMimeType(const String& override, ExceptionCode&);
    bool doneWithoutErrors() const { return !m_error && m_state == DONE; }
    String getAllResponseHeaders() const;
    String getResponseHeader(const String& name) const;
    String responseText(ExceptionCode&);
    String responseTextIgnoringResponseType() const { return m_responseBuilder.toStringPreserveCapacity(); }
    String responseMIMEType() const;

    Document* optionalResponseXML() const { return m_responseDocument.get(); }
    Document* responseXML(ExceptionCode&);

    Ref<Blob> createResponseBlob();
    RefPtr<JSC::ArrayBuffer> createResponseArrayBuffer();

    unsigned timeout() const { return m_timeoutMilliseconds; }
    void setTimeout(unsigned timeout, ExceptionCode&);

    bool responseCacheIsValid() const { return m_responseCacheIsValid; }
    void didCacheResponse();

    // Expose HTTP validation methods for other untrusted requests.
    static bool isAllowedHTTPMethod(const String&);
    static String uppercaseKnownHTTPMethod(const String&);
    static bool isAllowedHTTPHeader(const String&);

    enum class ResponseType { EmptyString, Arraybuffer, Blob, Document, Json, Text };
    void setResponseType(ResponseType, ExceptionCode&);
    ResponseType responseType() const;

    String responseURL() const;

    void setLastSendLineAndColumnNumber(unsigned lineNumber, unsigned columnNumber);
    void setLastSendURL(const String& url) { m_lastSendURL = url; }

    XMLHttpRequestUpload* upload();
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

    void didSendData(unsigned long long bytesSent, unsigned long long totalBytesToBeSent) override;
    void didReceiveResponse(unsigned long identifier, const ResourceResponse&) override;
    void didReceiveData(const char* data, int dataLength) override;
    void didFinishLoading(unsigned long identifier, double finishTime) override;
    void didFail(const ResourceError&) override;

    bool responseIsXML() const;

    bool initSend(ExceptionCode&);
    void sendBytesData(const void*, size_t, ExceptionCode&);

    void changeState(State newState);
    void callReadyStateChangeListener();
    void dropProtection();

    // Returns false when cancelling the loader within internalAbort() triggers an event whose callback creates a new loader. 
    // In that case, the function calling internalAbort should exit.
    bool internalAbort();

    void clearResponse();
    void clearResponseBuffers();
    void clearRequest();

    void createRequest(ExceptionCode&);

    void genericError();
    void networkError();
    void abortError();

    void dispatchErrorEvents(const AtomicString&);

    void resumeTimerFired();

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

    bool m_uploadEventsAllowed { true };
    bool m_uploadComplete { false };

    bool m_sameOriginRequest { true };

    // Used for progress event tracking.
    long long m_receivedLength { 0 };

    unsigned m_lastSendLineNumber { 0 };
    unsigned m_lastSendColumnNumber { 0 };
    String m_lastSendURL;
    ExceptionCode m_exceptionCode { 0 };

    XMLHttpRequestProgressEventThrottle m_progressEventThrottle;

    ResponseType m_responseType { ResponseType::EmptyString };
    bool m_responseCacheIsValid { false };

    Timer m_resumeTimer;
    bool m_dispatchErrorOnResuming { false };

    Timer m_networkErrorTimer;
    void networkErrorTimerFired();

    unsigned m_timeoutMilliseconds { 0 };
    std::chrono::steady_clock::time_point m_sendingTime;
    Timer m_timeoutTimer;
};

inline auto XMLHttpRequest::responseType() const -> ResponseType
{
    return m_responseType;
}

} // namespace WebCore
