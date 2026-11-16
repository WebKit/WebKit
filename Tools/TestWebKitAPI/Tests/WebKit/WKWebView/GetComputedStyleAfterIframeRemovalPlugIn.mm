/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"

#import "GetComputedStyleAfterIframeRemovalProtocol.h"
#import <WebKit/WKWebProcessPlugIn.h>
#import <WebKit/WKWebProcessPlugInBrowserContextControllerPrivate.h>
#import <WebKit/WKWebProcessPlugInFrame.h>
#import <WebKit/WKWebProcessPlugInLoadDelegate.h>
#import <WebKit/WKWebProcessPlugInScriptWorld.h>
#import <WebKit/_WKRemoteObjectInterface.h>
#import <WebKit/_WKRemoteObjectRegistry.h>
#import <wtf/RetainPtr.h>
#import <wtf/darwin/DispatchExtras.h>

// FIXME(rdar://171294437): Disabling the back/forward cache explicitly is (for some reason) not available
// on iOS, so we can only run this test on macOS at present.
#if PLATFORM(MAC)

// This plugin verifies the render tree cleanup invariant established by the fix in
// LocalFrame::frameWasDisconnectedFromOwner() (rdar://171020009).
//
// After an iframe is disconnected via HTMLFrameOwnerElement::disconnectContentFrame(),
// the iframe's render tree must be fully torn down before any code observes the
// post-disconnection state. The plugin uses didRemoveFrameFromHierarchy: to detect
// frame removal, then schedules a dispatch_async block to run after the full
// disconnectContentFrame() call stack (including disconnectOwnerElement()) unwinds.
// It then calls getComputedStyle on an element styled with a viewport unit (width:10vw).
// Because the render tree is gone, m_renderView is null, viewport factors resolve to
// zero, and the result is "0px" — confirming that the render tree is cleanly torn down.

@interface GetComputedStyleAfterIframeRemovalPlugIn : NSObject <WKWebProcessPlugIn, WKWebProcessPlugInLoadDelegate>
@end

@implementation GetComputedStyleAfterIframeRemovalPlugIn {
    RetainPtr<WKWebProcessPlugInBrowserContextController> _browserContextController;
    RetainPtr<id<GetComputedStyleAfterIframeRemovalProtocol>> _remoteObject;

    RetainPtr<JSContext> _savedIframeContext;
    BOOL _hasSavedIframeContext;
    BOOL _didReport;
}

- (void)webProcessPlugIn:(WKWebProcessPlugInController *)plugInController didCreateBrowserContextController:(WKWebProcessPlugInBrowserContextController *)browserContextController
{
    _browserContextController = browserContextController;

    auto interface = retainPtr([_WKRemoteObjectInterface remoteObjectInterfaceWithProtocol:@protocol(GetComputedStyleAfterIframeRemovalProtocol)]);
    _remoteObject = [[browserContextController _remoteObjectRegistry] remoteObjectProxyWithInterface:interface.get()];

    [browserContextController setLoadDelegate:self];
}

- (void)webProcessPlugInBrowserContextController:(WKWebProcessPlugInBrowserContextController *)controller didFinishDocumentLoadForFrame:(WKWebProcessPlugInFrame *)frame
{
    // Save state only from the subframe (non-main frame) with the styled target element.
    if (frame.isMainFrame || _hasSavedIframeContext)
        return;

    JSContext *context = [frame jsContextForWorld:WKWebProcessPlugInScriptWorld.normalWorld];
    if (!context)
        return;

    // Check that the target element exists in this frame.
    JSValue *element = [context evaluateScript:@"document.getElementById('target')"];
    if (!element || [element isNull] || [element isUndefined])
        return;

    _savedIframeContext = context;
    _hasSavedIframeContext = YES;
}

- (void)webProcessPlugInBrowserContextController:(WKWebProcessPlugInBrowserContextController *)controller didRemoveFrameFromHierarchy:(WKWebProcessPlugInFrame *)frame
{
    if (frame.isMainFrame || !_hasSavedIframeContext || _didReport)
        return;

    _hasSavedIframeContext = NO;
    _didReport = YES;

    RetainPtr<JSContext> savedContext = _savedIframeContext;
    RetainPtr<id<GetComputedStyleAfterIframeRemovalProtocol>> remoteObject = _remoteObject;
    _savedIframeContext = nil;

    // dispatch_async ensures this block runs after the entire disconnectContentFrame()
    // call stack unwinds — including disconnectOwnerElement() -> frameWasDisconnectedFromOwner().
    // At that point the render tree is fully torn down.
    dispatch_async(mainDispatchQueueSingleton(), ^{
        JSValue *result = [savedContext.get() evaluateScript:
            @"(function() {"
                "var el = document.getElementById('target');"
                "if (!el) return '(element gone)';"
                "try {"
                "    return window.getComputedStyle(el).width;"
                "} catch(e) {"
                "    return '(exception: ' + e + ')';"
                "}})()"];

        NSString *resultString = (result && ![result isNull] && ![result isUndefined]) ? [result toString] : @"(null)";
        [remoteObject.get() didNotCrashWithResult:resultString];
    });
}

@end

#endif // PLATFORM(MAC)
