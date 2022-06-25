/*
 * Copyright (C) 2006-2022 Apple Inc.  All rights reserved.
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

#import "WebInspectorClient.h"

#import "DOMNodeInternal.h"
#import "WebDelegateImplementationCaching.h"
#import "WebFrameInternal.h"
#import "WebFrameView.h"
#import "WebInspector.h"
#import "WebInspectorFrontend.h"
#import "WebInspectorPrivate.h"
#import "WebLocalizableStringsInternal.h"
#import "WebNodeHighlighter.h"
#import "WebPolicyDelegate.h"
#import "WebQuotaManager.h"
#import "WebSecurityOriginPrivate.h"
#import "WebUIDelegatePrivate.h"
#import "WebViewInternal.h"
#import <JavaScriptCore/InspectorAgentBase.h>
#import <SecurityInterface/SFCertificatePanel.h>
#import <SecurityInterface/SFCertificateView.h>
#import <WebCore/CertificateInfo.h>
#import <WebCore/Frame.h>
#import <WebCore/InspectorController.h>
#import <WebCore/InspectorFrontendClient.h>
#import <WebCore/Page.h>
#import <WebCore/ScriptController.h>
#import <WebCore/Settings.h>
#import <WebKitLegacy/DOMExtensions.h>
#import <algorithm>
#import <wtf/NakedPtr.h>
#import <wtf/text/Base64.h>

using namespace WebCore;
using namespace Inspector;

static const CGFloat minimumWindowWidth = 500;
static const CGFloat minimumWindowHeight = 400;
static const CGFloat initialWindowWidth = 1000;
static const CGFloat initialWindowHeight = 650;

@interface WebInspectorWindowController : NSWindowController <NSWindowDelegate, WebPolicyDelegate, WebUIDelegate> {
@private
    RetainPtr<WebView> _inspectedWebView;
    RetainPtr<WebView> _frontendWebView;
    NakedPtr<WebInspectorFrontendClient> _frontendClient;
    WebInspectorClient* _inspectorClient;
    BOOL _attachedToInspectedWebView;
    BOOL _shouldAttach;
    BOOL _visible;
    BOOL _destroyingInspectorView;
}
- (id)initWithInspectedWebView:(WebView *)inspectedWebView isUnderTest:(BOOL)isUnderTest;
- (NSString *)inspectorPagePath;
- (NSString *)inspectorTestPagePath;
- (WebView *)frontendWebView;
- (void)attach;
- (void)detach;
- (BOOL)attached;
- (void)setFrontendClient:(NakedPtr<WebInspectorFrontendClient>)frontendClient;
- (void)setInspectorClient:(NakedPtr<WebInspectorClient>)inspectorClient;
- (NakedPtr<WebInspectorClient>)inspectorClient;
- (void)setAttachedWindowHeight:(unsigned)height;
- (void)setDockingUnavailable:(BOOL)unavailable;
- (void)destroyInspectorView;
@end


// MARK: -

WebInspectorClient::WebInspectorClient(WebView* inspectedWebView)
    : m_inspectedWebView(inspectedWebView)
    , m_highlighter(adoptNS([[WebNodeHighlighter alloc] initWithInspectedWebView:inspectedWebView]))
{
}

void WebInspectorClient::inspectedPageDestroyed()
{
    delete this;
}

FrontendChannel* WebInspectorClient::openLocalFrontend(InspectorController* inspectedPageController)
{
    RetainPtr<WebInspectorWindowController> windowController = adoptNS([[WebInspectorWindowController alloc] initWithInspectedWebView:m_inspectedWebView isUnderTest:inspectedPageController->isUnderTest()]);
    [windowController.get() setInspectorClient:this];

    m_frontendPage = core([windowController.get() frontendWebView]);
    m_frontendClient = makeUnique<WebInspectorFrontendClient>(m_inspectedWebView, windowController.get(), inspectedPageController, m_frontendPage, createFrontendSettings());

    RetainPtr<WebInspectorFrontend> webInspectorFrontend = adoptNS([[WebInspectorFrontend alloc] initWithFrontendClient:m_frontendClient.get()]);
    [[m_inspectedWebView inspector] setFrontend:webInspectorFrontend.get()];

    m_frontendPage->inspectorController().setInspectorFrontendClient(m_frontendClient.get());

    return this;
}

void WebInspectorClient::bringFrontendToFront()
{
    ASSERT(m_frontendClient);
    m_frontendClient->bringToFront();
}

void WebInspectorClient::didResizeMainFrame(Frame*)
{
    if (m_frontendClient)
        m_frontendClient->attachAvailabilityChanged(canAttach());
}

void WebInspectorClient::windowFullScreenDidChange()
{
    if (m_frontendClient)
        m_frontendClient->attachAvailabilityChanged(canAttach());
}

bool WebInspectorClient::canAttach()
{
    return m_frontendClient->canAttach() && !inspectorAttachDisabled();
}

void WebInspectorClient::highlight()
{
    [m_highlighter.get() highlight];
}

void WebInspectorClient::hideHighlight()
{
    [m_highlighter.get() hideHighlight];
}

void WebInspectorClient::didSetSearchingForNode(bool enabled)
{
    WebInspector *inspector = [m_inspectedWebView inspector];

    ASSERT(isMainThread());

    if (enabled) {
        [[m_inspectedWebView window] makeKeyAndOrderFront:nil];
        [[m_inspectedWebView window] makeFirstResponder:m_inspectedWebView];
        [[NSNotificationCenter defaultCenter] postNotificationName:WebInspectorDidStartSearchingForNode object:inspector];
    } else
        [[NSNotificationCenter defaultCenter] postNotificationName:WebInspectorDidStopSearchingForNode object:inspector];
}

void WebInspectorClient::releaseFrontend()
{
    m_frontendClient = nullptr;
    m_frontendPage = nullptr;
}


WebInspectorFrontendClient::WebInspectorFrontendClient(WebView* inspectedWebView, WebInspectorWindowController* frontendWindowController, InspectorController* inspectedPageController, Page* frontendPage, std::unique_ptr<Settings> settings)
    : InspectorFrontendClientLocal(inspectedPageController, frontendPage, WTFMove(settings))
    , m_inspectedWebView(inspectedWebView)
    , m_frontendWindowController(frontendWindowController)
{
    [frontendWindowController setFrontendClient:this];
}

void WebInspectorFrontendClient::attachAvailabilityChanged(bool available)
{
    setDockingUnavailable(!available);
    [m_frontendWindowController.get() setDockingUnavailable:!available];
}

bool WebInspectorFrontendClient::canAttach()
{
    if ([[m_frontendWindowController window] styleMask] & NSWindowStyleMaskFullScreen)
        return false;

    return canAttachWindow();
}

void WebInspectorFrontendClient::frontendLoaded()
{
    [m_frontendWindowController.get() showWindow:nil];
    if ([m_frontendWindowController.get() attached])
        restoreAttachedWindowHeight();

    InspectorFrontendClientLocal::frontendLoaded();

    WebFrame *frame = [m_inspectedWebView mainFrame];

    WebFrameLoadDelegateImplementationCache* implementations = WebViewGetFrameLoadDelegateImplementations(m_inspectedWebView);
    if (implementations->didClearInspectorWindowObjectForFrameFunc)
        CallFrameLoadDelegate(implementations->didClearInspectorWindowObjectForFrameFunc, m_inspectedWebView,
                              @selector(webView:didClearInspectorWindowObject:forFrame:), [frame windowObject], frame);

    bool attached = [m_frontendWindowController.get() attached];
    setAttachedWindow(attached ? DockSide::Bottom : DockSide::Undocked);
}

void WebInspectorFrontendClient::startWindowDrag()
{
    [[m_frontendWindowController window] performWindowDragWithEvent:[NSApp currentEvent]];
}

String WebInspectorFrontendClient::localizedStringsURL() const
{
    NSBundle *bundle = [NSBundle bundleWithIdentifier:@"com.apple.WebInspectorUI"];
    if (!bundle)
        return String();

    NSString *path = [bundle pathForResource:@"localizedStrings" ofType:@"js"];
    if (!path.length)
        return String();
    
    return [NSURL fileURLWithPath:path isDirectory:NO].absoluteString;
}

void WebInspectorFrontendClient::bringToFront()
{
    updateWindowTitle();

    [m_frontendWindowController.get() showWindow:nil];

    // Use the window from the WebView since m_frontendWindowController's window
    // is not the same when the Inspector is docked.
    WebView *frontendWebView = [m_frontendWindowController.get() frontendWebView];
    [[frontendWebView window] makeFirstResponder:frontendWebView];
}

void WebInspectorFrontendClient::closeWindow()
{
    [m_frontendWindowController.get() destroyInspectorView];
}

void WebInspectorFrontendClient::reopen()
{
    WebInspector* inspector = [m_inspectedWebView inspector];
    [inspector close:nil];
    [inspector show:nil];
}

void WebInspectorFrontendClient::resetState()
{
    InspectorFrontendClientLocal::resetState();

    auto inspectorClient = [m_frontendWindowController inspectorClient];
    inspectorClient->deleteInspectorStartsAttached();
    inspectorClient->deleteInspectorAttachDisabled();

    [NSWindow removeFrameUsingName:[[m_frontendWindowController window] frameAutosaveName]];
}

void WebInspectorFrontendClient::setForcedAppearance(InspectorFrontendClient::Appearance appearance)
{
    NSWindow *window = [m_frontendWindowController window];
    ASSERT(window);

    switch (appearance) {
    case InspectorFrontendClient::Appearance::System:
        window.appearance = nil;
        break;

    case InspectorFrontendClient::Appearance::Light:
        window.appearance = [NSAppearance appearanceNamed:NSAppearanceNameAqua];
        break;

    case InspectorFrontendClient::Appearance::Dark:
        window.appearance = [NSAppearance appearanceNamed:NSAppearanceNameDarkAqua];
        break;
    }
}

bool WebInspectorFrontendClient::supportsDockSide(DockSide dockSide)
{
    switch (dockSide) {
    case DockSide::Undocked:
    case DockSide::Bottom:
        return true;

    case DockSide::Right:
    case DockSide::Left:
        return false;
    }

    ASSERT_NOT_REACHED();
    return false;
}

void WebInspectorFrontendClient::attachWindow(DockSide)
{
    if ([m_frontendWindowController.get() attached])
        return;
    [m_frontendWindowController.get() attach];
    restoreAttachedWindowHeight();
}

void WebInspectorFrontendClient::detachWindow()
{
    [m_frontendWindowController.get() detach];
}

void WebInspectorFrontendClient::setAttachedWindowHeight(unsigned height)
{
    [m_frontendWindowController.get() setAttachedWindowHeight:height];
}

void WebInspectorFrontendClient::setAttachedWindowWidth(unsigned)
{
    // Dock to right is not implemented in WebKit 1.
}

void WebInspectorFrontendClient::setSheetRect(const FloatRect& rect)
{
    m_sheetRect = rect;
}

void WebInspectorFrontendClient::inspectedURLChanged(const String& newURL)
{
    m_inspectedURL = newURL;
    updateWindowTitle();
}

void WebInspectorFrontendClient::showCertificate(const CertificateInfo& certificateInfo)
{
    ASSERT(!certificateInfo.isEmpty());

    RetainPtr<SFCertificatePanel> certificatePanel = adoptNS([[SFCertificatePanel alloc] init]);

    NSWindow *window = [[m_frontendWindowController frontendWebView] window];
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

#if ENABLE(INSPECTOR_TELEMETRY)
bool WebInspectorFrontendClient::supportsDiagnosticLogging()
{
    auto* page = frontendPage();
    return page ? page->settings().diagnosticLoggingEnabled() : false;
}

void WebInspectorFrontendClient::logDiagnosticEvent(const String& eventName, const DiagnosticLoggingClient::ValueDictionary& dictionary)
{
    if (auto* page = frontendPage())
        page->diagnosticLoggingClient().logDiagnosticMessageWithValueDictionary(eventName, "Legacy Web Inspector Frontend Diagnostics"_s, dictionary, ShouldSample::No);
}
#endif

void WebInspectorFrontendClient::updateWindowTitle() const
{
    NSString *title = [NSString stringWithFormat:UI_STRING_INTERNAL("Web Inspector — %@", "Web Inspector window title"), (NSString *)m_inspectedURL];
    [[m_frontendWindowController.get() window] setTitle:title];
}

bool WebInspectorFrontendClient::canSave(InspectorFrontendClient::SaveMode saveMode)
{
    switch (saveMode) {
    case InspectorFrontendClient::SaveMode::SingleFile:
        return true;

    case InspectorFrontendClient::SaveMode::FileVariants:
        return false;
    }

    ASSERT_NOT_REACHED();
    return false;
}

void WebInspectorFrontendClient::save(Vector<InspectorFrontendClient::SaveData>&& saveDatas, bool forceSaveAs)
{
    // FIXME: Share with WebInspectorUIProxyMac.

    ASSERT(saveDatas.size() == 1);

    auto suggestedURL = saveDatas[0].url;
    ASSERT(!suggestedURL.isEmpty());

    NSURL *platformURL = m_suggestedToActualURLMap.get(suggestedURL).get();
    if (!platformURL) {
        platformURL = [NSURL URLWithString:suggestedURL];
        // The user must confirm new filenames before we can save to them.
        forceSaveAs = true;
    }

    ASSERT(platformURL);
    if (!platformURL)
        return;

    // Necessary for the block below.
    String suggestedURLCopy = suggestedURL;
    String contentCopy = saveDatas[0].content;
    bool base64Encoded = saveDatas[0].base64Encoded;

    auto saveToURL = ^(NSURL *actualURL) {
        ASSERT(actualURL);

        m_suggestedToActualURLMap.set(suggestedURLCopy, actualURL);

        if (base64Encoded) {
            auto decodedData = base64Decode(contentCopy, Base64DecodeOptions::ValidatePadding);
            if (!decodedData)
                return;
            RetainPtr<NSData> dataContent = adoptNS([[NSData alloc] initWithBytes:decodedData->data() length:decodedData->size()]);
            [dataContent writeToURL:actualURL atomically:YES];
        } else
            [contentCopy writeToURL:actualURL atomically:YES encoding:NSUTF8StringEncoding error:NULL];
    };

    if (!forceSaveAs) {
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

    NSWindow *frontendWindow = [[m_frontendWindowController frontendWebView] window];
    NSWindow *window = frontendWindow ? frontendWindow : [NSApp keyWindow];
    if (window)
        [panel beginSheetModalForWindow:window completionHandler:completionHandler];
    else
        completionHandler([panel runModal]);
}

// MARK: -

@implementation WebInspectorWindowController
- (id)init
{
    if (!(self = [super initWithWindow:nil]))
        return nil;

    // Keep preferences separate from the rest of the client, making sure we are using expected preference values.

    auto preferences = adoptNS([[WebPreferences alloc] init]);
    [preferences setAllowsAnimatedImages:YES];
    [preferences setAuthorAndUserStylesEnabled:YES];
    [preferences setAutosaves:NO];
    [preferences setDefaultFixedFontSize:11];
    [preferences setFixedFontFamily:@"Menlo"];
    [preferences setJavaScriptEnabled:YES];
    [preferences setLoadsImagesAutomatically:YES];
    [preferences setMinimumFontSize:0];
    [preferences setMinimumLogicalFontSize:9];
    [preferences setPlugInsEnabled:NO];
    [preferences setTabsToLinks:NO];
    [preferences setUserStyleSheetEnabled:NO];
    [preferences setAllowFileAccessFromFileURLs:YES];
    [preferences setAllowUniversalAccessFromFileURLs:YES];
    [preferences setAllowTopNavigationToDataURLs:YES];
    [preferences setStorageBlockingPolicy:WebAllowAllStorage];

    _frontendWebView = adoptNS([[WebView alloc] init]);
    [_frontendWebView setPreferences:preferences.get()];
    [_frontendWebView setProhibitsMainFrameScrolling:YES];
    [_frontendWebView setUIDelegate:self];
    [_frontendWebView setPolicyDelegate:self];

    [self setWindowFrameAutosaveName:@"Web Inspector 2"];
    return self;
}

- (id)initWithInspectedWebView:(WebView *)webView isUnderTest:(BOOL)isUnderTest
{
    if (!(self = [self init]))
        return nil;

    _inspectedWebView = webView;

    NSString *pagePath = isUnderTest ? [self inspectorTestPagePath] : [self inspectorPagePath];
    auto request = adoptNS([[NSURLRequest alloc] initWithURL:[NSURL fileURLWithPath:pagePath]]);
    [[_frontendWebView mainFrame] loadRequest:request.get()];

    return self;
}

// MARK: -

- (NSString *)inspectorPagePath
{
    NSBundle *bundle = [NSBundle bundleWithIdentifier:@"com.apple.WebInspectorUI"];
    if (!bundle)
        return nil;

    return [bundle pathForResource:@"Main" ofType:@"html"];
}

- (NSString *)inspectorTestPagePath
{
    NSBundle *bundle = [NSBundle bundleWithIdentifier:@"com.apple.WebInspectorUI"];
    if (!bundle)
        return nil;

    // We might not have a Test.html in Production builds.
    return [bundle pathForResource:@"Test" ofType:@"html"];
}

// MARK: -

- (WebView *)frontendWebView
{
    return _frontendWebView.get();
}

- (NSWindow *)window
{
    if (auto *window = [super window])
        return window;

    NSUInteger styleMask = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable | NSWindowStyleMaskFullSizeContentView;
    auto window = adoptNS([[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, initialWindowWidth, initialWindowHeight) styleMask:styleMask backing:NSBackingStoreBuffered defer:NO]);
    [window setDelegate:self];
    [window setMinSize:NSMakeSize(minimumWindowWidth, minimumWindowHeight)];
    [window setCollectionBehavior:([window collectionBehavior] | NSWindowCollectionBehaviorFullScreenPrimary)];

    CGFloat approximatelyHalfScreenSize = ([window screen].frame.size.width / 2) - 4;
    CGFloat minimumFullScreenWidth = std::max<CGFloat>(636, approximatelyHalfScreenSize);
    [window setMinFullScreenContentSize:NSMakeSize(minimumFullScreenWidth, minimumWindowHeight)];
    [window setCollectionBehavior:([window collectionBehavior] | NSWindowCollectionBehaviorFullScreenAllowsTiling)];

    // FIXME: <rdar://94829409> Replace Stage Manager auxiliary window workaround.
    [window setToolbar:[NSToolbar new]];
    [[window toolbar] setVisible:NO];
    [window setToolbarStyle:NSWindowToolbarStylePreference];
    
    [window setTitlebarAppearsTransparent:YES];

    [self setWindow:window.get()];
    return window.get();
}

// MARK: -

- (NSRect)window:(NSWindow *)window willPositionSheet:(NSWindow *)sheet usingRect:(NSRect)rect
{
    if (_frontendClient)
        return NSMakeRect(0, _frontendClient->sheetRect().height(), _frontendClient->sheetRect().width(), 0);

    // AppKit doesn't know about our HTML toolbar, and places the sheet just a little bit too high.
    rect.origin.y -= 1;
    return rect;
}

- (BOOL)windowShouldClose:(id)sender
{
    [self destroyInspectorView];

    return YES;
}

- (void)windowDidEnterFullScreen:(NSNotification *)notification
{
    _inspectorClient->windowFullScreenDidChange();
}

- (void)windowDidExitFullScreen:(NSNotification *)notification
{
    _inspectorClient->windowFullScreenDidChange();
}

- (void)close
{
    if (!_visible)
        return;

    _visible = NO;

    if (_attachedToInspectedWebView) {
        if ([_inspectedWebView.get() _isClosed])
            return;

        [_frontendWebView removeFromSuperview];

        WebFrameView *frameView = [[_inspectedWebView.get() mainFrame] frameView];
        NSRect frameViewRect = [frameView frame];

        // Setting the height based on the previous height is done to work with
        // Safari's find banner. This assumes the previous height is the Y origin.
        frameViewRect.size.height += NSMinY(frameViewRect);
        frameViewRect.origin.y = 0.0;

        [frameView setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];
        [frameView setFrame:frameViewRect];

        [_inspectedWebView.get() displayIfNeeded];
    } else
        [super close];
}

- (IBAction)attachWindow:(id)sender
{
    _frontendClient->attachWindow(InspectorFrontendClient::DockSide::Bottom);
}

- (IBAction)showWindow:(id)sender
{
    if (_visible) {
        if (!_attachedToInspectedWebView)
            [super showWindow:sender]; // call super so the window will be ordered front if needed
        return;
    }

    _visible = YES;

    _shouldAttach = _inspectorClient->inspectorStartsAttached() && _frontendClient->canAttach();

    if (_shouldAttach) {
        WebFrameView *frameView = [[_inspectedWebView.get() mainFrame] frameView];

        [_frontendWebView removeFromSuperview];
        [_inspectedWebView.get() addSubview:_frontendWebView.get() positioned:NSWindowBelow relativeTo:(NSView *)frameView];
        [[_inspectedWebView.get() window] makeFirstResponder:_frontendWebView.get()];

        [_frontendWebView setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable | NSViewMaxYMargin)];
        [frameView setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable | NSViewMinYMargin)];

        _attachedToInspectedWebView = YES;
    } else {
        _attachedToInspectedWebView = NO;

        NSView *contentView = [[self window] contentView];
        [_frontendWebView setFrame:[contentView frame]];
        [_frontendWebView setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];
        [_frontendWebView removeFromSuperview];
        [contentView addSubview:_frontendWebView.get()];

        [super showWindow:nil];
    }
}

// MARK: -

- (void)attach
{
    if (_attachedToInspectedWebView)
        return;

    _inspectorClient->setInspectorStartsAttached(true);
    _frontendClient->setAttachedWindow(InspectorFrontendClient::DockSide::Bottom);

    [self close];
    [self showWindow:nil];
}

- (void)detach
{
    if (!_attachedToInspectedWebView)
        return;

    _inspectorClient->setInspectorStartsAttached(false);
    _frontendClient->setAttachedWindow(InspectorFrontendClient::DockSide::Undocked);

    [self close];
    [self showWindow:nil];
}

- (BOOL)attached
{
    return _attachedToInspectedWebView;
}

- (void)setFrontendClient:(NakedPtr<WebInspectorFrontendClient>)frontendClient
{
    _frontendClient = frontendClient;
}

- (void)setInspectorClient:(NakedPtr<WebInspectorClient>)inspectorClient
{
    _inspectorClient = inspectorClient;
}

- (NakedPtr<WebInspectorClient>)inspectorClient
{
    return _inspectorClient;
}

- (void)setAttachedWindowHeight:(unsigned)height
{
    if (!_attachedToInspectedWebView)
        return;

    WebFrameView *frameView = [[_inspectedWebView.get() mainFrame] frameView];
    NSRect frameViewRect = [frameView frame];

    // Setting the height based on the difference is done to work with
    // Safari's find banner. This assumes the previous height is the Y origin.
    CGFloat heightDifference = (NSMinY(frameViewRect) - height);
    frameViewRect.size.height += heightDifference;
    frameViewRect.origin.y = height;

    [_frontendWebView setFrame:NSMakeRect(0.0, 0.0, NSWidth(frameViewRect), height)];
    [frameView setFrame:frameViewRect];
}

- (void)setDockingUnavailable:(BOOL)unavailable
{
    // Do nothing.
}

- (void)destroyInspectorView
{
    RetainPtr<WebInspectorWindowController> protect(self);

    if (Page* frontendPage = _frontendClient->frontendPage())
        frontendPage->inspectorController().setInspectorFrontendClient(nullptr);
    if (Page* inspectedPage = [_inspectedWebView.get() page])
        inspectedPage->inspectorController().disconnectFrontend(*_inspectorClient);

    [[_inspectedWebView.get() inspector] releaseFrontend];
    _inspectorClient->releaseFrontend();

    if (_destroyingInspectorView)
        return;
    _destroyingInspectorView = YES;

    [self close];

    _visible = NO;

    [_frontendWebView close];
}

// MARK: -
// MARK: UI delegate

- (void)webView:(WebView *)sender runOpenPanelForFileButtonWithResultListener:(id<WebOpenPanelResultListener>)resultListener allowMultipleFiles:(BOOL)allowMultipleFiles
{
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    panel.canChooseDirectories = NO;
    panel.canChooseFiles = YES;
    panel.allowsMultipleSelection = allowMultipleFiles;

    auto completionHandler = ^(NSInteger result) {
        if (result == NSModalResponseCancel) {
            [resultListener cancel];
            return;
        }
        ASSERT(result == NSModalResponseOK);

        NSArray *URLs = panel.URLs;
        NSMutableArray *filenames = [NSMutableArray arrayWithCapacity:URLs.count];
        for (NSURL *URL in URLs)
            [filenames addObject:URL.path];

        [resultListener chooseFilenames:filenames];
    };

    if ([_frontendWebView window])
        [panel beginSheetModalForWindow:[_frontendWebView window] completionHandler:completionHandler];
    else
        completionHandler([panel runModal]);
}

- (void)webView:(WebView *)sender frame:(WebFrame *)frame exceededDatabaseQuotaForSecurityOrigin:(WebSecurityOrigin *)origin database:(NSString *)databaseIdentifier
{
    id <WebQuotaManager> databaseQuotaManager = origin.databaseQuotaManager;
    databaseQuotaManager.quota = std::max<unsigned long long>(5 * 1024 * 1024, databaseQuotaManager.usage * 1.25);
}

- (NSArray *)webView:(WebView *)sender contextMenuItemsForElement:(NSDictionary *)element defaultMenuItems:(NSArray *)defaultMenuItems
{
    auto menuItems = adoptNS([[NSMutableArray alloc] init]);

    for (NSMenuItem *item in defaultMenuItems) {
        switch (item.tag) {
        case WebMenuItemTagOpenLinkInNewWindow:
        case WebMenuItemTagOpenImageInNewWindow:
        case WebMenuItemTagOpenFrameInNewWindow:
        case WebMenuItemTagOpenMediaInNewWindow:
        case WebMenuItemTagDownloadLinkToDisk:
        case WebMenuItemTagDownloadImageToDisk:
            break;
        default:
            [menuItems addObject:item];
            break;
        }
    }

    return menuItems.autorelease();
}

// MARK: -
// MARK: Policy delegate

- (void)webView:(WebView *)webView decidePolicyForNavigationAction:(NSDictionary *)actionInformation request:(NSURLRequest *)request frame:(WebFrame *)frame decisionListener:(id<WebPolicyDecisionListener>)listener
{
    // Allow non-main frames to navigate anywhere.
    if (frame != [webView mainFrame]) {
        [listener use];
        return;
    }

    // Allow loading of the main inspector file.
    if ([[request URL] isFileURL] && [[[request URL] path] isEqualToString:[self inspectorPagePath]]) {
        [listener use];
        return;
    }

    // Allow loading of the test inspector file.
    NSString *testPagePath = [self inspectorTestPagePath];
    if (testPagePath && [[request URL] isFileURL] && [[[request URL] path] isEqualToString:testPagePath]) {
        [listener use];
        return;
    }

    // Prevent everything else from loading in the inspector's page.
    [listener ignore];

    // And instead load it in the inspected page.
    [[_inspectedWebView.get() mainFrame] loadRequest:request];
}

// MARK: -
// These methods can be used by UI elements such as menu items and toolbar buttons when the inspector is the key window.

// This method is really only implemented to keep any UI elements enabled.
- (void)showWebInspector:(id)sender
{
    [[_inspectedWebView.get() inspector] show:sender];
}

- (void)showErrorConsole:(id)sender
{
    [[_inspectedWebView.get() inspector] showConsole:sender];
}

- (void)toggleDebuggingJavaScript:(id)sender
{
    [[_inspectedWebView.get() inspector] toggleDebuggingJavaScript:sender];
}

- (void)toggleProfilingJavaScript:(id)sender
{
    [[_inspectedWebView.get() inspector] toggleProfilingJavaScript:sender];
}

- (BOOL)validateUserInterfaceItem:(id <NSValidatedUserInterfaceItem>)item
{
    BOOL isMenuItem = [(id)item isKindOfClass:[NSMenuItem class]];
    if ([item action] == @selector(toggleDebuggingJavaScript:) && isMenuItem) {
        NSMenuItem *menuItem = (NSMenuItem *)item;
        if ([[_inspectedWebView.get() inspector] isDebuggingJavaScript])
            [menuItem setTitle:UI_STRING_INTERNAL("Stop Debugging JavaScript", "title for Stop Debugging JavaScript menu item")];
        else
            [menuItem setTitle:UI_STRING_INTERNAL("Start Debugging JavaScript", "title for Start Debugging JavaScript menu item")];
    } else if ([item action] == @selector(toggleProfilingJavaScript:) && isMenuItem) {
        NSMenuItem *menuItem = (NSMenuItem *)item;
        if ([[_inspectedWebView.get() inspector] isProfilingJavaScript])
            [menuItem setTitle:UI_STRING_INTERNAL("Stop Profiling JavaScript", "title for Stop Profiling JavaScript menu item")];
        else
            [menuItem setTitle:UI_STRING_INTERNAL("Start Profiling JavaScript", "title for Start Profiling JavaScript menu item")];
    }

    return YES;
}

@end
