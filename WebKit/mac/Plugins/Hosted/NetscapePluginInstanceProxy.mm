/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#if USE(PLUGIN_HOST_PROCESS)

#import "NetscapePluginInstanceProxy.h"

#import "NetscapePluginHostProxy.h"
#import "WebDataSourceInternal.h"
#import "WebFrameInternal.h"
#import "WebHostedNetscapePluginView.h"
#import "WebNSURLExtras.h"
#import "WebKitNSStringExtras.h"
#import "WebKitPluginHost.h"
#import "WebPluginRequest.h"
#import "WebViewInternal.h"
#import "WebUIDelegate.h"
#import "WebUIDelegatePrivate.h"

#import <WebCore/DocumentLoader.h>
#import <WebCore/Frame.h>
#import <WebCore/FrameLoader.h>
#import <WebCore/FrameTree.h>

using namespace WebCore;

namespace WebKit {

NetscapePluginInstanceProxy::NetscapePluginInstanceProxy(NetscapePluginHostProxy* pluginHostProxy, WebHostedNetscapePluginView *pluginView, uint32_t pluginID, uint32_t renderContextID, boolean_t useSoftwareRenderer)
    : m_pluginHostProxy(pluginHostProxy)
    , m_pluginView(pluginView)
    , m_requestTimer(this, &NetscapePluginInstanceProxy::requestTimerFired)
    , m_pluginID(pluginID)
    , m_renderContextID(renderContextID)
    , m_useSoftwareRenderer(useSoftwareRenderer)
{
    ASSERT(m_pluginView);
    
    pluginHostProxy->addPluginInstance(this);
}

NetscapePluginInstanceProxy::~NetscapePluginInstanceProxy()
{
    ASSERT(!m_pluginHostProxy);
}

void NetscapePluginInstanceProxy::resize(NSRect size, NSRect clipRect)
{
    _WKPHResizePluginInstance(m_pluginHostProxy->port(), m_pluginID, size.origin.x, size.origin.y, size.size.width, size.size.height);
}

void NetscapePluginInstanceProxy::destroy()
{
    _WKPHDestroyPluginInstance(m_pluginHostProxy->port(), m_pluginID);
    
    m_pluginHostProxy->removePluginInstance(this);
    m_pluginHostProxy = 0;
}

void NetscapePluginInstanceProxy::pluginHostDied()
{
    m_pluginHostProxy = 0;
    
    [m_pluginView pluginHostDied];
    m_pluginView = nil;
}

void NetscapePluginInstanceProxy::focusChanged(bool hasFocus)
{
    _WKPHPluginInstanceFocusChanged(m_pluginHostProxy->port(), m_pluginID, hasFocus);
}

void NetscapePluginInstanceProxy::windowFocusChanged(bool hasFocus)
{
    _WKPHPluginInstanceWindowFocusChanged(m_pluginHostProxy->port(), m_pluginID, hasFocus);
}

void NetscapePluginInstanceProxy::windowFrameChanged(NSRect frame)
{
    _WKPHPluginInstanceWindowFrameChanged(m_pluginHostProxy->port(), m_pluginID, frame.origin.x, frame.origin.y, frame.size.width, frame.size.height,
                                          // FIXME: Is it always correct to pass the rect of the first screen here?
                                          NSMaxY([[[NSScreen screens] objectAtIndex:0] frame]));
}
    
void NetscapePluginInstanceProxy::startTimers(bool throttleTimers)
{
    _WKPHPluginInstanceStartTimers(m_pluginHostProxy->port(), m_pluginID, throttleTimers);
}
    
void NetscapePluginInstanceProxy::mouseEvent(NSView *pluginView, NSEvent *event, NPCocoaEventType type)
{
    NSPoint screenPoint = [[event window] convertBaseToScreen:[event locationInWindow]];
    NSPoint pluginPoint = [pluginView convertPoint:[event locationInWindow] fromView:nil];
    
    int clickCount;
    if (type == NPCocoaEventMouseEntered || type == NPCocoaEventMouseExited)
        clickCount = 0;
    else
        clickCount = [event clickCount];
    

    _WKPHPluginInstanceMouseEvent(m_pluginHostProxy->port(), m_pluginID,
                                  [event timestamp],
                                  type, [event modifierFlags],
                                  pluginPoint.x, pluginPoint.y,
                                  screenPoint.x, screenPoint.y,
                                  // FIXME: Is it always correct to pass the rect of the first screen here?
                                  NSMaxY([[[NSScreen screens] objectAtIndex:0] frame]),
                                  [event buttonNumber], clickCount, 
                                  [event deltaX], [event deltaY], [event deltaZ]);
}
    

void NetscapePluginInstanceProxy::stopTimers()
{
    _WKPHPluginInstanceStopTimers(m_pluginHostProxy->port(), m_pluginID);
}

void NetscapePluginInstanceProxy::status(const char* message)
{
    RetainPtr<CFStringRef> status(AdoptCF, CFStringCreateWithCString(NULL, message, kCFStringEncodingUTF8));
    
    if (!status)
        return;
    
    WebView *wv = [m_pluginView webView];
    [[wv _UIDelegateForwarder] webView:wv setStatusText:(NSString *)status.get()];
}

NPError NetscapePluginInstanceProxy::loadURL(const char* url, const char* target, bool post, const char* postData, uint32_t postLen, bool postDataIsFile, bool currentEventIsUserGesture, uint32_t& streamID)
{
    if (!url)
        return NPERR_INVALID_PARAM;
 
    // FIXME: Support posts.
    if (post)
        return NPERR_INVALID_PARAM;
    
    NSMutableURLRequest *request = [m_pluginView requestWithURLCString:url];
    
    return loadRequest(request, target, currentEventIsUserGesture, streamID);
}

void NetscapePluginInstanceProxy::performRequest(WebPluginRequest* pluginRequest)
{
    NSURLRequest *request = [pluginRequest request];
    NSString *frameName = [pluginRequest frameName];
    WebFrame *frame = nil;
    
    NSURL *URL = [request URL];
    NSString *JSString = [URL _webkit_scriptIfJavaScriptURL];
    
    ASSERT(frameName || JSString);
    if (frameName) {
        // FIXME - need to get rid of this window creation which
        // bypasses normal targeted link handling
        frame = kit(core([m_pluginView webFrame])->loader()->findFrameForNavigation(frameName));
        if (!frame) {
            WebView *currentWebView = [m_pluginView webView];
            NSDictionary *features = [[NSDictionary alloc] init];
            WebView *newWebView = [[currentWebView _UIDelegateForwarder] webView:currentWebView
                                                        createWebViewWithRequest:nil
                                                                  windowFeatures:features];
            [features release];

            if (!newWebView)
                return;
            
            frame = [newWebView mainFrame];
            core(frame)->tree()->setName(frameName);
            [[newWebView _UIDelegateForwarder] webViewShow:newWebView];
        }
    }

    if (JSString) {
        ASSERT(!frame || [m_pluginView webFrame] == frame);
        // FIXME: Evaluate the request.
    } else
        [frame loadRequest:request];
}

void NetscapePluginInstanceProxy::requestTimerFired(Timer<NetscapePluginInstanceProxy>*)
{
    ASSERT(!m_pluginRequests.isEmpty());
    
    RetainPtr<WebPluginRequest> request = m_pluginRequests.first();
    m_pluginRequests.removeFirst();
    
    if (!m_pluginRequests.isEmpty())
        m_requestTimer.startOneShot(0);
    
    performRequest(request.get());
}
    

NPError NetscapePluginInstanceProxy::loadRequest(NSURLRequest *request, const char* cTarget, bool currentEventIsUserGesture, uint32_t& streamID)
{
    NSURL *URL = [request URL];

    if (!URL) 
        return NPERR_INVALID_URL;

    // Don't allow requests to be loaded when the document loader is stopping all loaders.
    if ([[m_pluginView dataSource] _documentLoader]->isStopping())
        return NPERR_GENERIC_ERROR;
    
    NSString *target = nil;
    if (cTarget) {
        // Find the frame given the target string.
        target = [NSString stringWithCString:cTarget encoding:NSISOLatin1StringEncoding];
    }
    WebFrame *frame = [m_pluginView webFrame];

    // don't let a plugin start any loads if it is no longer part of a document that is being 
    // displayed unless the loads are in the same frame as the plugin.
    if ([[m_pluginView dataSource] _documentLoader] != core([m_pluginView webFrame])->loader()->activeDocumentLoader() &&
        (!cTarget || [frame findFrameNamed:target] != frame)) {
        return NPERR_GENERIC_ERROR; 
    }
    
    NSString *JSString = [URL _webkit_scriptIfJavaScriptURL];
    if (JSString != nil) {
        if (![[[m_pluginView webView] preferences] isJavaScriptEnabled]) {
            // Return NPERR_GENERIC_ERROR if JS is disabled. This is what Mozilla does.
            return NPERR_GENERIC_ERROR;
        }
    } else {
        if (!FrameLoader::canLoad(URL, String(), core([m_pluginView webFrame])->document()))
            return NPERR_GENERIC_ERROR;
    }
        
    if (cTarget || JSString) {
        // Make when targetting a frame or evaluating a JS string, perform the request after a delay because we don't
        // want to potentially kill the plug-in inside of its URL request.
        
        if (JSString && target && [frame findFrameNamed:target] != frame) {
            // For security reasons, only allow JS requests to be made on the frame that contains the plug-in.
            return NPERR_INVALID_PARAM;
        }

        RetainPtr<WebPluginRequest> pluginRequest(AdoptCF, [[WebPluginRequest alloc] initWithRequest:request 
                                                                                           frameName:target
                                                                                          notifyData:0
                                                                                    sendNotification:NO
                                                                             didStartFromUserGesture:currentEventIsUserGesture]);
        m_pluginRequests.append(pluginRequest.get());
        m_requestTimer.startOneShot(0);
        streamID = 0;
    } else {
        // FIXME: Load the stream.
        streamID = 0;
    }
    
    return NPERR_NO_ERROR;
}

} // namespace WebKit

#endif // USE(PLUGIN_HOST_PROCESS)
