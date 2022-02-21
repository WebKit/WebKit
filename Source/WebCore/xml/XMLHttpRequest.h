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
#include "BlobURL.h"
#include "ExceptionOr.h"
#include "FormData.h"
#include "ResourceResponse.h"
#include "SharedBuffer.h"
#include "ThreadableLoaderClient.h"
#include "UserGestureIndicator.h"
#include <wtf/URL.h>
#include "XMLHttpRequestEventTarget.h"
#include "XMLHttpRequestProgressEventThrottle.h"
#include <variant>
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
class TextResourceDecoder;
class ThreadableLoader;
class URLSearchParams;
class XMLHttpRequestUpload;
struct OwnedString;

class XMLHttpRequest final : public ActiveDOMObject, public RefCounted<XMLHttpRequest>, private ThreadableLoaderClient, public XMLHttpRequestEventTarget {
    WTF_MAKE_ISO_ALLOCATED(XMLHttpRequest);
public:
    static Ref<XMLHttpRequest> create(ScriptExecutionContext&);
    WEBCORE_EXPORT ~XMLHttpRequest();

    // Keep it in 3bits.
    enum State : uint8_t {
        UNSENT = 0,
        OPENED = 1,
        HEADERS_RECEIVED = 2,
        LOADING = 3,
        DONE = 4
    };

    virtual void didReachTimeout();

    EventTargetInterface eventTargetInterface() const override { return XMLHttpRequestEventTargetInterfaceType; }
    ScriptExecutionContext* scriptExecutionContext() const override { return ActiveDOMObject::scriptExecutionContext(); }

    using SendTypes = std::variant<RefPtr<Document>, RefPtr<Blob>, RefPtr<JSC::ArrayBufferView>, RefPtr<JSC::ArrayBuffer>, RefPtr<DOMFormData>, String, RefPtr<URLSearchParams>>;

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
    bool doneWithoutErrors() const { return !m_error && readyState() == DONE; }
    String getAllResponseHeaders() const;
    String getResponseHeader(const String& name) const;
    ExceptionOr<OwnedString> responseText();
    String responseTextIgnoringResponseType() const { return m_responseBuilder.toStringPreserveCapacity(); }
    enum class FinalMIMEType { Yes, No };
    String responseMIMEType(FinalMIMEType = FinalMIMEType::No) const;

    Document* optionalResponseXML() const { return m_responseDocument.get(); }
    ExceptionOr<Document*> responseXML();

    Ref<Blob> createResponseBlob();
    RefPtr<JSC::ArrayBuffer> createResponseArrayBuffer();

    unsigned timeout() const { return m_timeoutMilliseconds; }
    ExceptionOr<void> setTimeout(unsigned);

    bool responseCacheIsValid() const { return m_responseCacheIsValid; }
    void didCacheResponse();

    // Keep it in 3bits.
    enum class ResponseType : uint8_t {
        EmptyString = 0,
        Arraybuffer = 1,
        Blob = 2,
        Document = 3,
        Json = 4,
        Text = 5,
    };
    ExceptionOr<void> setResponseType(ResponseType);
    ResponseType responseType() const;

    String responseURL() const;

    XMLHttpRequestUpload& upload();
    XMLHttpRequestUpload* optionalUpload() const { return m_upload.get(); }

    const ResourceResponse& resourceResponse() const { return m_response; }

    using RefCounted<XMLHttpRequest>::ref;
    using RefCounted<XMLHttpRequest>::deref;

    size_t memoryCost() const;

    using EventTarget::dispatchEvent;
    void dispatchEvent(Event&) override;

private:
    friend class XMLHttpRequestUpload;
    explicit XMLHttpRequest(ScriptExecutionContext&);

    void updateHasRelevantEventListener();

    // EventTarget.
    void eventListenersDidChange() final;

    PAL::TextEncoding finalResponseCharset() const;

    // ActiveDOMObject
    void contextDestroyed() override;
    void suspend(ReasonForSuspension) override;
    void resume() override;
    void stop() override;
    const char* activeDOMObjectName() const override;
    bool virtualHasPendingActivity() const final;

    void refEventTarget() override { ref(); }
    void derefEventTarget() override { deref(); }

    Document* document() const;
    SecurityOrigin* securityOrigin() const;

    // ThreadableLoaderClient
    void didSendData(unsigned long long bytesSent, unsigned long long totalBytesToBeSent) override;
    void didReceiveResponse(ResourceLoaderIdentifier, const ResourceResponse&) override;
    void didReceiveData(const SharedBuffer&) override;
    void didFinishLoading(ResourceLoaderIdentifier, const NetworkLoadMetrics&) override;
    void didFail(const ResourceError&) override;
    void notifyIsDone(bool) final;

    std::optional<ExceptionOr<void>> prepareToSend();
    ExceptionOr<void> send(const URLSearchParams&);
    ExceptionOr<void> send(Document&);
    ExceptionOr<void> send(const String& = { });
    ExceptionOr<void> send(Blob&);
    ExceptionOr<void> send(DOMFormData&);
    ExceptionOr<void> send(JSC::ArrayBuffer&);
    ExceptionOr<void> send(JSC::ArrayBufferView&);
    ExceptionOr<void> sendBytesData(const void*, size_t);

    void changeState(State);
    void callReadyStateChangeListener();

    // Returns false when cancelling the loader within internalAbort() triggers an event whose callback creates a new loader. 
    // In that case, the function calling internalAbort should exit.
    bool internalAbort();

    void clearResponse();
    void clearResponseBuffers();
    void clearRequest();

    ExceptionOr<void> createRequest();

    void timeoutTimerFired();

    void genericError();
    void networkError();
    void abortError();

    void dispatchErrorEvents(const AtomString&);

    Ref<TextResourceDecoder> createDecoder() const;

    unsigned m_async : 1;
    unsigned m_includeCredentials : 1;
    unsigned m_sendFlag : 1;
    unsigned m_createdDocument : 1;
    unsigned m_error : 1;
    unsigned m_uploadListenerFlag : 1;
    unsigned m_uploadComplete : 1;
    unsigned m_wasAbortedByClient : 1;
    unsigned m_responseCacheIsValid : 1;
    unsigned m_readyState : 3; // State
    unsigned m_responseType : 3; // ResponseType

    unsigned m_timeoutMilliseconds { 0 };

    std::unique_ptr<XMLHttpRequestUpload> m_upload;

    URL m_url;
    BlobURLHandle m_blobURLLifetimeExtension;
    String m_method;
    HTTPHeaderMap m_requestHeaders;
    RefPtr<FormData> m_requestEntityBody;
    String m_mimeTypeOverride;

    struct LoadingActivity {
        Ref<XMLHttpRequest> protectedThis; // Keep object alive while loading even if there is no longer a JS wrapper.
        Ref<ThreadableLoader> loader;
    };
    std::optional<LoadingActivity> m_loadingActivity;

    String m_responseEncoding;

    ResourceResponse m_response;

    RefPtr<TextResourceDecoder> m_decoder;

    RefPtr<Document> m_responseDocument;

    SharedBufferBuilder m_binaryResponseBuilder;

    StringBuilder m_responseBuilder;

    // Used for progress event tracking.
    long long m_receivedLength { 0 };

    XMLHttpRequestProgressEventThrottle m_progressEventThrottle;

    mutable String m_allResponseHeaders;

    Timer m_timeoutTimer;

    MonotonicTime m_sendingTime;

    std::optional<ExceptionCode> m_exceptionCode;
    RefPtr<UserGestureToken> m_userGestureToken;
    std::atomic<bool> m_hasRelevantEventListener;
};

inline auto XMLHttpRequest::responseType() const -> ResponseType
{
    return static_cast<ResponseType>(m_responseType);
}

inline auto XMLHttpRequest::readyState() const -> State
{
    return static_cast<State>(m_readyState);
}

} // namespace WebCore
