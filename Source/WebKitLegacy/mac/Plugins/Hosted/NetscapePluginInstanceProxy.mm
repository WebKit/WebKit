/*
 * Copyright (C) 2008-2019 Apple Inc. All Rights Reserved.
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

#if USE(PLUGIN_HOST_PROCESS) && ENABLE(NETSCAPE_PLUGIN_API)

#import "NetscapePluginInstanceProxy.h"

#import "HostedNetscapePluginStream.h"
#import "NetscapePluginHostProxy.h"
#import "ProxyInstance.h"
#import "ProxyRuntimeObject.h"
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
#import <JavaScriptCore/CatchScope.h>
#import <JavaScriptCore/Completion.h>
#import <JavaScriptCore/Error.h>
#import <JavaScriptCore/JSGlobalObjectInlines.h>
#import <JavaScriptCore/JSLock.h>
#import <JavaScriptCore/PropertyNameArray.h>
#import <JavaScriptCore/SourceCode.h>
#import <JavaScriptCore/StrongInlines.h>
#import <WebCore/CookieJar.h>
#import <WebCore/Document.h>
#import <WebCore/DocumentLoader.h>
#import <WebCore/Frame.h>
#import <WebCore/FrameLoader.h>
#import <WebCore/FrameTree.h>
#import <WebCore/Page.h>
#import <WebCore/PlatformEventFactoryMac.h>
#import <WebCore/ProxyServer.h>
#import <WebCore/ScriptController.h>
#import <WebCore/SecurityOrigin.h>
#import <WebCore/UserGestureIndicator.h>
#import <WebCore/npruntime_impl.h>
#import <WebCore/runtime_object.h>
#import <mach/mach.h>
#import <utility>
#import <wtf/NeverDestroyed.h>
#import <wtf/RefCountedLeakCounter.h>
#import <wtf/URL.h>
#import <wtf/text/CString.h>

extern "C" {
#import "WebKitPluginClientServer.h"
#import "WebKitPluginHost.h"
}

using namespace JSC;
using namespace JSC::Bindings;
using namespace WebCore;

namespace WebKit {

class NetscapePluginInstanceProxy::PluginRequest : public RefCounted<NetscapePluginInstanceProxy::PluginRequest> {
public:
    static Ref<PluginRequest> create(uint32_t requestID, NSURLRequest* request, NSString* frameName, bool allowPopups)
    {
        return adoptRef(*new PluginRequest(requestID, request, frameName, allowPopups));
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
    RetainPtr<NSURLRequest> m_request;
    RetainPtr<NSString> m_frameName;
    bool m_allowPopups;
};

NetscapePluginInstanceProxy::LocalObjectMap::LocalObjectMap()
    : m_objectIDCounter(0)
{
}

NetscapePluginInstanceProxy::LocalObjectMap::~LocalObjectMap()
{
}

inline bool NetscapePluginInstanceProxy::LocalObjectMap::contains(uint32_t objectID) const
{
    return m_idToJSObjectMap.contains(objectID);
}

inline JSC::JSObject* NetscapePluginInstanceProxy::LocalObjectMap::get(uint32_t objectID) const
{
    if (objectID == HashTraits<uint32_t>::emptyValue() || HashTraits<uint32_t>::isDeletedValue(objectID))
        return nullptr;

    return m_idToJSObjectMap.get(objectID).get();
}

uint32_t NetscapePluginInstanceProxy::LocalObjectMap::idForObject(VM& vm, JSObject* object)
{
    // This method creates objects with refcount of 1, but doesn't increase refcount when returning
    // found objects. This extra count accounts for the main "reference" kept by plugin process.

    // To avoid excessive IPC, plugin process doesn't send each NPObject release/retain call to
    // Safari. It only sends one when the last reference is removed, and it can destroy the proxy
    // NPObject.

    // However, the browser may be sending the same object out to plug-in as a function call
    // argument at the same time - neither side can know what the other one is doing. So,
    // is to make PCForgetBrowserObject call return a boolean result, making it possible for 
    // the browser to make plugin host keep the proxy with zero refcount for a little longer.

    uint32_t objectID = 0;
    
    HashMap<JSC::JSObject*, std::pair<uint32_t, uint32_t>>::iterator iter = m_jsObjectToIDMap.find(object);
    if (iter != m_jsObjectToIDMap.end())
        return iter->value.first;
    
    do {
        objectID = ++m_objectIDCounter;
    } while (!m_objectIDCounter || m_objectIDCounter == static_cast<uint32_t>(-1) || m_idToJSObjectMap.contains(objectID));

    m_idToJSObjectMap.set(objectID, Strong<JSObject>(vm, object));
    m_jsObjectToIDMap.set(object, std::make_pair(objectID, 1));

    return objectID;
}

void NetscapePluginInstanceProxy::LocalObjectMap::retain(JSC::JSObject* object)
{
    HashMap<JSC::JSObject*, std::pair<uint32_t, uint32_t>>::iterator iter = m_jsObjectToIDMap.find(object);
    ASSERT(iter != m_jsObjectToIDMap.end());

    iter->value.second = iter->value.second + 1;
}

void NetscapePluginInstanceProxy::LocalObjectMap::release(JSC::JSObject* object)
{
    HashMap<JSC::JSObject*, std::pair<uint32_t, uint32_t>>::iterator iter = m_jsObjectToIDMap.find(object);
    ASSERT(iter != m_jsObjectToIDMap.end());

    ASSERT(iter->value.second > 0);
    iter->value.second = iter->value.second - 1;
    if (!iter->value.second) {
        m_idToJSObjectMap.remove(iter->value.first);
        m_jsObjectToIDMap.remove(iter);
    }
}

void NetscapePluginInstanceProxy::LocalObjectMap::clear()
{
    m_idToJSObjectMap.clear();
    m_jsObjectToIDMap.clear();
}

bool NetscapePluginInstanceProxy::LocalObjectMap::forget(uint32_t objectID)
{
    if (objectID == HashTraits<uint32_t>::emptyValue() || HashTraits<uint32_t>::isDeletedValue(objectID)) {
        LOG_ERROR("NetscapePluginInstanceProxy::LocalObjectMap::forget: local object id %u is not valid.", objectID);
        return true;
    }

    HashMap<uint32_t, JSC::Strong<JSC::JSObject>>::iterator iter = m_idToJSObjectMap.find(objectID);
    if (iter == m_idToJSObjectMap.end()) {
        LOG_ERROR("NetscapePluginInstanceProxy::LocalObjectMap::forget: local object %u doesn't exist.", objectID);
        return true;
    }

    HashMap<JSC::JSObject*, std::pair<uint32_t, uint32_t>>::iterator rIter = m_jsObjectToIDMap.find(iter->value.get());

    // If the object is being sent to plug-in right now, then it's not the time to forget.
    if (rIter->value.second != 1)
        return false;

    m_jsObjectToIDMap.remove(rIter);
    m_idToJSObjectMap.remove(iter);
    return true;
}

static uint32_t pluginIDCounter;

bool NetscapePluginInstanceProxy::m_inDestroy;

DEFINE_DEBUG_ONLY_GLOBAL(WTF::RefCountedLeakCounter, netscapePluginInstanceProxyCounter, ("NetscapePluginInstanceProxy"));

NetscapePluginInstanceProxy::NetscapePluginInstanceProxy(NetscapePluginHostProxy* pluginHostProxy, WebHostedNetscapePluginView *pluginView, bool fullFramePlugin)
    : m_pluginHostProxy(pluginHostProxy)
    , m_pluginView(pluginView)
    , m_requestTimer(*this, &NetscapePluginInstanceProxy::requestTimerFired)
    , m_currentURLRequestID(0)
    , m_renderContextID(0)
    , m_rendererType(UseSoftwareRenderer)
    , m_waitingForReply(false)
    , m_pluginFunctionCallDepth(0)
    , m_shouldStopSoon(false)
    , m_currentRequestID(0)
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

#ifndef NDEBUG
    netscapePluginInstanceProxyCounter.increment();
#endif
}

Ref<NetscapePluginInstanceProxy> NetscapePluginInstanceProxy::create(NetscapePluginHostProxy* pluginHostProxy, WebHostedNetscapePluginView *pluginView, bool fullFramePlugin)
{
    auto proxy = adoptRef(*new NetscapePluginInstanceProxy(pluginHostProxy, pluginView, fullFramePlugin));
    pluginHostProxy->addPluginInstance(proxy.ptr());
    return proxy;
}

NetscapePluginInstanceProxy::~NetscapePluginInstanceProxy()
{
    ASSERT(!m_pluginHostProxy);
    
    m_pluginID = 0;

#ifndef NDEBUG
    netscapePluginInstanceProxyCounter.decrement();
#endif
}

void NetscapePluginInstanceProxy::resize(NSRect size, NSRect clipRect)
{
    uint32_t requestID = 0;
    
    requestID = nextRequestID();

    _WKPHResizePluginInstance(m_pluginHostProxy->port(), m_pluginID, requestID,
                              size.origin.x, size.origin.y, size.size.width, size.size.height,
                              clipRect.origin.x, clipRect.origin.y, clipRect.size.width, clipRect.size.height);
    
    waitForReply<NetscapePluginInstanceProxy::BooleanReply>(requestID);
}


void NetscapePluginInstanceProxy::setShouldHostLayersInWindowServer(bool shouldHostLayersInWindowServer)
{
    _WKPHPluginShouldHostLayersInWindowServerChanged(m_pluginHostProxy->port(), m_pluginID, shouldHostLayersInWindowServer);
}

void NetscapePluginInstanceProxy::layerHostingModeChanged(bool hostsLayersInWindowServer, uint32_t renderContextID)
{
    setRenderContextID(renderContextID);

    [m_pluginView setHostsLayersInWindowServer:hostsLayersInWindowServer];
}

void NetscapePluginInstanceProxy::stopAllStreams()
{
    for (auto& stream : copyToVector(m_streams.values()))
        stream->stop();
}

void NetscapePluginInstanceProxy::cleanup()
{
    stopAllStreams();
    
    m_requestTimer.stop();
    
    // Clear the object map, this will cause any outstanding JS objects that the plug-in had a reference to 
    // to go away when the next garbage collection takes place.
    m_localObjects.clear();
    
    if (Frame* frame = core([m_pluginView webFrame]))
        frame->script().cleanupScriptObjectsForPlugin((__bridge void*)m_pluginView);
    
    ProxyInstanceSet instances;
    instances.swap(m_instances);
    
    // Invalidate all proxy instances.
    ProxyInstanceSet::const_iterator end = instances.end();
    for (ProxyInstanceSet::const_iterator it = instances.begin(); it != end; ++it)
        (*it)->invalidate();
    
    m_pluginView = nil;
    m_manualStream = nullptr;
}

void NetscapePluginInstanceProxy::invalidate()
{
    // If the plug-in host has died, the proxy will be null.
    if (!m_pluginHostProxy)
        return;
    
    m_pluginHostProxy->removePluginInstance(this);
    m_pluginHostProxy = nullptr;
}

void NetscapePluginInstanceProxy::destroy()
{
    uint32_t requestID = nextRequestID();

    ASSERT(!m_inDestroy);
    m_inDestroy = true;
    
    FrameLoadMap::iterator end = m_pendingFrameLoads.end();
    for (FrameLoadMap::iterator it = m_pendingFrameLoads.begin(); it != end; ++it)
        [(it->key) _setInternalLoadDelegate:nil];

    _WKPHDestroyPluginInstance(m_pluginHostProxy->port(), m_pluginID, requestID);
 
    // If the plug-in host crashes while we're waiting for a reply, the last reference to the instance proxy
    // will go away. Prevent this by protecting it here.
    Ref<NetscapePluginInstanceProxy> protect(*this);
    
    // We don't care about the reply here - we just want to block until the plug-in instance has been torn down.
    waitForReply<NetscapePluginInstanceProxy::BooleanReply>(requestID);

    m_inDestroy = false;
    
    cleanup();
    invalidate();
}

void NetscapePluginInstanceProxy::setManualStream(Ref<HostedNetscapePluginStream>&& manualStream)
{
    ASSERT(!m_manualStream);
    
    m_manualStream = WTFMove(manualStream);
}

bool NetscapePluginInstanceProxy::cancelStreamLoad(uint32_t streamID, NPReason reason) 
{
    HostedNetscapePluginStream* stream;
    
    if (m_manualStream && streamID == 1)
        stream = m_manualStream.get();
    else
        stream = m_streams.get(streamID);
    
    if (!stream)
        return false;
    
    stream->cancelLoad(reason);
    return true;
}

void NetscapePluginInstanceProxy::disconnectStream(HostedNetscapePluginStream* stream)
{
    if (stream == m_manualStream) {
        m_manualStream = nullptr;
        return;
    }

    ASSERT(m_streams.get(stream->streamID()) == stream);
    m_streams.remove(stream->streamID());
}
    
void NetscapePluginInstanceProxy::pluginHostDied()
{
    m_pluginHostProxy = nullptr;

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
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    NSPoint screenPoint = [[event window] convertBaseToScreen:[event locationInWindow]];
    ALLOW_DEPRECATED_DECLARATIONS_END
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
                                     [event isARepeat], [event keyCode], keyCharForEvent(event));
}

void NetscapePluginInstanceProxy::syntheticKeyDownWithCommandModifier(int keyCode, char character)
{
    NSData *charactersData = [NSData dataWithBytes:&character length:1];

    _WKPHPluginInstanceKeyboardEvent(m_pluginHostProxy->port(), m_pluginID,
                                     [NSDate timeIntervalSinceReferenceDate], 
                                     NPCocoaEventKeyDown, NSEventModifierFlagCommand,
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
    
    auto reply = waitForReply<NetscapePluginInstanceProxy::BooleanReply>(requestID);
    return reply && reply->m_result;
}

void NetscapePluginInstanceProxy::print(CGContextRef context, unsigned width, unsigned height)
{
    uint32_t requestID = nextRequestID();
    _WKPHPluginInstancePrint(m_pluginHostProxy->port(), m_pluginID, requestID, width, height);
    
    auto reply = waitForReply<NetscapePluginInstanceProxy::BooleanAndDataReply>(requestID);
    if (!reply || !reply->m_returnValue)
        return;

    RetainPtr<CGDataProvider> dataProvider = adoptCF(CGDataProviderCreateWithCFData(reply->m_result.get()));
    RetainPtr<CGColorSpaceRef> colorSpace = adoptCF(CGColorSpaceCreateDeviceRGB());
    RetainPtr<CGImageRef> image = adoptCF(CGImageCreate(width, height, 8, 32, width * 4, colorSpace.get(), kCGImageAlphaFirst, dataProvider.get(), 0, false, kCGRenderingIntentDefault));

    // Flip the context and draw the image.
    CGContextSaveGState(context);
    CGContextTranslateCTM(context, 0.0, height);
    CGContextScaleCTM(context, 1.0, -1.0);
    
    CGContextDrawImage(context, CGRectMake(0, 0, width, height), image.get());

    CGContextRestoreGState(context);
}

void NetscapePluginInstanceProxy::snapshot(CGContextRef context, unsigned width, unsigned height)
{
    uint32_t requestID = nextRequestID();
    _WKPHPluginInstanceSnapshot(m_pluginHostProxy->port(), m_pluginID, requestID, width, height);
    
    auto reply = waitForReply<NetscapePluginInstanceProxy::BooleanAndDataReply>(requestID);
    if (!reply || !reply->m_returnValue)
        return;

    RetainPtr<CGDataProvider> dataProvider = adoptCF(CGDataProviderCreateWithCFData(reply->m_result.get()));
    RetainPtr<CGColorSpaceRef> colorSpace = adoptCF(CGColorSpaceCreateDeviceRGB());
    RetainPtr<CGImageRef> image = adoptCF(CGImageCreate(width, height, 8, 32, width * 4, colorSpace.get(), kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host, dataProvider.get(), 0, false, kCGRenderingIntentDefault));

    CGContextDrawImage(context, CGRectMake(0, 0, width, height), image.get());
}

void NetscapePluginInstanceProxy::stopTimers()
{
    _WKPHPluginInstanceStopTimers(m_pluginHostProxy->port(), m_pluginID);
}

void NetscapePluginInstanceProxy::status(const char* message)
{
    RetainPtr<CFStringRef> status = adoptCF(CFStringCreateWithCString(0, message ? message : "", kCFStringEncodingUTF8));
    if (!status)
        return;

    WebView *wv = [m_pluginView webView];
    [[wv _UIDelegateForwarder] webView:wv setStatusText:(__bridge NSString *)status.get()];
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
            if (!postData)
                return NPERR_INVALID_PARAM;
            RetainPtr<CFStringRef> bufString = adoptCF(CFStringCreateWithCString(kCFAllocatorDefault, postData, kCFStringEncodingWindowsLatin1));
            if (!bufString)
                return NPERR_INVALID_PARAM;
            
            NSURL *fileURL = [NSURL _web_URLWithDataAsString:(__bridge NSString *)bufString.get()];
            NSString *path;
            if ([fileURL isFileURL])
                path = [fileURL path];
            else
                path = (__bridge NSString *)bufString.get();
            httpBody = [NSData dataWithContentsOfFile:path];
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
                        dataLength = std::min(static_cast<unsigned>([contentLength intValue]), dataLength);
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
    // Loading the request can cause the instance proxy to go away, so protect it.
    Ref<NetscapePluginInstanceProxy> protect(*this);

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
        frame = kit(core([m_pluginView webFrame])->loader().findFrameForNavigation(frameName));
        if (!frame) {
            WebView *currentWebView = [m_pluginView webView];
            WebView *newWebView = [[currentWebView _UIDelegateForwarder] webView:currentWebView createWebViewWithRequest:nil windowFeatures:@{ }];

            if (!newWebView) {
                _WKPHLoadURLNotify(m_pluginHostProxy->port(), m_pluginID, pluginRequest->requestID(), NPERR_GENERIC_ERROR);
                return;
            }
            
            frame = [newWebView mainFrame];
            core(frame)->tree().setName(frameName);
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
        
    PluginRequest* pluginRequest = it->value.get();
    _WKPHLoadURLNotify(m_pluginHostProxy->port(), m_pluginID, pluginRequest->requestID(), reason);
 
    m_pendingFrameLoads.remove(it);

    [webFrame _setInternalLoadDelegate:nil];
}

void NetscapePluginInstanceProxy::evaluateJavaScript(PluginRequest* pluginRequest)
{
    NSURL *URL = [pluginRequest->request() URL];
    NSString *JSString = [URL _webkit_scriptIfJavaScriptURL];
    ASSERT(JSString);

    Ref<NetscapePluginInstanceProxy> protect(*this); // Executing arbitrary JavaScript can destroy the proxy.

    NSString *result = [[m_pluginView webFrame] _stringByEvaluatingJavaScriptFromString:JSString forceUserGesture:pluginRequest->allowPopups()];
    
    // Don't continue if stringByEvaluatingJavaScriptFromString caused the plug-in to stop.
    if (!m_pluginHostProxy)
        return;

    if (pluginRequest->frameName() != nil)
        return;
        
    if ([result length] > 0) {
        // Don't call NPP_NewStream and other stream methods if there is no JS result to deliver. This is what Mozilla does.
        NSData *JSData = [result dataUsingEncoding:NSUTF8StringEncoding];
        
        auto stream = HostedNetscapePluginStream::create(this, pluginRequest->requestID(), pluginRequest->request());
        m_streams.add(stream->streamID(), stream.copyRef());
        
        RetainPtr<NSURLResponse> response = adoptNS([[NSURLResponse alloc] initWithURL:URL 
                                                                             MIMEType:@"text/plain" 
                                                                expectedContentLength:[JSData length]
                                                                     textEncodingName:nil]);
        stream->startStreamWithResponse(response.get());
        stream->didReceiveData(0, static_cast<const char*>([JSData bytes]), [JSData length]);
        stream->didFinishLoading(0);
    }
}

void NetscapePluginInstanceProxy::requestTimerFired()
{
    ASSERT(!m_pluginRequests.isEmpty());
    ASSERT(m_pluginView);
    
    RefPtr<PluginRequest> request = m_pluginRequests.first();
    m_pluginRequests.removeFirst();
    
    if (!m_pluginRequests.isEmpty())
        m_requestTimer.startOneShot(0_s);
    
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
    if (documentLoader != core([m_pluginView webFrame])->loader().activeDocumentLoader() &&
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
        if (!core([m_pluginView webFrame])->document()->securityOrigin().canDisplay(URL))
            return NPERR_GENERIC_ERROR;
    }
    
    // FIXME: Handle wraparound
    requestID = ++m_currentURLRequestID;
        
    if (cTarget || JSString) {
        // Make when targeting a frame or evaluating a JS string, perform the request after a delay because we don't
        // want to potentially kill the plug-in inside of its URL request.
        
        if (JSString && target && [frame findFrameNamed:target] != frame) {
            // For security reasons, only allow JS requests to be made on the frame that contains the plug-in.
            return NPERR_INVALID_PARAM;
        }

        auto pluginRequest = PluginRequest::create(requestID, request, target, allowPopups);
        m_pluginRequests.append(WTFMove(pluginRequest));
        m_requestTimer.startOneShot(0_s);
    } else {
        auto stream = HostedNetscapePluginStream::create(this, requestID, request);

        ASSERT(!m_streams.contains(requestID));
        m_streams.add(requestID, stream.copyRef());
        stream->start();
    }
    
    return NPERR_NO_ERROR;
}

std::unique_ptr<NetscapePluginInstanceProxy::Reply> NetscapePluginInstanceProxy::processRequestsAndWaitForReply(uint32_t requestID)
{
    ASSERT(m_pluginHostProxy);

    std::unique_ptr<Reply> reply;

    while (!(reply = m_replies.take(requestID))) {
        if (!m_pluginHostProxy->processRequests())
            return nullptr;

        // The host proxy can be destroyed while executing a nested processRequests() call, in which case it's normal
        // to get a success result, but be unable to keep looping.
        if (!m_pluginHostProxy)
            return nullptr;
    }

    return reply;
}

// NPRuntime support
bool NetscapePluginInstanceProxy::getWindowNPObject(uint32_t& objectID)
{
    Frame* frame = core([m_pluginView webFrame]);
    if (!frame)
        return false;
    
    if (!frame->script().canExecuteScripts(NotAboutToExecuteScript))
        objectID = 0;
    else
        objectID = m_localObjects.idForObject(pluginWorld().vm(), frame->windowProxy().jsWindowProxy(pluginWorld())->window());
        
    return true;
}

bool NetscapePluginInstanceProxy::getPluginElementNPObject(uint32_t& objectID)
{
    Frame* frame = core([m_pluginView webFrame]);
    if (!frame)
        return false;
    
    if (JSObject* object = frame->script().jsObjectForPluginElement([m_pluginView element]))
        objectID = m_localObjects.idForObject(pluginWorld().vm(), object);
    else
        objectID = 0;
    
    return true;
}

bool NetscapePluginInstanceProxy::forgetBrowserObjectID(uint32_t objectID)
{
    return m_localObjects.forget(objectID);
}
 
bool NetscapePluginInstanceProxy::evaluate(uint32_t objectID, const String& script, data_t& resultData, mach_msg_type_number_t& resultLength, bool allowPopups)
{
    resultData = nullptr;
    resultLength = 0;

    if (m_inDestroy)
        return false;

    if (!m_localObjects.contains(objectID)) {
        LOG_ERROR("NetscapePluginInstanceProxy::evaluate: local object %u doesn't exist.", objectID);
        return false;
    }

    Frame* frame = core([m_pluginView webFrame]);
    if (!frame)
        return false;

    VM& vm = pluginWorld().vm();
    JSLockHolder lock(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);

    Strong<JSGlobalObject> globalObject(vm, frame->script().globalObject(pluginWorld()));

    UserGestureIndicator gestureIndicator(allowPopups ? Optional<ProcessingUserGestureState>(ProcessingUserGesture) : WTF::nullopt);
    
    JSValue result = JSC::evaluate(globalObject.get(), JSC::makeSource(script, { }));
    
    marshalValue(globalObject.get(), result, resultData, resultLength);
    scope.clearException();
    return true;
}

bool NetscapePluginInstanceProxy::invoke(uint32_t objectID, const Identifier& methodName, data_t argumentsData, mach_msg_type_number_t argumentsLength, data_t& resultData, mach_msg_type_number_t& resultLength)
{
    resultData = nullptr;
    resultLength = 0;
    
    if (m_inDestroy)
        return false;
    
    JSObject* object = m_localObjects.get(objectID);
    if (!object) {
        LOG_ERROR("NetscapePluginInstanceProxy::invoke: local object %u doesn't exist.", objectID);
        return false;
    }
    
    Frame* frame = core([m_pluginView webFrame]);
    if (!frame)
        return false;

    VM& vm = pluginWorld().vm();
    JSLockHolder lock(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);

    JSGlobalObject* lexicalGlobalObject = frame->script().globalObject(pluginWorld());
    JSValue function = object->get(lexicalGlobalObject, methodName);
    auto callData = getCallData(vm, function);
    if (callData.type == CallData::Type::None)
        return false;

    MarkedArgumentBuffer argList;
    demarshalValues(lexicalGlobalObject, argumentsData, argumentsLength, argList);
    RELEASE_ASSERT(!argList.hasOverflowed());

    JSValue value = call(lexicalGlobalObject, function, callData, object, argList);
        
    marshalValue(lexicalGlobalObject, value, resultData, resultLength);
    scope.clearException();
    return true;
}

bool NetscapePluginInstanceProxy::invokeDefault(uint32_t objectID, data_t argumentsData, mach_msg_type_number_t argumentsLength, data_t& resultData, mach_msg_type_number_t& resultLength)
{
    if (m_inDestroy)
        return false;

    JSObject* object = m_localObjects.get(objectID);
    if (!object) {
        LOG_ERROR("NetscapePluginInstanceProxy::invokeDefault: local object %u doesn't exist.", objectID);
        return false;
    }
    
    Frame* frame = core([m_pluginView webFrame]);
    if (!frame)
        return false;

    VM& vm = pluginWorld().vm();
    JSLockHolder lock(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);

    JSGlobalObject* lexicalGlobalObject = frame->script().globalObject(pluginWorld());
    auto callData = getCallData(vm, object);
    if (callData.type == CallData::Type::None)
        return false;

    MarkedArgumentBuffer argList;
    demarshalValues(lexicalGlobalObject, argumentsData, argumentsLength, argList);
    RELEASE_ASSERT(!argList.hasOverflowed());

    JSValue value = call(lexicalGlobalObject, object, callData, object, argList);
    
    marshalValue(lexicalGlobalObject, value, resultData, resultLength);
    scope.clearException();
    return true;
}

bool NetscapePluginInstanceProxy::construct(uint32_t objectID, data_t argumentsData, mach_msg_type_number_t argumentsLength, data_t& resultData, mach_msg_type_number_t& resultLength)
{
    if (m_inDestroy)
        return false;

    JSObject* object = m_localObjects.get(objectID);
    if (!object) {
        LOG_ERROR("NetscapePluginInstanceProxy::construct: local object %u doesn't exist.", objectID);
        return false;
    }
    
    Frame* frame = core([m_pluginView webFrame]);
    if (!frame)
        return false;

    VM& vm = pluginWorld().vm();
    JSLockHolder lock(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);

    JSGlobalObject* lexicalGlobalObject = frame->script().globalObject(pluginWorld());

    auto constructData = getConstructData(vm, object);
    if (constructData.type == CallData::Type::None)
        return false;

    MarkedArgumentBuffer argList;
    demarshalValues(lexicalGlobalObject, argumentsData, argumentsLength, argList);
    RELEASE_ASSERT(!argList.hasOverflowed());

    JSValue value = JSC::construct(lexicalGlobalObject, object, constructData, argList);
    
    marshalValue(lexicalGlobalObject, value, resultData, resultLength);
    scope.clearException();
    return true;
}

bool NetscapePluginInstanceProxy::getProperty(uint32_t objectID, const Identifier& propertyName, data_t& resultData, mach_msg_type_number_t& resultLength)
{
    if (m_inDestroy)
        return false;

    JSObject* object = m_localObjects.get(objectID);
    if (!object) {
        LOG_ERROR("NetscapePluginInstanceProxy::getProperty: local object %u doesn't exist.", objectID);
        return false;
    }
    
    Frame* frame = core([m_pluginView webFrame]);
    if (!frame)
        return false;

    VM& vm = pluginWorld().vm();
    JSLockHolder lock(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);

    JSGlobalObject* lexicalGlobalObject = frame->script().globalObject(pluginWorld());
    JSValue value = object->get(lexicalGlobalObject, propertyName);
    
    marshalValue(lexicalGlobalObject, value, resultData, resultLength);
    scope.clearException();
    return true;
}
    
bool NetscapePluginInstanceProxy::getProperty(uint32_t objectID, unsigned propertyName, data_t& resultData, mach_msg_type_number_t& resultLength)
{
    JSObject* object = m_localObjects.get(objectID);
    if (!object) {
        LOG_ERROR("NetscapePluginInstanceProxy::getProperty: local object %u doesn't exist.", objectID);
        return false;
    }
    
    Frame* frame = core([m_pluginView webFrame]);
    if (!frame)
        return false;

    VM& vm = pluginWorld().vm();
    JSLockHolder lock(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);

    JSGlobalObject* lexicalGlobalObject = frame->script().globalObject(pluginWorld());
    JSValue value = object->get(lexicalGlobalObject, propertyName);
    
    marshalValue(lexicalGlobalObject, value, resultData, resultLength);
    scope.clearException();
    return true;
}

bool NetscapePluginInstanceProxy::setProperty(uint32_t objectID, const Identifier& propertyName, data_t valueData, mach_msg_type_number_t valueLength)
{
    if (m_inDestroy)
        return false;

    JSObject* object = m_localObjects.get(objectID);
    if (!object) {
        LOG_ERROR("NetscapePluginInstanceProxy::setProperty: local object %u doesn't exist.", objectID);
        return false;
    }
    
    Frame* frame = core([m_pluginView webFrame]);
    if (!frame)
        return false;

    VM& vm = pluginWorld().vm();
    JSLockHolder lock(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);

    JSGlobalObject* lexicalGlobalObject = frame->script().globalObject(pluginWorld());

    JSValue value = demarshalValue(lexicalGlobalObject, valueData, valueLength);
    PutPropertySlot slot(object);
    object->methodTable(vm)->put(object, lexicalGlobalObject, propertyName, value, slot);
    
    scope.clearException();
    return true;
}

bool NetscapePluginInstanceProxy::setProperty(uint32_t objectID, unsigned propertyName, data_t valueData, mach_msg_type_number_t valueLength)
{
    if (m_inDestroy)
        return false;

    JSObject* object = m_localObjects.get(objectID);
    if (!object) {
        LOG_ERROR("NetscapePluginInstanceProxy::setProperty: local object %u doesn't exist.", objectID);
        return false;
    }
    
    Frame* frame = core([m_pluginView webFrame]);
    if (!frame)
        return false;

    VM& vm = pluginWorld().vm();
    JSLockHolder lock(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);

    JSGlobalObject* lexicalGlobalObject = frame->script().globalObject(pluginWorld());
    
    JSValue value = demarshalValue(lexicalGlobalObject, valueData, valueLength);
    object->methodTable(vm)->putByIndex(object, lexicalGlobalObject, propertyName, value, false);
    
    scope.clearException();
    return true;
}

bool NetscapePluginInstanceProxy::removeProperty(uint32_t objectID, const Identifier& propertyName)
{
    if (m_inDestroy)
        return false;

    JSObject* object = m_localObjects.get(objectID);
    if (!object) {
        LOG_ERROR("NetscapePluginInstanceProxy::removeProperty: local object %u doesn't exist.", objectID);
        return false;
    }
    
    Frame* frame = core([m_pluginView webFrame]);
    if (!frame)
        return false;

    VM& vm = pluginWorld().vm();
    JSLockHolder lock(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);

    JSGlobalObject* lexicalGlobalObject = frame->script().globalObject(pluginWorld());
    if (!object->hasProperty(lexicalGlobalObject, propertyName)) {
        scope.clearException();
        return false;
    }

    JSCell::deleteProperty(object, lexicalGlobalObject, propertyName);
    scope.clearException();
    return true;
}
    
bool NetscapePluginInstanceProxy::removeProperty(uint32_t objectID, unsigned propertyName)
{
    if (m_inDestroy)
        return false;

    JSObject* object = m_localObjects.get(objectID);
    if (!object) {
        LOG_ERROR("NetscapePluginInstanceProxy::removeProperty: local object %u doesn't exist.", objectID);
        return false;
    }
    
    Frame* frame = core([m_pluginView webFrame]);
    if (!frame)
        return false;

    VM& vm = pluginWorld().vm();
    JSLockHolder lock(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);

    JSGlobalObject* lexicalGlobalObject = frame->script().globalObject(pluginWorld());
    if (!object->hasProperty(lexicalGlobalObject, propertyName)) {
        scope.clearException();
        return false;
    }
    
    object->methodTable(vm)->deletePropertyByIndex(object, lexicalGlobalObject, propertyName);
    scope.clearException();
    return true;
}

bool NetscapePluginInstanceProxy::hasProperty(uint32_t objectID, const Identifier& propertyName)
{
    if (m_inDestroy)
        return false;

    JSObject* object = m_localObjects.get(objectID);
    if (!object) {
        LOG_ERROR("NetscapePluginInstanceProxy::hasProperty: local object %u doesn't exist.", objectID);
        return false;
    }
    
    Frame* frame = core([m_pluginView webFrame]);
    if (!frame)
        return false;

    VM& vm = pluginWorld().vm();
    JSLockHolder lock(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);

    JSGlobalObject* lexicalGlobalObject = frame->script().globalObject(pluginWorld());
    bool result = object->hasProperty(lexicalGlobalObject, propertyName);
    scope.clearException();

    return result;
}

bool NetscapePluginInstanceProxy::hasProperty(uint32_t objectID, unsigned propertyName)
{
    if (m_inDestroy)
        return false;

    JSObject* object = m_localObjects.get(objectID);
    if (!object) {
        LOG_ERROR("NetscapePluginInstanceProxy::hasProperty: local object %u doesn't exist.", objectID);
        return false;
    }
    
    Frame* frame = core([m_pluginView webFrame]);
    if (!frame)
        return false;

    VM& vm = pluginWorld().vm();
    JSLockHolder lock(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);

    JSGlobalObject* lexicalGlobalObject = frame->script().globalObject(pluginWorld());
    bool result = object->hasProperty(lexicalGlobalObject, propertyName);
    scope.clearException();

    return result;
}
    
bool NetscapePluginInstanceProxy::hasMethod(uint32_t objectID, const Identifier& methodName)
{
    if (m_inDestroy)
        return false;

    JSObject* object = m_localObjects.get(objectID);
    if (!object) {
        LOG_ERROR("NetscapePluginInstanceProxy::hasMethod: local object %u doesn't exist.", objectID);
        return false;
    }

    Frame* frame = core([m_pluginView webFrame]);
    if (!frame)
        return false;

    VM& vm = pluginWorld().vm();
    JSLockHolder lock(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);

    JSGlobalObject* lexicalGlobalObject = frame->script().globalObject(pluginWorld());
    JSValue func = object->get(lexicalGlobalObject, methodName);
    scope.clearException();
    return !func.isUndefined();
}

bool NetscapePluginInstanceProxy::enumerate(uint32_t objectID, data_t& resultData, mach_msg_type_number_t& resultLength)
{
    if (m_inDestroy)
        return false;

    JSObject* object = m_localObjects.get(objectID);
    if (!object) {
        LOG_ERROR("NetscapePluginInstanceProxy::enumerate: local object %u doesn't exist.", objectID);
        return false;
    }
    
    Frame* frame = core([m_pluginView webFrame]);
    if (!frame)
        return false;

    VM& vm = pluginWorld().vm();
    JSLockHolder lock(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);

    JSGlobalObject* lexicalGlobalObject = frame->script().globalObject(pluginWorld());
 
    PropertyNameArray propertyNames(vm, PropertyNameMode::Strings, PrivateSymbolMode::Exclude);
    object->getPropertyNames(lexicalGlobalObject, propertyNames, DontEnumPropertiesMode::Exclude);

    RetainPtr<NSMutableArray> array = adoptNS([[NSMutableArray alloc] init]);
    for (unsigned i = 0; i < propertyNames.size(); i++) {
        uint64_t methodName = reinterpret_cast<uint64_t>(_NPN_GetStringIdentifier(propertyNames[i].string().utf8().data()));

        [array.get() addObject:[NSNumber numberWithLongLong:methodName]];
    }

    NSData *data = [NSPropertyListSerialization dataWithPropertyList:array.get() format:NSPropertyListBinaryFormat_v1_0 options:0 error:nullptr];
    ASSERT(data);

    resultLength = [data length];
    mig_allocate(reinterpret_cast<vm_address_t*>(&resultData), resultLength);

    memcpy(resultData, [data bytes], resultLength);

    scope.clearException();

    return true;
}

static bool getObjectID(NetscapePluginInstanceProxy* pluginInstanceProxy, JSObject* object, uint64_t& objectID)
{
    JSC::VM& vm = object->vm();
    if (object->classInfo(vm) != ProxyRuntimeObject::info())
        return false;

    ProxyRuntimeObject* runtimeObject = static_cast<ProxyRuntimeObject*>(object);
    ProxyInstance* instance = runtimeObject->getInternalProxyInstance();
    if (!instance)
        return false;

    if (instance->instanceProxy() != pluginInstanceProxy)
        return false;

    objectID = instance->objectID();
    return true;
}
    
void NetscapePluginInstanceProxy::addValueToArray(NSMutableArray *array, JSGlobalObject* lexicalGlobalObject, JSValue value)
{
    VM& vm = lexicalGlobalObject->vm();
    JSLockHolder lock(vm);

    if (value.isString()) {
        [array addObject:@(StringValueType)];
        [array addObject:asString(value)->value(lexicalGlobalObject)];
    } else if (value.isNumber()) {
        [array addObject:@(DoubleValueType)];
        [array addObject:@(value.toNumber(lexicalGlobalObject))];
    } else if (value.isBoolean()) {
        [array addObject:@(BoolValueType)];
        [array addObject:[NSNumber numberWithBool:value.toBoolean(lexicalGlobalObject)]];
    } else if (value.isNull())
        [array addObject:@(NullValueType)];
    else if (value.isObject()) {
        JSObject* object = asObject(value);
        uint64_t objectID;
        if (getObjectID(this, object, objectID)) {
            [array addObject:@(NPObjectValueType)];
            [array addObject:@(objectID)];
        } else {
            [array addObject:@(JSObjectValueType)];
            [array addObject:@(m_localObjects.idForObject(vm, object))];
        }
    } else
        [array addObject:@(VoidValueType)];
}

void NetscapePluginInstanceProxy::marshalValue(JSGlobalObject* lexicalGlobalObject, JSValue value, data_t& resultData, mach_msg_type_number_t& resultLength)
{
    RetainPtr<NSMutableArray> array = adoptNS([[NSMutableArray alloc] init]);
    
    addValueToArray(array.get(), lexicalGlobalObject, value);

    NSData *data = [NSPropertyListSerialization dataWithPropertyList:array.get() format:NSPropertyListBinaryFormat_v1_0 options:0 error:nullptr];
    ASSERT(data);
    
    resultLength = data.length;
    mig_allocate(reinterpret_cast<vm_address_t*>(&resultData), resultLength);
    
    memcpy(resultData, data.bytes, resultLength);
}

RetainPtr<NSData> NetscapePluginInstanceProxy::marshalValues(JSGlobalObject* lexicalGlobalObject, const ArgList& args)
{
    RetainPtr<NSMutableArray> array = adoptNS([[NSMutableArray alloc] init]);

    for (unsigned i = 0; i < args.size(); i++)
        addValueToArray(array.get(), lexicalGlobalObject, args.at(i));

    NSData *data = [NSPropertyListSerialization dataWithPropertyList:array.get() format:NSPropertyListBinaryFormat_v1_0 options:0 error:nullptr];
    ASSERT(data);

    return data;
}    

bool NetscapePluginInstanceProxy::demarshalValueFromArray(JSGlobalObject* lexicalGlobalObject, NSArray *array, NSUInteger& index, JSValue& result)
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
            result = jsNumber([[array objectAtIndex:index++] doubleValue]);
            return true;
        case StringValueType: {
            NSString *string = [array objectAtIndex:index++];
            
            result = jsString(lexicalGlobalObject->vm(), String(string));
            return true;
        }
        case JSObjectValueType: {
            uint32_t objectID = [[array objectAtIndex:index++] intValue];
            
            result = m_localObjects.get(objectID);
            ASSERT(result);
            return true;
        }
        case NPObjectValueType: {
            uint32_t objectID = [[array objectAtIndex:index++] intValue];

            Frame* frame = core([m_pluginView webFrame]);
            if (!frame)
                return false;
            
            if (!frame->script().canExecuteScripts(NotAboutToExecuteScript))
                return false;

            auto rootObject = frame->script().createRootObject((__bridge void*)m_pluginView);
            result = ProxyInstance::create(WTFMove(rootObject), this, objectID)->createRuntimeObject(lexicalGlobalObject);
            return true;
        }
        default:
            ASSERT_NOT_REACHED();
            return false;
    }
}

JSValue NetscapePluginInstanceProxy::demarshalValue(JSGlobalObject* lexicalGlobalObject, const char* valueData, mach_msg_type_number_t valueLength)
{
    RetainPtr<NSData> data = adoptNS([[NSData alloc] initWithBytesNoCopy:(void*)valueData length:valueLength freeWhenDone:NO]);

    NSArray *array = [NSPropertyListSerialization propertyListWithData:data.get() options:NSPropertyListImmutable format:nullptr error:nullptr];

    NSUInteger position = 0;
    JSValue value;
    bool result = demarshalValueFromArray(lexicalGlobalObject, array, position, value);
    ASSERT_UNUSED(result, result);

    return value;
}

void NetscapePluginInstanceProxy::demarshalValues(JSGlobalObject* lexicalGlobalObject, data_t valuesData, mach_msg_type_number_t valuesLength, MarkedArgumentBuffer& result)
{
    RetainPtr<NSData> data = adoptNS([[NSData alloc] initWithBytesNoCopy:valuesData length:valuesLength freeWhenDone:NO]);

    NSArray *array = [NSPropertyListSerialization propertyListWithData:data.get() options:NSPropertyListImmutable format:nullptr error:nullptr];

    NSUInteger position = 0;
    JSValue value;
    while (demarshalValueFromArray(lexicalGlobalObject, array, position, value))
        result.append(value);
}

void NetscapePluginInstanceProxy::retainLocalObject(JSC::JSValue value)
{
    if (!value.isObject() || value.inherits<ProxyRuntimeObject>(value.getObject()->vm()))
        return;

    m_localObjects.retain(asObject(value));
}

void NetscapePluginInstanceProxy::releaseLocalObject(JSC::JSValue value)
{
    if (!value.isObject() || value.inherits<ProxyRuntimeObject>(value.getObject()->vm()))
        return;

    m_localObjects.release(asObject(value));
}

RefPtr<Instance> NetscapePluginInstanceProxy::createBindingsInstance(Ref<RootObject>&& rootObject)
{
    uint32_t requestID = nextRequestID();
    
    if (_WKPHGetScriptableNPObject(m_pluginHostProxy->port(), m_pluginID, requestID) != KERN_SUCCESS)
        return nullptr;

    auto reply = waitForReply<GetScriptableNPObjectReply>(requestID);
    if (!reply)
        return nullptr;

    if (!reply->m_objectID)
        return nullptr;

    // Since the reply was non-null, "this" is still a valid pointer.
    return ProxyInstance::create(WTFMove(rootObject), this, reply->m_objectID);
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
    
void NetscapePluginInstanceProxy::didCallPluginFunction(bool& stopped)
{
    ASSERT(m_pluginFunctionCallDepth > 0);
    m_pluginFunctionCallDepth--;
    
    // If -stop was called while we were calling into a plug-in function, and we're no longer
    // inside a plug-in function, stop now.
    if (!m_pluginFunctionCallDepth && m_shouldStopSoon) {
        m_shouldStopSoon = false;
        [m_pluginView stop];
        stopped = true;
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
        auto* document = frame->document();
        if (!document)
            return false;
        
        auto* page = document->page();
        if (!page)
            return false;

        String cookieString = page->cookieJar().cookies(*document, url);
        WTF::CString cookieStringUTF8 = cookieString.utf8();
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

        auto* document = frame->document();
        if (!document)
            return false;

        auto* page = document->page();
        if (!page)
            return false;
        
        page->cookieJar().setCookies(*document, url, cookieString);
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

    Vector<ProxyServer> proxyServers = proxyServersForURL(url);
    WTF::CString proxyStringUTF8 = toString(proxyServers).utf8();

    proxyLength = proxyStringUTF8.length();
    mig_allocate(reinterpret_cast<vm_address_t*>(&proxyData), proxyLength);
    memcpy(proxyData, proxyStringUTF8.data(), proxyLength);
    
    return true;
}
    
bool NetscapePluginInstanceProxy::getAuthenticationInfo(data_t protocolData, data_t hostData, uint32_t port, data_t schemeData, data_t realmData, 
                                                        data_t& usernameData, mach_msg_type_number_t& usernameLength, data_t& passwordData, mach_msg_type_number_t& passwordLength)
{
    WTF::CString username;
    WTF::CString password;
    
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

void NetscapePluginInstanceProxy::privateBrowsingModeDidChange(bool isPrivateBrowsingEnabled)
{
    _WKPHPluginInstancePrivateBrowsingModeDidChange(m_pluginHostProxy->port(), m_pluginID, isPrivateBrowsingEnabled);
}

static String& globalExceptionString()
{
    static NeverDestroyed<String> exceptionString;
    return exceptionString;
}

void NetscapePluginInstanceProxy::setGlobalException(const String& exception)
{
    globalExceptionString() = exception;
}

void NetscapePluginInstanceProxy::moveGlobalExceptionToExecState(JSGlobalObject* lexicalGlobalObject)
{
    VM& vm = lexicalGlobalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (globalExceptionString().isNull())
        return;

    {
        JSLockHolder lock(vm);
        throwException(lexicalGlobalObject, scope, createError(lexicalGlobalObject, globalExceptionString()));
    }

    globalExceptionString() = String();
}

} // namespace WebKit

#endif // USE(PLUGIN_HOST_PROCESS) && ENABLE(NETSCAPE_PLUGIN_API)
