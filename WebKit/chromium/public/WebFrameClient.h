/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebFrameClient_h
#define WebFrameClient_h

#include "WebCommon.h"
#include "WebFileSystem.h"
#include "WebNavigationPolicy.h"
#include "WebNavigationType.h"
#include "WebURLError.h"

namespace WebKit {

class WebApplicationCacheHost;
class WebApplicationCacheHostClient;
class WebCookieJar;
class WebDataSource;
class WebFormElement;
class WebFrame;
class WebMediaPlayer;
class WebMediaPlayerClient;
class WebNode;
class WebPlugin;
class WebSecurityOrigin;
class WebSharedWorker;
class WebString;
class WebURL;
class WebURLRequest;
class WebURLResponse;
class WebWorker;
class WebWorkerClient;
struct WebPluginParams;
struct WebRect;
struct WebSize;
struct WebURLError;

class WebFrameClient {
public:
    // Factory methods -----------------------------------------------------

    // May return null.
    virtual WebPlugin* createPlugin(WebFrame*, const WebPluginParams&) { return 0; }

    // May return null.
    virtual WebWorker* createWorker(WebFrame*, WebWorkerClient*) { return 0; }

    // May return null.
    virtual WebSharedWorker* createSharedWorker(WebFrame*, const WebURL&, const WebString&, unsigned long long) { return 0; }

    // May return null.
    virtual WebMediaPlayer* createMediaPlayer(WebFrame*, WebMediaPlayerClient*) { return 0; }

    // May return null.
    virtual WebApplicationCacheHost* createApplicationCacheHost(WebFrame*, WebApplicationCacheHostClient*) { return 0; }

    
    // Services ------------------------------------------------------------

    // A frame specific cookie jar.  May return null, in which case
    // WebKitClient::cookieJar() will be called to access cookies.
    virtual WebCookieJar* cookieJar() { return 0; }


    // General notifications -----------------------------------------------

    // This frame has been detached from the view.
    //
    // FIXME: Do not use this in new code. Currently this is used by code in
    // Chromium that errantly caches WebKit objects.
    virtual void frameDetached(WebFrame*) { }

    // This frame is about to be closed.
    virtual void willClose(WebFrame*) { }

    // Controls whether plugins are allowed for this frame.
    virtual bool allowPlugins(WebFrame*, bool enabledPerSettings) { return enabledPerSettings; }

    // Notifies the client that the frame would have instantiated a plug-in if plug-ins were enabled.
    virtual void didNotAllowPlugins(WebFrame*) { }

    // Controls whether images are allowed for this frame.
    virtual bool allowImages(WebFrame*, bool enabledPerSettings) { return enabledPerSettings; }


    // Load commands -------------------------------------------------------

    // The client should handle the navigation externally.
    virtual void loadURLExternally(
        WebFrame*, const WebURLRequest&, WebNavigationPolicy) { }


    // Navigational queries ------------------------------------------------

    // The client may choose to alter the navigation policy.  Otherwise,
    // defaultPolicy should just be returned.
    virtual WebNavigationPolicy decidePolicyForNavigation(
        WebFrame*, const WebURLRequest&, WebNavigationType,
        const WebNode& originatingNode,
        WebNavigationPolicy defaultPolicy, bool isRedirect) { return defaultPolicy; }

    // Query if the specified request can be handled.
    virtual bool canHandleRequest(
        WebFrame*, const WebURLRequest& request) { return true; }

    // Returns an error corresponding to canHandledRequest() returning false.
    virtual WebURLError cannotHandleRequestError(
        WebFrame*, const WebURLRequest& request) { return WebURLError(); }

    // Returns an error corresponding to a user cancellation event.
    virtual WebURLError cancelledError(
        WebFrame*, const WebURLRequest& request) { return WebURLError(); }

    // Notify that a URL cannot be handled.
    virtual void unableToImplementPolicyWithError(
        WebFrame*, const WebURLError&) { }


    // Navigational notifications ------------------------------------------

    // A form submission has been requested, but the page's submit event handler
    // hasn't yet had a chance to run (and possibly alter/interrupt the submit.)
    virtual void willSendSubmitEvent(WebFrame*, const WebFormElement&) { }

    // A form submission is about to occur.
    virtual void willSubmitForm(WebFrame*, const WebFormElement&) { }

    // A client-side redirect will occur.  This may correspond to a <META
    // refresh> or some script activity.
    virtual void willPerformClientRedirect(
        WebFrame*, const WebURL& from, const WebURL& to,
        double interval, double fireTime) { }

    // A client-side redirect was cancelled.
    virtual void didCancelClientRedirect(WebFrame*) { }

    // A client-side redirect completed.
    virtual void didCompleteClientRedirect(WebFrame*, const WebURL& fromURL) { }

    // A datasource has been created for a new navigation.  The given
    // datasource will become the provisional datasource for the frame.
    virtual void didCreateDataSource(WebFrame*, WebDataSource*) { }

    // A new provisional load has been started.
    virtual void didStartProvisionalLoad(WebFrame*) { }

    // The provisional load was redirected via a HTTP 3xx response.
    virtual void didReceiveServerRedirectForProvisionalLoad(WebFrame*) { }

    // The provisional load failed.
    virtual void didFailProvisionalLoad(WebFrame*, const WebURLError&) { }

    // Notifies the client to commit data for the given frame.  The client
    // may optionally prevent default processing by setting preventDefault
    // to true before returning.  If default processing is prevented, then
    // it is up to the client to manually call commitDocumentData on the
    // WebFrame.  It is only valid to call commitDocumentData within a call
    // to didReceiveDocumentData.  If commitDocumentData is not called,
    // then an empty document will be loaded.
    virtual void didReceiveDocumentData(
        WebFrame*, const char* data, size_t length, bool& preventDefault) { }

    // The provisional datasource is now committed.  The first part of the
    // response body has been received, and the encoding of the response
    // body is known.
    virtual void didCommitProvisionalLoad(WebFrame*, bool isNewNavigation) { }

    // The window object for the frame has been cleared of any extra
    // properties that may have been set by script from the previously
    // loaded document.
    virtual void didClearWindowObject(WebFrame*) { }

    // The document element has been created.
    virtual void didCreateDocumentElement(WebFrame*) { }

    // The page title is available.
    virtual void didReceiveTitle(WebFrame*, const WebString& title) { }

    // The icons for the page have changed.
    virtual void didChangeIcons(WebFrame*) { }

    // The frame's document finished loading.
    virtual void didFinishDocumentLoad(WebFrame*) { }

    // The 'load' event was dispatched.
    virtual void didHandleOnloadEvents(WebFrame*) { }

    // The frame's document or one of its subresources failed to load.
    virtual void didFailLoad(WebFrame*, const WebURLError&) { }

    // The frame's document and all of its subresources succeeded to load.
    virtual void didFinishLoad(WebFrame*) { }

    // The navigation resulted in no change to the documents within the page.
    // For example, the navigation may have just resulted in scrolling to a
    // named anchor or a PopState event may have been dispatched.
    virtual void didNavigateWithinPage(WebFrame*, bool isNewNavigation) { }

    // The navigation resulted in scrolling the page to a named anchor instead
    // of downloading a new document.
    virtual void didChangeLocationWithinPage(WebFrame*) { }

    // Called upon update to scroll position, document state, and other
    // non-navigational events related to the data held by WebHistoryItem.
    // WARNING: This method may be called very frequently.
    virtual void didUpdateCurrentHistoryItem(WebFrame*) { }


    // Low-level resource notifications ------------------------------------

    // An identifier was assigned to the specified request.  The client
    // should remember this association if interested in subsequent events.
    virtual void assignIdentifierToRequest(
        WebFrame*, unsigned identifier, const WebURLRequest&) { }

    // A request is about to be sent out, and the client may modify it.  Request
    // is writable, and changes to the URL, for example, will change the request
    // made.  If this request is the result of a redirect, then redirectResponse
    // will be non-null and contain the response that triggered the redirect.
    virtual void willSendRequest(
        WebFrame*, unsigned identifier, WebURLRequest&,
        const WebURLResponse& redirectResponse) { }

    // Response headers have been received for the resource request given
    // by identifier.
    virtual void didReceiveResponse(
        WebFrame*, unsigned identifier, const WebURLResponse&) { }

    // The resource request given by identifier succeeded.
    virtual void didFinishResourceLoad(
        WebFrame*, unsigned identifier) { }

    // The resource request given by identifier failed.
    virtual void didFailResourceLoad(
        WebFrame*, unsigned identifier, const WebURLError&) { }

    // The specified request was satified from WebCore's memory cache.
    virtual void didLoadResourceFromMemoryCache(
        WebFrame*, const WebURLRequest&, const WebURLResponse&) { }

    // This frame has displayed inactive content (such as an image) from an
    // insecure source.  Inactive content cannot spread to other frames.
    virtual void didDisplayInsecureContent(WebFrame*) { }

    // The indicated security origin has run active content (such as a
    // script) from an insecure source.  Note that the insecure content can
    // spread to other frames in the same origin.
    virtual void didRunInsecureContent(WebFrame*, const WebSecurityOrigin&) { }


    // Script notifications ------------------------------------------------

    // Controls whether scripts are allowed to execute for this frame.
    virtual bool allowScript(WebFrame*, bool enabledPerSettings) { return enabledPerSettings; }

    // Controls whether access to Web Databases is allowed for this frame.
    virtual bool allowDatabase(WebFrame*, const WebString& name, const WebString& displayName, unsigned long estimatedSize) { return true; }

    // Notifies the client that the frame would have executed script if script were enabled.
    virtual void didNotAllowScript(WebFrame*) { }

    // Script in the page tried to allocate too much memory.
    virtual void didExhaustMemoryAvailableForScript(WebFrame*) { }

    // Notifies that a new script context has been created for this frame.
    // This is similar to didClearWindowObject but only called once per
    // frame context.
    virtual void didCreateScriptContext(WebFrame*) { }

    // Notifies that this frame's script context has been destroyed.
    virtual void didDestroyScriptContext(WebFrame*) { }

    // Notifies that a garbage-collected context was created - content
    // scripts.
    virtual void didCreateIsolatedScriptContext(WebFrame*) { }


    // Geometry notifications ----------------------------------------------

    // The frame's document finished the initial layout of a page.
    virtual void didFirstLayout(WebFrame*) { }

    // The frame's document finished the initial non-empty layout of a page.
    virtual void didFirstVisuallyNonEmptyLayout(WebFrame*) { }

    // The size of the content area changed.
    virtual void didChangeContentsSize(WebFrame*, const WebSize&) { }

    // The main frame scrolled.
    virtual void didChangeScrollOffset(WebFrame*) { }


    // Find-in-page notifications ------------------------------------------

    // Notifies how many matches have been found so far, for a given
    // identifier.  |finalUpdate| specifies whether this is the last update
    // (all frames have completed scoping).
    virtual void reportFindInPageMatchCount(
        int identifier, int count, bool finalUpdate) { }

    // Notifies what tick-mark rect is currently selected.   The given
    // identifier lets the client know which request this message belongs
    // to, so that it can choose to ignore the message if it has moved on
    // to other things.  The selection rect is expected to have coordinates
    // relative to the top left corner of the web page area and represent
    // where on the screen the selection rect is currently located.
    virtual void reportFindInPageSelection(
        int identifier, int activeMatchOrdinal, const WebRect& selection) { }

    // FileSystem ----------------------------------------------------

    // Requests to open a FileSystem.
    // |size| indicates how much storage space (in bytes) the caller expects
    // to need.
    // WebFileSystemCallbacks::didOpenFileSystem() must be called with
    // a name and root path for the requested FileSystem when the operation
    // is completed successfully. WebFileSystemCallbacks::didFail() must be
    // called otherwise.
    virtual void openFileSystem(
        WebFrame*, WebFileSystem::Type, long long size,
        WebFileSystemCallbacks*) { }

protected:
    ~WebFrameClient() { }
};

} // namespace WebKit

#endif
