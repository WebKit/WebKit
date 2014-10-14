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

#ifndef XMLHttpRequest_h
#define XMLHttpRequest_h

#include "ActiveDOMObject.h"
#include "EventListener.h"
#include "EventTarget.h"
#include "FormData.h"
#include "ResourceResponse.h"
#include "ScriptWrappable.h"
#include "ThreadableLoaderClient.h"
#include "XMLHttpRequestProgressEventThrottle.h"
#include <wtf/text/AtomicStringHash.h>
#include <wtf/text/StringBuilder.h>

namespace JSC {
class ArrayBuffer;
class ArrayBufferView;
}

namespace WebCore {

class Blob;
class Document;
class DOMFormData;
class ResourceRequest;
class SecurityOrigin;
class SharedBuffer;
class TextResourceDecoder;
class ThreadableLoader;

class XMLHttpRequest final : public ScriptWrappable, public RefCounted<XMLHttpRequest>, public EventTargetWithInlineData, private ThreadableLoaderClient, public ActiveDOMObject {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static PassRefPtr<XMLHttpRequest> create(ScriptExecutionContext&);
    ~XMLHttpRequest();

    // These exact numeric values are important because JS expects them.
    enum State {
        UNSENT = 0,
        OPENED = 1,
        HEADERS_RECEIVED = 2,
        LOADING = 3,
        DONE = 4
    };
    
    enum ResponseTypeCode {
        ResponseTypeDefault,
        ResponseTypeText,
        ResponseTypeJSON,
        ResponseTypeDocument,

        // Binary format
        ResponseTypeBlob,
        ResponseTypeArrayBuffer
    };
    static const ResponseTypeCode FirstBinaryResponseType = ResponseTypeBlob;

#if ENABLE(XHR_TIMEOUT)
    virtual void didTimeout();
#endif

    virtual EventTargetInterface eventTargetInterface() const override { return XMLHttpRequestEventTargetInterfaceType; }
    virtual ScriptExecutionContext* scriptExecutionContext() const override { return ActiveDOMObject::scriptExecutionContext(); }

    const URL& url() const { return m_url; }
    String statusText() const;
    int status() const;
    State readyState() const;
    bool withCredentials() const { return m_includeCredentials; }
    void setWithCredentials(bool, ExceptionCode&);
    void open(const String& method, const URL&, ExceptionCode&);
    void open(const String& method, const URL&, bool async, ExceptionCode&);
    void open(const String& method, const URL&, bool async, const String& user, ExceptionCode&);
    void open(const String& method, const URL&, bool async, const String& user, const String& password, ExceptionCode&);
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
    Document* responseXML(ExceptionCode&);
    Document* optionalResponseXML() const { return m_responseDocument.get(); }
    Blob* responseBlob();
    Blob* optionalResponseBlob() const { return m_responseBlob.get(); }
#if ENABLE(XHR_TIMEOUT)
    unsigned long timeout() const { return m_timeoutMilliseconds; }
    void setTimeout(unsigned long timeout, ExceptionCode&);
#endif

    bool responseCacheIsValid() const { return m_responseCacheIsValid; }
    void didCacheResponseJSON();

    void sendForInspectorXHRReplay(PassRefPtr<FormData>, ExceptionCode&);

    // Expose HTTP validation methods for other untrusted requests.
    static bool isAllowedHTTPMethod(const String&);
    static String uppercaseKnownHTTPMethod(const String&);
    static bool isAllowedHTTPHeader(const String&);

    void setResponseType(const String&, ExceptionCode&);
    String responseType();
    ResponseTypeCode responseTypeCode() const { return m_responseTypeCode; }

    // response attribute has custom getter.
    JSC::ArrayBuffer* responseArrayBuffer();
    JSC::ArrayBuffer* optionalResponseArrayBuffer() const { return m_responseArrayBuffer.get(); }

    void setLastSendLineAndColumnNumber(unsigned lineNumber, unsigned columnNumber);
    void setLastSendURL(const String& url) { m_lastSendURL = url; }

    XMLHttpRequestUpload* upload();
    XMLHttpRequestUpload* optionalUpload() const { return m_upload.get(); }

    DEFINE_ATTRIBUTE_EVENT_LISTENER(readystatechange);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(abort);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(error);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(load);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(loadend);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(loadstart);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(progress);
#if ENABLE(XHR_TIMEOUT)
    DEFINE_ATTRIBUTE_EVENT_LISTENER(timeout);
#endif

    using RefCounted<XMLHttpRequest>::ref;
    using RefCounted<XMLHttpRequest>::deref;

private:
    explicit XMLHttpRequest(ScriptExecutionContext&);

    // ActiveDOMObject
    virtual void contextDestroyed() override;
    virtual bool canSuspend() const override;
    virtual void suspend(ReasonForSuspension) override;
    virtual void resume() override;
    virtual void stop() override;

    virtual void refEventTarget() override { ref(); }
    virtual void derefEventTarget() override { deref(); }

    Document* document() const;
    SecurityOrigin* securityOrigin() const;

#if ENABLE(DASHBOARD_SUPPORT)
    bool usesDashboardBackwardCompatibilityMode() const;
#endif

    virtual void didSendData(unsigned long long bytesSent, unsigned long long totalBytesToBeSent) override;
    virtual void didReceiveResponse(unsigned long identifier, const ResourceResponse&) override;
    virtual void didReceiveData(const char* data, int dataLength) override;
    virtual void didFinishLoading(unsigned long identifier, double finishTime) override;
    virtual void didFail(const ResourceError&) override;
    virtual void didFailRedirectCheck() override;

    bool responseIsXML() const;

    bool initSend(ExceptionCode&);
    void sendBytesData(const void*, size_t, ExceptionCode&);

    String getRequestHeader(const String& name) const;
    void setRequestHeaderInternal(const String& name, const String& value);

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

    bool shouldDecodeResponse() const { return m_responseTypeCode < FirstBinaryResponseType; }

    void dispatchErrorEvents(const AtomicString&);

    std::unique_ptr<XMLHttpRequestUpload> m_upload;

    URL m_url;
    String m_method;
    HTTPHeaderMap m_requestHeaders;
    RefPtr<FormData> m_requestEntityBody;
    String m_mimeTypeOverride;
    bool m_async;
    bool m_includeCredentials;
#if ENABLE(XHR_TIMEOUT)
    unsigned long m_timeoutMilliseconds;
#endif
    RefPtr<Blob> m_responseBlob;

    RefPtr<ThreadableLoader> m_loader;
    State m_state;

    ResourceResponse m_response;
    String m_responseEncoding;

    RefPtr<TextResourceDecoder> m_decoder;

    StringBuilder m_responseBuilder;
    bool m_createdDocument;
    RefPtr<Document> m_responseDocument;
    
    RefPtr<SharedBuffer> m_binaryResponseBuilder;
    RefPtr<JSC::ArrayBuffer> m_responseArrayBuffer;

    bool m_error;

    bool m_uploadEventsAllowed;
    bool m_uploadComplete;

    bool m_sameOriginRequest;

    // Used for onprogress tracking
    long long m_receivedLength;

    unsigned m_lastSendLineNumber;
    unsigned m_lastSendColumnNumber;
    String m_lastSendURL;
    ExceptionCode m_exceptionCode;

    XMLHttpRequestProgressEventThrottle m_progressEventThrottle;

    // An enum corresponding to the allowed string values for the responseType attribute.
    ResponseTypeCode m_responseTypeCode;
    bool m_responseCacheIsValid;
};

} // namespace WebCore

#endif // XMLHttpRequest_h
