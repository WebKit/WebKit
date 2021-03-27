/*
 * Copyright (C) 2010-2020 Apple Inc. All rights reserved.
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
#import "WebInspectorUIProxy.h"

#if PLATFORM(MAC)

#import "APIInspectorClient.h"
#import "APIInspectorConfiguration.h"
#import "APIUIClient.h"
#import "GlobalFindInPageState.h"
#import "WKInspectorPrivateMac.h"
#import "WKInspectorViewController.h"
#import "WKObject.h"
#import "WKViewInternal.h"
#import "WKWebViewInternal.h"
#import "WebInspectorUIMessages.h"
#import "WebPageGroup.h"
#import "WebPageProxy.h"
#import "_WKInspectorConfigurationInternal.h"
#import "_WKInspectorInternal.h"
#import "_WKInspectorWindow.h"
#import <SecurityInterface/SFCertificatePanel.h>
#import <SecurityInterface/SFCertificateView.h>
#import <WebCore/CertificateInfo.h>
#import <WebCore/InspectorFrontendClientLocal.h>
#import <WebCore/LocalizedStrings.h>
#import <wtf/text/Base64.h>

static const NSUInteger windowStyleMask = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable | NSWindowStyleMaskFullSizeContentView;

// The time we keep our WebView alive before closing it and its process.
// Reusing the WebView improves start up time for people that jump in and out of the Inspector.
static const Seconds webViewCloseTimeout { 1_min };

static void* kWindowContentLayoutObserverContext = &kWindowContentLayoutObserverContext;

@interface WKWebInspectorUIProxyObjCAdapter () <NSWindowDelegate, WKInspectorViewControllerDelegate>

- (instancetype)initWithWebInspectorUIProxy:(WebKit::WebInspectorUIProxy*)inspectorProxy;
- (void)invalidate;

@end

@implementation WKWebInspectorUIProxyObjCAdapter {
    WebKit::WebInspectorUIProxy* _inspectorProxy;
}

- (WKInspectorRef)inspectorRef
{
    return toAPI(_inspectorProxy);
}

- (_WKInspector *)inspector
{
    if (_inspectorProxy)
        return wrapper(*_inspectorProxy);
    return nil;
}

- (instancetype)initWithWebInspectorUIProxy:(WebKit::WebInspectorUIProxy*)inspectorProxy
{
    ASSERT_ARG(inspectorProxy, inspectorProxy);

    if (!(self = [super init]))
        return nil;

    // Unretained to avoid a reference cycle.
    _inspectorProxy = inspectorProxy;

    return self;
}

- (void)invalidate
{
    _inspectorProxy = nullptr;
}

- (NSRect)window:(NSWindow *)window willPositionSheet:(NSWindow *)sheet usingRect:(NSRect)rect
{
    if (_inspectorProxy)
        return NSMakeRect(0, _inspectorProxy->sheetRect().height(), _inspectorProxy->sheetRect().width(), 0);
    return rect;
}

- (void)windowDidMove:(NSNotification *)notification
{
    if (_inspectorProxy)
        _inspectorProxy->windowFrameDidChange();
}

- (void)windowDidResize:(NSNotification *)notification
{
    if (_inspectorProxy)
        _inspectorProxy->windowFrameDidChange();
}

- (void)windowWillClose:(NSNotification *)notification
{
    if (_inspectorProxy)
        _inspectorProxy->close();
}

- (void)windowDidEnterFullScreen:(NSNotification *)notification
{
    if (_inspectorProxy)
        _inspectorProxy->windowFullScreenDidChange();
}

- (void)windowDidExitFullScreen:(NSNotification *)notification
{
    if (_inspectorProxy)
        _inspectorProxy->windowFullScreenDidChange();
}

- (void)inspectedViewFrameDidChange:(NSNotification *)notification
{
    // Resizing the views while inside this notification can lead to bad results when entering
    // or exiting full screen. To avoid that we need to perform the work after a delay. We only
    // depend on this for enforcing the height constraints, so a small delay isn't terrible. Most
    // of the time the views will already have the correct frames because of autoresizing masks.

    dispatch_after(DISPATCH_TIME_NOW, dispatch_get_main_queue(), ^{
        if (_inspectorProxy)
            _inspectorProxy->inspectedViewFrameDidChange();
    });
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary<NSKeyValueChangeKey, id> *)change context:(void *)context
{
    if (context != kWindowContentLayoutObserverContext) {
        [super observeValueForKeyPath:keyPath ofObject:object change:change context:context];
        return;
    }

    NSWindow *window = object;
    ASSERT([window isKindOfClass:[NSWindow class]]);
    if (window.inLiveResize)
        return;

    dispatch_after(DISPATCH_TIME_NOW, dispatch_get_main_queue(), ^{
        if (_inspectorProxy)
            _inspectorProxy->inspectedViewFrameDidChange();
    });
}

// MARK: WKInspectorViewControllerDelegate methods

- (void)inspectorViewControllerDidBecomeActive:(WKInspectorViewController *)inspectorViewController
{
    if (_inspectorProxy)
        _inspectorProxy->didBecomeActive();
}

- (void)inspectorViewControllerInspectorDidCrash:(WKInspectorViewController *)inspectorViewController
{
    if (_inspectorProxy)
        _inspectorProxy->closeForCrash();
}

- (BOOL)inspectorViewControllerInspectorIsUnderTest:(WKInspectorViewController *)inspectorViewController
{
    return _inspectorProxy ? _inspectorProxy->isUnderTest() : false;
}

- (void)inspectorViewController:(WKInspectorViewController *)inspectorViewController willMoveToWindow:(NSWindow *)newWindow
{
    if (_inspectorProxy)
        _inspectorProxy->attachmentWillMoveFromWindow(inspectorViewController.webView.window);
}

- (void)inspectorViewControllerDidMoveToWindow:(WKInspectorViewController *)inspectorViewController
{
    if (_inspectorProxy)
        _inspectorProxy->attachmentDidMoveToWindow(inspectorViewController.webView.window);
}

- (void)inspectorViewController:(WKInspectorViewController *)inspectorViewController openURLExternally:(NSURL *)url
{
    if (_inspectorProxy)
        _inspectorProxy->openURLExternally(url.absoluteString);
}

@end

namespace WebKit {
using namespace WebCore;

void WebInspectorUIProxy::didBecomeActive()
{
    m_inspectorPage->send(Messages::WebInspectorUI::UpdateFindString(WebKit::stringForFind()));
}

void WebInspectorUIProxy::attachmentViewDidChange(NSView *oldView, NSView *newView)
{
    [[NSNotificationCenter defaultCenter] removeObserver:m_objCAdapter.get() name:NSViewFrameDidChangeNotification object:oldView];
    [[NSNotificationCenter defaultCenter] addObserver:m_objCAdapter.get() selector:@selector(inspectedViewFrameDidChange:) name:NSViewFrameDidChangeNotification object:newView];

    if (m_isAttached)
        attach(m_attachmentSide);
}

void WebInspectorUIProxy::attachmentWillMoveFromWindow(NSWindow *oldWindow)
{
    if (m_isObservingContentLayoutRect) {
        m_isObservingContentLayoutRect = false;
        [oldWindow removeObserver:m_objCAdapter.get() forKeyPath:@"contentLayoutRect" context:kWindowContentLayoutObserverContext];
    }
}

void WebInspectorUIProxy::attachmentDidMoveToWindow(NSWindow *newWindow)
{
    if (m_isAttached && !!newWindow) {
        m_isObservingContentLayoutRect = true;
        [newWindow addObserver:m_objCAdapter.get() forKeyPath:@"contentLayoutRect" options:0 context:kWindowContentLayoutObserverContext];
        inspectedViewFrameDidChange();
    }
}

void WebInspectorUIProxy::updateInspectorWindowTitle() const
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

RetainPtr<NSWindow> WebInspectorUIProxy::createFrontendWindow(NSRect savedWindowFrame, InspectionTargetType targetType)
{
    NSRect windowFrame = !NSIsEmptyRect(savedWindowFrame) ? savedWindowFrame : NSMakeRect(0, 0, initialWindowWidth, initialWindowHeight);
    auto window = adoptNS([[_WKInspectorWindow alloc] initWithContentRect:windowFrame styleMask:windowStyleMask backing:NSBackingStoreBuffered defer:NO]);
    [window setMinSize:NSMakeSize(minimumWindowWidth, minimumWindowHeight)];
    [window setReleasedWhenClosed:NO];
    [window setCollectionBehavior:([window collectionBehavior] | NSWindowCollectionBehaviorFullScreenPrimary)];

    bool forRemoteTarget = targetType == InspectionTargetType::Remote;
    [window setForRemoteTarget:forRemoteTarget];

    CGFloat approximatelyHalfScreenSize = ([window screen].frame.size.width / 2) - 4;
    CGFloat minimumFullScreenWidth = std::max<CGFloat>(636, approximatelyHalfScreenSize);
    [window setMinFullScreenContentSize:NSMakeSize(minimumFullScreenWidth, minimumWindowHeight)];
    [window setCollectionBehavior:([window collectionBehavior] | NSWindowCollectionBehaviorFullScreenAllowsTiling)];

    [window setTitlebarAppearsTransparent:YES];

    // Center the window if the saved frame was empty.
    if (NSIsEmptyRect(savedWindowFrame))
        [window center];

    return window;
}

WebPageProxy* WebInspectorUIProxy::platformCreateFrontendPage()
{
    ASSERT(inspectedPage());
    ASSERT(!m_inspectorPage);

    m_closeFrontendAfterInactivityTimer.stop();

    if (m_inspectorViewController) {
        ASSERT(m_objCAdapter);
        return [m_inspectorViewController webView]->_page.get();
    }

    m_objCAdapter = adoptNS([[WKWebInspectorUIProxyObjCAdapter alloc] initWithWebInspectorUIProxy:this]);
    NSView *inspectedView = inspectedPage()->inspectorAttachmentView();
    [[NSNotificationCenter defaultCenter] addObserver:m_objCAdapter.get() selector:@selector(inspectedViewFrameDidChange:) name:NSViewFrameDidChangeNotification object:inspectedView];

    auto configuration = inspectedPage()->uiClient().configurationForLocalInspector(*inspectedPage(),  *this);
    m_inspectorViewController = adoptNS([[WKInspectorViewController alloc] initWithConfiguration: WebKit::wrapper(configuration.get()) inspectedPage:inspectedPage()]);
    [m_inspectorViewController.get() setDelegate:m_objCAdapter.get()];

    WebPageProxy *inspectorPage = [m_inspectorViewController webView]->_page.get();
    ASSERT(inspectorPage);

    return inspectorPage;
}

void WebInspectorUIProxy::platformCreateFrontendWindow()
{
    ASSERT(!m_inspectorWindow);

    NSString *savedWindowFrameString = inspectedPage()->pageGroup().preferences().inspectorWindowFrame();
    NSRect savedWindowFrame = NSRectFromString(savedWindowFrameString);

    m_inspectorWindow = WebInspectorUIProxy::createFrontendWindow(savedWindowFrame, InspectionTargetType::Local);
    [m_inspectorWindow setDelegate:m_objCAdapter.get()];

    WKWebView *inspectorView = [m_inspectorViewController webView];
    NSView *contentView = [m_inspectorWindow contentView];
    inspectorView.frame = [contentView bounds];
    [contentView addSubview:inspectorView];

    updateInspectorWindowTitle();
    applyForcedAppearance();
}

void WebInspectorUIProxy::closeFrontendPage()
{
    ASSERT(!m_isAttached || !m_inspectorWindow);

    if (m_inspectorViewController) {
        [m_inspectorViewController.get() setDelegate:nil];
        m_inspectorViewController = nil;
    }

    if (m_objCAdapter) {
        [[NSNotificationCenter defaultCenter] removeObserver:m_objCAdapter.get()];

        [m_objCAdapter invalidate];
        m_objCAdapter = nil;
    }
}

void WebInspectorUIProxy::closeFrontendAfterInactivityTimerFired()
{
    closeFrontendPage();
}

void WebInspectorUIProxy::platformCloseFrontendPageAndWindow()
{
    if (m_inspectorWindow) {
        [m_inspectorWindow setDelegate:nil];
        [m_inspectorWindow close];
        m_inspectorWindow = nil;
    }

    m_closeFrontendAfterInactivityTimer.startOneShot(webViewCloseTimeout);
}

void WebInspectorUIProxy::platformDidCloseForCrash()
{
    m_closeFrontendAfterInactivityTimer.stop();

    closeFrontendPage();
}

void WebInspectorUIProxy::platformInvalidate()
{
    m_closeFrontendAfterInactivityTimer.stop();

    closeFrontendPage();
}

void WebInspectorUIProxy::platformHide()
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

void WebInspectorUIProxy::platformResetState()
{
    inspectedPage()->pageGroup().preferences().deleteInspectorWindowFrame();
}

void WebInspectorUIProxy::platformBringToFront()
{
    // If the Web Inspector is no longer in the same window as the inspected view,
    // then we need to reopen the Inspector to get it attached to the right window.
    // This can happen when dragging tabs to another window in Safari.
    if (m_isAttached && [m_inspectorViewController webView].window != inspectedPage()->platformWindow()) {
        open();
        return;
    }

    // FIXME <rdar://problem/10937688>: this will not bring a background tab in Safari to the front, only its window.
    [[m_inspectorViewController webView].window makeKeyAndOrderFront:nil];
    [[m_inspectorViewController webView].window makeFirstResponder:[m_inspectorViewController webView]];
}

void WebInspectorUIProxy::platformBringInspectedPageToFront()
{
    [inspectedPage()->platformWindow() makeKeyAndOrderFront:nil];
}

bool WebInspectorUIProxy::platformIsFront()
{
    // FIXME <rdar://problem/10937688>: this will not return false for a background tab in Safari, only a background window.
    return m_isVisible && [m_inspectorViewController webView].window.isMainWindow;
}

bool WebInspectorUIProxy::platformCanAttach(bool webProcessCanAttach)
{
    NSView *inspectedView = inspectedPage()->inspectorAttachmentView();
    if ([WKInspectorViewController viewIsInspectorWebView:inspectedView])
        return webProcessCanAttach;

    if (inspectedView.hidden)
        return false;

    static const float minimumAttachedHeight = 250;
    static const float maximumAttachedHeightRatio = 0.75;
    static const float minimumAttachedWidth = 500;

    NSRect inspectedViewFrame = inspectedView.frame;

    float maximumAttachedHeight = NSHeight(inspectedViewFrame) * maximumAttachedHeightRatio;
    return minimumAttachedHeight <= maximumAttachedHeight && minimumAttachedWidth <= NSWidth(inspectedViewFrame);
}

void WebInspectorUIProxy::platformAttachAvailabilityChanged(bool available)
{
    // Do nothing.
}

void WebInspectorUIProxy::platformSetForcedAppearance(InspectorFrontendClient::Appearance appearance)
{
    m_frontendAppearance = appearance;

    applyForcedAppearance();
}

void WebInspectorUIProxy::platformInspectedURLChanged(const String& urlString)
{
    m_urlString = urlString;

    updateInspectorWindowTitle();
}

void WebInspectorUIProxy::platformShowCertificate(const CertificateInfo& certificateInfo)
{
    ASSERT(!certificateInfo.isEmpty());

    RetainPtr<SFCertificatePanel> certificatePanel = adoptNS([[SFCertificatePanel alloc] init]);

    NSWindow *window;
    if (m_inspectorWindow)
        window = m_inspectorWindow.get();
    else
        window = [[m_inspectorViewController webView] window];

    if (!window)
        window = [NSApp keyWindow];

#if HAVE(SEC_TRUST_SERIALIZATION)
    [certificatePanel beginSheetForWindow:window modalDelegate:nil didEndSelector:NULL contextInfo:nullptr trust:certificateInfo.trust() showGroup:YES];
#else
    [certificatePanel beginSheetForWindow:window modalDelegate:nil didEndSelector:NULL contextInfo:nullptr certificates:(NSArray *)certificateInfo.certificateChain() showGroup:YES];
#endif

    // This must be called after the trust panel has been displayed, because the certificateView doesn't exist beforehand.
    SFCertificateView *certificateView = [certificatePanel certificateView];
    [certificateView setDisplayTrust:YES];
    [certificateView setEditableTrust:NO];
    [certificateView setDisplayDetails:YES];
    [certificateView setDetailsDisclosed:YES];
}

void WebInspectorUIProxy::platformSave(const String& suggestedURL, const String& content, bool base64Encoded, bool forceSaveDialog)
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
            if (!base64Decode(contentCopy, out, Base64ValidatePadding))
                return;
            RetainPtr<NSData> dataContent = adoptNS([[NSData alloc] initWithBytes:out.data() length:out.size()]);
            [dataContent writeToURL:actualURL atomically:YES];
        } else
            [contentCopy writeToURL:actualURL atomically:YES encoding:NSUTF8StringEncoding error:NULL];

        m_inspectorPage->send(Messages::WebInspectorUI::DidSave([actualURL absoluteString]));
    };

    if (!forceSaveDialog) {
        saveToURL(platformURL);
        return;
    }

    NSSavePanel *panel = [NSSavePanel savePanel];
    panel.nameFieldStringValue = platformURL.lastPathComponent;

    // If we have a file URL we've already saved this file to a path and
    // can provide a good directory to show. Otherwise, use the system's
    // default behavior for the initial directory to show in the dialog.
    if (platformURL.isFileURL)
        panel.directoryURL = [platformURL URLByDeletingLastPathComponent];

    auto completionHandler = ^(NSInteger result) {
        if (result == NSModalResponseCancel)
            return;
        ASSERT(result == NSModalResponseOK);
        saveToURL(panel.URL);
    };

    NSWindow *window = m_inspectorWindow ? m_inspectorWindow.get() : [NSApp keyWindow];
    if (window)
        [panel beginSheetModalForWindow:window completionHandler:completionHandler];
    else
        completionHandler([panel runModal]);
}

void WebInspectorUIProxy::platformAppend(const String& suggestedURL, const String& content)
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

    m_inspectorPage->send(Messages::WebInspectorUI::DidAppend([actualURL absoluteString]));
}

void WebInspectorUIProxy::windowFrameDidChange()
{
    ASSERT(!m_isAttached);
    ASSERT(m_isVisible);
    ASSERT(m_inspectorWindow);

    if (m_isAttached || !m_isVisible || !m_inspectorWindow)
        return;

    NSString *frameString = NSStringFromRect([m_inspectorWindow frame]);
    inspectedPage()->pageGroup().preferences().setInspectorWindowFrame(frameString);
}

void WebInspectorUIProxy::windowFullScreenDidChange()
{
    ASSERT(!m_isAttached);
    ASSERT(m_isVisible);
    ASSERT(m_inspectorWindow);

    if (m_isAttached || !m_isVisible || !m_inspectorWindow)
        return;

    attachAvailabilityChanged(platformCanAttach(canAttach()));    
}

void WebInspectorUIProxy::inspectedViewFrameDidChange(CGFloat currentDimension)
{
    if (!m_isVisible)
        return;

    if (!m_isAttached) {
        // Check if the attach availability changed. We need to do this here in case
        // the attachment view is not the WKView.
        attachAvailabilityChanged(platformCanAttach(canAttach()));
        return;
    }

    if (!inspectedPage())
        return;

    NSView *inspectedView = inspectedPage()->inspectorAttachmentView();
    WKWebView *inspectorView = [m_inspectorViewController webView];

    NSRect inspectedViewFrame = inspectedView.frame;
    NSRect oldInspectorViewFrame = inspectorView.frame;
    NSRect newInspectorViewFrame = NSZeroRect;
    NSRect parentBounds = inspectedView.superview.bounds;
    CGFloat inspectedViewTop = NSMaxY(inspectedViewFrame);

    switch (m_attachmentSide) {
    case AttachmentSide::Bottom: {
        if (!currentDimension)
            currentDimension = NSHeight(oldInspectorViewFrame);

        CGFloat parentHeight = NSHeight(parentBounds);
        CGFloat inspectorHeight = InspectorFrontendClientLocal::constrainedAttachedWindowHeight(currentDimension, parentHeight);

        // Preserve the top position of the inspected view so banners in Safari still work.
        inspectedViewFrame = NSMakeRect(0, inspectorHeight, NSWidth(parentBounds), inspectedViewTop - inspectorHeight);
        newInspectorViewFrame = NSMakeRect(0, 0, NSWidth(inspectedViewFrame), inspectorHeight);
        break;
    }

    case AttachmentSide::Right: {
        if (!currentDimension)
            currentDimension = NSWidth(oldInspectorViewFrame);

        CGFloat parentWidth = NSWidth(parentBounds);
        CGFloat inspectorWidth = InspectorFrontendClientLocal::constrainedAttachedWindowWidth(currentDimension, parentWidth);

        // Preserve the top position of the inspected view so banners in Safari still work. But don't use that
        // top position for the inspector view since the banners only stretch as wide as the inspected view.
        inspectedViewFrame = NSMakeRect(0, 0, parentWidth - inspectorWidth, inspectedViewTop);
        newInspectorViewFrame = NSMakeRect(parentWidth - inspectorWidth, 0, inspectorWidth, NSHeight(parentBounds));

        if (NSWindow *inspectorWindow = inspectorView.window) {
            NSRect contentLayoutRect = [inspectedView.superview convertRect:inspectorWindow.contentLayoutRect fromView:nil];
            newInspectorViewFrame = NSIntersectionRect(newInspectorViewFrame, contentLayoutRect);
        }
        break;
    }

    case AttachmentSide::Left: {
        if (!currentDimension)
            currentDimension = NSWidth(oldInspectorViewFrame);

        CGFloat parentWidth = NSWidth(parentBounds);
        CGFloat inspectorWidth = InspectorFrontendClientLocal::constrainedAttachedWindowWidth(currentDimension, parentWidth);

        // Preserve the top position of the inspected view so banners in Safari still work. But don't use that
        // top position for the inspector view since the banners only stretch as wide as the inspected view.
        inspectedViewFrame = NSMakeRect(inspectorWidth, 0, parentWidth - inspectorWidth, inspectedViewTop);
        newInspectorViewFrame = NSMakeRect(0, 0, inspectorWidth, NSHeight(parentBounds));

        if (NSWindow *inspectorWindow = inspectorView.window) {
            NSRect contentLayoutRect = [inspectedView.superview convertRect:inspectorWindow.contentLayoutRect fromView:nil];
            newInspectorViewFrame = NSIntersectionRect(newInspectorViewFrame, contentLayoutRect);
        }
        break;
    }
    }

    if (NSEqualRects(oldInspectorViewFrame, newInspectorViewFrame) && NSEqualRects([inspectedView frame], inspectedViewFrame))
        return;

    // Disable screen updates to make sure the layers for both views resize in sync.
    [inspectorView.window disableScreenUpdatesUntilFlush];

    [inspectorView setFrame:newInspectorViewFrame];
    [inspectedView setFrame:inspectedViewFrame];
}

unsigned WebInspectorUIProxy::platformInspectedWindowHeight()
{
    NSView *inspectedView = inspectedPage()->inspectorAttachmentView();
    NSRect inspectedViewRect = [inspectedView frame];
    return static_cast<unsigned>(inspectedViewRect.size.height);
}

unsigned WebInspectorUIProxy::platformInspectedWindowWidth()
{
    NSView *inspectedView = inspectedPage()->inspectorAttachmentView();
    NSRect inspectedViewRect = [inspectedView frame];
    return static_cast<unsigned>(inspectedViewRect.size.width);
}

void WebInspectorUIProxy::platformAttach()
{
    NSView *inspectedView = inspectedPage()->inspectorAttachmentView();
    WKWebView *inspectorView = [m_inspectorViewController webView];

    if (m_inspectorWindow) {
        [m_inspectorWindow setDelegate:nil];
        [m_inspectorWindow close];
        m_inspectorWindow = nil;
    }

    [inspectorView removeFromSuperview];

    CGFloat currentDimension;
    switch (m_attachmentSide) {
    case AttachmentSide::Bottom:
        [inspectorView setAutoresizingMask:NSViewWidthSizable | NSViewMaxYMargin];
        currentDimension = inspectorPagePreferences().inspectorAttachedHeight();
        break;
    case AttachmentSide::Right:
        [inspectorView setAutoresizingMask:NSViewHeightSizable | NSViewMinXMargin];
        currentDimension = inspectorPagePreferences().inspectorAttachedWidth();
        break;
    case AttachmentSide::Left:
        [inspectorView setAutoresizingMask:NSViewHeightSizable | NSViewMaxXMargin];
        currentDimension = inspectorPagePreferences().inspectorAttachedWidth();
        break;
    }

    inspectedViewFrameDidChange(currentDimension);

    [inspectedView.superview addSubview:inspectorView positioned:NSWindowBelow relativeTo:inspectedView];
    [inspectorView.window makeFirstResponder:inspectorView];
}

void WebInspectorUIProxy::platformDetach()
{
    NSView *inspectedView = inspectedPage()->inspectorAttachmentView();
    WKWebView *inspectorView = [m_inspectorViewController webView];

    [inspectorView removeFromSuperview];

    [inspectorView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];

    // Make sure that we size the inspected view's frame after detaching so that it takes up the space that the
    // attached inspector used to. Preserve the top position of the inspected view so banners in Safari still work.

    inspectedView.frame = NSMakeRect(0, 0, NSWidth(inspectedView.superview.bounds), NSMaxY(inspectedView.frame));

    // Return early if we are not visible. This means the inspector was closed while attached
    // and we should not create and show the inspector window.
    if (!m_isVisible)
        return;

    open();
}

void WebInspectorUIProxy::platformSetAttachedWindowHeight(unsigned height)
{
    if (!m_isAttached)
        return;

    inspectedViewFrameDidChange(height);
}

void WebInspectorUIProxy::platformSetAttachedWindowWidth(unsigned width)
{
    if (!m_isAttached)
        return;

    inspectedViewFrameDidChange(width);
}

void WebInspectorUIProxy::platformSetSheetRect(const FloatRect& rect)
{
    m_sheetRect = rect;
}

void WebInspectorUIProxy::platformStartWindowDrag()
{
    if (auto* webView = [m_inspectorViewController webView]) {
        if (webView->_page)
            webView->_page->startWindowDrag();
    }
}

String WebInspectorUIProxy::inspectorPageURL()
{
    return [WKInspectorViewController URLForInspectorResource:@"Main.html"].absoluteString;
}

String WebInspectorUIProxy::inspectorTestPageURL()
{
    return [WKInspectorViewController URLForInspectorResource:@"Test.html"].absoluteString;
}

static NSDictionary *systemVersionPlist()
{
    NSString *systemLibraryPath = [NSSearchPathForDirectoriesInDomains(NSLibraryDirectory, NSSystemDomainMask, YES) objectAtIndex:0];
    NSString *systemVersionPlistPath = [systemLibraryPath stringByAppendingPathComponent:@"CoreServices/SystemVersion.plist"];
    NSDictionary *systemVersionInfo = [NSDictionary dictionaryWithContentsOfFile:systemVersionPlistPath];
    return systemVersionInfo;
}

DebuggableInfoData WebInspectorUIProxy::infoForLocalDebuggable()
{
    NSDictionary *plist = systemVersionPlist();

    DebuggableInfoData result;
    result.debuggableType = Inspector::DebuggableType::WebPage;
    result.targetPlatformName = "macOS"_s;
    result.targetBuildVersion = plist[@"ProductBuildVersion"];
    result.targetProductVersion = plist[@"ProductUserVisibleVersion"];
    result.targetIsSimulator = false;

    return result;
}

void WebInspectorUIProxy::applyForcedAppearance()
{
    NSAppearance *platformAppearance;
    switch (m_frontendAppearance) {
    case InspectorFrontendClient::Appearance::System:
        platformAppearance = nil;
        break;

    case InspectorFrontendClient::Appearance::Light:
        platformAppearance = [NSAppearance appearanceNamed:NSAppearanceNameAqua];
        break;

    case InspectorFrontendClient::Appearance::Dark:
        platformAppearance = [NSAppearance appearanceNamed:NSAppearanceNameDarkAqua];
        break;
    }

    if (NSWindow *window = m_inspectorWindow.get())
        window.appearance = platformAppearance;

    [m_inspectorViewController webView].appearance = platformAppearance;
}

} // namespace WebKit

#endif // PLATFORM(MAC)
