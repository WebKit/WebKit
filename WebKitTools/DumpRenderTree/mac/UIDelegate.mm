/*
 * Copyright (C) 2006. 2007 Apple Inc. All rights reserved.
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
#import "UIDelegate.h"

#import "DumpRenderTree.h"
#import "DumpRenderTreeDraggingInfo.h"
#import "EventSendingController.h"
#import "LayoutTestController.h"
#import <WebKit/WebFramePrivate.h>
#import <WebKit/WebHTMLViewPrivate.h>
#import <WebKit/WebSecurityOriginPrivate.h>
#import <WebKit/WebUIDelegatePrivate.h>
#import <WebKit/WebView.h>
#import <WebKit/WebViewPrivate.h>
#import <wtf/Assertions.h>

DumpRenderTreeDraggingInfo *draggingInfo = nil;

@implementation UIDelegate

- (void)webView:(WebView *)sender setFrame:(NSRect)frame
{
    m_frame = frame;
}

- (NSRect)webViewFrame:(WebView *)sender
{
    return m_frame;
}

- (void)webView:(WebView *)sender addMessageToConsole:(NSDictionary *)dictionary
{
    NSString *message = [dictionary objectForKey:@"message"];
    NSNumber *lineNumber = [dictionary objectForKey:@"lineNumber"];

    NSRange range = [message rangeOfString:@"file://"];
    if (range.location != NSNotFound)
        message = [[message substringToIndex:range.location] stringByAppendingString:[[message substringFromIndex:NSMaxRange(range)] lastPathComponent]];

    printf ("CONSOLE MESSAGE: line %d: %s\n", [lineNumber intValue], [message UTF8String]);
}

- (void)webView:(WebView *)sender runJavaScriptAlertPanelWithMessage:(NSString *)message initiatedByFrame:(WebFrame *)frame
{
    if (!done)
        printf("ALERT: %s\n", [message UTF8String]);
}

- (BOOL)webView:(WebView *)sender runJavaScriptConfirmPanelWithMessage:(NSString *)message initiatedByFrame:(WebFrame *)frame
{
    if (!done)
        printf("CONFIRM: %s\n", [message UTF8String]);
    return YES;
}

- (NSString *)webView:(WebView *)sender runJavaScriptTextInputPanelWithPrompt:(NSString *)prompt defaultText:(NSString *)defaultText initiatedByFrame:(WebFrame *)frame
{
    if (!done)
        printf("PROMPT: %s, default text: %s\n", [prompt UTF8String], [defaultText UTF8String]);
    return defaultText;
}

- (BOOL)webView:(WebView *)c runBeforeUnloadConfirmPanelWithMessage:(NSString *)message initiatedByFrame:(WebFrame *)frame
{
    if (!done)
        printf("CONFIRM NAVIGATION: %s\n", [message UTF8String]);
    return YES;
}


- (void)webView:(WebView *)sender dragImage:(NSImage *)anImage at:(NSPoint)viewLocation offset:(NSSize)initialOffset event:(NSEvent *)event pasteboard:(NSPasteboard *)pboard source:(id)sourceObj slideBack:(BOOL)slideFlag forView:(NSView *)view
{
     assert(!draggingInfo);
     draggingInfo = [[DumpRenderTreeDraggingInfo alloc] initWithImage:anImage offset:initialOffset pasteboard:pboard source:sourceObj];
     [sender draggingUpdated:draggingInfo];
     [EventSendingController replaySavedEvents];
}

- (void)webViewFocus:(WebView *)webView
{
    gLayoutTestController->setWindowIsKey(true);
}

- (void)webViewUnfocus:(WebView *)webView
{
    gLayoutTestController->setWindowIsKey(false);
}

- (WebView *)webView:(WebView *)sender createWebViewWithRequest:(NSURLRequest *)request
{
    if (!gLayoutTestController->canOpenWindows())
        return nil;
    
    // Make sure that waitUntilDone has been called.
    ASSERT(gLayoutTestController->waitToDump());

    WebView *webView = createWebViewAndOffscreenWindow();
    
    if (gLayoutTestController->newWindowsCopyBackForwardList())
        [webView _loadBackForwardListFromOtherView:sender];
    
    return [webView autorelease];
}

- (void)webViewClose:(WebView *)sender
{
    NSWindow* window = [sender window];
 
    if (gLayoutTestController->callCloseOnWebViews())
        [sender close];
    
    [window close];
}

- (void)webView:(WebView *)sender frame:(WebFrame *)frame exceededDatabaseQuotaForSecurityOrigin:(WebSecurityOrigin *)origin database:(NSString *)databaseIdentifier
{
    if (!done && gLayoutTestController->dumpDatabaseCallbacks())
        printf("UI DELEGATE DATABASE CALLBACK: exceededDatabaseQuotaForSecurityOrigin:{%s, %s, %i} database:%s\n", [[origin protocol] UTF8String], [[origin host] UTF8String], 
            [origin port], [databaseIdentifier UTF8String]);

    static const unsigned long long defaultQuota = 5 * 1024 * 1024;    
    [origin setQuota:defaultQuota];
}

- (void)webView:(WebView *)sender setStatusText:(NSString *)text
{
    if (gLayoutTestController->dumpStatusCallbacks())
        printf("UI DELEGATE STATUS CALLBACK: setStatusText:%s\n", [text UTF8String]);
}

- (void)webView:(WebView *)webView decidePolicyForGeolocationRequestFromOrigin:(WebSecurityOrigin *)origin frame:(WebFrame *)frame listener:(id<WebGeolocationPolicyListener>)listener
{
    if (gLayoutTestController->isGeolocationPermissionSet()) {
        if (gLayoutTestController->geolocationPermission())
            [listener allow];
        else
            [listener deny];
    }
}

- (BOOL)webView:(WebView *)sender shouldHaltPlugin:(DOMNode *)pluginNode
{
    return NO;
}

- (void)dealloc
{
    [draggingInfo release];
    draggingInfo = nil;

    [super dealloc];
}

@end
