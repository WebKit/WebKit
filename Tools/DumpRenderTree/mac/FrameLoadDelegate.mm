/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "DumpRenderTree.h"
#import "FrameLoadDelegate.h"

#import "AccessibilityController.h"
#import "EventSendingController.h"
#import "GCController.h"
#import "JSWrapper.h"
#import "NavigationController.h"
#import "ObjCController.h"
#import "ObjCPlugin.h"
#import "ObjCPluginFunction.h"
#import "ReftestFunctions.h"
#import "TestRunner.h"
#import "TextInputController.h"
#import "WebCoreTestSupport.h"
#import "WorkQueue.h"
#import "WorkQueueItem.h"
#import <Foundation/NSNotification.h>
#import <JavaScriptCore/JavaScriptCore.h>
#import <WebKit/WebFramePrivate.h>
#import <WebKit/WebHTMLViewPrivate.h>
#import <WebKit/WebNSURLExtras.h>
#import <WebKit/WebScriptWorld.h>
#import <WebKit/WebSecurityOriginPrivate.h>
#import <WebKit/WebViewPrivate.h>
#import <wtf/Assertions.h>

#if !PLATFORM(IOS_FAMILY)
#import "AppleScriptController.h"
#endif

#if PLATFORM(IOS_FAMILY)
#import <WebKit/WebCoreThreadMessage.h>
#endif

#ifndef NSEC_PER_MSEC
#define NSEC_PER_MSEC 1000000ull
#endif

@interface NSURL (DRTExtras)
- (NSString *)_drt_descriptionSuitableForTestResult;
@end

@interface NSError (DRTExtras)
- (NSString *)_drt_descriptionSuitableForTestResult;
@end

@interface NSURLResponse (DRTExtras)
- (NSString *)_drt_descriptionSuitableForTestResult;
@end

@interface NSURLRequest (DRTExtras)
- (NSString *)_drt_descriptionSuitableForTestResult;
@end

@interface WebFrame (DRTExtras)
- (NSString *)_drt_descriptionSuitableForTestResult;
@end

IGNORE_WARNINGS_BEGIN("deprecated-implementations")
@implementation WebFrame (DRTExtras)
IGNORE_WARNINGS_END
- (NSString *)_drt_descriptionSuitableForTestResult
{
    BOOL isMainFrame = (self == [[self webView] mainFrame]);
    NSString *name = [self name];
    if (isMainFrame) {
        if ([name length])
            return [NSString stringWithFormat:@"main frame \"%@\"", name];
        else
            return @"main frame";
    } else {
        if (name)
            return [NSString stringWithFormat:@"frame \"%@\"", name];
        else
            return @"frame (anonymous)";
    }
}

- (NSString *)_drt_printFrameUserGestureStatus
{
    BOOL isUserGesture = [[self webView] _isProcessingUserGesture];
    return [NSString stringWithFormat:@"Frame with user gesture \"%@\"", isUserGesture ? @"true" : @"false"];
}
@end

@implementation FrameLoadDelegate

- (id)init
{
    if ((self = [super init])) {
        gcController = new GCController;
        accessibilityController = new AccessibilityController;
#if !PLATFORM(IOS_FAMILY)
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(webViewProgressFinishedNotification:) name:WebViewProgressFinishedNotification object:nil];
#endif
    }
    return self;
}

- (void)dealloc
{
#if !PLATFORM(IOS_FAMILY)
    [[NSNotificationCenter defaultCenter] removeObserver:self];
#endif
    delete gcController;
    delete accessibilityController;
    [super dealloc];
}

- (void)dumpAfterWaitAttributeIsRemoved:(id)dummy
{
#if PLATFORM(IOS_FAMILY)
    WebThreadLock();
#endif
    if (WTR::hasReftestWaitAttribute(mainFrame.globalContext))
        [self performSelector:@selector(dumpAfterWaitAttributeIsRemoved:) withObject:nil afterDelay:0];
    else
        dump();
}

- (void)readyToDumpState
{
#if PLATFORM(IOS_FAMILY)
    WebThreadLock();
#endif
    WTR::sendTestRenderedEvent(mainFrame.globalContext);
    [self dumpAfterWaitAttributeIsRemoved:nil];
}

// Execute messages in the work queue until they're all done, or one of them starts a new load.
- (void)processWork:(id)dummy
{
    // if another load started, then wait for it to complete.
    if (topLoadingFrame)
        return;

#if PLATFORM(IOS_FAMILY)
    WebThreadLock();
#endif
    // If we finish all the commands, we're ready to dump state.
    if (DRT::WorkQueue::singleton().processWork() && !gTestRunner->waitToDump())
        [self readyToDumpState];
}

- (void)resetToConsistentState
{
    accessibilityController->resetToConsistentState();
}

- (void)webView:(WebView *)webView locationChangeDone:(NSError *)error forDataSource:(WebDataSource *)dataSource
{
    if ([dataSource webFrame] == topLoadingFrame) {
        topLoadingFrame = nil;
        auto& workQueue = DRT::WorkQueue::singleton();
        workQueue.setFrozen(true); // first complete load freezes the queue for the rest of this test
        if (!gTestRunner->waitToDump()) {
            if (workQueue.count())
                [self performSelector:@selector(processWork:) withObject:nil afterDelay:0];
            else
                [self readyToDumpState];
        }
    }
}

- (void)webView:(WebView *)sender didStartProvisionalLoadForFrame:(WebFrame *)frame
{
    ASSERT([frame provisionalDataSource]);

    if (!done && gTestRunner->dumpFrameLoadCallbacks()) {
        NSString *string = [NSString stringWithFormat:@"%@ - didStartProvisionalLoadForFrame", [frame _drt_descriptionSuitableForTestResult]];
        printf ("%s\n", [string UTF8String]);
    }

    if (!done && gTestRunner->dumpUserGestureInFrameLoadCallbacks()) {
        NSString *string = [NSString stringWithFormat:@"%@ - in didStartProvisionalLoadForFrame", [frame _drt_printFrameUserGestureStatus]];
        printf ("%s\n", [string UTF8String]);
    }

    // Make sure we only set this once per test.  If it gets cleared, and then set again, we might
    // end up doing two dumps for one test.
    if (!topLoadingFrame && !done)
        topLoadingFrame = frame;

    if (!done && gTestRunner->stopProvisionalFrameLoads()) {
        NSString *string = [NSString stringWithFormat:@"%@ - stopping load in didStartProvisionalLoadForFrame callback", [frame _drt_descriptionSuitableForTestResult]];
        printf ("%s\n", [string UTF8String]);
        [frame stopLoading];
    }

    if (!done && gTestRunner->useDeferredFrameLoading()) {
        [sender setDefersCallbacks:YES];
        int64_t deferredWaitTime = 5 * NSEC_PER_MSEC;
        dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, deferredWaitTime);
        dispatch_after(when, dispatch_get_main_queue(), ^{
#if PLATFORM(IOS_FAMILY)
            WebThreadLock();
#endif
            [sender setDefersCallbacks:NO];
        });
    }
}

- (void)webView:(WebView *)sender didCommitLoadForFrame:(WebFrame *)frame
{
    if (!done && gTestRunner->dumpFrameLoadCallbacks()) {
        NSString *string = [NSString stringWithFormat:@"%@ - didCommitLoadForFrame", [frame _drt_descriptionSuitableForTestResult]];
        printf ("%s\n", [string UTF8String]);
    }

    ASSERT(![frame provisionalDataSource]);
    ASSERT([frame dataSource]);
    gTestRunner->setWindowIsKey(true);
#if !PLATFORM(IOS_FAMILY)
    NSView *documentView = [[mainFrame frameView] documentView];
    [[[mainFrame webView] window] makeFirstResponder:documentView];
#endif
}

- (void)webView:(WebView *)sender didFailProvisionalLoadWithError:(NSError *)error forFrame:(WebFrame *)frame
{
    if (!done && gTestRunner->dumpFrameLoadCallbacks()) {
        NSString *string = [NSString stringWithFormat:@"%@ - didFailProvisionalLoadWithError", [frame _drt_descriptionSuitableForTestResult]];
        printf("%s\n", [string UTF8String]);
        if (error.code == WebKitErrorCannotShowURL) {
            string = [NSString stringWithFormat:@"%@ - (ErrorCodeCannotShowURL)", [frame _drt_descriptionSuitableForTestResult]];
            printf("%s\n", [string UTF8String]);
        }
    }

    if ([error domain] == NSURLErrorDomain && ([error code] == NSURLErrorServerCertificateHasUnknownRoot || [error code] == NSURLErrorServerCertificateUntrusted)) {
        // <http://webkit.org/b/31200> In order to prevent extra frame load delegate logging being generated if the first test to use SSL
        // is set to log frame load delegate calls we ignore SSL certificate errors on localhost and 127.0.0.1 from within dumpRenderTree.
        // Those are the only hosts that we use SSL with at present.  If we hit this code path then we've found another host that we need
        // to apply the workaround to.
        ASSERT_NOT_REACHED();
        return;
    }

    ASSERT([frame provisionalDataSource]);
    [self webView:sender locationChangeDone:error forDataSource:[frame provisionalDataSource]];
}

- (void)webView:(WebView *)sender didFinishLoadForFrame:(WebFrame *)frame
{
    ASSERT([frame dataSource]);
    ASSERT(frame == [[frame dataSource] webFrame]);
    
    if (!done && gTestRunner->dumpFrameLoadCallbacks()) {
        NSString *string = [NSString stringWithFormat:@"%@ - didFinishLoadForFrame", [frame _drt_descriptionSuitableForTestResult]];
        printf ("%s\n", [string UTF8String]);
    }

    [self webView:sender locationChangeDone:nil forDataSource:[frame dataSource]];
    [gNavigationController webView:sender didFinishLoadForFrame:frame];
}

- (void)webView:(WebView *)sender didFailLoadWithError:(NSError *)error forFrame:(WebFrame *)frame
{
    if (!done && gTestRunner->dumpFrameLoadCallbacks()) {
        NSString *string = [NSString stringWithFormat:@"%@ - didFailLoadWithError", [frame _drt_descriptionSuitableForTestResult]];
        printf ("%s\n", [string UTF8String]);
    }

    ASSERT(![frame provisionalDataSource]);
    ASSERT([frame dataSource]);
    
    [self webView:sender locationChangeDone:error forDataSource:[frame dataSource]];
}

IGNORE_WARNINGS_BEGIN("deprecated-implementations")
- (void)webView:(WebView *)webView windowScriptObjectAvailable:(WebScriptObject *)windowScriptObject
IGNORE_WARNINGS_END
{
    if (!done && gTestRunner->dumpFrameLoadCallbacks()) {
        NSString *string = [NSString stringWithFormat:@"?? - windowScriptObjectAvailable"];
        printf ("%s\n", [string UTF8String]);
    }

    ASSERT_NOT_REACHED();
}

- (void)didClearWindowObjectInStandardWorldForFrame:(WebFrame *)frame
{
    auto context = [frame globalContext];
    auto webView = [frame webView];
    auto windowObject = [frame windowObject];

    gTestRunner->makeWindowObject(context);
    gcController->makeWindowObject(context);
    accessibilityController->makeWindowObject(context);

    WebCoreTestSupport::injectInternalsObject(context);

#if PLATFORM(MAC)
    [windowObject setValue:adoptNS([[AppleScriptController alloc] initWithWebView:webView]).get() forKey:@"appleScriptController"];
#endif

    [windowObject setValue:adoptNS([[EventSendingController alloc] init]).get() forKey:@"eventSender"];
    [windowObject setValue:gNavigationController.get() forKey:@"navigationController"];
    [windowObject setValue:adoptNS([[ObjCController alloc] init]).get() forKey:@"objCController"];
    [windowObject setValue:adoptNS([[ObjCPlugin alloc] init]).get() forKey:@"objCPlugin"];
    [windowObject setValue:adoptNS([[ObjCPluginFunction alloc] init]).get() forKey:@"objCPluginFunction"];
    [windowObject setValue:adoptNS([[TextInputController alloc] initWithWebView:webView]).get() forKey:@"textInputController"];
}

- (void)didClearWindowObjectForFrame:(WebFrame *)frame inIsolatedWorld:(WebScriptWorld *)world
{
    JSGlobalContextRef ctx = [frame _globalContextForScriptWorld:world];
    if (!ctx)
        return;

    JSObjectRef globalObject = JSContextGetGlobalObject(ctx);
    if (!globalObject)
        return;

    JSObjectSetProperty(ctx, globalObject, WTR::createJSString("__worldID").get(), JSValueMakeNumber(ctx, worldIDForWorld(world)), kJSPropertyAttributeReadOnly, 0);
}

- (void)webView:(WebView *)sender didClearWindowObjectForFrame:(WebFrame *)frame inScriptWorld:(WebScriptWorld *)world
{
    if (world == [WebScriptWorld standardWorld])
        [self didClearWindowObjectInStandardWorldForFrame:frame];
    else
        [self didClearWindowObjectForFrame:frame inIsolatedWorld:world];
}

- (void)webView:(WebView *)sender didReceiveTitle:(NSString *)title forFrame:(WebFrame *)frame
{
    if (!done && gTestRunner->dumpFrameLoadCallbacks()) {
        NSString *string = [NSString stringWithFormat:@"%@ - didReceiveTitle: %@", [frame _drt_descriptionSuitableForTestResult], title];
        printf ("%s\n", [string UTF8String]);
    }

    if (gTestRunner->dumpTitleChanges())
        printf("TITLE CHANGED: '%s'\n", [title UTF8String]);
}

- (void)webView:(WebView *)sender didReceiveServerRedirectForProvisionalLoadForFrame:(WebFrame *)frame
{
    if (!done && gTestRunner->dumpFrameLoadCallbacks()) {
        NSString *string = [NSString stringWithFormat:@"%@ - didReceiveServerRedirectForProvisionalLoadForFrame", [frame _drt_descriptionSuitableForTestResult]];
        printf ("%s\n", [string UTF8String]);
    }
}

- (void)webView:(WebView *)sender didChangeLocationWithinPageForFrame:(WebFrame *)frame
{
    if (!done && gTestRunner->dumpFrameLoadCallbacks()) {
        NSString *string = [NSString stringWithFormat:@"%@ - didChangeLocationWithinPageForFrame", [frame _drt_descriptionSuitableForTestResult]];
        printf ("%s\n", [string UTF8String]);
    }
}

- (void)webView:(WebView *)sender willPerformClientRedirectToURL:(NSURL *)URL delay:(NSTimeInterval)seconds fireDate:(NSDate *)date forFrame:(WebFrame *)frame
{
    if (!done && gTestRunner->dumpFrameLoadCallbacks()) {
        NSString *string = [NSString stringWithFormat:@"%@ - willPerformClientRedirectToURL: %@", [frame _drt_descriptionSuitableForTestResult], [URL _drt_descriptionSuitableForTestResult]];
        printf ("%s\n", [string UTF8String]);
    }

    if (!done && gTestRunner->dumpUserGestureInFrameLoadCallbacks()) {
        NSString *string = [NSString stringWithFormat:@"%@ - in willPerformClientRedirect", [frame _drt_printFrameUserGestureStatus]];
        printf ("%s\n", [string UTF8String]);
    }
}

- (void)webView:(WebView *)sender didCancelClientRedirectForFrame:(WebFrame *)frame
{
    if (!done && gTestRunner->dumpFrameLoadCallbacks()) {
        NSString *string = [NSString stringWithFormat:@"%@ - didCancelClientRedirectForFrame", [frame _drt_descriptionSuitableForTestResult]];
        printf ("%s\n", [string UTF8String]);
    }
    gTestRunner->setDidCancelClientRedirect(true);
}

- (void)webView:(WebView *)sender didFinishDocumentLoadForFrame:(WebFrame *)frame
{
    if (!done && gTestRunner->dumpFrameLoadCallbacks()) {
        NSString *string = [NSString stringWithFormat:@"%@ - didFinishDocumentLoadForFrame", [frame _drt_descriptionSuitableForTestResult]];
        printf ("%s\n", [string UTF8String]);
    } else if (!done) {
        unsigned pendingFrameUnloadEvents = [frame _pendingFrameUnloadEventCount];
        if (pendingFrameUnloadEvents) {
            NSString *string = [NSString stringWithFormat:@"%@ - has %u onunload handler(s)", [frame _drt_descriptionSuitableForTestResult], pendingFrameUnloadEvents];
            printf ("%s\n", [string UTF8String]);
        }
    }
}

- (void)webView:(WebView *)sender didHandleOnloadEventsForFrame:(WebFrame *)frame
{
    if (!done && gTestRunner->dumpFrameLoadCallbacks()) {
        NSString *string = [NSString stringWithFormat:@"%@ - didHandleOnloadEventsForFrame", [frame _drt_descriptionSuitableForTestResult]];
        printf ("%s\n", [string UTF8String]);
    }
}

- (void)webViewDidDisplayInsecureContent:(WebView *)sender
{
    if (!done && gTestRunner->dumpFrameLoadCallbacks())
        printf ("didDisplayInsecureContent\n");
}

- (void)webView:(WebView *)sender didRunInsecureContent:(WebSecurityOrigin *)origin
{
    if (!done && gTestRunner->dumpFrameLoadCallbacks())
        printf ("didRunInsecureContent\n");
}

- (void)webView:(WebView *)sender didDetectXSS:(NSURL *)insecureURL
{
    if (!done && gTestRunner->dumpFrameLoadCallbacks())
        printf ("didDetectXSS\n");
}

- (void)webViewProgressFinishedNotification:(NSNotification *)notification
{
    if (!done && gTestRunner->dumpProgressFinishedCallback())
        printf ("postProgressFinishedNotification\n");
}

@end
