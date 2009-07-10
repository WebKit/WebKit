/*
 *  Copyright (C) 2003, 2006, 2008 Apple Inc. All rights reserved.
 *  Copyright (C) 2005, 2006 Alexey Proskuryakov <ap@nypop.com>
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
#include "AtomicStringHash.h"
#include "EventListener.h"
#include "EventTarget.h"
#include "FormData.h"
#include "ResourceResponse.h"
#include "ScriptString.h"
#include "ThreadableLoaderClient.h"
#include <wtf/OwnPtr.h>

namespace WebCore {

class Document;
class File;
struct ResourceRequest;
class TextResourceDecoder;
class ThreadableLoader;

class XMLHttpRequest : public RefCounted<XMLHttpRequest>, public EventTarget, private ThreadableLoaderClient, public ActiveDOMObject {
public:
    static PassRefPtr<XMLHttpRequest> create(ScriptExecutionContext* context) { return adoptRef(new XMLHttpRequest(context)); }
    ~XMLHttpRequest();

    // These exact numeric values are important because JS expects them.
    enum State {
        UNSENT = 0,
        OPENED = 1,
        HEADERS_RECEIVED = 2,
        LOADING = 3,
        DONE = 4
    };

    virtual XMLHttpRequest* toXMLHttpRequest() { return this; }

    virtual void contextDestroyed();
    virtual bool canSuspend() const;
    virtual void stop();

    virtual ScriptExecutionContext* scriptExecutionContext() const;

    String statusText(ExceptionCode&) const;
    int status(ExceptionCode&) const;
    State readyState() const;
    bool withCredentials() const { return m_includeCredentials; }
    void setWithCredentials(bool, ExceptionCode&);
    void open(const String& method, const KURL&, bool async, ExceptionCode&);
    void open(const String& method, const KURL&, bool async, const String& user, ExceptionCode&);
    void open(const String& method, const KURL&, bool async, const String& user, const String& password, ExceptionCode&);
    void send(ExceptionCode&);
    void send(Document*, ExceptionCode&);
    void send(const String&, ExceptionCode&);
    void send(File*, ExceptionCode&);
    void abort();
    void setRequestHeader(const AtomicString& name, const String& value, ExceptionCode&);
    void overrideMimeType(const String& override);
    String getAllResponseHeaders(ExceptionCode&) const;
    String getResponseHeader(const AtomicString& name, ExceptionCode&) const;
    const ScriptString& responseText() const;
    Document* responseXML() const;
    void setLastSendLineNumber(unsigned lineNumber) { m_lastSendLineNumber = lineNumber; }
    void setLastSendURL(const String& url) { m_lastSendURL = url; }

    XMLHttpRequestUpload* upload();
    XMLHttpRequestUpload* optionalUpload() const { return m_upload.get(); }

    void setOnreadystatechange(PassRefPtr<EventListener> eventListener) { m_onReadyStateChangeListener = eventListener; }
    EventListener* onreadystatechange() const { return m_onReadyStateChangeListener.get(); }

    void setOnabort(PassRefPtr<EventListener> eventListener) { m_onAbortListener = eventListener; }
    EventListener* onabort() const { return m_onAbortListener.get(); }

    void setOnerror(PassRefPtr<EventListener> eventListener) { m_onErrorListener = eventListener; }
    EventListener* onerror() const { return m_onErrorListener.get(); }

    void setOnload(PassRefPtr<EventListener> eventListener) { m_onLoadListener = eventListener; }
    EventListener* onload() const { return m_onLoadListener.get(); }

    void setOnloadstart(PassRefPtr<EventListener> eventListener) { m_onLoadStartListener = eventListener; }
    EventListener* onloadstart() const { return m_onLoadStartListener.get(); }

    void setOnprogress(PassRefPtr<EventListener> eventListener) { m_onProgressListener = eventListener; }
    EventListener* onprogress() const { return m_onProgressListener.get(); }

    typedef Vector<RefPtr<EventListener> > ListenerVector;
    typedef HashMap<AtomicString, ListenerVector> EventListenersMap;

    // useCapture is not used, even for add/remove pairing (for Firefox compatibility).
    virtual void addEventListener(const AtomicString& eventType, PassRefPtr<EventListener>, bool useCapture);
    virtual void removeEventListener(const AtomicString& eventType, EventListener*, bool useCapture);
    virtual bool dispatchEvent(PassRefPtr<Event>, ExceptionCode&);
    EventListenersMap& eventListeners() { return m_eventListeners; }

    using RefCounted<XMLHttpRequest>::ref;
    using RefCounted<XMLHttpRequest>::deref;

private:
    XMLHttpRequest(ScriptExecutionContext*);
    
    virtual void refEventTarget() { ref(); }
    virtual void derefEventTarget() { deref(); }

    Document* document() const;

#if ENABLE(DASHBOARD_SUPPORT)
    bool usesDashboardBackwardCompatibilityMode() const;
#endif

    virtual void didSendData(unsigned long long bytesSent, unsigned long long totalBytesToBeSent);
    virtual void didReceiveResponse(const ResourceResponse&);
    virtual void didReceiveData(const char* data, int lengthReceived);
    virtual void didFinishLoading(unsigned long identifier);
    virtual void didFail(const ResourceError&);
    virtual void didFailRedirectCheck();
    virtual void didReceiveAuthenticationCancellation(const ResourceResponse&);

    // Special versions for the preflight
    void didReceiveResponsePreflight(const ResourceResponse&);
    void didFinishLoadingPreflight();

    void updateAndDispatchOnProgress(unsigned int len);

    String responseMIMEType() const;
    bool responseIsXML() const;

    bool initSend(ExceptionCode&);

    String getRequestHeader(const AtomicString& name) const;
    void setRequestHeaderInternal(const AtomicString& name, const String& value);
    bool isSafeRequestHeader(const String&) const;

    void changeState(State newState);
    void callReadyStateChangeListener();
    void dropProtection();
    void internalAbort();
    void clearResponse();
    void clearRequest();

    void createRequest(ExceptionCode&);

    void makeSameOriginRequest(ExceptionCode&);
    void makeCrossOriginAccessRequest(ExceptionCode&);

    void makeSimpleCrossOriginAccessRequest(ExceptionCode&);
    void makeCrossOriginAccessRequestWithPreflight(ExceptionCode&);
    void handleAsynchronousPreflightResult();

    void loadRequestSynchronously(ResourceRequest&, ExceptionCode&);
    void loadRequestAsynchronously(ResourceRequest&);

    void genericError();
    void networkError();
    void abortError();

    void dispatchReadyStateChangeEvent();
    void dispatchXMLHttpRequestProgressEvent(EventListener* listener, const AtomicString& type, bool lengthComputable, unsigned loaded, unsigned total);
    void dispatchAbortEvent();
    void dispatchErrorEvent();
    void dispatchLoadEvent();
    void dispatchLoadStartEvent();
    void dispatchProgressEvent(long long expectedLength);

    RefPtr<EventListener> m_onReadyStateChangeListener;
    RefPtr<EventListener> m_onAbortListener;
    RefPtr<EventListener> m_onErrorListener;
    RefPtr<EventListener> m_onLoadListener;
    RefPtr<EventListener> m_onLoadStartListener;
    RefPtr<EventListener> m_onProgressListener;
    EventListenersMap m_eventListeners;

    RefPtr<XMLHttpRequestUpload> m_upload;

    KURL m_url;
    String m_method;
    HTTPHeaderMap m_requestHeaders;
    RefPtr<FormData> m_requestEntityBody;
    String m_mimeTypeOverride;
    bool m_async;
    bool m_includeCredentials;

    RefPtr<ThreadableLoader> m_loader;
    State m_state;

    ResourceResponse m_response;
    String m_responseEncoding;

    RefPtr<TextResourceDecoder> m_decoder;

    // Unlike most strings in the DOM, we keep this as a ScriptString, not a WebCore::String.
    // That's because these strings can easily get huge (they are filled from the network with
    // no parsing) and because JS can easily observe many intermediate states, so it's very useful
    // to be able to share the buffer with JavaScript versions of the whole or partial string.
    // In contrast, this string doesn't interact much with the rest of the engine so it's not that
    // big a cost that it isn't a String.
    ScriptString m_responseText;
    mutable bool m_createdDocument;
    mutable RefPtr<Document> m_responseXML;

    bool m_error;

    bool m_uploadEventsAllowed;
    bool m_uploadComplete;

    bool m_sameOriginRequest;
    bool m_allowAccess;
    bool m_inPreflight;
    bool m_didTellLoaderAboutRequest;

    // Used for onprogress tracking
    long long m_receivedLength;
    
    unsigned m_lastSendLineNumber;
    String m_lastSendURL;
    ExceptionCode m_exceptionCode;
};

} // namespace WebCore

#endif // XMLHttpRequest_h
