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

#import "WebHostedNetscapePluginView.h"

#import "NetscapePluginInstanceProxy.h"
#import "NetscapePluginHostManager.h"
#import "NetscapePluginHostProxy.h"
#import "WebKitSystemInterface.h"
#import "WebView.h"
#import "WebViewInternal.h"
#import "WebUIDelegate.h"

#import <CoreFoundation/CoreFoundation.h>
#import <WebCore/WebCoreObjCExtras.h>
#import <wtf/Assertions.h>

using namespace WebKit;

extern "C" {
#include "WebKitPluginClientServer.h"
#include "WebKitPluginHost.h"
}

@implementation WebHostedNetscapePluginView

+ (void)initialize
{
    WebCoreObjCFinalizeOnMainThread(self);
    WKSendUserChangeNotifications();
}

- (id)initWithFrame:(NSRect)frame
      pluginPackage:(WebNetscapePluginPackage *)pluginPackage
                URL:(NSURL *)URL
            baseURL:(NSURL *)baseURL
           MIMEType:(NSString *)MIME
      attributeKeys:(NSArray *)keys
    attributeValues:(NSArray *)values
       loadManually:(BOOL)loadManually
         DOMElement:(DOMElement *)element
{
    self = [super initWithFrame:frame pluginPackage:pluginPackage URL:URL baseURL:baseURL MIMEType:MIME attributeKeys:keys attributeValues:values loadManually:loadManually DOMElement:element];
    if (!self)
        return nil;
    
    return self;
}    

- (void)handleMouseMoved:(NSEvent *)event
{
    if (_isStarted && _proxy)
        _proxy->mouseEvent(self, event, NPCocoaEventMouseMoved);
}

- (void)setAttributeKeys:(NSArray *)keys andValues:(NSArray *)values
{
    ASSERT(!_attributeKeys && !_attributeValues);
    
    _attributeKeys.adoptNS([keys copy]);
    _attributeValues.adoptNS([values copy]);
}    

- (BOOL)createPlugin
{
    ASSERT(!_proxy);

    NetscapePluginHostProxy* pluginHost = NetscapePluginHostManager::shared().hostForPackage(_pluginPackage.get());
    
    if (!pluginHost)
        return NO;

    NSString *userAgent = [[self webView] userAgentForURL:_baseURL.get()];

    _proxy = pluginHost->instantiatePlugin(_MIMEType.get(), _attributeKeys.get(), _attributeValues.get(), userAgent, _sourceURL.get());
    if (!_proxy) 
        return NO;
    
    _pluginLayer = WKMakeRenderLayer(_proxy->renderContextID());
    self.wantsLayer = YES;

    // Update the window frame.
    _proxy->windowFrameChanged([[self window] frame]);
    
    return YES;
}

- (void)setLayer:(CALayer *)newLayer
{
    [super setLayer:newLayer];
    
    if (_pluginLayer)
        [newLayer addSublayer:_pluginLayer.get()];
}

- (void)loadStream
{
}

- (void)updateAndSetWindow
{
    if (!_proxy)
        return;
    
    // Use AppKit to convert view coordinates to NSWindow coordinates.
    NSRect boundsInWindow = [self convertRect:[self bounds] toView:nil];
    NSRect visibleRectInWindow = [self convertRect:[self visibleRect] toView:nil];
    
    // Flip Y to convert NSWindow coordinates to top-left-based window coordinates.
    float borderViewHeight = [[self currentWindow] frame].size.height;
    boundsInWindow.origin.y = borderViewHeight - NSMaxY(boundsInWindow);
    visibleRectInWindow.origin.y = borderViewHeight - NSMaxY(visibleRectInWindow);

    _proxy->resize(boundsInWindow, visibleRectInWindow);
}

- (void)windowFocusChanged:(BOOL)hasFocus
{
    if (_proxy)
        _proxy->windowFocusChanged(hasFocus);
}

- (BOOL)shouldStop
{
    return YES;
}

- (void)destroyPlugin
{
    if (_proxy) {
        _proxy->destroy();
        _proxy = 0;
    }
    
    _pluginLayer = 0;
}

- (void)startTimers
{
    if (_proxy)
        _proxy->startTimers(_isCompletelyObscured);
}

- (void)stopTimers
{
    if (_proxy)
        _proxy->stopTimers();
}

- (void)focusChanged
{
    if (_proxy)
        _proxy->focusChanged(_hasFocus);
}

- (void)windowFrameDidChange:(NSNotification *)notification 
{
    if (_proxy && [self window])
        _proxy->windowFrameChanged([[self window] frame]);
}

- (void)addWindowObservers
{
    [super addWindowObservers];
    
    ASSERT([self window]);
    
    NSWindow *theWindow = [self window];
    
    NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
    [notificationCenter addObserver:self selector:@selector(windowFrameDidChange:) 
                               name:NSWindowDidMoveNotification object:theWindow];
    [notificationCenter addObserver:self selector:@selector(windowFrameDidChange:)
                               name:NSWindowDidResizeNotification object:theWindow];    
}

- (void)removeWindowObservers
{
    [super removeWindowObservers];
    
    NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
    [notificationCenter removeObserver:self name:NSWindowDidMoveNotification object:nil];
    [notificationCenter removeObserver:self name:NSWindowDidResizeNotification object:nil];
}
    
- (void)mouseDown:(NSEvent *)event
{
    if (_isStarted && _proxy)
        _proxy->mouseEvent(self, event, NPCocoaEventMouseDown);
}

- (void)mouseUp:(NSEvent *)event
{
    if (_isStarted && _proxy)
        _proxy->mouseEvent(self, event, NPCocoaEventMouseUp);
}

- (void)mouseDragged:(NSEvent *)event
{
    if (_isStarted && _proxy)
        _proxy->mouseEvent(self, event, NPCocoaEventMouseDragged);
}

- (void)mouseEntered:(NSEvent *)event
{
    if (_isStarted && _proxy)
        _proxy->mouseEvent(self, event, NPCocoaEventMouseEntered);
}

- (void)mouseExited:(NSEvent *)event
{
    if (_isStarted && _proxy)
        _proxy->mouseEvent(self, event, NPCocoaEventMouseExited);
}

@end

#endif
