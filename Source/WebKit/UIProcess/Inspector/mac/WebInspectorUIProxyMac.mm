/*
 * Copyright (C) 2010-2025 Apple Inc. All rights reserved.
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
#import "Logging.h"
#import "MessageSenderInlines.h"
#import "WKAPICast.h"
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
#import "_WKInspectorWindowInternal.h"
#import <SecurityInterface/SFCertificatePanel.h>
#import <SecurityInterface/SFCertificateView.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import <WebCore/CertificateInfo.h>
#import <WebCore/Color.h>
#import <WebCore/InspectorFrontendClientLocal.h>
#import <WebCore/LocalizedStrings.h>
#import <pal/spi/cf/CFUtilitiesSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/CompletionHandler.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/darwin/DispatchExtras.h>
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
    WeakPtr<WebKit::WebInspectorUIProxy> _inspectorProxy;
}

- (WKInspectorRef)inspectorRef
{
    return toAPI(self._protectedInspector.get());
}

- (_WKInspector *)inspector
{
    if (RefPtr proxy = _inspectorProxy.get())
        return wrapper(*proxy);
    return nil;
}

- (RefPtr<WebKit::WebInspectorUIProxy>)_protectedInspector
{
    return _inspectorProxy.get();
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
    if (RefPtr proxy = _inspectorProxy.get())
        return NSMakeRect(0, proxy->sheetRect().height(), proxy->sheetRect().width(), 0);
    return rect;
}

- (void)windowDidMove:(NSNotification *)notification
{
    if (RefPtr proxy = _inspectorProxy.get())
        proxy->windowFrameDidChange();
}

- (void)windowDidResize:(NSNotification *)notification
{
    if (RefPtr proxy = _inspectorProxy.get())
        proxy->windowFrameDidChange();
}

- (void)windowWillClose:(NSNotification *)notification
{
    if (RefPtr proxy = _inspectorProxy.get())
        proxy->close();
}

- (void)windowDidEnterFullScreen:(NSNotification *)notification
{
    if (RefPtr proxy = _inspectorProxy.get())
        proxy->windowFullScreenDidChange();
}

- (void)windowDidExitFullScreen:(NSNotification *)notification
{
    if (RefPtr proxy = _inspectorProxy.get())
        proxy->windowFullScreenDidChange();
}

- (void)inspectedViewFrameDidChange:(NSNotification *)notification
{
    // Resizing the views while inside this notification can lead to bad results when entering
    // or exiting full screen. To avoid that we need to perform the work after a delay. We only
    // depend on this for enforcing the height constraints, so a small delay isn't terrible. Most
    // of the time the views will already have the correct frames because of autoresizing masks.

    dispatch_after(DISPATCH_TIME_NOW, mainDispatchQueueSingleton(), ^{
        if (RefPtr proxy = _inspectorProxy.get())
            proxy->inspectedViewFrameDidChange();
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

    dispatch_after(DISPATCH_TIME_NOW, mainDispatchQueueSingleton(), ^{
        if (RefPtr proxy = _inspectorProxy.get())
            proxy->inspectedViewFrameDidChange();
    });
}

// MARK: WKInspectorViewControllerDelegate methods

- (void)inspectorViewControllerDidBecomeActive:(WKInspectorViewController *)inspectorViewController
{
    if (RefPtr proxy = _inspectorProxy.get())
        proxy->didBecomeActive();
}

- (void)inspectorViewControllerInspectorDidCrash:(WKInspectorViewController *)inspectorViewController
{
    if (RefPtr proxy = _inspectorProxy.get())
        proxy->closeForCrash();
}

- (BOOL)inspectorViewControllerInspectorIsUnderTest:(WKInspectorViewController *)inspectorViewController
{
    return _inspectorProxy ? _inspectorProxy->isUnderTest() : false;
}

- (BOOL)inspectorViewControllerInspectorIsHorizontallyAttached:(WKInspectorViewController *)inspectorViewController
{
    RefPtr inspector = _inspectorProxy.get();
    if (!inspector->isAttached())
        return NO;

    switch (inspector->attachmentSide()) {
    case WebKit::AttachmentSide::Bottom:
        return NO;
    case WebKit::AttachmentSide::Right:
    case WebKit::AttachmentSide::Left:
        return YES;
    }
    ASSERT_NOT_REACHED();
    return NO;
}

- (void)inspectorViewController:(WKInspectorViewController *)inspectorViewController willMoveToWindow:(NSWindow *)newWindow
{
    if (RefPtr proxy = _inspectorProxy.get())
        proxy->attachmentWillMoveFromWindow(retainPtr(inspectorViewController.webView.window).get());
}

- (void)inspectorViewControllerDidMoveToWindow:(WKInspectorViewController *)inspectorViewController
{
    if (RefPtr proxy = _inspectorProxy.get())
        proxy->attachmentDidMoveToWindow(retainPtr(inspectorViewController.webView.window).get());
}

- (void)inspectorViewController:(WKInspectorViewController *)inspectorViewController openURLExternally:(NSURL *)url
{
    if (RefPtr proxy = _inspectorProxy.get())
        proxy->openURLExternally(url.absoluteString);
}

@end

@interface WKWebInspectorUISaveController : NSViewController

- (id)initWithSaveDatas:(Vector<WebCore::InspectorFrontendClient::SaveData>&&)saveDatas savePanel:(NSSavePanel *)savePanel;

@property (nonatomic, readonly) NSString *content;
@property (nonatomic, readonly) BOOL base64Encoded;

@end

@implementation WKWebInspectorUISaveController {
    Vector<WebCore::InspectorFrontendClient::SaveData> _saveDatas;

    RetainPtr<NSSavePanel> _savePanel;
    RetainPtr<NSPopUpButton> _popUpButton;
}

- (id)initWithSaveDatas:(Vector<WebCore::InspectorFrontendClient::SaveData>&&)saveDatas savePanel:(NSSavePanel *)savePanel
{
    if (!(self = [super init]))
        return nil;

    _saveDatas = WTF::move(saveDatas);

    _savePanel = savePanel;

    self.view = adoptNS([[NSView alloc] init]).get();

    RetainPtr label = [NSTextField labelWithString:WEB_UI_STRING("Format:", "Label for the save data format selector when saving data in Web Inspector").createNSString().get()];
    label.get().textColor = NSColor.secondaryLabelColor;
    label.get().font = [NSFont systemFontOfSize:NSFont.smallSystemFontSize];
    label.get().alignment = NSTextAlignmentRight;

    _popUpButton = adoptNS([[NSPopUpButton alloc] init]);
    [_popUpButton setAction:@selector(_popUpButtonAction:)];
    [_popUpButton setTarget:self];
    [_popUpButton addItemsWithTitles:createNSArray(_saveDatas, [] (const auto& item) {
        return item.displayType.createNSString();
    }).get()];
    [_popUpButton selectItemAtIndex:0];

    RetainPtr<NSView> view = self.view;
    [view addSubview:label.get()];
    [view addSubview:_popUpButton.get()];

    [label setTranslatesAutoresizingMaskIntoConstraints:NO];
    [_popUpButton setTranslatesAutoresizingMaskIntoConstraints:NO];

    [NSLayoutConstraint activateConstraints:@[
        [retainPtr(label.get().topAnchor) constraintEqualToAnchor:retainPtr(view.get().topAnchor).get() constant:8.0],
        [retainPtr(label.get().leadingAnchor) constraintEqualToAnchor:retainPtr(view.get().leadingAnchor).get() constant:0.0],
        [retainPtr(label.get().bottomAnchor) constraintEqualToAnchor:retainPtr(self.view.bottomAnchor).get() constant:-8.0],
        [retainPtr(label.get().widthAnchor) constraintEqualToConstant:64.0],

        [retainPtr([_popUpButton topAnchor]) constraintEqualToAnchor:retainPtr(view.get().topAnchor).get() constant:8.0],
        [retainPtr([_popUpButton leadingAnchor]) constraintEqualToAnchor:retainPtr(label.get().trailingAnchor).get() constant:8.0],
        [retainPtr([_popUpButton bottomAnchor]) constraintEqualToAnchor:retainPtr(view.get().bottomAnchor).get() constant:-8.0],
        [retainPtr([_popUpButton trailingAnchor]) constraintEqualToAnchor:retainPtr(view.get().trailingAnchor).get() constant:-20.0],
    ]];

    if (_saveDatas.size() > 1)
        [_savePanel setAccessoryView:self.view];

    [self _updateSavePanel];

    return self;
}

- (NSString *)content
{
    return _saveDatas[[_popUpButton indexOfSelectedItem]].content.createNSString().autorelease();
}

- (BOOL)base64Encoded
{
    return _saveDatas[[_popUpButton indexOfSelectedItem]].base64Encoded;
}

- (void)_updateSavePanel
{
    RetainPtr suggestedURL = _saveDatas[[_popUpButton indexOfSelectedItem]].url.createNSString();

    if (RetainPtr<UTType> type = [UTType typeWithFilenameExtension:retainPtr(suggestedURL.get().pathExtension).get()])
        [_savePanel setAllowedContentTypes:@[ type.get() ]];
    else
        [_savePanel setAllowedContentTypes:@[ ]];
}

- (IBAction)_popUpButtonAction:(id)sender
{
    [self _updateSavePanel];
}

@end

namespace WebKit {
using namespace WebCore;

void WebInspectorUIProxy::didBecomeActive()
{
    protect(protect(inspectorPage())->legacyMainFrameProcess())->send(Messages::WebInspectorUI::UpdateFindString(WebKit::stringForFind()), m_inspectorPage->webPageIDInMainFrameProcess());
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
        SUPPRESS_UNRETAINED_ARG RetainPtr debugTitle = adoptNS([[NSString alloc] initWithFormat:WEB_UI_NSSTRING(@"Web Inspector [%d] — %@", "Web Inspector window title when inspecting Web Inspector"), level, m_urlString.createNSString().get()]);
        [m_inspectorWindow setTitle:debugTitle.get()];
    } else {
        SUPPRESS_UNRETAINED_ARG RetainPtr title = adoptNS([[NSString alloc] initWithFormat:WEB_UI_NSSTRING(@"Web Inspector — %@", "Web Inspector window title"), m_urlString.createNSString().get()]);
        [m_inspectorWindow setTitle:title.get()];
    }
}

RetainPtr<NSWindow> WebInspectorUIProxy::createFrontendWindow(NSRect savedWindowFrame, InspectionTargetType targetType, WebPageProxy* inspectedPage)
{
    NSRect windowFrame = !NSIsEmptyRect(savedWindowFrame) ? savedWindowFrame : NSMakeRect(0, 0, initialWindowWidth, initialWindowHeight);
    auto window = adoptNS([[_WKInspectorWindow alloc] initWithContentRect:windowFrame styleMask:windowStyleMask backing:NSBackingStoreBuffered defer:NO]);
    [window setMinSize:NSMakeSize(minimumWindowWidth, minimumWindowHeight)];
    [window setReleasedWhenClosed:NO];
    [window setCollectionBehavior:([window collectionBehavior] | NSWindowCollectionBehaviorFullScreenPrimary)];

    bool forRemoteTarget = targetType == InspectionTargetType::Remote;
    [window setForRemoteTarget:forRemoteTarget];

    if (inspectedPage)
        [window setInspectedWebView:inspectedPage->cocoaView().get()];

    CGFloat approximatelyHalfScreenSize = ([window screen].frame.size.width / 2) - 4;
    CGFloat minimumFullScreenWidth = std::max<CGFloat>(636, approximatelyHalfScreenSize);
    [window setMinFullScreenContentSize:NSMakeSize(minimumFullScreenWidth, minimumWindowHeight)];
    [window setCollectionBehavior:([window collectionBehavior] | NSWindowCollectionBehaviorFullScreenAllowsTiling | NSWindowCollectionBehaviorAuxiliary)];

    [window setTitlebarAppearsTransparent:YES];

    // Center the window if the saved frame was empty.
    if (NSIsEmptyRect(savedWindowFrame))
        [window center];

    return window;
}

void WebInspectorUIProxy::showSavePanel(NSWindow *frontendWindow, NSURL *platformURL, Vector<InspectorFrontendClient::SaveData>&& saveDatas, bool forceSaveAs, CompletionHandler<void(NSURL *)>&& completionHandler)
{
    ASSERT(platformURL);

    RetainPtr savePanel = [NSSavePanel savePanel];
    [savePanel setExtensionHidden:NO];

    auto controller = adoptNS([[WKWebInspectorUISaveController alloc] initWithSaveDatas:WTF::move(saveDatas) savePanel:savePanel.get()]);

    auto saveToURL = [controller, completionHandler = WTF::move(completionHandler)] (NSURL *actualURL) mutable {
        ASSERT(actualURL);

        if ([controller base64Encoded]) {
            String contentString = [controller content];
            auto decodedData = base64Decode(contentString, { Base64DecodeOption::ValidatePadding });
            if (!decodedData)
                return;
            RetainPtr dataContent = toNSData(decodedData->span());
            [dataContent writeToURL:actualURL atomically:YES];
        } else
            [retainPtr([controller content]) writeToURL:actualURL atomically:YES encoding:NSUTF8StringEncoding error:NULL];

        completionHandler(actualURL);
    };

    if (!forceSaveAs) {
        saveToURL(platformURL);
        return;
    }

    [savePanel setNameFieldStringValue:platformURL.lastPathComponent];

    // If we have a file URL we've already saved this file to a path and
    // can provide a good directory to show. Otherwise, use the system's
    // default behavior for the initial directory to show in the dialog.
    if (platformURL.isFileURL)
        [savePanel setDirectoryURL:[platformURL URLByDeletingLastPathComponent]];

    auto didShowModal = [savePanel, saveToURL = WTF::move(saveToURL)] (NSInteger result) mutable {
        if (result == NSModalResponseCancel)
            return;

        ASSERT(result == NSModalResponseOK);
        saveToURL(retainPtr([savePanel URL]).get());
    };

    if (RetainPtr window = frontendWindow ?: [NSApp keyWindow])
        [savePanel beginSheetModalForWindow:window.get() completionHandler:makeBlockPtr(WTF::move(didShowModal)).get()];
    else
        didShowModal([savePanel runModal]);
}

RefPtr<WebPageProxy> WebInspectorUIProxy::platformCreateFrontendPage()
{
    RefPtr inspectedPage = m_inspectedPage.get();
    ASSERT(m_inspectedPage);
    ASSERT(!m_inspectorPage);

    m_closeFrontendAfterInactivityTimer.stop();

    if (m_inspectorViewController) {
        ASSERT(m_objCAdapter);
        return [m_inspectorViewController webView]->_page.get();
    }

    m_objCAdapter = adoptNS([[WKWebInspectorUIProxyObjCAdapter alloc] initWithWebInspectorUIProxy:this]);
    RetainPtr inspectedView = inspectedPage->inspectorAttachmentView();
    [[NSNotificationCenter defaultCenter] addObserver:m_objCAdapter.get() selector:@selector(inspectedViewFrameDidChange:) name:NSViewFrameDidChangeNotification object:inspectedView.get()];

    Ref configuration = inspectedPage->uiClient().configurationForLocalInspector(*inspectedPage, *this);
    m_inspectorViewController = adoptNS([[WKInspectorViewController alloc] initWithConfiguration:protectedWrapper(configuration.get()).get() inspectedPage:inspectedPage.get()]);
    [m_inspectorViewController setDelegate:m_objCAdapter.get()];

    RefPtr inspectorPage = [m_inspectorViewController webView]->_page.get();
    ASSERT(inspectorPage);
    return inspectorPage;
}

void WebInspectorUIProxy::platformCreateFrontendWindow()
{
    ASSERT(!m_inspectorWindow);

    NSRect savedWindowFrame = NSZeroRect;
    if (RefPtr inspectedPage = this->inspectedPage()) {
        RetainPtr savedWindowFrameString = inspectedPage->pageGroup().preferences().inspectorWindowFrame().createNSString();
        savedWindowFrame = NSRectFromString(savedWindowFrameString.get());
    }

    m_inspectorWindow = WebInspectorUIProxy::createFrontendWindow(savedWindowFrame, InspectionTargetType::Local, protect(inspectedPage()).get());
    [m_inspectorWindow setDelegate:m_objCAdapter.get()];

    RetainPtr<WKWebView> inspectorView = [m_inspectorViewController webView];
    RetainPtr<NSView> contentView = [m_inspectorWindow contentView];
    inspectorView.get().frame = [contentView bounds];
    [contentView addSubview:inspectorView.get()];

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
    if (RefPtr inspectedPage = m_inspectedPage.get())
        inspectedPage->pageGroup().preferences().deleteInspectorWindowFrame();
}

void WebInspectorUIProxy::platformBringToFront()
{
    // If the Web Inspector is no longer in the same window as the inspected view,
    // then we need to reopen the Inspector to get it attached to the right window.
    // This can happen when dragging tabs to another window in Safari.
    RefPtr inspectedPage = m_inspectedPage.get();
    if (m_isAttached && inspectedPage && [m_inspectorViewController webView].window != inspectedPage->platformWindow()) {
        if (m_isOpening) {
            // <rdar://88358696> If we are currently opening an attached inspector, the windows should have already
            // matched, and calling back to `open` isn't going to correct this. As a fail-safe to prevent reentrancy,
            // fall back to detaching the inspector when there is a mismatch in the web view's window and the
            // inspector's window.
            RELEASE_LOG(Inspector, "WebInspectorUIProxy::platformBringToFront - Inspected and inspector windows did not match while opening inspector. Falling back to detached inspector. Inspected page had window: %s", inspectedPage->platformWindow() ? "YES" : "NO");
            detach();
            return;
        }

        open();
        return;
    }

    // FIXME <rdar://problem/10937688>: this will not bring a background tab in Safari to the front, only its window.
    [retainPtr([m_inspectorViewController webView].window) makeKeyAndOrderFront:nil];
    [retainPtr([m_inspectorViewController webView].window) makeFirstResponder:retainPtr([m_inspectorViewController webView]).get()];
}

void WebInspectorUIProxy::platformBringInspectedPageToFront()
{
    if (RefPtr inspectedPage = m_inspectedPage.get())
        [protect(inspectedPage->platformWindow()) makeKeyAndOrderFront:nil];
}

bool WebInspectorUIProxy::platformIsFront()
{
    // FIXME <rdar://problem/10937688>: this will not return false for a background tab in Safari, only a background window.
    return m_isVisible && [m_inspectorViewController webView].window.isMainWindow;
}

bool WebInspectorUIProxy::platformCanAttach(bool webProcessCanAttach)
{
    RefPtr inspectedPage = m_inspectedPage.get();
    if (!inspectedPage)
        return false;

    RetainPtr inspectedView = inspectedPage->inspectorAttachmentView();
    if ([WKInspectorViewController viewIsInspectorWebView:inspectedView.get()])
        return webProcessCanAttach;

    if (inspectedView.get().hidden)
        return false;

    static const float minimumAttachedHeight = 250;
    static const float maximumAttachedHeightRatio = 0.75;
    static const float minimumAttachedWidth = 500;

    NSRect inspectedViewFrame = inspectedView.get().frame;

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

    RetainPtr<NSWindow> window;
    if (m_inspectorWindow)
        window = m_inspectorWindow.get();
    else
        window = [[m_inspectorViewController webView] window];

    if (!window)
        window = [NSApp keyWindow];

    [certificatePanel beginSheetForWindow:window.get() modalDelegate:nil didEndSelector:NULL contextInfo:nullptr trust:certificateInfo.trust().get() showGroup:YES];

    // This must be called after the trust panel has been displayed, because the certificateView doesn't exist beforehand.
    RetainPtr<SFCertificateView> certificateView = [certificatePanel certificateView];
    [certificateView setDisplayTrust:YES];
    [certificateView setEditableTrust:NO];
    [certificateView setDisplayDetails:YES];
    [certificateView setDetailsDisclosed:YES];
}

void WebInspectorUIProxy::platformRevealFileExternally(const String& path)
{
    [[NSWorkspace sharedWorkspace] activateFileViewerSelectingURLs:@[ adoptNS([[NSURL alloc] initWithString:path.createNSString().get()]).get() ]];
}

void WebInspectorUIProxy::platformSave(Vector<InspectorFrontendClient::SaveData>&& saveDatas, bool forceSaveAs)
{
    RetainPtr<NSString> urlCommonPrefix;
    for (auto& item : saveDatas) {
        if (!urlCommonPrefix)
            urlCommonPrefix = item.url.createNSString();
        else
            urlCommonPrefix = [urlCommonPrefix commonPrefixWithString:item.url.createNSString().get() options:0];
    }
    if ([urlCommonPrefix hasSuffix:@"."])
        urlCommonPrefix = [urlCommonPrefix substringToIndex:[urlCommonPrefix length] - 1];

    RetainPtr platformURL = m_suggestedToActualURLMap.get(urlCommonPrefix.get());
    if (!platformURL) {
        platformURL = adoptNS([[NSURL alloc] initWithString:urlCommonPrefix.get()]);
        // The user must confirm new filenames before we can save to them.
        forceSaveAs = true;
    }

    WebInspectorUIProxy::showSavePanel(m_inspectorWindow.get(), platformURL.get(), WTF::move(saveDatas), forceSaveAs, [urlCommonPrefix, protectedThis = Ref { *this }] (NSURL *actualURL) {
        protectedThis->m_suggestedToActualURLMap.set(urlCommonPrefix.get(), actualURL);
    });
}

void WebInspectorUIProxy::platformLoad(const String& path, CompletionHandler<void(const String&)>&& completionHandler)
{
    if (auto contents = FileSystem::readEntireFile(path))
        completionHandler(String { byteCast<Latin1Character>(contents->span()) });
    else
        completionHandler(nullString());
}

void WebInspectorUIProxy::platformPickColorFromScreen(CompletionHandler<void(const std::optional<WebCore::Color>&)>&& completionHandler)
{
    auto sampler = adoptNS([[NSColorSampler alloc] init]);
    [sampler.get() showSamplerWithSelectionHandler:makeBlockPtr([completionHandler = WTF::move(completionHandler)](NSColor *selectedColor) mutable {
        if (!selectedColor) {
            completionHandler(std::nullopt);
            return;
        }

        completionHandler(Color::createAndPreserveColorSpace(RetainPtr { selectedColor.CGColor }.get()));
    }).get()];
}

void WebInspectorUIProxy::windowFrameDidChange()
{
    ASSERT(!m_isAttached);
    ASSERT(m_isVisible);
    ASSERT(m_inspectorWindow);

    RefPtr inspectedPage = m_inspectedPage.get();
    if (m_isAttached || !m_isVisible || !m_inspectorWindow || !inspectedPage)
        return;

    RetainPtr frameString = NSStringFromRect([m_inspectorWindow frame]);
    inspectedPage->pageGroup().preferences().setInspectorWindowFrame(frameString.get());
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

    RefPtr inspectedPage = m_inspectedPage.get();
    if (!inspectedPage)
        return;

    RetainPtr inspectedView = inspectedPage->inspectorAttachmentView();
    RetainPtr<WKWebView> inspectorView = [m_inspectorViewController webView];

    NSRect inspectedViewFrame = inspectedView.get().frame;
    NSRect oldInspectorViewFrame = inspectorView.get().frame;
    NSRect newInspectorViewFrame = NSZeroRect;
    NSRect parentBounds = inspectedView.get().superview.bounds;
    CGFloat inspectedViewTop = NSMaxY(inspectedViewFrame);

    auto frameAdjustedForContentLayoutRect = [&](NSRect frameIgnoringContentLayoutRect) {
#if ENABLE(CONTENT_INSET_BACKGROUND_FILL)
        bool drawsScrollPocket = inspectedPage->preferences().contentInsetBackgroundFillEnabled() && inspectedPage->pendingOrActualObscuredContentInsets().top();
        if (drawsScrollPocket)
            return frameIgnoringContentLayoutRect;
#endif

        RetainPtr inspectorWindow = [inspectorView window];
        if (!inspectorWindow)
            return frameIgnoringContentLayoutRect;

        auto contentLayoutRect = [retainPtr([inspectedView superview]) convertRect:[inspectorWindow contentLayoutRect] fromView:nil];
        return NSIntersectionRect(frameIgnoringContentLayoutRect, contentLayoutRect);
    };

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
        newInspectorViewFrame = frameAdjustedForContentLayoutRect(NSMakeRect(parentWidth - inspectorWidth, 0, inspectorWidth, NSHeight(parentBounds)));
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
        newInspectorViewFrame = frameAdjustedForContentLayoutRect(NSMakeRect(0, 0, inspectorWidth, NSHeight(parentBounds)));
        break;
    }
    }

    if (NSEqualRects(oldInspectorViewFrame, newInspectorViewFrame) && NSEqualRects([inspectedView frame], inspectedViewFrame))
        return;

    // Disable screen updates to make sure the layers for both views resize in sync.
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    [retainPtr(inspectorView.get().window) disableScreenUpdatesUntilFlush];
    ALLOW_DEPRECATED_DECLARATIONS_END

    [inspectorView setFrame:newInspectorViewFrame];
    [inspectedView setFrame:inspectedViewFrame];
}

void WebInspectorUIProxy::platformAttach()
{
    ASSERT(protect(inspectedPage()));
    RetainPtr inspectedView = protect(inspectedPage())->inspectorAttachmentView();
    RetainPtr<WKWebView> inspectorView = [m_inspectorViewController webView];

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
        currentDimension = protect(inspectorPagePreferences())->inspectorAttachedHeight();
        break;
    case AttachmentSide::Right:
        [inspectorView setAutoresizingMask:NSViewHeightSizable | NSViewMinXMargin];
        currentDimension = protect(inspectorPagePreferences())->inspectorAttachedWidth();
        break;
    case AttachmentSide::Left:
        [inspectorView setAutoresizingMask:NSViewHeightSizable | NSViewMaxXMargin];
        currentDimension = protect(inspectorPagePreferences())->inspectorAttachedWidth();
        break;
    }

    inspectedViewFrameDidChange(currentDimension);

    [retainPtr(inspectedView.get().superview) addSubview:inspectorView.get() positioned:NSWindowBelow relativeTo:inspectedView.get()];
    [retainPtr(inspectorView.get().window) makeFirstResponder:inspectorView.get()];

    [m_inspectorViewController didAttachOrDetach];
}

void WebInspectorUIProxy::platformDetach()
{
    RefPtr inspectedPage = m_inspectedPage.get();
    RetainPtr inspectedView = inspectedPage ? inspectedPage->inspectorAttachmentView() : nil;
    RetainPtr<WKWebView> inspectorView = [m_inspectorViewController webView];

    [inspectorView removeFromSuperview];
    [inspectorView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];

    // Make sure that we size the inspected view's frame after detaching so that it takes up the space that the
    // attached inspector used to. Preserve the top position of the inspected view so banners in Safari still work.
    if (inspectedView)
        inspectedView.get().frame = NSMakeRect(0, 0, NSWidth(inspectedView.get().superview.bounds), NSMaxY(inspectedView.get().frame));

    // Return early if we are not visible. This means the inspector was closed while attached
    // and we should not create and show the inspector window.
    if (!m_isVisible)
        return;

    open();

    [m_inspectorViewController didAttachOrDetach];
}

void WebInspectorUIProxy::platformSetAttachedWindowHeight(unsigned height)
{
    if (!m_isAttached)
        return;

    NSEdgeInsets webViewInsets = [m_inspectorViewController webView].safeAreaInsets;
    inspectedViewFrameDidChange(height + webViewInsets.top + webViewInsets.bottom);
}

void WebInspectorUIProxy::platformSetAttachedWindowWidth(unsigned width)
{
    if (!m_isAttached)
        return;

    NSEdgeInsets webViewInsets = [m_inspectorViewController webView].safeAreaInsets;
    inspectedViewFrameDidChange(width + webViewInsets.left + webViewInsets.right);
}

void WebInspectorUIProxy::platformSetSheetRect(const FloatRect& rect)
{
    m_sheetRect = rect;
}

void WebInspectorUIProxy::platformStartWindowDrag()
{
    if (RetainPtr webView = [m_inspectorViewController webView]) {
        if (RefPtr page = webView->_page)
            page->startWindowDrag();
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

DebuggableInfoData WebInspectorUIProxy::infoForLocalDebuggable()
{
    RetainPtr plist = bridge_cast(adoptCF(_CFCopySystemVersionDictionary()));

    DebuggableInfoData result;
    result.debuggableType = Inspector::DebuggableType::WebPage;
    result.targetPlatformName = "macOS"_s;
    result.targetBuildVersion = plist.get()[static_cast<NSString *>(_kCFSystemVersionBuildVersionKey)];
    result.targetProductVersion = plist.get()[static_cast<NSString *>(_kCFSystemVersionProductUserVisibleVersionKey)];
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

    if (RetainPtr window = m_inspectorWindow.get())
        window.get().appearance = platformAppearance;

    [m_inspectorViewController webView].appearance = platformAppearance;
}

} // namespace WebKit

#endif // PLATFORM(MAC)
