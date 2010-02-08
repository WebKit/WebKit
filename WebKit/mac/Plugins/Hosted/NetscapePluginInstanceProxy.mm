/*
 * Copyright (C) 2008, 2009, 2010 Apple Inc. All Rights Reserved.
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

#import "HostedNetscapePluginStream.h"
#import "NetscapePluginHostProxy.h"
#import "ProxyInstance.h"
#import "WebDataSourceInternal.h"
#import "WebFrameInternal.h"
#import "WebHostedNetscapePluginView.h"
#import "WebKitNSStringExtras.h"
#import "WebNSDataExtras.h"
#import "WebNSURLExtras.h"
#import "WebPluginRequest.h"
#import "WebUIDelegate.h"
#import "WebUIDelegatePrivate.h"
#import "WebViewInternal.h"
#import <JavaScriptCore/Error.h>
#import <JavaScriptCore/JSLock.h>
#import <JavaScriptCore/PropertyNameArray.h>
#import <WebCore/CString.h>
#import <WebCore/CookieJar.h>
#import <WebCore/DocumentLoader.h>
#import <WebCore/Frame.h>
#import <WebCore/FrameLoader.h>
#import <WebCore/FrameTree.h>
#import <WebCore/KURL.h>
#import <WebCore/SecurityOrigin.h>
#import <WebCore/ScriptController.h>
#import <WebCore/ScriptValue.h>
#import <WebCore/StringSourceProvider.h>
#import <WebCore/npruntime_impl.h>
#import <WebCore/runtime_object.h>
#import <WebKitSystemInterface.h>
#import <mach/mach.h>
#import <utility>
#import <wtf/RefCountedLeakCounter.h>

extern "C" {
#import "WebKitPluginClientServer.h"
#import "WebKitPluginHost.h"
}

using namespace JSC;
using namespace JSC::Bindings;
using namespace std;
using namespace WebCore;

namespace WebKit {

class NetscapePluginInstanceProxy::PluginRequest : public RefCounted<NetscapePluginInstanceProxy::PluginRequest> {
public:
    static PassRefPtr<PluginRequest> create(uint32_t requestID, NSURLRequest* request, NSString* frameName, bool allowPopups)
    {
        return adoptRef(new PluginRequest(requestID, request, frameName, allowPopups));
    }

    uint32_t requestID() const { return m_requestID; }
    NSURLRequest* request() const { return m_request.get(); }
    NSString* frameName() const { return m_frameName.get(); }
    bool allowPopups() const { return m_allowPopups; }
    
private:
    PluginRequest(uint32_t requestID, NSURLRequest* request, NSString* frameName, bool allowPopups)
        : m_requestID(requestID)
        , m_request(request)
        , m_frameName(frameName)
        , m_allowPopups(allowPopups)
    {
    }
    
    uint32_t m_requestID;
    RetainPtr<NSURLRequest*> m_request;
    RetainPtr<NSString*> m_frameName;
    bool m_allowPopups;
};

static uint32_t pluginIDCounter;

#ifndef NDEBUG
static WTF::RefCountedLeakCounter netscapePluginInstanceProxyCounter("NetscapePluginInstanceProxy");
#endif

NetscapePluginInstanceProxy::NetscapePluginInstanceProxy(NetscapePluginHostProxy* pluginHostProxy, WebHostedNetscapePluginView *pluginView, bool fullFramePlugin)
    : m_pluginHostProxy(pluginHostProxy)
    , m_pluginView(pluginView)
    , m_requestTimer(this, &NetscapePluginInstanceProxy::requestTimerFired)
    , m_currentURLRequestID(0)
    , m_renderContextID(0)
    , m_useSoftwareRenderer(false)
    , m_waitingForReply(false)
    , m_objectIDCounter(0)
    , m_urlCheckCounter(0)
    , m_pluginFunctionCallDepth(0)
    , m_shouldStopSoon(false)
    , m_currentRequestID(0)
    , m_inDestroy(false)
    , m_pluginIsWaitingForDraw(false)
{
    ASSERT(m_pluginView);
    
    if (fullFramePlugin) {
        // For full frame plug-ins, the first requestID will always be the one for the already
        // open stream.
        ++m_currentURLRequestID;
    }
    
    // Assign a plug-in ID.
    do {
        m_pluginID = ++pluginIDCounter;
    } while (pluginHostProxy->pluginInstance(m_pluginID) || !m_pluginID);
    
    pluginHostProxy->addPluginInstance(this);

#ifndef NDEBUG
    netscapePluginInstanceProxyCounter.increment();
#endif
}

NetscapePluginInstanceProxy::~NetscapePluginInstanceProxy()
{
    ASSERT(!m_pluginHostProxy);
    
    m_pluginID = 0;
    deleteAllValues(m_replies);

#ifndef NDEBUG
    netscapePluginInstanceProxyCounter.decrement();
#endif
}

void NetscapePluginInstanceProxy::resize(NSRect size, NSRect clipRect, bool sync)
{
    uint32_t requestID = 0;
    
    if (sync)
        requestID = nextRequestID();

    _WKPHResizePluginInstance(m_pluginHostProxy->port(), m_pluginID, requestID,
                              size.origin.x, size.origin.y, size.size.width, size.size.height,
                              clipRect.origin.x, clipRect.origin.y, clipRect.size.width, clipRect.size.height);
    
    if (sync)
        waitForReply<NetscapePluginInstanceProxy::BooleanReply>(requestID);
}

void NetscapePluginInstanceProxy::stopAllStreams()
{
    Vector<RefPtr<HostedNetscapePluginStream> > streamsCopy;
    copyValuesToVector(m_streams, streamsCopy);
    for (size_t i = 0; i < streamsCopy.size(); i++)
        streamsCopy[i]->stop();
}

void NetscapePluginInstanceProxy::cleanup()
{
    stopAllStreams();
    
    m_requestTimer.stop();
    
    // Clear the object map, this will cause any outstanding JS objects that the plug-in had a reference to 
    // to go away when the next garbage collection takes place.
    m_objects.clear();
    
    if (Frame* frame = core([m_pluginView webFrame]))
        frame->script()->cleanupScriptObjectsForPlugin(m_pluginView);
    
    ProxyInstanceSet instances;
    instances.swap(m_instances);
    
    // Invalidate all proxy instances.
    ProxyInstanceSet::const_iterator end = instances.end();
    for (ProxyInstanceSet::const_iterator it = instances.begin(); it != end; ++it)
        (*it)->invalidate();
    
    m_pluginView = nil;
    m_manualStream = 0;
}

void NetscapePluginInstanceProxy::invalidate()
{
    // If the plug-in host has died, the proxy will be null.
    if (!m_pluginHostProxy)
        return;
    
    m_pluginHostProxy->removePluginInstance(this);
    m_pluginHostProxy = 0;
}

void NetscapePluginInstanceProxy::destroy()
{
    uint32_t requestID = nextRequestID();
    
    m_inDestroy = true;
    
    FrameLoadMap::iterator end = m_pendingFrameLoads.end();
    for (FrameLoadMap::iterator it = m_pendingFrameLoads.begin(); it != end; ++it)
        [(it->first) _setInternalLoadDelegate:nil];

    _WKPHDestroyPluginInstance(m_pluginHostProxy->port(), m_pluginID, requestID);
 
    // If the plug-in host crashes while we're waiting for a reply, the last reference to the instance proxy
    // will go away. Prevent this by protecting it here.
    RefPtr<NetscapePluginInstanceProxy> protect(this);
    
    // We don't care about the reply here - we just want to block until the plug-in instance has been torn down.
    waitForReply<NetscapePluginInstanceProxy::BooleanReply>(requestID);

    m_inDestroy = false;
    
    cleanup();
    invalidate();
}

void NetscapePluginInstanceProxy::setManualStream(PassRefPtr<HostedNetscapePluginStream> manualStream) 
{
    ASSERT(!m_manualStream);
    
    m_manualStream = manualStream;
}

bool NetscapePluginInstanceProxy::cancelStreamLoad(uint32_t streamID, NPReason reason) 
{
    HostedNetscapePluginStream* stream = 0;
    
    if (m_manualStream && streamID == 1)
        stream = m_manualStream.get();
    else
        stream = m_streams.get(streamID).get();
    
    if (!stream)
        return false;
    
    stream->cancelLoad(reason);
    return true;
}

void NetscapePluginInstanceProxy::disconnectStream(HostedNetscapePluginStream* stream)
{
    if (stream == m_manualStream) {
        m_manualStream = 0;
        return;
    }

    ASSERT(m_streams.get(stream->streamID()) == stream);
    m_streams.remove(stream->streamID());
}
    
void NetscapePluginInstanceProxy::pluginHostDied()
{
    m_pluginHostProxy = 0;

    [m_pluginView pluginHostDied];

    cleanup();
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
                                  NSMaxY([[[NSScreen screens] objectAtIndex:0] frame]),
                                  [event buttonNumber], clickCount, 
                                  [event deltaX], [event deltaY], [event deltaZ]);
}
    
void NetscapePluginInstanceProxy::keyEvent(NSView *pluginView, NSEvent *event, NPCocoaEventType type)
{
    NSData *charactersData = [[event characters] dataUsingEncoding:NSUTF8StringEncoding];
    NSData *charactersIgnoringModifiersData = [[event charactersIgnoringModifiers] dataUsingEncoding:NSUTF8StringEncoding];
    
    _WKPHPluginInstanceKeyboardEvent(m_pluginHostProxy->port(), m_pluginID,
                                     [event timestamp], 
                                     type, [event modifierFlags], 
                                     const_cast<char*>(reinterpret_cast<const char*>([charactersData bytes])), [charactersData length], 
                                     const_cast<char*>(reinterpret_cast<const char*>([charactersIgnoringModifiersData bytes])), [charactersIgnoringModifiersData length], 
                                     [event isARepeat], [event keyCode], WKGetNSEventKeyChar(event));
}

void NetscapePluginInstanceProxy::syntheticKeyDownWithCommandModifier(int keyCode, char character)
{
    NSData *charactersData = [NSData dataWithBytes:&character length:1];

    _WKPHPluginInstanceKeyboardEvent(m_pluginHostProxy->port(), m_pluginID, 
                                     [NSDate timeIntervalSinceReferenceDate], 
                                     NPCocoaEventKeyDown, NSCommandKeyMask,
                                     const_cast<char*>(reinterpret_cast<const char*>([charactersData bytes])), [charactersData length], 
                                     const_cast<char*>(reinterpret_cast<const char*>([charactersData bytes])), [charactersData length], 
                                     false, keyCode, character);
}

void NetscapePluginInstanceProxy::flagsChanged(NSEvent *event)
{
    _WKPHPluginInstanceKeyboardEvent(m_pluginHostProxy->port(), m_pluginID, 
                                     [event timestamp], NPCocoaEventFlagsChanged, 
                                     [event modifierFlags], 0, 0, 0, 0, false, [event keyCode], 0);
}

void NetscapePluginInstanceProxy::insertText(NSString *text)
{
    NSData *textData = [text dataUsingEncoding:NSUTF8StringEncoding];
    
    _WKPHPluginInstanceInsertText(m_pluginHostProxy->port(), m_pluginID,
                                  const_cast<char*>(reinterpret_cast<const char*>([textData bytes])), [textData length]);
}

bool NetscapePluginInstanceProxy::wheelEvent(NSView *pluginView, NSEvent *event)
{
    NSPoint pluginPoint = [pluginView convertPoint:[event locationInWindow] fromView:nil];

    uint32_t requestID = nextRequestID();
    _WKPHPluginInstanceWheelEvent(m_pluginHostProxy->port(), m_pluginID, requestID,
                                  [event timestamp], [event modifierFlags], 
                                  pluginPoint.x, pluginPoint.y, [event buttonNumber], 
                                  [event deltaX], [event deltaY], [event deltaZ]);
    
    // Protect ourselves in case waiting for the reply causes us to be deleted.
    RefPtr<NetscapePluginInstanceProxy> protect(this);

    auto_ptr<NetscapePluginInstanceProxy::BooleanReply> reply = waitForReply<NetscapePluginInstanceProxy::BooleanReply>(requestID);
    if (!reply.get() || !reply->m_result)
        return false;
    
    return true;
}

void NetscapePluginInstanceProxy::print(CGContextRef context, unsigned width, unsigned height)
{
    uint32_t requestID = nextRequestID();
    _WKPHPluginInstancePrint(m_pluginHostProxy->port(), m_pluginID, requestID, width, height);
    
    auto_ptr<NetscapePluginInstanceProxy::BooleanAndDataReply> reply = waitForReply<NetscapePluginInstanceProxy::BooleanAndDataReply>(requestID);
    if (!reply.get() || !reply->m_returnValue)
        return;

    RetainPtr<CGDataProvider> dataProvider(AdoptCF, CGDataProviderCreateWithCFData(reply->m_result.get()));
    RetainPtr<CGColorSpaceRef> colorSpace(AdoptCF, CGColorSpaceCreateDeviceRGB());
    RetainPtr<CGImageRef> image(AdoptCF, CGImageCreate(width, height, 8, 32, width * 4, colorSpace.get(), kCGImageAlphaFirst, dataProvider.get(), 0, false, kCGRenderingIntentDefault));

    // Flip the context and draw the image.
    CGContextSaveGState(context);
    CGContextTranslateCTM(context, 0.0, height);
    CGContextScaleCTM(context, 1.0, -1.0);
    
    CGContextDrawImage(context, CGRectMake(0, 0, width, height), image.get());

    CGContextRestoreGState(context);
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

NPError NetscapePluginInstanceProxy::loadURL(const char* url, const char* target, const char* postData, uint32_t postLen, LoadURLFlags flags, uint32_t& streamID)
{
    if (!url)
        return NPERR_INVALID_PARAM;
 
    NSMutableURLRequest *request = [m_pluginView requestWithURLCString:url];

    if (flags & IsPost) {
        NSData *httpBody = nil;

        if (flags & PostDataIsFile) {
            // If we're posting a file, buf is either a file URL or a path to the file.
            RetainPtr<CFStringRef> bufString(AdoptCF, CFStringCreateWithCString(kCFAllocatorDefault, postData, kCFStringEncodingWindowsLatin1));
            if (!bufString)
                return NPERR_INVALID_PARAM;
            
            NSURL *fileURL = [NSURL _web_URLWithDataAsString:(NSString *)bufString.get()];
            NSString *path;
            if ([fileURL isFileURL])
                path = [fileURL path];
            else
                path = (NSString *)bufString.get();
            httpBody = [NSData dataWithContentsOfFile:[path _webkit_fixedCarbonPOSIXPath]];
            if (!httpBody)
                return NPERR_FILE_NOT_FOUND;
        } else
            httpBody = [NSData dataWithBytes:postData length:postLen];

        if (![httpBody length])
            return NPERR_INVALID_PARAM;

        [request setHTTPMethod:@"POST"];
        
        if (flags & AllowHeadersInPostData) {
            if ([httpBody _web_startsWithBlankLine])
                httpBody = [httpBody subdataWithRange:NSMakeRange(1, [httpBody length] - 1)];
            else {
                NSInteger location = [httpBody _web_locationAfterFirstBlankLine];
                if (location != NSNotFound) {
                    // If the blank line is somewhere in the middle of postData, everything before is the header.
                    NSData *headerData = [httpBody subdataWithRange:NSMakeRange(0, location)];
                    NSMutableDictionary *header = [headerData _webkit_parseRFC822HeaderFields];
                    unsigned dataLength = [httpBody length] - location;

                    // Sometimes plugins like to set Content-Length themselves when they post,
                    // but CFNetwork does not like that. So we will remove the header
                    // and instead truncate the data to the requested length.
                    NSString *contentLength = [header objectForKey:@"Content-Length"];

                    if (contentLength)
                        dataLength = min(static_cast<unsigned>([contentLength intValue]), dataLength);
                    [header removeObjectForKey:@"Content-Length"];

                    if ([header count] > 0)
                        [request setAllHTTPHeaderFields:header];

                    // Everything after the blank line is the actual content of the POST.
                    httpBody = [httpBody subdataWithRange:NSMakeRange(location, dataLength)];
                }
            }
        }

        if (![httpBody length])
            return NPERR_INVALID_PARAM;

        // Plug-ins expect to receive uncached data when doing a POST (3347134).
        [request setCachePolicy:NSURLRequestReloadIgnoringCacheData];
        [request setHTTPBody:httpBody];
    }
    
    return loadRequest(request, target, flags & AllowPopups, streamID);
}

void NetscapePluginInstanceProxy::performRequest(PluginRequest* pluginRequest)
{
    ASSERT(m_pluginView);
    
    NSURLRequest *request = pluginRequest->request();
    NSString *frameName = pluginRequest->frameName();
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

            if (!newWebView) {
                _WKPHLoadURLNotify(m_pluginHostProxy->port(), m_pluginID, pluginRequest->requestID(), NPERR_GENERIC_ERROR);
                return;
            }
            
            frame = [newWebView mainFrame];
            core(frame)->tree()->setName(frameName);
            [[newWebView _UIDelegateForwarder] webViewShow:newWebView];
        }
    }

    if (JSString) {
        ASSERT(!frame || [m_pluginView webFrame] == frame);
        evaluateJavaScript(pluginRequest);
    } else {
        [frame loadRequest:request];

        // Check if another plug-in view or even this view is waiting for the frame to load.
        // If it is, tell it that the load was cancelled because it will be anyway.
        WebHostedNetscapePluginView *view = [frame _internalLoadDelegate];
        if (view != nil) {
            ASSERT([view isKindOfClass:[WebHostedNetscapePluginView class]]);
            [view webFrame:frame didFinishLoadWithReason:NPRES_USER_BREAK];
        }
        m_pendingFrameLoads.set(frame, pluginRequest);
        [frame _setInternalLoadDelegate:m_pluginView];
    }

}

void NetscapePluginInstanceProxy::webFrameDidFinishLoadWithReason(WebFrame* webFrame, NPReason reason)
{
    FrameLoadMap::iterator it = m_pendingFrameLoads.find(webFrame);
    ASSERT(it != m_pendingFrameLoads.end());
        
    PluginRequest* pluginRequest = it->second.get();
    _WKPHLoadURLNotify(m_pluginHostProxy->port(), m_pluginID, pluginRequest->requestID(), reason);
 
    m_pendingFrameLoads.remove(it);

    [webFrame _setInternalLoadDelegate:nil];
}

void NetscapePluginInstanceProxy::evaluateJavaScript(PluginRequest* pluginRequest)
{
    NSURL *URL = [pluginRequest->request() URL];
    NSString *JSString = [URL _webkit_scriptIfJavaScriptURL];
    ASSERT(JSString);
    
    NSString *result = [[m_pluginView webFrame] _stringByEvaluatingJavaScriptFromString:JSString forceUserGesture:pluginRequest->allowPopups()];
    
    // Don't continue if stringByEvaluatingJavaScriptFromString caused the plug-in to stop.
    if (!m_pluginHostProxy)
        return;

    if (pluginRequest->frameName() != nil)
        return;
        
    if ([result length] > 0) {
        // Don't call NPP_NewStream and other stream methods if there is no JS result to deliver. This is what Mozilla does.
        NSData *JSData = [result dataUsingEncoding:NSUTF8StringEncoding];
        
        RefPtr<HostedNetscapePluginStream> stream = HostedNetscapePluginStream::create(this, pluginRequest->requestID(), pluginRequest->request());
        
        RetainPtr<NSURLResponse> response(AdoptNS, [[NSURLResponse alloc] initWithURL:URL 
                                                                             MIMEType:@"text/plain" 
                                                                expectedContentLength:[JSData length]
                                                                     textEncodingName:nil]);
        stream->startStreamWithResponse(response.get());
        stream->didReceiveData(0, static_cast<const char*>([JSData bytes]), [JSData length]);
        stream->didFinishLoading(0);
    }
}

void NetscapePluginInstanceProxy::requestTimerFired(Timer<NetscapePluginInstanceProxy>*)
{
    ASSERT(!m_pluginRequests.isEmpty());
    ASSERT(m_pluginView);
    
    RefPtr<PluginRequest> request = m_pluginRequests.first();
    m_pluginRequests.removeFirst();
    
    if (!m_pluginRequests.isEmpty())
        m_requestTimer.startOneShot(0);
    
    performRequest(request.get());
}
    
NPError NetscapePluginInstanceProxy::loadRequest(NSURLRequest *request, const char* cTarget, bool allowPopups, uint32_t& requestID)
{
    NSURL *URL = [request URL];

    if (!URL) 
        return NPERR_INVALID_URL;

    // Don't allow requests to be loaded when the document loader is stopping all loaders.
    DocumentLoader* documentLoader = [[m_pluginView dataSource] _documentLoader];
    if (!documentLoader || documentLoader->isStopping())
        return NPERR_GENERIC_ERROR;

    NSString *target = nil;
    if (cTarget) {
        // Find the frame given the target string.
        target = [NSString stringWithCString:cTarget encoding:NSISOLatin1StringEncoding];
    }
    WebFrame *frame = [m_pluginView webFrame];

    // don't let a plugin start any loads if it is no longer part of a document that is being 
    // displayed unless the loads are in the same frame as the plugin.
    if (documentLoader != core([m_pluginView webFrame])->loader()->activeDocumentLoader() &&
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
        if (!SecurityOrigin::canLoad(URL, String(), core([m_pluginView webFrame])->document()))
            return NPERR_GENERIC_ERROR;
    }
    
    // FIXME: Handle wraparound
    requestID = ++m_currentURLRequestID;
        
    if (cTarget || JSString) {
        // Make when targetting a frame or evaluating a JS string, perform the request after a delay because we don't
        // want to potentially kill the plug-in inside of its URL request.
        
        if (JSString && target && [frame findFrameNamed:target] != frame) {
            // For security reasons, only allow JS requests to be made on the frame that contains the plug-in.
            return NPERR_INVALID_PARAM;
        }

        RefPtr<PluginRequest> pluginRequest = PluginRequest::create(requestID, request, target, allowPopups);
        m_pluginRequests.append(pluginRequest.release());
        m_requestTimer.startOneShot(0);
    } else {
        RefPtr<HostedNetscapePluginStream> stream = HostedNetscapePluginStream::create(this, requestID, request);

        ASSERT(!m_streams.contains(requestID));
        m_streams.add(requestID, stream);
        stream->start();
    }
    
    return NPERR_NO_ERROR;
}

NetscapePluginInstanceProxy::Reply* NetscapePluginInstanceProxy::processRequestsAndWaitForReply(uint32_t requestID)
{
    Reply* reply = 0;
    
    while (!(reply = m_replies.take(requestID))) {
        if (!m_pluginHostProxy->processRequests())
            return 0;
    }
    
    ASSERT(reply);
    return reply;
}
    
uint32_t NetscapePluginInstanceProxy::idForObject(JSObject* object)
{
    uint32_t objectID = 0;
    
    // Assign an object ID.
    do {
        objectID = ++m_objectIDCounter;
    } while (!m_objectIDCounter || m_objectIDCounter == static_cast<uint32_t>(-1) || m_objects.contains(objectID));
    
    m_objects.set(objectID, object);
    
    return objectID;
}

// NPRuntime support
bool NetscapePluginInstanceProxy::getWindowNPObject(uint32_t& objectID)
{
    Frame* frame = core([m_pluginView webFrame]);
    if (!frame)
        return false;
    
    if (!frame->script()->canExecuteScripts())
        objectID = 0;
    else
        objectID = idForObject(frame->script()->windowShell(pluginWorld())->window());
        
    return true;
}

bool NetscapePluginInstanceProxy::getPluginElementNPObject(uint32_t& objectID)
{
    Frame* frame = core([m_pluginView webFrame]);
    if (!frame)
        return false;
    
    if (JSObject* object = frame->script()->jsObjectForPluginElement([m_pluginView element]))
        objectID = idForObject(object);
    else
        objectID = 0;
    
    return true;
}

void NetscapePluginInstanceProxy::releaseObject(uint32_t objectID)
{
    m_objects.remove(objectID);
}
 
bool NetscapePluginInstanceProxy::evaluate(uint32_t objectID, const String& script, data_t& resultData, mach_msg_type_number_t& resultLength, bool allowPopups)
{
    resultData = 0;
    resultLength = 0;

    if (!m_objects.contains(objectID))
        return false;

    Frame* frame = core([m_pluginView webFrame]);
    if (!frame)
        return false;

    JSLock lock(SilenceAssertionsOnly);
    
    ProtectedPtr<JSGlobalObject> globalObject = frame->script()->globalObject(pluginWorld());
    ExecState* exec = globalObject->globalExec();

    bool oldAllowPopups = frame->script()->allowPopupsFromPlugin();
    frame->script()->setAllowPopupsFromPlugin(allowPopups);
    
    globalObject->globalData()->timeoutChecker.start();
    Completion completion = JSC::evaluate(exec, globalObject->globalScopeChain(), makeSource(script));
    globalObject->globalData()->timeoutChecker.stop();
    ComplType type = completion.complType();

    frame->script()->setAllowPopupsFromPlugin(oldAllowPopups);
    
    JSValue result;
    if (type == Normal)
        result = completion.value();
    
    if (!result)
        result = jsUndefined();
    
    marshalValue(exec, result, resultData, resultLength);
    exec->clearException();
    return true;
}

bool NetscapePluginInstanceProxy::invoke(uint32_t objectID, const Identifier& methodName, data_t argumentsData, mach_msg_type_number_t argumentsLength, data_t& resultData, mach_msg_type_number_t& resultLength)
{
    resultData = 0;
    resultLength = 0;
    
    if (m_inDestroy)
        return false;
    
    JSObject* object = m_objects.get(objectID);
    if (!object)
        return false;
    
    Frame* frame = core([m_pluginView webFrame]);
    if (!frame)
        return false;
    
    ExecState* exec = frame->script()->globalObject(pluginWorld())->globalExec();
    JSLock lock(SilenceAssertionsOnly);
    JSValue function = object->get(exec, methodName);
    CallData callData;
    CallType callType = function.getCallData(callData);
    if (callType == CallTypeNone)
        return false;

    MarkedArgumentBuffer argList;
    demarshalValues(exec, argumentsData, argumentsLength, argList);

    ProtectedPtr<JSGlobalObject> globalObject = frame->script()->globalObject(pluginWorld());
    globalObject->globalData()->timeoutChecker.start();
    JSValue value = call(exec, function, callType, callData, object, argList);
    globalObject->globalData()->timeoutChecker.stop();
        
    marshalValue(exec, value, resultData, resultLength);
    exec->clearException();
    return true;
}

bool NetscapePluginInstanceProxy::invokeDefault(uint32_t objectID, data_t argumentsData, mach_msg_type_number_t argumentsLength, data_t& resultData, mach_msg_type_number_t& resultLength)
{
    if (m_inDestroy)
        return false;

    JSObject* object = m_objects.get(objectID);
    if (!object)
        return false;
    
    Frame* frame = core([m_pluginView webFrame]);
    if (!frame)
        return false;
    
    ExecState* exec = frame->script()->globalObject(pluginWorld())->globalExec();
    JSLock lock(SilenceAssertionsOnly);    
    CallData callData;
    CallType callType = object->getCallData(callData);
    if (callType == CallTypeNone)
        return false;

    MarkedArgumentBuffer argList;
    demarshalValues(exec, argumentsData, argumentsLength, argList);

    ProtectedPtr<JSGlobalObject> globalObject = frame->script()->globalObject(pluginWorld());
    globalObject->globalData()->timeoutChecker.start();
    JSValue value = call(exec, object, callType, callData, object, argList);
    globalObject->globalData()->timeoutChecker.stop();
    
    marshalValue(exec, value, resultData, resultLength);
    exec->clearException();
    return true;
}

bool NetscapePluginInstanceProxy::construct(uint32_t objectID, data_t argumentsData, mach_msg_type_number_t argumentsLength, data_t& resultData, mach_msg_type_number_t& resultLength)
{
    if (m_inDestroy)
        return false;

    JSObject* object = m_objects.get(objectID);
    if (!object)
        return false;
    
    Frame* frame = core([m_pluginView webFrame]);
    if (!frame)
        return false;
    
    ExecState* exec = frame->script()->globalObject(pluginWorld())->globalExec();
    JSLock lock(SilenceAssertionsOnly);

    ConstructData constructData;
    ConstructType constructType = object->getConstructData(constructData);
    if (constructType == ConstructTypeNone)
        return false;

    MarkedArgumentBuffer argList;
    demarshalValues(exec, argumentsData, argumentsLength, argList);

    ProtectedPtr<JSGlobalObject> globalObject = frame->script()->globalObject(pluginWorld());
    globalObject->globalData()->timeoutChecker.start();
    JSValue value = JSC::construct(exec, object, constructType, constructData, argList);
    globalObject->globalData()->timeoutChecker.stop();
    
    marshalValue(exec, value, resultData, resultLength);
    exec->clearException();
    return true;
}

bool NetscapePluginInstanceProxy::getProperty(uint32_t objectID, const Identifier& propertyName, data_t& resultData, mach_msg_type_number_t& resultLength)
{
    if (m_inDestroy)
        return false;

    JSObject* object = m_objects.get(objectID);
    if (!object)
        return false;
    
    Frame* frame = core([m_pluginView webFrame]);
    if (!frame)
        return false;
    
    ExecState* exec = frame->script()->globalObject(pluginWorld())->globalExec();
    JSLock lock(SilenceAssertionsOnly);    
    JSValue value = object->get(exec, propertyName);
    
    marshalValue(exec, value, resultData, resultLength);
    exec->clearException();
    return true;
}
    
bool NetscapePluginInstanceProxy::getProperty(uint32_t objectID, unsigned propertyName, data_t& resultData, mach_msg_type_number_t& resultLength)
{
    JSObject* object = m_objects.get(objectID);
    if (!object)
        return false;
    
    Frame* frame = core([m_pluginView webFrame]);
    if (!frame)
        return false;
    
    ExecState* exec = frame->script()->globalObject(pluginWorld())->globalExec();
    JSLock lock(SilenceAssertionsOnly);    
    JSValue value = object->get(exec, propertyName);
    
    marshalValue(exec, value, resultData, resultLength);
    exec->clearException();
    return true;
}

bool NetscapePluginInstanceProxy::setProperty(uint32_t objectID, const Identifier& propertyName, data_t valueData, mach_msg_type_number_t valueLength)
{
    if (m_inDestroy)
        return false;

    JSObject* object = m_objects.get(objectID);
    if (!object)
        return false;
    
    Frame* frame = core([m_pluginView webFrame]);
    if (!frame)
        return false;
    
    ExecState* exec = frame->script()->globalObject(pluginWorld())->globalExec();
    JSLock lock(SilenceAssertionsOnly);    

    JSValue value = demarshalValue(exec, valueData, valueLength);
    PutPropertySlot slot;
    object->put(exec, propertyName, value, slot);
    
    exec->clearException();
    return true;
}

bool NetscapePluginInstanceProxy::setProperty(uint32_t objectID, unsigned propertyName, data_t valueData, mach_msg_type_number_t valueLength)
{
    if (m_inDestroy)
        return false;

    JSObject* object = m_objects.get(objectID);
    if (!object)
        return false;
    
    Frame* frame = core([m_pluginView webFrame]);
    if (!frame)
        return false;
    
    ExecState* exec = frame->script()->globalObject(pluginWorld())->globalExec();
    JSLock lock(SilenceAssertionsOnly);    
    
    JSValue value = demarshalValue(exec, valueData, valueLength);
    object->put(exec, propertyName, value);
    
    exec->clearException();
    return true;
}

bool NetscapePluginInstanceProxy::removeProperty(uint32_t objectID, const Identifier& propertyName)
{
    if (m_inDestroy)
        return false;

    JSObject* object = m_objects.get(objectID);
    if (!object)
        return false;
    
    Frame* frame = core([m_pluginView webFrame]);
    if (!frame)
        return false;

    ExecState* exec = frame->script()->globalObject(pluginWorld())->globalExec();
    if (!object->hasProperty(exec, propertyName)) {
        exec->clearException();
        return false;
    }
    
    JSLock lock(SilenceAssertionsOnly);
    object->deleteProperty(exec, propertyName);
    exec->clearException();    
    return true;
}
    
bool NetscapePluginInstanceProxy::removeProperty(uint32_t objectID, unsigned propertyName)
{
    if (m_inDestroy)
        return false;

    JSObject* object = m_objects.get(objectID);
    if (!object)
        return false;
    
    Frame* frame = core([m_pluginView webFrame]);
    if (!frame)
        return false;
    
    ExecState* exec = frame->script()->globalObject(pluginWorld())->globalExec();
    if (!object->hasProperty(exec, propertyName)) {
        exec->clearException();
        return false;
    }
    
    JSLock lock(SilenceAssertionsOnly);
    object->deleteProperty(exec, propertyName);
    exec->clearException();    
    return true;
}

bool NetscapePluginInstanceProxy::hasProperty(uint32_t objectID, const Identifier& propertyName)
{
    if (m_inDestroy)
        return false;

    JSObject* object = m_objects.get(objectID);
    if (!object)
        return false;
    
    Frame* frame = core([m_pluginView webFrame]);
    if (!frame)
        return false;
    
    ExecState* exec = frame->script()->globalObject(pluginWorld())->globalExec();
    bool result = object->hasProperty(exec, propertyName);
    exec->clearException();
    
    return result;
}

bool NetscapePluginInstanceProxy::hasProperty(uint32_t objectID, unsigned propertyName)
{
    if (m_inDestroy)
        return false;

    JSObject* object = m_objects.get(objectID);
    if (!object)
        return false;
    
    Frame* frame = core([m_pluginView webFrame]);
    if (!frame)
        return false;
    
    ExecState* exec = frame->script()->globalObject(pluginWorld())->globalExec();
    bool result = object->hasProperty(exec, propertyName);
    exec->clearException();
    
    return result;
}
    
bool NetscapePluginInstanceProxy::hasMethod(uint32_t objectID, const Identifier& methodName)
{
    if (m_inDestroy)
        return false;

    JSObject* object = m_objects.get(objectID);
    if (!object)
        return false;

    Frame* frame = core([m_pluginView webFrame]);
    if (!frame)
        return false;
    
    ExecState* exec = frame->script()->globalObject(pluginWorld())->globalExec();
    JSLock lock(SilenceAssertionsOnly);
    JSValue func = object->get(exec, methodName);
    exec->clearException();
    return !func.isUndefined();
}

bool NetscapePluginInstanceProxy::enumerate(uint32_t objectID, data_t& resultData, mach_msg_type_number_t& resultLength)
{
    if (m_inDestroy)
        return false;

    JSObject* object = m_objects.get(objectID);
    if (!object)
        return false;
    
    Frame* frame = core([m_pluginView webFrame]);
    if (!frame)
        return false;
    
    ExecState* exec = frame->script()->globalObject(pluginWorld())->globalExec();
    JSLock lock(SilenceAssertionsOnly);
 
    PropertyNameArray propertyNames(exec);
    object->getPropertyNames(exec, propertyNames);

    RetainPtr<NSMutableArray*> array(AdoptNS, [[NSMutableArray alloc] init]);
    for (unsigned i = 0; i < propertyNames.size(); i++) {
        uint64_t methodName = reinterpret_cast<uint64_t>(_NPN_GetStringIdentifier(propertyNames[i].ustring().UTF8String().c_str()));

        [array.get() addObject:[NSNumber numberWithLongLong:methodName]];
    }

    NSData *data = [NSPropertyListSerialization dataFromPropertyList:array.get() format:NSPropertyListBinaryFormat_v1_0 errorDescription:0];
    ASSERT(data);

    resultLength = [data length];
    mig_allocate(reinterpret_cast<vm_address_t*>(&resultData), resultLength);

    memcpy(resultData, [data bytes], resultLength);

    exec->clearException();

    return true;
}

void NetscapePluginInstanceProxy::addValueToArray(NSMutableArray *array, ExecState* exec, JSValue value)
{
    JSLock lock(SilenceAssertionsOnly);

    if (value.isString()) {
        [array addObject:[NSNumber numberWithInt:StringValueType]];
        [array addObject:String(value.toString(exec))];
    } else if (value.isNumber()) {
        [array addObject:[NSNumber numberWithInt:DoubleValueType]];
        [array addObject:[NSNumber numberWithDouble:value.toNumber(exec)]];
    } else if (value.isBoolean()) {
        [array addObject:[NSNumber numberWithInt:BoolValueType]];
        [array addObject:[NSNumber numberWithBool:value.toBoolean(exec)]];
    } else if (value.isNull())
        [array addObject:[NSNumber numberWithInt:NullValueType]];
    else if (value.isObject()) {
        JSObject* object = asObject(value);
        if (object->classInfo() == &RuntimeObjectImp::s_info) {
            RuntimeObjectImp* imp = static_cast<RuntimeObjectImp*>(object);
            if (ProxyInstance* instance = static_cast<ProxyInstance*>(imp->getInternalInstance())) {
                [array addObject:[NSNumber numberWithInt:NPObjectValueType]];
                [array addObject:[NSNumber numberWithInt:instance->objectID()]];
            }
        } else {
            [array addObject:[NSNumber numberWithInt:JSObjectValueType]];
            [array addObject:[NSNumber numberWithInt:idForObject(object)]];
        }
    } else
        [array addObject:[NSNumber numberWithInt:VoidValueType]];
}

void NetscapePluginInstanceProxy::marshalValue(ExecState* exec, JSValue value, data_t& resultData, mach_msg_type_number_t& resultLength)
{
    RetainPtr<NSMutableArray*> array(AdoptNS, [[NSMutableArray alloc] init]);
    
    addValueToArray(array.get(), exec, value);

    RetainPtr<NSData *> data = [NSPropertyListSerialization dataFromPropertyList:array.get() format:NSPropertyListBinaryFormat_v1_0 errorDescription:0];
    ASSERT(data);
    
    resultLength = [data.get() length];
    mig_allocate(reinterpret_cast<vm_address_t*>(&resultData), resultLength);
    
    memcpy(resultData, [data.get() bytes], resultLength);
}

RetainPtr<NSData *> NetscapePluginInstanceProxy::marshalValues(ExecState* exec, const ArgList& args)
{
    RetainPtr<NSMutableArray*> array(AdoptNS, [[NSMutableArray alloc] init]);

    for (unsigned i = 0; i < args.size(); i++)
        addValueToArray(array.get(), exec, args.at(i));

    RetainPtr<NSData *> data = [NSPropertyListSerialization dataFromPropertyList:array.get() format:NSPropertyListBinaryFormat_v1_0 errorDescription:0];
    ASSERT(data);

    return data;
}    

bool NetscapePluginInstanceProxy::demarshalValueFromArray(ExecState* exec, NSArray *array, NSUInteger& index, JSValue& result)
{
    if (index == [array count])
        return false;
                  
    int type = [[array objectAtIndex:index++] intValue];
    switch (type) {
        case VoidValueType:
            result = jsUndefined();
            return true;
        case NullValueType:
            result = jsNull();
            return true;
        case BoolValueType:
            result = jsBoolean([[array objectAtIndex:index++] boolValue]);
            return true;
        case DoubleValueType:
            result = jsNumber(exec, [[array objectAtIndex:index++] doubleValue]);
            return true;
        case StringValueType: {
            NSString *string = [array objectAtIndex:index++];
            
            result = jsString(exec, String(string));
            return true;
        }
        case JSObjectValueType: {
            uint32_t objectID = [[array objectAtIndex:index++] intValue];
            
            result = m_objects.get(objectID);
            ASSERT(result);
            return true;
        }
        case NPObjectValueType: {
            uint32_t objectID = [[array objectAtIndex:index++] intValue];

            Frame* frame = core([m_pluginView webFrame]);
            if (!frame)
                return false;
            
            if (!frame->script()->canExecuteScripts())
                return false;

            RefPtr<RootObject> rootObject = frame->script()->createRootObject(m_pluginView);
            if (!rootObject)
                return false;
            
            result = ProxyInstance::create(rootObject.release(), this, objectID)->createRuntimeObject(exec);
            return true;
        }
        default:
            ASSERT_NOT_REACHED();
            return false;
    }
}

JSValue NetscapePluginInstanceProxy::demarshalValue(ExecState* exec, const char* valueData, mach_msg_type_number_t valueLength)
{
    RetainPtr<NSData*> data(AdoptNS, [[NSData alloc] initWithBytesNoCopy:(void*)valueData length:valueLength freeWhenDone:NO]);

    RetainPtr<NSArray*> array = [NSPropertyListSerialization propertyListFromData:data.get()
                                                                 mutabilityOption:NSPropertyListImmutable
                                                                           format:0
                                                                 errorDescription:0];
    NSUInteger position = 0;
    JSValue value;
    bool result = demarshalValueFromArray(exec, array.get(), position, value);
    ASSERT_UNUSED(result, result);

    return value;
}

void NetscapePluginInstanceProxy::demarshalValues(ExecState* exec, data_t valuesData, mach_msg_type_number_t valuesLength, MarkedArgumentBuffer& result)
{
    RetainPtr<NSData*> data(AdoptNS, [[NSData alloc] initWithBytesNoCopy:valuesData length:valuesLength freeWhenDone:NO]);

    RetainPtr<NSArray*> array = [NSPropertyListSerialization propertyListFromData:data.get()
                                                                 mutabilityOption:NSPropertyListImmutable
                                                                           format:0
                                                                 errorDescription:0];
    NSUInteger position = 0;
    JSValue value;
    while (demarshalValueFromArray(exec, array.get(), position, value))
        result.append(value);
}

PassRefPtr<Instance> NetscapePluginInstanceProxy::createBindingsInstance(PassRefPtr<RootObject> rootObject)
{
    uint32_t requestID = nextRequestID();
    
    if (_WKPHGetScriptableNPObject(m_pluginHostProxy->port(), m_pluginID, requestID) != KERN_SUCCESS)
        return 0;

    // If the plug-in host crashes while we're waiting for a reply, the last reference to the instance proxy
    // will go away. Prevent this by protecting it here.
    RefPtr<NetscapePluginInstanceProxy> protect(this);
    
    auto_ptr<GetScriptableNPObjectReply> reply = waitForReply<GetScriptableNPObjectReply>(requestID);
    if (!reply.get())
        return 0;

    if (!reply->m_objectID)
        return 0;

    return ProxyInstance::create(rootObject, this, reply->m_objectID);
}

void NetscapePluginInstanceProxy::addInstance(ProxyInstance* instance)
{
    ASSERT(!m_instances.contains(instance));
    
    m_instances.add(instance);
}
    
void NetscapePluginInstanceProxy::removeInstance(ProxyInstance* instance)
{
    ASSERT(m_instances.contains(instance));
    
    m_instances.remove(instance);
}
 
void NetscapePluginInstanceProxy::willCallPluginFunction()
{
    m_pluginFunctionCallDepth++;
}
    
void NetscapePluginInstanceProxy::didCallPluginFunction()
{
    ASSERT(m_pluginFunctionCallDepth > 0);
    m_pluginFunctionCallDepth--;
    
    // If -stop was called while we were calling into a plug-in function, and we're no longer
    // inside a plug-in function, stop now.
    if (!m_pluginFunctionCallDepth && m_shouldStopSoon) {
        m_shouldStopSoon = false;
        [m_pluginView stop];
    }
}
    
bool NetscapePluginInstanceProxy::shouldStop()
{
    if (m_pluginFunctionCallDepth) {
        m_shouldStopSoon = true;
        return false;
    }
    
    return true;
}

uint32_t NetscapePluginInstanceProxy::nextRequestID()
{
    uint32_t requestID = ++m_currentRequestID;
    
    // We don't want to return the HashMap empty/deleted "special keys"
    if (requestID == 0 || requestID == static_cast<uint32_t>(-1))
        return nextRequestID();
    
    return requestID;
}

void NetscapePluginInstanceProxy::invalidateRect(double x, double y, double width, double height)
{
    ASSERT(m_pluginView);
    
    m_pluginIsWaitingForDraw = true;
    [m_pluginView invalidatePluginContentRect:NSMakeRect(x, y, width, height)];
}

void NetscapePluginInstanceProxy::didDraw()
{
    if (!m_pluginIsWaitingForDraw)
        return;
    
    m_pluginIsWaitingForDraw = false;
    _WKPHPluginInstanceDidDraw(m_pluginHostProxy->port(), m_pluginID);
}
    
bool NetscapePluginInstanceProxy::getCookies(data_t urlData, mach_msg_type_number_t urlLength, data_t& cookiesData, mach_msg_type_number_t& cookiesLength)
{
    ASSERT(m_pluginView);
    
    NSURL *url = [m_pluginView URLWithCString:urlData];
    if (!url)
        return false;
    
    if (Frame* frame = core([m_pluginView webFrame])) {
        String cookieString = cookies(frame->document(), url); 
        WebCore::CString cookieStringUTF8 = cookieString.utf8();
        if (cookieStringUTF8.isNull())
            return false;
        
        cookiesLength = cookieStringUTF8.length();
        mig_allocate(reinterpret_cast<vm_address_t*>(&cookiesData), cookiesLength);
        memcpy(cookiesData, cookieStringUTF8.data(), cookiesLength);
        
        return true;
    }

    return false;
}
    
bool NetscapePluginInstanceProxy::setCookies(data_t urlData, mach_msg_type_number_t urlLength, data_t cookiesData, mach_msg_type_number_t cookiesLength)
{
    ASSERT(m_pluginView);
    
    NSURL *url = [m_pluginView URLWithCString:urlData];
    if (!url)
        return false;

    if (Frame* frame = core([m_pluginView webFrame])) {
        String cookieString = String::fromUTF8(cookiesData, cookiesLength);
        if (!cookieString)
            return false;
        
        WebCore::setCookies(frame->document(), url, cookieString);
        return true;
    }

    return false;
}

bool NetscapePluginInstanceProxy::getProxy(data_t urlData, mach_msg_type_number_t urlLength, data_t& proxyData, mach_msg_type_number_t& proxyLength)
{
    ASSERT(m_pluginView);
    
    NSURL *url = [m_pluginView URLWithCString:urlData];
    if (!url)
        return false;

    WebCore::CString proxyStringUTF8 = proxiesForURL(url);

    proxyLength = proxyStringUTF8.length();
    mig_allocate(reinterpret_cast<vm_address_t*>(&proxyData), proxyLength);
    memcpy(proxyData, proxyStringUTF8.data(), proxyLength);
    
    return true;
}
    
bool NetscapePluginInstanceProxy::getAuthenticationInfo(data_t protocolData, data_t hostData, uint32_t port, data_t schemeData, data_t realmData, 
                                                        data_t& usernameData, mach_msg_type_number_t& usernameLength, data_t& passwordData, mach_msg_type_number_t& passwordLength)
{
    WebCore::CString username;
    WebCore::CString password;
    
    if (!WebKit::getAuthenticationInfo(protocolData, hostData, port, schemeData, realmData, username, password))
        return false;
    
    usernameLength = username.length();
    mig_allocate(reinterpret_cast<vm_address_t*>(&usernameData), usernameLength);
    memcpy(usernameData, username.data(), usernameLength);
    
    passwordLength = password.length();
    mig_allocate(reinterpret_cast<vm_address_t*>(&passwordData), passwordLength);
    memcpy(passwordData, password.data(), passwordLength);
    
    return true;
}

bool NetscapePluginInstanceProxy::convertPoint(double sourceX, double sourceY, NPCoordinateSpace sourceSpace, 
                                               double& destX, double& destY, NPCoordinateSpace destSpace)
{
    ASSERT(m_pluginView);

    return [m_pluginView convertFromX:sourceX andY:sourceY space:sourceSpace toX:&destX andY:&destY space:destSpace];
}

uint32_t NetscapePluginInstanceProxy::checkIfAllowedToLoadURL(const char* url, const char* target)
{
    uint32_t checkID;
    
    // Assign a check ID
    do {
        checkID = ++m_urlCheckCounter;
    } while (m_urlChecks.contains(checkID) || !m_urlCheckCounter);

    NSString *frameName = target ? [NSString stringWithCString:target encoding:NSISOLatin1StringEncoding] : nil;

    NSNumber *contextInfo = [[NSNumber alloc] initWithUnsignedInt:checkID];
    WebPluginContainerCheck *check = [WebPluginContainerCheck checkWithRequest:[m_pluginView requestWithURLCString:url]
                                                                        target:frameName
                                                                  resultObject:m_pluginView
                                                                      selector:@selector(_containerCheckResult:contextInfo:)
                                                                    controller:m_pluginView 
                                                                   contextInfo:contextInfo];
    
    [contextInfo release];
    m_urlChecks.set(checkID, check);
    [check start];
    
    return checkID;
}

void NetscapePluginInstanceProxy::cancelCheckIfAllowedToLoadURL(uint32_t checkID)
{
    URLCheckMap::iterator it = m_urlChecks.find(checkID);
    if (it == m_urlChecks.end())
        return;
    
    WebPluginContainerCheck *check = it->second.get();
    [check cancel];
    m_urlChecks.remove(it);
}

void NetscapePluginInstanceProxy::checkIfAllowedToLoadURLResult(uint32_t checkID, bool allowed)
{
    _WKPHCheckIfAllowedToLoadURLResult(m_pluginHostProxy->port(), m_pluginID, checkID, allowed);
}

void NetscapePluginInstanceProxy::resolveURL(const char* url, const char* target, data_t& resolvedURLData, mach_msg_type_number_t& resolvedURLLength)
{
    ASSERT(m_pluginView);
    
    WebCore::CString resolvedURL = [m_pluginView resolvedURLStringForURL:url target:target];
    
    resolvedURLLength = resolvedURL.length();
    mig_allocate(reinterpret_cast<vm_address_t*>(&resolvedURLData), resolvedURLLength);
    memcpy(resolvedURLData, resolvedURL.data(), resolvedURLLength);
}

void NetscapePluginInstanceProxy::privateBrowsingModeDidChange(bool isPrivateBrowsingEnabled)
{
    _WKPHPluginInstancePrivateBrowsingModeDidChange(m_pluginHostProxy->port(), m_pluginID, isPrivateBrowsingEnabled);
}

static String& globalExceptionString()
{
    DEFINE_STATIC_LOCAL(String, exceptionString, ());
    return exceptionString;
}

void NetscapePluginInstanceProxy::setGlobalException(const String& exception)
{
    globalExceptionString() = exception;
}

void NetscapePluginInstanceProxy::moveGlobalExceptionToExecState(ExecState* exec)
{
    if (globalExceptionString().isNull())
        return;

    {
        JSLock lock(SilenceAssertionsOnly);
        throwError(exec, GeneralError, globalExceptionString());
    }

    globalExceptionString() = UString();
}

} // namespace WebKit

#endif // USE(PLUGIN_HOST_PROCESS)
