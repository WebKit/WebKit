/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
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
#import "EditingDelegate.h"
#import "WorkQueue.h"
#import "WorkQueueItem.h"
#import <JavaScriptCore/JSRetainPtr.h>
#import <JavaScriptCore/JSStringRef.h>
#import <JavaScriptCore/JSStringRefCF.h>
#import <JavaScriptCore/RetainPtr.h>
#import <WebKit/WebBackForwardList.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebHTMLViewPrivate.h>
#import <WebKit/WebHistory.h>
#import <WebKit/WebNSURLExtras.h>
#import <WebKit/WebPreferences.h>
#import <WebKit/WebView.h>
#import <WebKit/WebViewPrivate.h>

#import <Foundation/Foundation.h>

void LayoutTestController::dumpAsText()
{
    ::dumpAsText = true;
}

void LayoutTestController::dumpBackForwardList()
{
    ::dumpBackForwardList = true;
}

void LayoutTestController::dumpChildFramesAsText()
{
    ::dumpChildFramesAsText = true;
}

void LayoutTestController::dumpChildFrameScrollPositions()
{
    ::dumpChildFrameScrollPositions = true;
}

void LayoutTestController::dumpDOMAsWebArchive()
{
    ::dumpDOMAsWebArchive = true;
}

void LayoutTestController::dumpEditingCallbacks()
{
    ::shouldDumpEditingCallbacks = true;
}

void LayoutTestController::dumpFrameLoadCallbacks()
{
    ::shouldDumpFrameLoadCallbacks = true;
}

void LayoutTestController::dumpResourceLoadCallbacks()
{
    ::shouldDumpResourceLoadCallbacks = true;
}

void LayoutTestController::dumpSelectionRect()
{
    ::dumpSelectionRect = true;
}

void LayoutTestController::dumpSourceAsWebArchive()
{
    ::dumpSourceAsWebArchive = true;
}

void LayoutTestController::dumpTitleChanges()
{
    ::dumpTitleChanges = true;
}

void LayoutTestController::repaintSweepHorizontally()
{
    ::repaintSweepHorizontally = true;
}

void LayoutTestController::setCallCloseOnWebViews()
{
    ::closeWebViews = true;
}

void LayoutTestController::setCanOpenWindows()
{
    ::canOpenWindows = true;
}

void LayoutTestController::setCloseRemainingWindowsWhenComplete()
{
    ::closeRemainingWindowsWhenComplete = true;
}

void LayoutTestController::testRepaint()
{
    ::testRepaint = true;
}

void LayoutTestController::addFileToPasteboardOnDrag()
{
    ::addFileToPasteboardOnDrag = true;
}

void LayoutTestController::addDisallowedURL(JSStringRef url)
{
    RetainPtr<CFStringRef> urlCF(AdoptCF, JSStringCopyCFString(kCFAllocatorDefault, url));

    if (!disallowedURLs)
        disallowedURLs = [[NSMutableSet alloc] init];

    // Canonicalize the URL
    NSURLRequest* request = [NSURLRequest requestWithURL:[NSURL URLWithString:(NSString *)urlCF.get()]];
    request = [NSURLProtocol canonicalRequestForRequest:request];

    [disallowedURLs addObject:[request URL]];
}

void LayoutTestController::clearBackForwardList()
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

JSStringRef LayoutTestController::copyDecodedHostName(JSStringRef name)
{
    RetainPtr<CFStringRef> nameCF(AdoptCF, JSStringCopyCFString(kCFAllocatorDefault, name));
    NSString *nameNS = (NSString *)nameCF.get();
    return JSStringCreateWithCFString((CFStringRef)[nameNS _web_decodeHostName]);
}

JSStringRef LayoutTestController::copyEncodedHostName(JSStringRef name)
{
    RetainPtr<CFStringRef> nameCF(AdoptCF, JSStringCopyCFString(kCFAllocatorDefault, name));
    NSString *nameNS = (NSString *)nameCF.get();
    return JSStringCreateWithCFString((CFStringRef)[nameNS _web_encodeHostName]);
}

void LayoutTestController::display()
{
    displayWebView();
}

void LayoutTestController::keepWebHistory()
{
    if (![WebHistory optionalSharedHistory]) {
        WebHistory *history = [[WebHistory alloc] init];
        [WebHistory setOptionalSharedHistory:history];
        [history release];
    }
}

void LayoutTestController::notifyDone()
{
    if (waitToDump && !topLoadingFrame && !WorkQueue::shared()->count())
        dump();
    waitToDump = false;
}

void LayoutTestController::queueBackNavigation(int howFarBack)
{
    WorkQueue::shared()->queue(new BackItem(howFarBack));
}

void LayoutTestController::queueForwardNavigation(int howFarForward)
{
    WorkQueue::shared()->queue(new ForwardItem(howFarForward));
}

void LayoutTestController::queueLoad(JSStringRef url, JSStringRef target)
{
    RetainPtr<CFStringRef> urlCF(AdoptCF, JSStringCopyCFString(kCFAllocatorDefault, url));
    NSString *urlNS = (NSString *)urlCF.get();

    NSURL *nsurl = [NSURL URLWithString:urlNS relativeToURL:[[[mainFrame dataSource] response] URL]];
    NSString* nsurlString = [nsurl absoluteString];

    JSRetainPtr<JSStringRef> absoluteURL(Adopt, JSStringCreateWithUTF8CString([nsurlString UTF8String]));
    WorkQueue::shared()->queue(new LoadItem(absoluteURL.get(), target));
}

void LayoutTestController::queueReload()
{
    WorkQueue::shared()->queue(new ReloadItem);
}

void LayoutTestController::queueScript(JSStringRef script)
{
    WorkQueue::shared()->queue(new ScriptItem(script));
}

void LayoutTestController::setAcceptsEditing(bool newAcceptsEditing)
{
    [(EditingDelegate *)[[mainFrame webView] editingDelegate] setAcceptsEditing:newAcceptsEditing];
}

void LayoutTestController::setCustomPolicyDelegate(bool setDelegate)
{
    if (setDelegate)
        [[mainFrame webView] setPolicyDelegate:policyDelegate];
    else
        [[mainFrame webView] setPolicyDelegate:nil];
}

void LayoutTestController::setMainFrameIsFirstResponder(bool flag)
{
    NSView *documentView = [[mainFrame frameView] documentView];
    
    NSResponder *firstResponder = flag ? documentView : nil;
    [[[mainFrame webView] window] makeFirstResponder:firstResponder];
        
    if ([documentView isKindOfClass:[WebHTMLView class]])
        [(WebHTMLView *)documentView _updateActiveState];
}

void LayoutTestController::setTabKeyCyclesThroughElements(bool cycles)
{
    [[mainFrame webView] setTabKeyCyclesThroughElements:cycles];
}

void LayoutTestController::setUseDashboardCompatibilityMode(bool flag)
{
    [[mainFrame webView] _setDashboardBehavior:WebDashboardBehaviorUseBackwardCompatibilityMode to:flag];
}

void LayoutTestController::setUserStyleSheetEnabled(bool flag)
{
    [[WebPreferences standardPreferences] setUserStyleSheetEnabled:flag];
}

void LayoutTestController::setUserStyleSheetLocation(JSStringRef path)
{
    RetainPtr<CFStringRef> pathCF(AdoptCF, JSStringCopyCFString(kCFAllocatorDefault, path));
    NSURL *url = [NSURL URLWithString:(NSString *)pathCF.get()];
    [[WebPreferences standardPreferences] setUserStyleSheetLocation:url];
}

void LayoutTestController::setWindowIsKey(bool flag)
{
    windowIsKey = flag;
    NSView *documentView = [[mainFrame frameView] documentView];
    if ([documentView isKindOfClass:[WebHTMLView class]])
        [(WebHTMLView *)documentView _updateActiveState];
}

@interface WaitToDumpWatchdog : NSObject
+ (void)waitUntilDoneWatchdogFired;
@end

@implementation WaitToDumpWatchdog
+ (void)waitUntilDoneWatchdogFired
{
    const char* message = "FAIL: Timed out waiting for notifyDone to be called\n";
    fprintf(stderr, message);
    fprintf(stdout, message);
    dump();
}
@end

void LayoutTestController::waitUntilDone()
{
    waitToDump = true;
    if (!waitToDumpWatchdog)
        waitToDumpWatchdog = [[NSTimer scheduledTimerWithTimeInterval:waitToDumpWatchdogInterval target:[WaitToDumpWatchdog class] selector:@selector(waitUntilDoneWatchdogFired) userInfo:nil repeats:NO] retain];
}

int LayoutTestController::windowCount()
{
    return CFArrayGetCount(allWindowsRef);
}
