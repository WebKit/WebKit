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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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
#import "AppleScriptController.h"
#import "EventSendingController.h"
#import "GCController.h"
#import "LayoutTestController.h"
#import "NavigationController.h"
#import "ObjCController.h"
#import "ObjCPlugin.h"
#import "ObjCPluginFunction.h"
#import "PlainTextController.h"
#import "TextInputController.h"
#import "WorkQueue.h"
#import "WorkQueueItem.h"
#import <JavaScriptCore/JavaScriptCore.h>
#import <WebKit/WebFramePrivate.h>
#import <WebKit/WebHTMLViewPrivate.h>
#import <WebKit/WebKit.h>
#import <WebKit/WebNSURLExtras.h>
#import <WebKit/WebSecurityOriginPrivate.h>
#import <wtf/Assertions.h>

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

@implementation WebFrame (DRTExtras)
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
@end

@implementation FrameLoadDelegate

- (id)init
{
    if ((self = [super init])) {
        gcController = new GCController;
        accessibilityController = new AccessibilityController;
    }
    return self;
}

- (void)dealloc
{
    delete gcController;
    delete accessibilityController;
    [super dealloc];
}

// Exec messages in the work queue until they're all done, or one of them starts a new load
- (void)processWork:(id)dummy
{
    // if another load started, then wait for it to complete.
    if (topLoadingFrame)
        return;

    // if we finish all the commands, we're ready to dump state
    if (WorkQueue::shared()->processWork() && !gLayoutTestController->waitToDump())
        dump();
}

- (void)resetToConsistentState
{
    accessibilityController->resetToConsistentState();
}

- (void)webView:(WebView *)c locationChangeDone:(NSError *)error forDataSource:(WebDataSource *)dataSource
{
    if ([dataSource webFrame] == topLoadingFrame) {
        topLoadingFrame = nil;
        WorkQueue::shared()->setFrozen(true); // first complete load freezes the queue for the rest of this test
        if (!gLayoutTestController->waitToDump()) {
            if (WorkQueue::shared()->count())
                [self performSelector:@selector(processWork:) withObject:nil afterDelay:0];
            else
                dump();
        }
    }
}

- (void)webView:(WebView *)sender didStartProvisionalLoadForFrame:(WebFrame *)frame
{
    if (!done && gLayoutTestController->dumpFrameLoadCallbacks()) {
        NSString *string = [NSString stringWithFormat:@"%@ - didStartProvisionalLoadForFrame", [frame _drt_descriptionSuitableForTestResult]];
        printf ("%s\n", [string UTF8String]);
    }
    
    ASSERT([frame provisionalDataSource]);
    // Make sure we only set this once per test.  If it gets cleared, and then set again, we might
    // end up doing two dumps for one test.
    if (!topLoadingFrame && !done)
        topLoadingFrame = frame;

    if (!done && gLayoutTestController->stopProvisionalFrameLoads()) {
        NSString *string = [NSString stringWithFormat:@"%@ - stopping load in didStartProvisionalLoadForFrame callback", [frame _drt_descriptionSuitableForTestResult]];
        printf ("%s\n", [string UTF8String]);
        [frame stopLoading];
    }
}

- (void)webView:(WebView *)sender didCommitLoadForFrame:(WebFrame *)frame
{
    if (!done && gLayoutTestController->dumpFrameLoadCallbacks()) {
        NSString *string = [NSString stringWithFormat:@"%@ - didCommitLoadForFrame", [frame _drt_descriptionSuitableForTestResult]];
        printf ("%s\n", [string UTF8String]);
    }
    
    ASSERT(![frame provisionalDataSource]);
    ASSERT([frame dataSource]);
    
    gLayoutTestController->setWindowIsKey(true);
    NSView *documentView = [[mainFrame frameView] documentView];
    [[[mainFrame webView] window] makeFirstResponder:documentView];
}

- (void)webView:(WebView *)sender didFailProvisionalLoadWithError:(NSError *)error forFrame:(WebFrame *)frame
{
    if (!done && gLayoutTestController->dumpFrameLoadCallbacks()) {
        NSString *string = [NSString stringWithFormat:@"%@ - didFailProvisionalLoadWithError", [frame _drt_descriptionSuitableForTestResult]];
        printf("%s\n", [string UTF8String]);
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
    
    if (!done && gLayoutTestController->dumpFrameLoadCallbacks()) {
        NSString *string = [NSString stringWithFormat:@"%@ - didFinishLoadForFrame", [frame _drt_descriptionSuitableForTestResult]];
        printf ("%s\n", [string UTF8String]);
    }
    
    // FIXME: This call to displayIfNeeded can be removed when <rdar://problem/5092361> is fixed.
    // After that is fixed, we will reenable painting after WebCore is done loading the document, 
    // and this call will no longer be needed.
    if ([[sender mainFrame] isEqual:frame])
        [sender displayIfNeeded];
    [self webView:sender locationChangeDone:nil forDataSource:[frame dataSource]];
    [gNavigationController webView:sender didFinishLoadForFrame:frame];
}

- (void)webView:(WebView *)sender didFailLoadWithError:(NSError *)error forFrame:(WebFrame *)frame
{
    if (!done && gLayoutTestController->dumpFrameLoadCallbacks()) {
        NSString *string = [NSString stringWithFormat:@"%@ - didFailLoadWithError", [frame _drt_descriptionSuitableForTestResult]];
        printf ("%s\n", [string UTF8String]);
    }
    
    ASSERT(![frame provisionalDataSource]);
    ASSERT([frame dataSource]);
    
    [self webView:sender locationChangeDone:error forDataSource:[frame dataSource]];    
}

- (void)webView:(WebView *)webView windowScriptObjectAvailable:(WebScriptObject *)windowScriptObject
{
    if (!done && gLayoutTestController->dumpFrameLoadCallbacks()) {
        NSString *string = [NSString stringWithFormat:@"?? - windowScriptObjectAvailable"];
        printf ("%s\n", [string UTF8String]);
    }
    
    ASSERT_NOT_REACHED();
}

- (void)webView:(WebView *)sender didClearWindowObject:(WebScriptObject *)obj forFrame:(WebFrame *)frame
{
    ASSERT(obj == [frame windowObject]);
    ASSERT([obj JSObject] == JSContextGetGlobalObject([frame globalContext]));

    // Make New-Style LayoutTestController
    JSContextRef context = [frame globalContext];
    JSObjectRef globalObject = JSContextGetGlobalObject(context);
    JSValueRef exception = 0;

    ASSERT(gLayoutTestController);
    gLayoutTestController->makeWindowObject(context, globalObject, &exception);
    ASSERT(!exception);

    gcController->makeWindowObject(context, globalObject, &exception);
    ASSERT(!exception);

    accessibilityController->makeWindowObject(context, globalObject, &exception);
    ASSERT(!exception);

    // Make Old-Style controllers

    AppleScriptController *asc = [[AppleScriptController alloc] initWithWebView:sender];
    [obj setValue:asc forKey:@"appleScriptController"];
    [asc release];

    EventSendingController *esc = [[EventSendingController alloc] init];
    [obj setValue:esc forKey:@"eventSender"];
    [esc release];
    
    [obj setValue:gNavigationController forKey:@"navigationController"];
    
    ObjCController *occ = [[ObjCController alloc] init];
    [obj setValue:occ forKey:@"objCController"];
    [occ release];

    ObjCPlugin *plugin = [[ObjCPlugin alloc] init];
    [obj setValue:plugin forKey:@"objCPlugin"];
    [plugin release];
    
    ObjCPluginFunction *pluginFunction = [[ObjCPluginFunction alloc] init];
    [obj setValue:pluginFunction forKey:@"objCPluginFunction"];
    [pluginFunction release];

    [obj setValue:[PlainTextController sharedPlainTextController] forKey:@"plainText"];

    TextInputController *tic = [[TextInputController alloc] initWithWebView:sender];
    [obj setValue:tic forKey:@"textInputController"];
    [tic release];
}

- (void)webView:(WebView *)sender didReceiveTitle:(NSString *)title forFrame:(WebFrame *)frame
{
    if (!done && gLayoutTestController->dumpFrameLoadCallbacks()) {
        NSString *string = [NSString stringWithFormat:@"%@ - didReceiveTitle: %@", [frame _drt_descriptionSuitableForTestResult], title];
        printf ("%s\n", [string UTF8String]);
    }
    
    if (gLayoutTestController->dumpTitleChanges())
        printf("TITLE CHANGED: %s\n", [title UTF8String]);
}

- (void)webView:(WebView *)sender didReceiveServerRedirectForProvisionalLoadForFrame:(WebFrame *)frame
{
    if (!done && gLayoutTestController->dumpFrameLoadCallbacks()) {
        NSString *string = [NSString stringWithFormat:@"%@ - didReceiveServerRedirectForProvisionalLoadForFrame", [frame _drt_descriptionSuitableForTestResult]];
        printf ("%s\n", [string UTF8String]);
    }
}

- (void)webView:(WebView *)sender didChangeLocationWithinPageForFrame:(WebFrame *)frame
{
    if (!done && gLayoutTestController->dumpFrameLoadCallbacks()) {
        NSString *string = [NSString stringWithFormat:@"%@ - didChangeLocationWithinPageForFrame", [frame _drt_descriptionSuitableForTestResult]];
        printf ("%s\n", [string UTF8String]);
    }
}

- (void)webView:(WebView *)sender willPerformClientRedirectToURL:(NSURL *)URL delay:(NSTimeInterval)seconds fireDate:(NSDate *)date forFrame:(WebFrame *)frame
{
    if (!done && gLayoutTestController->dumpFrameLoadCallbacks()) {
        NSString *string = [NSString stringWithFormat:@"%@ - willPerformClientRedirectToURL: %@ ", [frame _drt_descriptionSuitableForTestResult], [URL _drt_descriptionSuitableForTestResult]];
        printf ("%s\n", [string UTF8String]);
    }
}

- (void)webView:(WebView *)sender didCancelClientRedirectForFrame:(WebFrame *)frame
{
    if (!done && gLayoutTestController->dumpFrameLoadCallbacks()) {
        NSString *string = [NSString stringWithFormat:@"%@ - didCancelClientRedirectForFrame", [frame _drt_descriptionSuitableForTestResult]];
        printf ("%s\n", [string UTF8String]);
    }
}

- (void)webView:(WebView *)sender didFinishDocumentLoadForFrame:(WebFrame *)frame
{
    if (!done && gLayoutTestController->dumpFrameLoadCallbacks()) {
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
    if (!done && gLayoutTestController->dumpFrameLoadCallbacks()) {
        NSString *string = [NSString stringWithFormat:@"%@ - didHandleOnloadEventsForFrame", [frame _drt_descriptionSuitableForTestResult]];
        printf ("%s\n", [string UTF8String]);
    }
}

- (void)webViewDidDisplayInsecureContent:(WebView *)sender
{
    if (!done && gLayoutTestController->dumpFrameLoadCallbacks())
        printf ("didDisplayInsecureContent\n");
}

- (void)webView:(WebView *)sender didRunInsecureContent:(WebSecurityOrigin *)origin
{
    if (!done && gLayoutTestController->dumpFrameLoadCallbacks())
        printf ("didRunInsecureContent\n");
}

@end
