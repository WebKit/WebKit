/*
 * Copyright (C) 2010, 2014 Apple Inc. All rights reserved.
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
#import "WebInspectorProxy.h"

#if PLATFORM(MAC) && WK_API_ENABLED

#import "WKAPICast.h"
#import "WKInspectorPrivateMac.h"
#import "WKMutableArray.h"
#import "WKOpenPanelParameters.h"
#import "WKOpenPanelResultListener.h"
#import "WKPreferencesInternal.h"
#import "WKProcessPoolInternal.h"
#import "WKRetainPtr.h"
#import "WKURLCF.h"
#import "WKViewInternal.h"
#import "WKWebViewConfigurationPrivate.h"
#import "WKWebViewInternal.h"
#import "WebInspectorUIMessages.h"
#import "WebPageGroup.h"
#import "WebPageProxy.h"
#import "WebPreferences.h"
#import "WebProcessProxy.h"
#import <WebCore/InspectorFrontendClientLocal.h>
#import <WebCore/LocalizedStrings.h>
#import <WebCore/SoftLinking.h>
#import <wtf/text/Base64.h>

SOFT_LINK_STAGED_FRAMEWORK(WebInspectorUI, PrivateFrameworks, A)

using namespace WebCore;
using namespace WebKit;

static const NSUInteger windowStyleMask = NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask | NSResizableWindowMask | NSFullSizeContentViewWindowMask;

// The time we keep our WebView alive before closing it and its process.
// Reusing the WebView improves start up time for people that jump in and out of the Inspector.
static const unsigned webViewCloseTimeout = 60;

// WKWebInspectorProxyObjCAdapter is a helper ObjC object used as a delegate or notification observer
// for the sole purpose of getting back into the C++ code from an ObjC caller.

@interface WKWebInspectorProxyObjCAdapter ()

- (id)initWithWebInspectorProxy:(WebInspectorProxy*)inspectorProxy;
- (void)close;
- (void)didRelaunchProcess;

@end

@implementation WKWebInspectorProxyObjCAdapter

- (WKInspectorRef)inspectorRef
{
    return toAPI(static_cast<WebInspectorProxy*>(_inspectorProxy));
}

- (id)initWithWebInspectorProxy:(WebInspectorProxy*)inspectorProxy
{
    ASSERT_ARG(inspectorProxy, inspectorProxy);

    if (!(self = [super init]))
        return nil;

    _inspectorProxy = static_cast<void*>(inspectorProxy); // Not retained to prevent cycles

    return self;
}

- (void)close
{
    _inspectorProxy = nullptr;
}

- (void)didRelaunchProcess
{
    static_cast<WebInspectorProxy*>(_inspectorProxy)->didRelaunchInspectorPageProcess();
}

- (void)windowDidMove:(NSNotification *)notification
{
    static_cast<WebInspectorProxy*>(_inspectorProxy)->windowFrameDidChange();
}

- (void)windowDidResize:(NSNotification *)notification
{
    static_cast<WebInspectorProxy*>(_inspectorProxy)->windowFrameDidChange();
}

- (void)windowWillClose:(NSNotification *)notification
{
    static_cast<WebInspectorProxy*>(_inspectorProxy)->close();
}

- (void)windowDidEnterFullScreen:(NSNotification *)notification
{
    static_cast<WebInspectorProxy*>(_inspectorProxy)->windowFullScreenDidChange();
}

- (void)windowDidExitFullScreen:(NSNotification *)notification
{
    static_cast<WebInspectorProxy*>(_inspectorProxy)->windowFullScreenDidChange();
}

- (void)inspectedViewFrameDidChange:(NSNotification *)notification
{
    // Resizing the views while inside this notification can lead to bad results when entering
    // or exiting full screen. To avoid that we need to perform the work after a delay. We only
    // depend on this for enforcing the height constraints, so a small delay isn't terrible. Most
    // of the time the views will already have the correct frames because of autoresizing masks.

    dispatch_after(DISPATCH_TIME_NOW, dispatch_get_main_queue(), ^{
        if (!_inspectorProxy)
            return;
        static_cast<WebInspectorProxy*>(_inspectorProxy)->inspectedViewFrameDidChange();
    });
}

@end

@interface WKWebInspectorWKWebView : WKWebView {
@private
    WKWebInspectorProxyObjCAdapter *_inspectorProxyObjCAdapter;
}

@property (nonatomic, assign) WKWebInspectorProxyObjCAdapter *inspectorProxyObjCAdapter;
@end

@implementation WKWebInspectorWKWebView

@synthesize inspectorProxyObjCAdapter = _inspectorProxyObjCAdapter;

- (NSInteger)tag
{
    return WKInspectorViewTag;
}

- (void)_didRelaunchProcess
{
    [self.inspectorProxyObjCAdapter didRelaunchProcess];
}

@end

namespace WebKit {

static WKRect getWindowFrame(WKPageRef, const void* clientInfo)
{
    WebInspectorProxy* webInspectorProxy = static_cast<WebInspectorProxy*>(const_cast<void*>(clientInfo));
    ASSERT(webInspectorProxy);

    return webInspectorProxy->inspectorWindowFrame();
}

static void setWindowFrame(WKPageRef, WKRect frame, const void* clientInfo)
{
    WebInspectorProxy* webInspectorProxy = static_cast<WebInspectorProxy*>(const_cast<void*>(clientInfo));
    ASSERT(webInspectorProxy);

    webInspectorProxy->setInspectorWindowFrame(frame);
}

static unsigned long long exceededDatabaseQuota(WKPageRef, WKFrameRef, WKSecurityOriginRef, WKStringRef, WKStringRef, unsigned long long, unsigned long long, unsigned long long currentDatabaseUsage, unsigned long long expectedUsage, const void*)
{
    return std::max<unsigned long long>(expectedUsage, currentDatabaseUsage * 1.25);
}

static void runOpenPanel(WKPageRef page, WKFrameRef frame, WKOpenPanelParametersRef parameters, WKOpenPanelResultListenerRef listener, const void* clientInfo)
{
    WebInspectorProxy* webInspectorProxy = static_cast<WebInspectorProxy*>(const_cast<void*>(clientInfo));
    ASSERT(webInspectorProxy);

    NSOpenPanel *openPanel = [NSOpenPanel openPanel];
    [openPanel setAllowsMultipleSelection:WKOpenPanelParametersGetAllowsMultipleFiles(parameters)];

    WKRetain(listener);

    auto completionHandler = ^(NSInteger result) {
        if (result == NSFileHandlingPanelOKButton) {
            WKMutableArrayRef fileURLs = WKMutableArrayCreate();

            for (NSURL* nsURL in [openPanel URLs]) {
                WKURLRef wkURL = WKURLCreateWithCFURL(reinterpret_cast<CFURLRef>(nsURL));
                WKArrayAppendItem(fileURLs, wkURL);
                WKRelease(wkURL);
            }

            WKOpenPanelResultListenerChooseFiles(listener, fileURLs);

            WKRelease(fileURLs);
        } else
            WKOpenPanelResultListenerCancel(listener);

        WKRelease(listener);
    };

    if (webInspectorProxy->inspectorWindow())
        [openPanel beginSheetModalForWindow:webInspectorProxy->inspectorWindow() completionHandler:completionHandler];
    else
        completionHandler([openPanel runModal]);
}

void WebInspectorProxy::attachmentViewDidChange(NSView *oldView, NSView *newView)
{
    [[NSNotificationCenter defaultCenter] removeObserver:m_inspectorProxyObjCAdapter.get() name:NSViewFrameDidChangeNotification object:oldView];
    [[NSNotificationCenter defaultCenter] addObserver:m_inspectorProxyObjCAdapter.get() selector:@selector(inspectedViewFrameDidChange:) name:NSViewFrameDidChangeNotification object:newView];

    if (m_isAttached)
        attach(m_attachmentSide);
}

void WebInspectorProxy::setInspectorWindowFrame(WKRect& frame)
{
    if (m_isAttached)
        return;
    [m_inspectorWindow setFrame:NSMakeRect(frame.origin.x, frame.origin.y, frame.size.width, frame.size.height) display:YES];
}

WKRect WebInspectorProxy::inspectorWindowFrame()
{
    if (m_isAttached)
        return WKRectMake(0, 0, 0, 0);

    NSRect frame = m_inspectorWindow.get().frame;
    return WKRectMake(frame.origin.x, frame.origin.y, frame.size.width, frame.size.height);
}

void WebInspectorProxy::closeTimerFired()
{
    ASSERT(!m_isAttached || !m_inspectorWindow);
    if (m_isAttached || m_inspectorWindow)
        return;

    if (m_inspectorView) {
        m_inspectorView->_page->close();
        [m_inspectorView setInspectorProxyObjCAdapter:nil];
        m_inspectorView = nil;
    }

    [[NSNotificationCenter defaultCenter] removeObserver:m_inspectorProxyObjCAdapter.get()];

    [m_inspectorProxyObjCAdapter close];
    m_inspectorProxyObjCAdapter = nil;
}

void WebInspectorProxy::createInspectorWindow()
{
    ASSERT(!m_inspectorWindow);

    NSRect windowFrame = NSMakeRect(0, 0, initialWindowWidth, initialWindowHeight);

    // Restore the saved window frame, if there was one.
    NSString *savedWindowFrameString = inspectedPage()->pageGroup().preferences().inspectorWindowFrame();
    NSRect savedWindowFrame = NSRectFromString(savedWindowFrameString);
    if (!NSIsEmptyRect(savedWindowFrame))
        windowFrame = savedWindowFrame;

    NSWindow *window = [[NSWindow alloc] initWithContentRect:windowFrame styleMask:windowStyleMask backing:NSBackingStoreBuffered defer:NO];
    [window setDelegate:m_inspectorProxyObjCAdapter.get()];
    [window setMinSize:NSMakeSize(minimumWindowWidth, minimumWindowHeight)];
    [window setReleasedWhenClosed:NO];
    [window setCollectionBehavior:([window collectionBehavior] | NSWindowCollectionBehaviorFullScreenPrimary)];

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101100
    CGFloat approximatelyHalfScreenSize = (window.screen.frame.size.width / 2) - 4;
    CGFloat minimumFullScreenWidth = std::max<CGFloat>(636, approximatelyHalfScreenSize);
    [window setMinFullScreenContentSize:NSMakeSize(minimumFullScreenWidth, minimumWindowHeight)];
    [window setCollectionBehavior:([window collectionBehavior] | NSWindowCollectionBehaviorFullScreenAllowsTiling)];
#endif

    window.titlebarAppearsTransparent = YES;

    m_inspectorWindow = adoptNS(window);

    NSView *contentView = [window contentView];

    [m_inspectorView setFrame:[contentView bounds]];
    [m_inspectorView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [contentView addSubview:m_inspectorView.get()];

    // Center the window if the saved frame was empty.
    if (NSIsEmptyRect(savedWindowFrame))
        [window center];

    updateInspectorWindowTitle();
}

void WebInspectorProxy::updateInspectorWindowTitle() const
{
    if (!m_inspectorWindow)
        return;

    unsigned level = inspectionLevel();
    if (level > 1) {
        NSString *debugTitle = [NSString stringWithFormat:WEB_UI_STRING("Web Inspector [%d] — %@", "Web Inspector window title when inspecting Web Inspector"), level, (NSString *)m_urlString];
        [m_inspectorWindow setTitle:debugTitle];
    } else {
        NSString *title = [NSString stringWithFormat:WEB_UI_STRING("Web Inspector — %@", "Web Inspector window title"), (NSString *)m_urlString];
        [m_inspectorWindow setTitle:title];
    }
}

WebPageProxy* WebInspectorProxy::platformCreateInspectorPage()
{
    ASSERT(inspectedPage());

    m_closeTimer.stop();

    if (m_inspectorView) {
        ASSERT(m_inspectorProxyObjCAdapter);
        return m_inspectorView->_page.get();
    }

    ASSERT(!m_inspectorView);
    ASSERT(!m_inspectorProxyObjCAdapter);

    NSView *inspectedView = inspectedPage()->inspectorAttachmentView();

    NSRect initialRect;
    if (m_isAttached) {
        NSRect inspectedViewFrame = inspectedView.frame;

        switch (m_attachmentSide) {
        case AttachmentSide::Bottom:
            initialRect = NSMakeRect(0, 0, NSWidth(inspectedViewFrame), inspectorPagePreferences().inspectorAttachedHeight());
            break;
        case AttachmentSide::Right:
            initialRect = NSMakeRect(0, 0, inspectorPagePreferences().inspectorAttachedWidth(), NSHeight(inspectedViewFrame));
            break;
        }
    } else {
        initialRect = NSMakeRect(0, 0, initialWindowWidth, initialWindowHeight);

        NSString *windowFrameString = inspectedPage()->pageGroup().preferences().inspectorWindowFrame();
        NSRect windowFrame = NSRectFromString(windowFrameString);
        if (!NSIsEmptyRect(windowFrame))
            initialRect = [NSWindow contentRectForFrameRect:windowFrame styleMask:windowStyleMask];
    }

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    WKPreferences *preferences = [configuration preferences];
#ifndef NDEBUG
    // Allow developers to inspect the Web Inspector in debug builds without changing settings.
    preferences._developerExtrasEnabled = YES;
    preferences._logsPageMessagesToSystemConsoleEnabled = YES;
#endif
    preferences._allowFileAccessFromFileURLs = YES;
    preferences._javaScriptRuntimeFlags = 0;
    if (isUnderTest()) {
        preferences._hiddenPageDOMTimerThrottlingEnabled = NO;
        preferences._pageVisibilityBasedProcessSuppressionEnabled = NO;
    }

    [configuration setProcessPool: ::WebKit::wrapper(inspectorProcessPool())];
    [configuration _setGroupIdentifier:inspectorPageGroupIdentifier()];

    m_inspectorView = adoptNS([[WKWebInspectorWKWebView alloc] initWithFrame:initialRect configuration:configuration.get()]);
    ASSERT(m_inspectorView);

    [m_inspectorView _setAutomaticallyAdjustsContentInsets:NO];

    m_inspectorProxyObjCAdapter = adoptNS([[WKWebInspectorProxyObjCAdapter alloc] initWithWebInspectorProxy:this]);
    ASSERT(m_inspectorProxyObjCAdapter);

    [m_inspectorView setInspectorProxyObjCAdapter:m_inspectorProxyObjCAdapter.get()];

    [[NSNotificationCenter defaultCenter] addObserver:m_inspectorProxyObjCAdapter.get() selector:@selector(inspectedViewFrameDidChange:) name:NSViewFrameDidChangeNotification object:inspectedView];

    WebPageProxy* inspectorPage = m_inspectorView->_page.get();
    ASSERT(inspectorPage);

    WKPageUIClientV2 uiClient = {
        { 2, this },
        0, // createNewPage_deprecatedForUseWithV0
        0, // showPage
        0, // closePage
        0, // takeFocus
        0, // focus
        0, // unfocus
        0, // runJavaScriptAlert
        0, // runJavaScriptConfirm
        0, // runJavaScriptPrompt
        0, // setStatusText
        0, // mouseDidMoveOverElement_deprecatedForUseWithV0
        0, // missingPluginButtonClicked_deprecatedForUseWithV0
        0, // didNotHandleKeyEvent
        0, // didNotHandleWheelEvent
        0, // areToolbarsVisible
        0, // setToolbarsVisible
        0, // isMenuBarVisible
        0, // setMenuBarVisible
        0, // isStatusBarVisible
        0, // setStatusBarVisible
        0, // isResizable
        0, // setResizable
        getWindowFrame,
        setWindowFrame,
        0, // runBeforeUnloadConfirmPanel
        0, // didDraw
        0, // pageDidScroll
        exceededDatabaseQuota,
        runOpenPanel,
        0, // decidePolicyForGeolocationPermissionRequest
        0, // headerHeight
        0, // footerHeight
        0, // drawHeader
        0, // drawFooter
        0, // printFrame
        0, // runModal
        0, // unused
        0, // saveDataToFileInDownloadsFolder
        0, // shouldInterruptJavaScript
        0, // createPage
        0, // mouseDidMoveOverElement
        0, // decidePolicyForNotificationPermissionRequest
        0, // unavailablePluginButtonClicked_deprecatedForUseWithV1
        0, // showColorPicker
        0, // hideColorPicker
        0, // unavailablePluginButtonClicked
    };

    WKPageSetPageUIClient(toAPI(inspectorPage), &uiClient.base);

    return inspectorPage;
}

bool WebInspectorProxy::platformCanAttach(bool webProcessCanAttach)
{
    if ([m_inspectorWindow styleMask] & NSFullScreenWindowMask)
        return false;

    NSView *inspectedView = inspectedPage()->inspectorAttachmentView();
    if ([inspectedView isKindOfClass:[WKView class]] || [inspectedView isKindOfClass:[WKWebView class]])
        return webProcessCanAttach;

    static const float minimumAttachedHeight = 250;
    static const float maximumAttachedHeightRatio = 0.75;
    static const float minimumAttachedWidth = 500;

    NSRect inspectedViewFrame = inspectedView.frame;

    float maximumAttachedHeight = NSHeight(inspectedViewFrame) * maximumAttachedHeightRatio;
    return minimumAttachedHeight <= maximumAttachedHeight && minimumAttachedWidth <= NSWidth(inspectedViewFrame);
}

void WebInspectorProxy::platformOpen()
{
    if (m_isAttached)
        platformAttach();
    else
        createInspectorWindow();

    platformBringToFront();
}

void WebInspectorProxy::platformDidClose()
{
    if (m_inspectorWindow) {
        [m_inspectorWindow setDelegate:nil];
        [m_inspectorWindow close];
        m_inspectorWindow = nil;
    }

    m_closeTimer.startOneShot(webViewCloseTimeout);
}

void WebInspectorProxy::platformInvalidate()
{
    m_closeTimer.stop();

    closeTimerFired();
}

void WebInspectorProxy::platformHide()
{
    if (m_isAttached) {
        platformDetach();
        return;
    }

    if (m_inspectorWindow) {
        [m_inspectorWindow setDelegate:nil];
        [m_inspectorWindow close];
        m_inspectorWindow = nil;
    }
}

void WebInspectorProxy::platformBringToFront()
{
    // If the Web Inspector is no longer in the same window as the inspected view,
    // then we need to reopen the Inspector to get it attached to the right window.
    // This can happen when dragging tabs to another window in Safari.
    if (m_isAttached && m_inspectorView.get().window != inspectedPage()->platformWindow()) {
        platformOpen();
        return;
    }

    // FIXME <rdar://problem/10937688>: this will not bring a background tab in Safari to the front, only its window.
    [m_inspectorView.get().window makeKeyAndOrderFront:nil];
    [m_inspectorView.get().window makeFirstResponder:m_inspectorView.get()];
}

bool WebInspectorProxy::platformIsFront()
{
    // FIXME <rdar://problem/10937688>: this will not return false for a background tab in Safari, only a background window.
    return m_isVisible && [m_inspectorView.get().window isMainWindow];
}

void WebInspectorProxy::platformAttachAvailabilityChanged(bool available)
{
    // Do nothing.
}

void WebInspectorProxy::platformInspectedURLChanged(const String& urlString)
{
    m_urlString = urlString;

    updateInspectorWindowTitle();
}

void WebInspectorProxy::platformSave(const String& suggestedURL, const String& content, bool base64Encoded, bool forceSaveDialog)
{
    ASSERT(!suggestedURL.isEmpty());
    
    NSURL *platformURL = m_suggestedToActualURLMap.get(suggestedURL).get();
    if (!platformURL) {
        platformURL = [NSURL URLWithString:suggestedURL];
        // The user must confirm new filenames before we can save to them.
        forceSaveDialog = true;
    }
    
    ASSERT(platformURL);
    if (!platformURL)
        return;

    // Necessary for the block below.
    String suggestedURLCopy = suggestedURL;
    String contentCopy = content;

    auto saveToURL = ^(NSURL *actualURL) {
        ASSERT(actualURL);

        m_suggestedToActualURLMap.set(suggestedURLCopy, actualURL);

        if (base64Encoded) {
            Vector<char> out;
            if (!base64Decode(contentCopy, out, Base64FailOnInvalidCharacterOrExcessPadding))
                return;
            RetainPtr<NSData> dataContent = adoptNS([[NSData alloc] initWithBytes:out.data() length:out.size()]);
            [dataContent writeToURL:actualURL atomically:YES];
        } else
            [contentCopy writeToURL:actualURL atomically:YES encoding:NSUTF8StringEncoding error:NULL];

        m_inspectorPage->process().send(Messages::WebInspectorUI::DidSave([actualURL absoluteString]), m_inspectorPage->pageID());
    };

    if (!forceSaveDialog) {
        saveToURL(platformURL);
        return;
    }

    NSSavePanel *panel = [NSSavePanel savePanel];
    panel.nameFieldStringValue = platformURL.lastPathComponent;
    panel.directoryURL = [platformURL URLByDeletingLastPathComponent];

    auto completionHandler = ^(NSInteger result) {
        if (result == NSFileHandlingPanelCancelButton)
            return;
        ASSERT(result == NSFileHandlingPanelOKButton);
        saveToURL(panel.URL);
    };

    if (m_inspectorWindow)
        [panel beginSheetModalForWindow:m_inspectorWindow.get() completionHandler:completionHandler];
    else
        completionHandler([panel runModal]);
}

void WebInspectorProxy::platformAppend(const String& suggestedURL, const String& content)
{
    ASSERT(!suggestedURL.isEmpty());
    
    RetainPtr<NSURL> actualURL = m_suggestedToActualURLMap.get(suggestedURL);
    // Do not append unless the user has already confirmed this filename in save().
    if (!actualURL)
        return;

    NSFileHandle *handle = [NSFileHandle fileHandleForWritingToURL:actualURL.get() error:NULL];
    [handle seekToEndOfFile];
    [handle writeData:[content dataUsingEncoding:NSUTF8StringEncoding]];
    [handle closeFile];

    m_inspectorPage->process().send(Messages::WebInspectorUI::DidAppend([actualURL absoluteString]), m_inspectorPage->pageID());
}

void WebInspectorProxy::windowFrameDidChange()
{
    ASSERT(!m_isAttached);
    ASSERT(m_isVisible);
    ASSERT(m_inspectorWindow);

    if (m_isAttached || !m_isVisible || !m_inspectorWindow)
        return;

    NSString *frameString = NSStringFromRect([m_inspectorWindow frame]);
    inspectedPage()->pageGroup().preferences().setInspectorWindowFrame(frameString);
}

void WebInspectorProxy::windowFullScreenDidChange()
{
    ASSERT(!m_isAttached);
    ASSERT(m_isVisible);
    ASSERT(m_inspectorWindow);

    if (m_isAttached || !m_isVisible || !m_inspectorWindow)
        return;

    attachAvailabilityChanged(platformCanAttach(canAttach()));    
}

void WebInspectorProxy::inspectedViewFrameDidChange(CGFloat currentDimension)
{
    if (!m_isVisible)
        return;

    if (!m_isAttached) {
        // Check if the attach availability changed. We need to do this here in case
        // the attachment view is not the WKView.
        attachAvailabilityChanged(platformCanAttach(canAttach()));
        return;
    }

    NSView *inspectedView = inspectedPage()->inspectorAttachmentView();
    NSRect inspectedViewFrame = [inspectedView frame];
    NSRect inspectorFrame = NSZeroRect;
    NSRect parentBounds = [[inspectedView superview] bounds];
    CGFloat inspectedViewTop = NSMaxY(inspectedViewFrame);

    switch (m_attachmentSide) {
    case AttachmentSide::Bottom: {
        if (!currentDimension)
            currentDimension = NSHeight([m_inspectorView frame]);

        CGFloat parentHeight = NSHeight(parentBounds);
        CGFloat inspectorHeight = InspectorFrontendClientLocal::constrainedAttachedWindowHeight(currentDimension, parentHeight);

        // Preserve the top position of the inspected view so banners in Safari still work.
        inspectedViewFrame = NSMakeRect(0, inspectorHeight, NSWidth(parentBounds), inspectedViewTop - inspectorHeight);
        inspectorFrame = NSMakeRect(0, 0, NSWidth(inspectedViewFrame), inspectorHeight);
        break;
    }

    case AttachmentSide::Right: {
        if (!currentDimension)
            currentDimension = NSWidth([m_inspectorView frame]);

        CGFloat parentWidth = NSWidth(parentBounds);
        CGFloat inspectorWidth = InspectorFrontendClientLocal::constrainedAttachedWindowWidth(currentDimension, parentWidth);

        // Preserve the top position of the inspected view so banners in Safari still work. But don't use that
        // top position for the inspector view since the banners only stretch as wide as the the inspected view.
        inspectedViewFrame = NSMakeRect(0, 0, parentWidth - inspectorWidth, inspectedViewTop);
        CGFloat insetExcludingBanners = 0;
        if ([inspectedView isKindOfClass:[WKView class]])
            insetExcludingBanners = ((WKView *)inspectedView)._topContentInset - ((WKView *)inspectedView)._totalHeightOfBanners;
        inspectorFrame = NSMakeRect(parentWidth - inspectorWidth, 0, inspectorWidth, NSHeight(parentBounds) - insetExcludingBanners);
        break;
    }
    }

    if (NSEqualRects([m_inspectorView frame], inspectorFrame) && NSEqualRects([inspectedView frame], inspectedViewFrame))
        return;

    // Disable screen updates to make sure the layers for both views resize in sync.
    [[m_inspectorView window] disableScreenUpdatesUntilFlush];

    [m_inspectorView setFrame:inspectorFrame];
    [inspectedView setFrame:inspectedViewFrame];
}

unsigned WebInspectorProxy::platformInspectedWindowHeight()
{
    NSView *inspectedView = inspectedPage()->inspectorAttachmentView();
    NSRect inspectedViewRect = [inspectedView frame];
    return static_cast<unsigned>(inspectedViewRect.size.height);
}

unsigned WebInspectorProxy::platformInspectedWindowWidth()
{
    NSView *inspectedView = inspectedPage()->inspectorAttachmentView();
    NSRect inspectedViewRect = [inspectedView frame];
    return static_cast<unsigned>(inspectedViewRect.size.width);
}

void WebInspectorProxy::platformAttach()
{
    NSView *inspectedView = inspectedPage()->inspectorAttachmentView();

    if (m_inspectorWindow) {
        [m_inspectorWindow setDelegate:nil];
        [m_inspectorWindow close];
        m_inspectorWindow = nil;
    }

    [m_inspectorView removeFromSuperview];

    CGFloat currentDimension;

    switch (m_attachmentSide) {
    case AttachmentSide::Bottom:
        [m_inspectorView setAutoresizingMask:NSViewWidthSizable | NSViewMaxYMargin];
        currentDimension = inspectorPagePreferences().inspectorAttachedHeight();
        break;
    case AttachmentSide::Right:
        [m_inspectorView setAutoresizingMask:NSViewHeightSizable | NSViewMinXMargin];
        currentDimension = inspectorPagePreferences().inspectorAttachedWidth();
        break;
    }

    inspectedViewFrameDidChange(currentDimension);

    [[inspectedView superview] addSubview:m_inspectorView.get() positioned:NSWindowBelow relativeTo:inspectedView];
    [m_inspectorView.get().window makeFirstResponder:m_inspectorView.get()];
}

void WebInspectorProxy::platformDetach()
{
    NSView *inspectedView = inspectedPage()->inspectorAttachmentView();

    [m_inspectorView removeFromSuperview];

    // Make sure that we size the inspected view's frame after detaching so that it takes up the space that the
    // attached inspector used to. Preserve the top position of the inspected view so banners in Safari still work.

    inspectedView.frame = NSMakeRect(0, 0, NSWidth(inspectedView.superview.bounds), NSMaxY(inspectedView.frame));

    // Return early if we are not visible. This means the inspector was closed while attached
    // and we should not create and show the inspector window.
    if (!m_isVisible)
        return;

    createInspectorWindow();

    platformBringToFront();
}

void WebInspectorProxy::platformSetAttachedWindowHeight(unsigned height)
{
    if (!m_isAttached)
        return;

    inspectedViewFrameDidChange(height);
}

void WebInspectorProxy::platformSetAttachedWindowWidth(unsigned width)
{
    if (!m_isAttached)
        return;

    inspectedViewFrameDidChange(width);
}

void WebInspectorProxy::platformSetToolbarHeight(unsigned height)
{
}

void WebInspectorProxy::platformStartWindowDrag()
{
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101100
    m_inspectorView->_page->startWindowDrag();
#endif
}

String WebInspectorProxy::inspectorPageURL()
{
    // Call the soft link framework function to dlopen it, then [NSBundle bundleWithIdentifier:] will work.
    WebInspectorUILibrary();

    NSString *path = [[NSBundle bundleWithIdentifier:@"com.apple.WebInspectorUI"] pathForResource:@"Main" ofType:@"html"];
    ASSERT([path length]);

    return [[NSURL fileURLWithPath:path] absoluteString];
}

String WebInspectorProxy::inspectorTestPageURL()
{
    // Call the soft link framework function to dlopen it, then [NSBundle bundleWithIdentifier:] will work.
    WebInspectorUILibrary();

    NSString *path = [[NSBundle bundleWithIdentifier:@"com.apple.WebInspectorUI"] pathForResource:@"Test" ofType:@"html"];

    // We might not have a Test.html in Production builds.
    if (!path)
        return String();

    return [[NSURL fileURLWithPath:path] absoluteString];
}

String WebInspectorProxy::inspectorBaseURL()
{
    // Call the soft link framework function to dlopen it, then [NSBundle bundleWithIdentifier:] will work.
    WebInspectorUILibrary();

    NSString *path = [[NSBundle bundleWithIdentifier:@"com.apple.WebInspectorUI"] resourcePath];
    ASSERT([path length]);

    return [[NSURL fileURLWithPath:path] absoluteString];
}

} // namespace WebKit

#endif // PLATFORM(MAC) && WK_API_ENABLED
