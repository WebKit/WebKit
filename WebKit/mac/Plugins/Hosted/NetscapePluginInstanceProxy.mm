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

#import "HostedNetscapePluginStream.h"
#import "NetscapePluginHostProxy.h"
#import "WebDataSourceInternal.h"
#import "WebFrameInternal.h"
#import "WebHostedNetscapePluginView.h"
#import "WebNSDataExtras.h"
#import "WebNSURLExtras.h"
#import "WebKitNSStringExtras.h"
#import "WebPluginRequest.h"
#import "WebViewInternal.h"
#import "WebUIDelegate.h"
#import "WebUIDelegatePrivate.h"

#import <mach/mach.h>
#import <WebCore/DocumentLoader.h>
#import <WebCore/Frame.h>
#import <WebCore/FrameLoader.h>
#import <WebCore/FrameTree.h>
#import <WebCore/ScriptController.h>
#import <WebCore/ScriptValue.h>
#include <runtime/JSLock.h>
#import <utility>

extern "C" {
#import "WebKitPluginClientServer.h"
#import "WebKitPluginHost.h"
}

using namespace JSC;
using namespace std;
using namespace WebCore;

namespace WebKit {

class NetscapePluginInstanceProxy::PluginRequest {
public:
    PluginRequest(uint32_t requestID, NSURLRequest *request, NSString *frameName, bool didStartFromUserGesture)
        : m_requestID(requestID)
        , m_request(request)
        , m_frameName(frameName)
        , m_didStartFromUserGesture(didStartFromUserGesture)
    {
    }
    
    uint32_t requestID() const { return m_requestID; }
    NSURLRequest *request() const { return m_request.get(); }
    NSString *frameName() const { return m_frameName.get(); }
    bool didStartFromUserGesture() const { return m_didStartFromUserGesture; }
    
private:
    uint32_t m_requestID;
    RetainPtr<NSURLRequest *> m_request;
    RetainPtr<NSString *> m_frameName;
    bool m_didStartFromUserGesture;
};

static uint32_t pluginIDCounter;

NetscapePluginInstanceProxy::NetscapePluginInstanceProxy(NetscapePluginHostProxy* pluginHostProxy, WebHostedNetscapePluginView *pluginView)
    : m_pluginHostProxy(pluginHostProxy)
    , m_pluginView(pluginView)
    , m_requestTimer(this, &NetscapePluginInstanceProxy::requestTimerFired)
    , m_currentRequestID(0)
    , m_renderContextID(0)
    , m_useSoftwareRenderer(false)
    , m_waitingForReply(false)
    , m_objectIDCounter(0)
{
    ASSERT(m_pluginView);
    
    // Assign a plug-in ID.
    do {
        m_pluginID = ++pluginIDCounter;
    } while (pluginHostProxy->pluginInstance(m_pluginID) || !m_pluginID);
    
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

void NetscapePluginInstanceProxy::stopAllStreams()
{
    Vector<RefPtr<HostedNetscapePluginStream> > streamsCopy;
    copyValuesToVector(m_streams, streamsCopy);
    for (size_t i = 0; i < streamsCopy.size(); i++)
        streamsCopy[i]->stop();
}

void NetscapePluginInstanceProxy::destroy()
{
    stopAllStreams();
    
    _WKPHDestroyPluginInstance(m_pluginHostProxy->port(), m_pluginID);
    
    // Clear the object map, this will cause any outstanding JS objects that the plug-in had a reference to 
    // to go away when the next garbage collection takes place.
    m_objects.clear();
    
    m_pluginHostProxy->removePluginInstance(this);
    m_pluginHostProxy = 0;
}

HostedNetscapePluginStream *NetscapePluginInstanceProxy::pluginStream(uint32_t streamID)
{
    return m_streams.get(streamID).get();
}

void NetscapePluginInstanceProxy::disconnectStream(HostedNetscapePluginStream* stream)
{
    m_streams.remove(stream->streamID());
}
    
void NetscapePluginInstanceProxy::pluginHostDied()
{
    stopAllStreams();

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
    
void NetscapePluginInstanceProxy::keyEvent(NSView *pluginView, NSEvent *event, NPCocoaEventType type)
{
    NSData *charactersData = [[event characters] dataUsingEncoding:NSUTF8StringEncoding];
    NSData *charactersIgnoringModifiersData = [[event charactersIgnoringModifiers] dataUsingEncoding:NSUTF8StringEncoding];
    
    _WKPHPluginInstanceKeyboardEvent(m_pluginHostProxy->port(), m_pluginID,
                                     [event timestamp], 
                                     type, [event modifierFlags], 
                                     const_cast<char*>(reinterpret_cast<const char*>([charactersData bytes])), [charactersData length], 
                                     const_cast<char*>(reinterpret_cast<const char*>([charactersIgnoringModifiersData bytes])), [charactersIgnoringModifiersData length], 
                                     [event isARepeat], [event keyCode]);
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
    
    return loadRequest(request, target, flags & CurrentEventIsUserGesture, streamID);
}

void NetscapePluginInstanceProxy::performRequest(PluginRequest* pluginRequest)
{
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

            if (!newWebView)
                return;
            
            frame = [newWebView mainFrame];
            core(frame)->tree()->setName(frameName);
            [[newWebView _UIDelegateForwarder] webViewShow:newWebView];
        }
    }

    if (JSString) {
        ASSERT(!frame || [m_pluginView webFrame] == frame);
        evaluateJavaScript(pluginRequest);
    } else
        [frame loadRequest:request];
}

void NetscapePluginInstanceProxy::evaluateJavaScript(PluginRequest* pluginRequest)
{
    NSURL *URL = [pluginRequest->request() URL];
    NSString *JSString = [URL _webkit_scriptIfJavaScriptURL];
    ASSERT(JSString);
    
    NSString *result = [[m_pluginView webFrame] _stringByEvaluatingJavaScriptFromString:JSString forceUserGesture:pluginRequest->didStartFromUserGesture()];
    
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
    
    PluginRequest* request = m_pluginRequests.first();
    m_pluginRequests.removeFirst();
    
    if (!m_pluginRequests.isEmpty())
        m_requestTimer.startOneShot(0);
    
    performRequest(request);
    delete request;
}
    
NPError NetscapePluginInstanceProxy::loadRequest(NSURLRequest *request, const char* cTarget, bool currentEventIsUserGesture, uint32_t& requestID)
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
    
    // FIXME: Handle wraparound
    requestID = ++m_currentRequestID;
        
    if (cTarget || JSString) {
        // Make when targetting a frame or evaluating a JS string, perform the request after a delay because we don't
        // want to potentially kill the plug-in inside of its URL request.
        
        if (JSString && target && [frame findFrameNamed:target] != frame) {
            // For security reasons, only allow JS requests to be made on the frame that contains the plug-in.
            return NPERR_INVALID_PARAM;
        }

        PluginRequest* pluginRequest = new PluginRequest(requestID, request, target, currentEventIsUserGesture);
        m_pluginRequests.append(pluginRequest);
        m_requestTimer.startOneShot(0);
    } else {
        RefPtr<HostedNetscapePluginStream> stream = HostedNetscapePluginStream::create(this, requestID, request);

        m_streams.add(requestID, stream);
        stream->start();
    }
    
    return NPERR_NO_ERROR;
}

void NetscapePluginInstanceProxy::processRequestsAndWaitForReply()
{
    while (!m_currentReply.get()) {
        kern_return_t kr = mach_msg_server_once(WebKitPluginClient_server, WKPCWebKitPluginClient_subsystem.maxsize + MAX_TRAILER_SIZE, m_pluginHostProxy->clientPort(), 0);
        if (kr != KERN_SUCCESS) {
            m_currentReply.reset();
            break;
        }
    }
}
    
uint32_t NetscapePluginInstanceProxy::idForObject(JSC::JSObject* object)
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
    
    if (!frame->script()->isEnabled())
        objectID = 0;
    else
        objectID = idForObject(frame->script()->windowShell()->window());
        
    return true;
}
    
void NetscapePluginInstanceProxy::releaseObject(uint32_t objectID)
{
    m_objects.remove(objectID);
}
 
bool NetscapePluginInstanceProxy::evaluate(uint32_t objectID, const String& script, data_t& resultData, mach_msg_type_number_t& resultLength)
{
    resultData = 0;
    resultLength = 0;

    if (!m_objects.contains(objectID))
        return false;

    Frame* frame = core([m_pluginView webFrame]);
    if (!frame)
        return false;
    
    ExecState* exec = frame->script()->globalObject()->globalExec();
    JSValuePtr value = frame->loader()->executeScript(script).jsValue();
    
    marshalValue(exec, value, resultData, resultLength);
    return true;
}

bool NetscapePluginInstanceProxy::invoke(uint32_t objectID, const Identifier& methodName, data_t argumentsData, mach_msg_type_number_t argumentsLength, data_t& resultData, mach_msg_type_number_t& resultLength)
{
    resultData = 0;
    resultLength = 0;
    
    JSObject* object = m_objects.get(objectID);
    if (!object)
        return false;
    
    Frame* frame = core([m_pluginView webFrame]);
    if (!frame)
        return false;
    
    ExecState* exec = frame->script()->globalObject()->globalExec();
    JSLock lock(false);
    JSValuePtr function = object->get(exec, methodName);
    CallData callData;
    CallType callType = function->getCallData(callData);
    if (callType == CallTypeNone)
        return false;
    
    ArgList argList;
    demarshalValues(exec, argumentsData, argumentsLength, argList);

    ProtectedPtr<JSGlobalObject> globalObject = frame->script()->globalObject();
    globalObject->startTimeoutCheck();
    JSValuePtr value = call(exec, function, callType, callData, object, argList);
    globalObject->stopTimeoutCheck();
    
    exec->clearException();
    
    marshalValue(exec, value, resultData, resultLength);
    return true;
}

bool NetscapePluginInstanceProxy::removeProperty(uint32_t objectID, const JSC::Identifier& propertyName)
{
    JSObject* object = m_objects.get(objectID);
    if (!object)
        return false;
    
    Frame* frame = core([m_pluginView webFrame]);
    if (!frame)
        return false;

    ExecState* exec = frame->script()->globalObject()->globalExec();
    if (!object->hasProperty(exec, propertyName)) {
        exec->clearException();
        return false;
    }
    
    JSLock lock(false);
    object->deleteProperty(exec, propertyName);
    exec->clearException();    
    return true;
}
    
bool NetscapePluginInstanceProxy::removeProperty(uint32_t objectID, unsigned propertyName)
{
    JSObject* object = m_objects.get(objectID);
    if (!object)
        return false;
    
    Frame* frame = core([m_pluginView webFrame]);
    if (!frame)
        return false;
    
    ExecState* exec = frame->script()->globalObject()->globalExec();
    if (!object->hasProperty(exec, propertyName)) {
        exec->clearException();
        return false;
    }
    
    JSLock lock(false);
    object->deleteProperty(exec, propertyName);
    exec->clearException();    
    return true;
}

bool NetscapePluginInstanceProxy::hasProperty(uint32_t objectID, const JSC::Identifier& propertyName)
{
    JSObject* object = m_objects.get(objectID);
    if (!object)
        return false;
    
    Frame* frame = core([m_pluginView webFrame]);
    if (!frame)
        return false;
    
    ExecState* exec = frame->script()->globalObject()->globalExec();
    bool result = object->hasProperty(exec, propertyName);
    exec->clearException();
    
    return result;
}

bool NetscapePluginInstanceProxy::hasProperty(uint32_t objectID, unsigned propertyName)
{
    JSObject* object = m_objects.get(objectID);
    if (!object)
        return false;
    
    Frame* frame = core([m_pluginView webFrame]);
    if (!frame)
        return false;
    
    ExecState* exec = frame->script()->globalObject()->globalExec();
    bool result = object->hasProperty(exec, propertyName);
    exec->clearException();
    
    return result;
}
    
bool NetscapePluginInstanceProxy::hasMethod(uint32_t objectID, const JSC::Identifier& methodName)
{
    JSObject* object = m_objects.get(objectID);
    if (!object)
        return false;

    Frame* frame = core([m_pluginView webFrame]);
    if (!frame)
        return false;
    
    ExecState* exec = frame->script()->globalObject()->globalExec();
    JSLock lock(false);
    JSValuePtr func = object->get(exec, methodName);
    exec->clearException();
    return !func->isUndefined();
}

void NetscapePluginInstanceProxy::marshalValue(ExecState* exec, JSValuePtr value, data_t& resultData, mach_msg_type_number_t& resultLength)
{
    RetainPtr<NSMutableArray*> array(AdoptNS, [[NSMutableArray alloc] init]);
    
    JSLock lock(false);

    if (value->isString()) {
        [array.get() addObject:[NSNumber numberWithInt:StringValueType]];
        
        [array.get() addObject:String(value->toString(exec))];
    } else if (value->isNumber()) {
    } else if (value->isBoolean()) {
    } else if (value->isNull()) {
    } else if (value->isObject()) {
        [array.get() addObject:[NSNumber numberWithInt:ObjectValueType]];
        
        uint32_t objectID = idForObject(asObject(value));
        
        [array.get() addObject:[NSNumber numberWithInt:objectID]];
    } else {
        [array.get() addObject:[NSNumber numberWithInt:VoidValueType]];
    }

    RetainPtr<NSData*> data = [NSPropertyListSerialization dataFromPropertyList:array.get() format:NSPropertyListBinaryFormat_v1_0 errorDescription:0];
    ASSERT(data);
    
    resultLength = [data.get() length];
    mig_allocate(reinterpret_cast<vm_address_t*>(&resultData), resultLength);
    
    memcpy(resultData, [data.get() bytes], resultLength);
}

bool NetscapePluginInstanceProxy::demarshalValueFromArray(ExecState* exec, NSArray *array, NSUInteger& index, JSValuePtr& result)
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
        case ObjectValueType: {
            uint32_t objectID = [[array objectAtIndex:index++] intValue];
            
            result = m_objects.get(objectID);
            return true;
        }
        default:
            return false;
    }
}
    
void NetscapePluginInstanceProxy::demarshalValues(ExecState* exec, data_t valuesData, mach_msg_type_number_t valuesLength, ArgList& result)
{
    RetainPtr<NSData*> data(AdoptNS, [[NSData alloc] initWithBytesNoCopy:valuesData length:valuesLength freeWhenDone:NO]);
    
    RetainPtr<NSArray*> array = [NSPropertyListSerialization propertyListFromData:data.get()
                                                                 mutabilityOption:NSPropertyListImmutable
                                                                           format:0
                                                                 errorDescription:0];
    NSUInteger position = 0;
    JSValuePtr value;
    while (demarshalValueFromArray(exec, array.get(), position, value))
        result.append(value);
}

} // namespace WebKit

#endif // USE(PLUGIN_HOST_PROCESS)
