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
#import "UIDelegate.h"

#import "DumpRenderTree.h"
#import "DumpRenderTreeDraggingInfo.h"
#import "EventSendingController.h"
#import "MockWebNotificationProvider.h"
#import "TestRunner.h"

#import <WebKit/WebApplicationCache.h>
#import <WebKit/WebFramePrivate.h>
#import <WebKit/WebHTMLViewPrivate.h>
#import <WebKit/WebDatabaseManagerPrivate.h>
#import <WebKit/WebQuotaManager.h>
#import <WebKit/WebSecurityOriginPrivate.h>
#import <WebKit/WebUIDelegatePrivate.h>
#import <WebKit/WebView.h>
#import <WebKit/WebViewPrivate.h>
#import <wtf/Assertions.h>

#if !PLATFORM(IOS_FAMILY)
DumpRenderTreeDraggingInfo *draggingInfo = nil;
#endif

@implementation UIDelegate

- (void)resetWindowOrigin
{
    windowOrigin = NSZeroPoint;
}

- (void)resetToConsistentStateBeforeTesting:(const TestOptions&)options
{
    m_enableDragDestinationActionLoad = options.enableDragDestinationActionLoad;
}

- (void)webView:(WebView *)sender setFrame:(NSRect)frame
{
    // FIXME: Do we need to resize an NSWindow too?
    windowOrigin = frame.origin;
    [sender setFrameSize:frame.size];
}

- (NSRect)webViewFrame:(WebView *)sender
{
    NSSize size = [sender frame].size;
    return NSMakeRect(windowOrigin.x, windowOrigin.y, size.width, size.height);
}

- (void)webView:(WebView *)sender addMessageToConsole:(NSDictionary *)dictionary withSource:(NSString *)source
{
    if (done)
        return;

    NSString *message = [dictionary objectForKey:@"message"];
    NSNumber *lineNumber = [dictionary objectForKey:@"lineNumber"];

    NSRange range = [message rangeOfString:@"file://"];
    if (range.location != NSNotFound)
        message = [[message substringToIndex:range.location] stringByAppendingString:[[message substringFromIndex:NSMaxRange(range)] lastPathComponent]];

    auto out = gTestRunner->dumpJSConsoleLogInStdErr() ? stderr : stdout;
    fprintf(out, "CONSOLE MESSAGE: ");
    if ([lineNumber intValue])
        fprintf(out, "line %d: ", [lineNumber intValue]);
    fprintf(out, "%s\n", [message UTF8String]);
}

- (void)modalWindowWillClose:(NSNotification *)notification
{
#if !PLATFORM(IOS_FAMILY)
    [[NSNotificationCenter defaultCenter] removeObserver:self name:NSWindowWillCloseNotification object:nil];
    [NSApp abortModal];
#endif
}

- (void)webViewRunModal:(WebView *)sender
{
#if !PLATFORM(IOS_FAMILY)
    gTestRunner->setWindowIsKey(false);
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(modalWindowWillClose:) name:NSWindowWillCloseNotification object:nil];
    [NSApp runModalForWindow:[sender window]];
    gTestRunner->setWindowIsKey(true);
#endif
}

- (void)webView:(WebView *)sender runJavaScriptAlertPanelWithMessage:(NSString *)message initiatedByFrame:(WebFrame *)frame
{
    if (!done) {
        printf("ALERT: %s\n", [message UTF8String]);
        fflush(stdout);
    }
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
    
    return !gTestRunner->shouldStayOnPageAfterHandlingBeforeUnload();
}


#if !PLATFORM(IOS_FAMILY)
- (void)webView:(WebView *)sender dragImage:(NSImage *)anImage at:(NSPoint)viewLocation offset:(NSSize)initialOffset event:(NSEvent *)event pasteboard:(NSPasteboard *)pboard source:(id)sourceObj slideBack:(BOOL)slideFlag forView:(NSView *)view
{
     assert(!draggingInfo);
     draggingInfo = [[DumpRenderTreeDraggingInfo alloc] initWithImage:anImage offset:initialOffset pasteboard:pboard source:sourceObj];
     [sender draggingUpdated:draggingInfo];
     [EventSendingController replaySavedEvents];
}
#endif

- (void)webViewFocus:(WebView *)webView
{
    gTestRunner->setWindowIsKey(true);
}

- (void)webViewUnfocus:(WebView *)webView
{
    gTestRunner->setWindowIsKey(false);
}

- (WebView *)webView:(WebView *)sender createWebViewWithRequest:(NSURLRequest *)request
{
    if (!gTestRunner->canOpenWindows())
        return nil;
    
    // Make sure that waitUntilDone has been called.
    ASSERT(gTestRunner->waitToDump());

    WebView *webView = createWebViewAndOffscreenWindow();
    
    if (gTestRunner->newWindowsCopyBackForwardList())
        [webView _loadBackForwardListFromOtherView:sender];
    
    return [webView autorelease];
}

- (void)webViewClose:(WebView *)sender
{
    NSWindow* window = [sender window];
 
    if (gTestRunner->callCloseOnWebViews())
        [sender close];
    
    [window close];
}

- (void)webView:(WebView *)sender frame:(WebFrame *)frame exceededDatabaseQuotaForSecurityOrigin:(WebSecurityOrigin *)origin database:(NSString *)databaseIdentifier
{
    if (!done && gTestRunner->dumpDatabaseCallbacks()) {
        printf("UI DELEGATE DATABASE CALLBACK: exceededDatabaseQuotaForSecurityOrigin:{%s, %s, %i} database:%s\n", [[origin protocol] UTF8String], [[origin host] UTF8String], 
            [origin port], [databaseIdentifier UTF8String]);
    }

    NSDictionary *databaseDetails = [[WebDatabaseManager sharedWebDatabaseManager] detailsForDatabase:databaseIdentifier withOrigin:origin];
    unsigned long long expectedSize = [[databaseDetails objectForKey:WebDatabaseExpectedSizeKey] unsignedLongLongValue];
    unsigned long long defaultQuota = 5 * 1024 * 1024;
    double testDefaultQuota = gTestRunner->databaseDefaultQuota();
    if (testDefaultQuota >= 0)
        defaultQuota = testDefaultQuota;

    unsigned long long newQuota = defaultQuota;

    double maxQuota = gTestRunner->databaseMaxQuota();
    if (maxQuota >= 0) {
        if (defaultQuota < expectedSize && expectedSize <= maxQuota) {
            newQuota = expectedSize;
            printf("UI DELEGATE DATABASE CALLBACK: increased quota to %llu\n", newQuota);
        }
    }
    [[origin databaseQuotaManager] setQuota:newQuota];
}

- (void)webView:(WebView *)sender exceededApplicationCacheOriginQuotaForSecurityOrigin:(WebSecurityOrigin *)origin totalSpaceNeeded:(NSUInteger)totalSpaceNeeded
{
    if (!done && gTestRunner->dumpApplicationCacheDelegateCallbacks()) {
        // For example, numbers from 30000 - 39999 will output as 30000.
        // Rounding up or down not really matter for these tests. It's
        // sufficient to just get a range of 10000 to determine if we were
        // above or below a threshold.
        unsigned long truncatedSpaceNeeded = static_cast<unsigned long>((totalSpaceNeeded / 10000) * 10000);
        printf("UI DELEGATE APPLICATION CACHE CALLBACK: exceededApplicationCacheOriginQuotaForSecurityOrigin:{%s, %s, %i} totalSpaceNeeded:~%lu\n",
            [[origin protocol] UTF8String], [[origin host] UTF8String], [origin port], truncatedSpaceNeeded);
    }

    if (gTestRunner->disallowIncreaseForApplicationCacheQuota())
        return;

    static const unsigned long long defaultOriginQuota = [WebApplicationCache defaultOriginQuota];
    [[origin applicationCacheQuotaManager] setQuota:defaultOriginQuota];
}

- (void)webView:(WebView *)sender setStatusText:(NSString *)text
{
    if (!done && gTestRunner->dumpStatusCallbacks())
        printf("UI DELEGATE STATUS CALLBACK: setStatusText:%s\n", [text UTF8String]);
}

- (void)webView:(WebView *)webView decidePolicyForGeolocationRequestFromOrigin:(WebSecurityOrigin *)origin frame:(WebFrame *)frame listener:(id<WebAllowDenyPolicyListener>)listener
{
    if (!gTestRunner->isGeolocationPermissionSet()) {
        if (!m_pendingGeolocationPermissionListeners)
            m_pendingGeolocationPermissionListeners = [[NSMutableSet set] retain];
        [m_pendingGeolocationPermissionListeners addObject:listener];
        return;
    }

    if (gTestRunner->geolocationPermission())
        [listener allow];
    else
        [listener deny];
}

- (void)didSetMockGeolocationPermission
{
    ASSERT(gTestRunner->isGeolocationPermissionSet());
    if (m_pendingGeolocationPermissionListeners && !m_timer)
        m_timer = [NSTimer scheduledTimerWithTimeInterval:0 target:self selector:@selector(timerFired) userInfo:0 repeats:NO];
}

- (int)numberOfPendingGeolocationPermissionRequests
{
    if (!m_pendingGeolocationPermissionListeners)
        return 0;
    return [m_pendingGeolocationPermissionListeners count];
}


- (void)timerFired
{
    m_timer = 0;
    NSEnumerator* enumerator = [m_pendingGeolocationPermissionListeners objectEnumerator];
    id<WebAllowDenyPolicyListener> listener;
    while ((listener = [enumerator nextObject])) {
        if (gTestRunner->geolocationPermission())
            [listener allow];
        else
            [listener deny];
    }
    [m_pendingGeolocationPermissionListeners removeAllObjects];
    [m_pendingGeolocationPermissionListeners release];
    m_pendingGeolocationPermissionListeners = nil;
}

- (BOOL)webView:(WebView *)sender shouldHaltPlugin:(DOMNode *)pluginNode
{
    return NO;
}

- (BOOL)webView:(WebView *)webView supportsFullScreenForElement:(DOMElement*)element withKeyboard:(BOOL)withKeyboard
{
#if PLATFORM(IOS_FAMILY)
    return NO;
#else
    return YES;
#endif
}

#if ENABLE(FULLSCREEN_API)
- (void)enterFullScreenWithListener:(NSObject<WebKitFullScreenListener>*)listener
{
    [listener webkitWillEnterFullScreen];
    [listener webkitDidEnterFullScreen];
}

- (void)webView:(WebView *)webView enterFullScreenForElement:(DOMElement*)element listener:(NSObject<WebKitFullScreenListener>*)listener
{
    if (!gTestRunner->hasCustomFullScreenBehavior())
        [self performSelector:@selector(enterFullScreenWithListener:) withObject:listener afterDelay:0];
}

- (void)exitFullScreenWithListener:(NSObject<WebKitFullScreenListener>*)listener
{
    [listener webkitWillExitFullScreen];
    [listener webkitDidExitFullScreen];
}

- (void)webView:(WebView *)webView exitFullScreenForElement:(DOMElement*)element listener:(NSObject<WebKitFullScreenListener>*)listener
{
    if (!gTestRunner->hasCustomFullScreenBehavior())
        [self performSelector:@selector(exitFullScreenWithListener:) withObject:listener afterDelay:0];
}

- (void)webView:(WebView *)sender closeFullScreenWithListener:(NSObject<WebKitFullScreenListener>*)listener
{
    [listener webkitWillExitFullScreen];
    [listener webkitDidExitFullScreen];
}
#endif

- (BOOL)webView:(WebView *)webView didPressMissingPluginButton:(DOMElement *)element
{
    if (!done)
        printf("MISSING PLUGIN BUTTON PRESSED\n");
    return TRUE;
}

- (void)webView:(WebView *)webView decidePolicyForNotificationRequestFromOrigin:(WebSecurityOrigin *)origin listener:(id<WebAllowDenyPolicyListener>)listener
{
    MockWebNotificationProvider *provider = (MockWebNotificationProvider *)[webView _notificationProvider];
    switch ([provider policyForOrigin:origin]) {
    case WebNotificationPermissionAllowed:
        [listener allow];
        break;
    case WebNotificationPermissionDenied:
        [listener deny];
        break;
    case WebNotificationPermissionNotAllowed:
        [provider setWebNotificationOrigin:[origin stringValue] permission:YES];
        [listener allow];
        break;
    }
}

- (NSData *)webCryptoMasterKeyForWebView:(WebView *)sender
{
    // Any 128 bit key would do, all we need for testing is to implement the callback.
    return [NSData dataWithBytes:"\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f" length:16];
}

- (NSString *)signedPublicKeyAndChallengeStringForWebView:(WebView *)sender
{
    // Any fake response would do, all we need for testing is to implement the callback.
    return @"MIHFMHEwXDANBgkqhkiG9w0BAQEFAANLADBIAkEAnX0TILJrOMUue%2BPtwBRE6XfV%0AWtKQbsshxk5ZhcUwcwyvcnIq9b82QhJdoACdD34rqfCAIND46fXKQUnb0mvKzQID%0AAQABFhFNb3ppbGxhSXNNeUZyaWVuZDANBgkqhkiG9w0BAQQFAANBAAKv2Eex2n%2FS%0Ar%2F7iJNroWlSzSMtTiQTEB%2BADWHGj9u1xrUrOilq%2Fo2cuQxIfZcNZkYAkWP4DubqW%0Ai0%2F%2FrgBvmco%3D";
}

- (void)webView:(WebView *)sender runOpenPanelForFileButtonWithResultListener:(id<WebOpenPanelResultListener>)resultListener allowMultipleFiles:(BOOL)allowMultipleFiles
{
    printf("OPEN FILE PANEL\n");

    auto& openPanelFiles = gTestRunner->openPanelFiles();
    if (openPanelFiles.empty()) {
        [resultListener cancel];
        return;
    }

    NSURL *baseURL = [NSURL URLWithString:[NSString stringWithUTF8String:gTestRunner->testURL().c_str()]];
    auto filePaths = adoptNS([[NSMutableArray alloc] initWithCapacity:openPanelFiles.size()]);
    for (auto& filePath : openPanelFiles) {
        NSURL *fileURL = [NSURL fileURLWithPath:[NSString stringWithUTF8String:filePath.c_str()] relativeToURL:baseURL];
        [filePaths addObject:fileURL.path];
    }

#if PLATFORM(IOS_FAMILY)
    NSURL *firstURL = [NSURL fileURLWithPath:[NSString stringWithUTF8String:openPanelFiles[0].c_str()] relativeToURL:baseURL];
    NSString *displayString = firstURL.lastPathComponent;
    const std::vector<char>& iconData = gTestRunner->openPanelFilesMediaIcon();
    CGImageRef imageRef;
    if (!iconData.empty()) {
        RetainPtr<CFDataRef> dataRef = adoptCF(CFDataCreate(nullptr, (unsigned char *)iconData.data(), iconData.size()));
        RetainPtr<CGDataProviderRef> imageProviderRef = adoptCF(CGDataProviderCreateWithCFData(dataRef.get()));
        imageRef = CGImageCreateWithJPEGDataProvider(imageProviderRef.get(), nullptr, true, kCGRenderingIntentDefault);
    }
#endif

    if (allowMultipleFiles) {
#if PLATFORM(IOS_FAMILY)
        [resultListener chooseFilenames:filePaths.get() displayString:displayString iconImage:imageRef];
#else
        [resultListener chooseFilenames:filePaths.get()];
#endif
        return;
    }

#if PLATFORM(IOS_FAMILY)
    [resultListener chooseFilename:[filePaths firstObject] displayString:displayString iconImage:imageRef];
#else
    [resultListener chooseFilename:[filePaths firstObject]];
#endif
}

#if !PLATFORM(IOS_FAMILY)

- (NSUInteger)webView:(WebView *)webView dragDestinationActionMaskForDraggingInfo:(id <NSDraggingInfo>)draggingInfo
{
    WebDragDestinationAction actions = WebDragDestinationActionAny;
    if (!m_enableDragDestinationActionLoad)
        actions &= ~WebDragDestinationActionLoad;
    return actions;
}

#endif

- (void)dealloc
{
#if !PLATFORM(IOS_FAMILY)
    [draggingInfo release];
    draggingInfo = nil;
#endif
    [m_pendingGeolocationPermissionListeners release];
    m_pendingGeolocationPermissionListeners = nil;

    [super dealloc];
}

@end
