/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#import "LayoutTestController.h"

#import "DumpRenderTree.h"

#import <JavaScriptCore/Assertions.h>
#import <WebKit/WebBackForwardList.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebHTMLView.h>
#import <WebKit/WebHTMLViewPrivate.h>
#import <WebKit/WebHistory.h>
#import <WebKit/WebNSURLExtras.h>
#import <WebKit/WebPreferences.h>
#import <WebKit/WebScriptObject.h>
#import <WebKit/WebView.h>
#import <WebKit/WebViewPrivate.h>

@implementation LayoutTestController

+ (BOOL)isSelectorExcludedFromWebScript:(SEL)aSelector
{
    if (0
            || aSelector == @selector(accessStoredWebScriptObject)
            || aSelector == @selector(addDisallowedURL:)    
            || aSelector == @selector(addFileToPasteboardOnDrag)
            || aSelector == @selector(clearBackForwardList)
            || aSelector == @selector(decodeHostName:)
            || aSelector == @selector(display)
            || aSelector == @selector(dumpAsText)
            || aSelector == @selector(dumpBackForwardList)
            || aSelector == @selector(dumpChildFrameScrollPositions)
            || aSelector == @selector(dumpChildFramesAsText)
            || aSelector == @selector(dumpDOMAsWebArchive)
            || aSelector == @selector(dumpEditingCallbacks)
            || aSelector == @selector(dumpFrameLoadCallbacks)
            || aSelector == @selector(dumpResourceLoadCallbacks)
            || aSelector == @selector(dumpSelectionRect)
            || aSelector == @selector(dumpSourceAsWebArchive)
            || aSelector == @selector(dumpTitleChanges)
            || aSelector == @selector(encodeHostName:)
            || aSelector == @selector(keepWebHistory)
            || aSelector == @selector(notifyDone)
            || aSelector == @selector(queueBackNavigation:)
            || aSelector == @selector(queueForwardNavigation:)
            || aSelector == @selector(queueLoad:target:)
            || aSelector == @selector(queueReload)
            || aSelector == @selector(queueScript:)
            || aSelector == @selector(repaintSweepHorizontally)
            || aSelector == @selector(setAcceptsEditing:)
            || aSelector == @selector(setCallCloseOnWebViews:)
            || aSelector == @selector(setCanOpenWindows)
            || aSelector == @selector(setCloseRemainingWindowsWhenComplete:)
            || aSelector == @selector(setCustomPolicyDelegate:)
            || aSelector == @selector(setMainFrameIsFirstResponder:)
            || aSelector == @selector(setTabKeyCyclesThroughElements:)
            || aSelector == @selector(setUseDashboardCompatibilityMode:)
            || aSelector == @selector(setUserStyleSheetEnabled:)
            || aSelector == @selector(setUserStyleSheetLocation:)
            || aSelector == @selector(setWindowIsKey:)
            || aSelector == @selector(storeWebScriptObject:)
            || aSelector == @selector(testRepaint)
            || aSelector == @selector(waitUntilDone)
            || aSelector == @selector(windowCount)
        )
        return NO;
    return YES;
}

+ (NSString *)webScriptNameForSelector:(SEL)aSelector
{
    if (aSelector == @selector(setWindowIsKey:))
        return @"setWindowIsKey";
    if (aSelector == @selector(setMainFrameIsFirstResponder:))
        return @"setMainFrameIsFirstResponder";
    if (aSelector == @selector(queueBackNavigation:))
        return @"queueBackNavigation";
    if (aSelector == @selector(queueForwardNavigation:))
        return @"queueForwardNavigation";
    if (aSelector == @selector(queueScript:))
        return @"queueScript";
    if (aSelector == @selector(queueLoad:target:))
        return @"queueLoad";
    if (aSelector == @selector(setAcceptsEditing:))
        return @"setAcceptsEditing";
    if (aSelector == @selector(setTabKeyCyclesThroughElements:))
        return @"setTabKeyCyclesThroughElements";
    if (aSelector == @selector(storeWebScriptObject:))
        return @"storeWebScriptObject";
    if (aSelector == @selector(setUserStyleSheetLocation:))
        return @"setUserStyleSheetLocation";
    if (aSelector == @selector(setUserStyleSheetEnabled:))
        return @"setUserStyleSheetEnabled";
    if (aSelector == @selector(addDisallowedURL:))
        return @"addDisallowedURL";
    if (aSelector == @selector(setCallCloseOnWebViews:))
        return @"setCallCloseOnWebViews";
    if (aSelector == @selector(setCloseRemainingWindowsWhenComplete:))
        return @"setCloseRemainingWindowsWhenComplete";
    if (aSelector == @selector(setCustomPolicyDelegate:))
        return @"setCustomPolicyDelegate";
    if (aSelector == @selector(setUseDashboardCompatibilityMode:))
        return @"setUseDashboardCompatiblityMode";
    if (aSelector == @selector(encodeHostName:))
        return @"encodeHostName";
    if (aSelector == @selector(decodeHostName:))
        return @"decodeHostName";    
    
    return nil;
}

- (void)clearBackForwardList
{
    WebBackForwardList *backForwardList = [[mainFrame webView] backForwardList];
    WebHistoryItem *item = [[backForwardList currentItem] retain];

    // We clear the history by setting the back/forward list's capacity to 0
    // then restoring it back and adding back the current item.
    int capacity = [backForwardList capacity];
    [backForwardList setCapacity:0];
    [backForwardList setCapacity:capacity];
    [backForwardList addItem:item];
    [backForwardList goToItem:item];
    [item release];
}

- (void)setUseDashboardCompatibilityMode:(BOOL)flag
{
    [[mainFrame webView] _setDashboardBehavior:WebDashboardBehaviorUseBackwardCompatibilityMode to:flag];
}

- (void)setCloseRemainingWindowsWhenComplete:(BOOL)closeWindows
{
    closeRemainingWindowsWhenComplete = closeWindows;
}

- (void)setCustomPolicyDelegate:(BOOL)setDelegate
{
    if (setDelegate)
        [[mainFrame webView] setPolicyDelegate:policyDelegate];
    else
        [[mainFrame webView] setPolicyDelegate:nil];
}

- (void)keepWebHistory
{
    if (![WebHistory optionalSharedHistory]) {
        WebHistory *history = [[WebHistory alloc] init];
        [WebHistory setOptionalSharedHistory:history];
        [history release];
    }
}

- (void)setCallCloseOnWebViews:(BOOL)callClose
{
    closeWebViews = callClose;
}

- (void)setCanOpenWindows
{
    canOpenWindows = YES;
}

- (void)waitUntilDone
{
    waitToDump = YES;
    if (!waitToDumpWatchdog)
        waitToDumpWatchdog = [[NSTimer scheduledTimerWithTimeInterval:waitToDumpWatchdogInterval target:self selector:@selector(waitUntilDoneWatchdogFired) userInfo:nil repeats:NO] retain];
}

- (void)waitUntilDoneWatchdogFired
{
    const char* message = "FAIL: Timed out waiting for notifyDone to be called\n";
    fprintf(stderr, message);
    fprintf(stdout, message);
    dump();
}

- (void)notifyDone
{
    if (waitToDump && !topLoadingFrame && [workQueue count] == 0)
        dump();
    waitToDump = NO;
}

- (void)dumpAsText
{
    dumpAsText = YES;
}

- (void)addFileToPasteboardOnDrag
{
    addFileToPasteboardOnDrag = YES;
}

- (void)addDisallowedURL:(NSString *)urlString
{
    if (!disallowedURLs)
        disallowedURLs = [[NSMutableSet alloc] init];
    
    
    // Canonicalize the URL
    NSURLRequest* request = [NSURLRequest requestWithURL:[NSURL URLWithString:urlString]];
    request = [NSURLProtocol canonicalRequestForRequest:request];
    
    [disallowedURLs addObject:[request URL]];
}

- (void)setUserStyleSheetLocation:(NSString *)path
{
    NSURL *url = [NSURL URLWithString:path];
    [[WebPreferences standardPreferences] setUserStyleSheetLocation:url];
}

- (void)setUserStyleSheetEnabled:(BOOL)flag
{
    [[WebPreferences standardPreferences] setUserStyleSheetEnabled:flag];
}

- (void)dumpDOMAsWebArchive
{
    dumpDOMAsWebArchive = YES;
}

- (void)dumpSourceAsWebArchive
{
    dumpSourceAsWebArchive = YES;
}

- (void)dumpSelectionRect
{
    dumpSelectionRect = YES;
}

- (void)dumpTitleChanges
{
    dumpTitleChanges = YES;
}

- (void)dumpBackForwardList
{
    dumpBackForwardList = YES;
}

- (int)windowCount
{
    return CFArrayGetCount(allWindowsRef);
}

- (void)dumpChildFrameScrollPositions
{
    dumpChildFrameScrollPositions = YES;
}

- (void)dumpChildFramesAsText
{
    dumpChildFramesAsText = YES;
}

- (void)dumpEditingCallbacks
{
    shouldDumpEditingCallbacks = YES;
}

- (void)dumpResourceLoadCallbacks
{
    shouldDumpResourceLoadCallbacks = YES;
}

- (void)dumpFrameLoadCallbacks
{
    shouldDumpFrameLoadCallbacks = YES;
}

- (void)setWindowIsKey:(BOOL)flag
{
    windowIsKey = flag;
    NSView *documentView = [[mainFrame frameView] documentView];
    if ([documentView isKindOfClass:[WebHTMLView class]])
        [(WebHTMLView *)documentView _updateActiveState];
}

- (void)setMainFrameIsFirstResponder:(BOOL)flag
{
    NSView *documentView = [[mainFrame frameView] documentView];
    
    NSResponder *firstResponder = flag ? documentView : nil;
    [[[mainFrame webView] window] makeFirstResponder:firstResponder];
        
    if ([documentView isKindOfClass:[WebHTMLView class]])
        [(WebHTMLView *)documentView _updateActiveState];
}

- (void)display
{
    displayWebView();
}

- (void)testRepaint
{
    testRepaint = YES;
}

- (void)repaintSweepHorizontally
{
    repaintSweepHorizontally = YES;
}

- (id)invokeUndefinedMethodFromWebScript:(NSString *)name withArguments:(NSArray *)args
{
    return nil;
}

- (void)_addWorkForTarget:(id)target selector:(SEL)selector arg1:(id)arg1 arg2:(id)arg2
{
    if (workQueueFrozen)
        return;
    NSMethodSignature *sig = [target methodSignatureForSelector:selector];
    NSInvocation *work = [NSInvocation invocationWithMethodSignature:sig];
    [work retainArguments];
    [work setTarget:target];
    [work setSelector:selector];
    if (arg1) {
        [work setArgument:&arg1 atIndex:2];
        if (arg2)
            [work setArgument:&arg2 atIndex:3];
    }
    [workQueue addObject:work];
}

- (void)_doLoad:(NSURL *)url target:(NSString *)target
{
    WebFrame *targetFrame;
    if (target && ![target isKindOfClass:[WebUndefined class]])
        targetFrame = [mainFrame findFrameNamed:target];
    else
        targetFrame = mainFrame;
    [targetFrame loadRequest:[NSURLRequest requestWithURL:url]];
}

- (void)_doBackOrForwardNavigation:(NSNumber *)index
{
    int bfIndex = [index intValue];
    if (bfIndex == 1)
        [[mainFrame webView] goForward];
    if (bfIndex == -1)
        [[mainFrame webView] goBack];
    else {        
        WebBackForwardList *bfList = [[mainFrame webView] backForwardList];
        [[mainFrame webView] goToBackForwardItem:[bfList itemAtIndex:bfIndex]];
    }
}

- (void)queueBackNavigation:(int)howFarBack
{
    [self _addWorkForTarget:self selector:@selector(_doBackOrForwardNavigation:) arg1:[NSNumber numberWithInt:-howFarBack] arg2:nil];
}

- (void)queueForwardNavigation:(int)howFarForward
{
    [self _addWorkForTarget:self selector:@selector(_doBackOrForwardNavigation:) arg1:[NSNumber numberWithInt:howFarForward] arg2:nil];
}

- (void)queueReload
{
    [self _addWorkForTarget:[mainFrame webView] selector:@selector(reload:) arg1:self arg2:nil];
}

- (void)queueScript:(NSString *)script
{
    [self _addWorkForTarget:[mainFrame webView] selector:@selector(stringByEvaluatingJavaScriptFromString:) arg1:script arg2:nil];
}

- (void)queueLoad:(NSString *)URLString target:(NSString *)target
{
    NSURL *URL = [NSURL URLWithString:URLString relativeToURL:[[[mainFrame dataSource] response] URL]];
    [self _addWorkForTarget:self selector:@selector(_doLoad:target:) arg1:URL arg2:target];
}

- (void)setAcceptsEditing:(BOOL)newAcceptsEditing
{
    [(EditingDelegate *)[[mainFrame webView] editingDelegate] setAcceptsEditing:newAcceptsEditing];
}

- (void)setTabKeyCyclesThroughElements:(BOOL)newTabKeyCyclesThroughElements
{
    [[mainFrame webView] setTabKeyCyclesThroughElements:newTabKeyCyclesThroughElements];
}

- (void)storeWebScriptObject:(WebScriptObject *)webScriptObject
{
    if (webScriptObject == storedWebScriptObject)
        return;

    [storedWebScriptObject release];
    storedWebScriptObject = [webScriptObject retain];
}

- (void)accessStoredWebScriptObject
{
    JSObjectRef jsObject = [storedWebScriptObject JSObject];
    ASSERT(!jsObject);

    [storedWebScriptObject callWebScriptMethod:@"" withArguments:nil];
    [storedWebScriptObject evaluateWebScript:@""];
    [storedWebScriptObject setValue:[WebUndefined undefined] forKey:@"key"];
    [storedWebScriptObject valueForKey:@"key"];
    [storedWebScriptObject removeWebScriptKey:@"key"];
    [storedWebScriptObject stringRepresentation];
    [storedWebScriptObject webScriptValueAtIndex:0];
    [storedWebScriptObject setWebScriptValueAtIndex:0 value:[WebUndefined undefined]];
    [storedWebScriptObject setException:@"exception"];
}

- (void)dealloc
{
    [storedWebScriptObject release];
    [super dealloc];
}

- (NSString*)decodeHostName:(NSString*)name
{
    return [name _web_decodeHostName];
}

- (NSString*)encodeHostName:(NSString*)name
{
    return [name _web_encodeHostName];
}

@end
