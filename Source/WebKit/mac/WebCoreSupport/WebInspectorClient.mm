/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc.  All rights reserved.
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
#import "WebUIDelegate.h"
#import "WebViewInternal.h"
#import <algorithm>
#import <bindings/ScriptValue.h>
#import <inspector/InspectorAgentBase.h>
#import <WebCore/InspectorController.h>
#import <WebCore/InspectorFrontendClient.h>
#import <WebCore/MainFrame.h>
#import <WebCore/Page.h>
#import <WebCore/ScriptController.h>
#import <WebCore/SoftLinking.h>
#import <WebKitLegacy/DOMExtensions.h>
#import <WebKitSystemInterface.h>
#import <wtf/text/Base64.h>

SOFT_LINK_STAGED_FRAMEWORK(WebInspectorUI, PrivateFrameworks, A)

using namespace WebCore;

static const CGFloat minimumWindowWidth = 500;
static const CGFloat minimumWindowHeight = 400;
static const CGFloat initialWindowWidth = 1000;
static const CGFloat initialWindowHeight = 650;

@interface WebInspectorWindowController : NSWindowController <NSWindowDelegate, WebPolicyDelegate, WebUIDelegate> {
@private
    RetainPtr<WebView> _inspectedWebView;
    WebView *_webView;
    WebInspectorFrontendClient* _frontendClient;
    WebInspectorClient* _inspectorClient;
    BOOL _attachedToInspectedWebView;
    BOOL _shouldAttach;
    BOOL _visible;
    BOOL _destroyingInspectorView;
}
- (id)initWithInspectedWebView:(WebView *)webView isUnderTest:(BOOL)isUnderTest;
- (NSString *)inspectorPagePath;
- (NSString *)inspectorTestPagePath;
- (WebView *)webView;
- (void)attach;
- (void)detach;
- (BOOL)attached;
- (void)setFrontendClient:(WebInspectorFrontendClient*)frontendClient;
- (void)setInspectorClient:(WebInspectorClient*)inspectorClient;
- (WebInspectorClient*)inspectorClient;
- (void)setAttachedWindowHeight:(unsigned)height;
- (void)setDockingUnavailable:(BOOL)unavailable;
- (void)destroyInspectorView:(bool)notifyInspectorController;
@end


// MARK: -

WebInspectorClient::WebInspectorClient(WebView *webView)
    : m_webView(webView)
    , m_highlighter(adoptNS([[WebNodeHighlighter alloc] initWithInspectedWebView:webView]))
    , m_frontendPage(nullptr)
{
}

void WebInspectorClient::inspectorDestroyed()
{
    closeInspectorFrontend();
    delete this;
}

InspectorFrontendChannel* WebInspectorClient::openInspectorFrontend(InspectorController* inspectorController)
{
    RetainPtr<WebInspectorWindowController> windowController = adoptNS([[WebInspectorWindowController alloc] initWithInspectedWebView:m_webView isUnderTest:inspectorController->isUnderTest()]);
    [windowController.get() setInspectorClient:this];

    m_frontendPage = core([windowController.get() webView]);
    m_frontendClient = std::make_unique<WebInspectorFrontendClient>(m_webView, windowController.get(), inspectorController, m_frontendPage, createFrontendSettings());

    RetainPtr<WebInspectorFrontend> webInspectorFrontend = adoptNS([[WebInspectorFrontend alloc] initWithFrontendClient:m_frontendClient.get()]);
    [[m_webView inspector] setFrontend:webInspectorFrontend.get()];

    m_frontendPage->inspectorController().setInspectorFrontendClient(m_frontendClient.get());

    return this;
}

void WebInspectorClient::closeInspectorFrontend()
{
    if (m_frontendClient)
        m_frontendClient->disconnectFromBackend();
}

void WebInspectorClient::bringFrontendToFront()
{
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
    WebInspector *inspector = [m_webView inspector];

    ASSERT(isMainThread());

    if (enabled)
        [[NSNotificationCenter defaultCenter] postNotificationName:WebInspectorDidStartSearchingForNode object:inspector];
    else
        [[NSNotificationCenter defaultCenter] postNotificationName:WebInspectorDidStopSearchingForNode object:inspector];
}

void WebInspectorClient::releaseFrontend()
{
    m_frontendClient = nullptr;
    m_frontendPage = nullptr;
}


WebInspectorFrontendClient::WebInspectorFrontendClient(WebView* inspectedWebView, WebInspectorWindowController* windowController, InspectorController* inspectorController, Page* frontendPage, std::unique_ptr<Settings> settings)
    : InspectorFrontendClientLocal(inspectorController,  frontendPage, WTF::move(settings))
    , m_inspectedWebView(inspectedWebView)
    , m_windowController(windowController)
{
    [windowController setFrontendClient:this];
}

void WebInspectorFrontendClient::attachAvailabilityChanged(bool available)
{
    setDockingUnavailable(!available);
    [m_windowController.get() setDockingUnavailable:!available];
}

bool WebInspectorFrontendClient::canAttach()
{
    if ([[m_windowController window] styleMask] & NSFullScreenWindowMask)
        return false;

    return canAttachWindow();
}

void WebInspectorFrontendClient::frontendLoaded()
{
    [m_windowController.get() showWindow:nil];
    if ([m_windowController.get() attached])
        restoreAttachedWindowHeight();

    InspectorFrontendClientLocal::frontendLoaded();

    WebFrame *frame = [m_inspectedWebView mainFrame];

    WebFrameLoadDelegateImplementationCache* implementations = WebViewGetFrameLoadDelegateImplementations(m_inspectedWebView);
    if (implementations->didClearInspectorWindowObjectForFrameFunc)
        CallFrameLoadDelegate(implementations->didClearInspectorWindowObjectForFrameFunc, m_inspectedWebView,
                              @selector(webView:didClearInspectorWindowObject:forFrame:), [frame windowObject], frame);

    bool attached = [m_windowController.get() attached];
    setAttachedWindow(attached ? DockSide::Bottom : DockSide::Undocked);
}

void WebInspectorFrontendClient::startWindowDrag()
{
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101100
    [[m_windowController window] performWindowDragWithEvent:[NSApp currentEvent]];
#endif
}

String WebInspectorFrontendClient::localizedStringsURL()
{
    // Call the soft link framework function to dlopen it, then [NSBundle bundleWithIdentifier:] will work.
    WebInspectorUILibrary();

    NSString *path = [[NSBundle bundleWithIdentifier:@"com.apple.WebInspectorUI"] pathForResource:@"localizedStrings" ofType:@"js"];
    if ([path length])
        return [[NSURL fileURLWithPath:path] absoluteString];
    return String();
}

void WebInspectorFrontendClient::bringToFront()
{
    updateWindowTitle();

    [m_windowController.get() showWindow:nil];

    // Use the window from the WebView since m_windowController's window
    // is not the same when the Inspector is docked.
    WebView *webView = [m_windowController.get() webView];
    [[webView window] makeFirstResponder:webView];
}

void WebInspectorFrontendClient::closeWindow()
{
    [m_windowController.get() destroyInspectorView:true];
}

void WebInspectorFrontendClient::disconnectFromBackend()
{
    [m_windowController.get() destroyInspectorView:false];
}

void WebInspectorFrontendClient::attachWindow(DockSide)
{
    if ([m_windowController.get() attached])
        return;
    [m_windowController.get() attach];
    restoreAttachedWindowHeight();
}

void WebInspectorFrontendClient::detachWindow()
{
    [m_windowController.get() detach];
}

void WebInspectorFrontendClient::setAttachedWindowHeight(unsigned height)
{
    [m_windowController.get() setAttachedWindowHeight:height];
}

void WebInspectorFrontendClient::setAttachedWindowWidth(unsigned)
{
    // Dock to right is not implemented in WebKit 1.
}

void WebInspectorFrontendClient::setToolbarHeight(unsigned height)
{
#if __MAC_OS_X_VERSION_MIN_REQUIRED <= 1090
    [[m_windowController window] setContentBorderThickness:height forEdge:NSMaxYEdge];
#endif
}

void WebInspectorFrontendClient::inspectedURLChanged(const String& newURL)
{
    m_inspectedURL = newURL;
    updateWindowTitle();
}

void WebInspectorFrontendClient::updateWindowTitle() const
{
    NSString *title = [NSString stringWithFormat:UI_STRING_INTERNAL("Web Inspector — %@", "Web Inspector window title"), (NSString *)m_inspectedURL];
    [[m_windowController.get() window] setTitle:title];
}

void WebInspectorFrontendClient::save(const String& suggestedURL, const String& content, bool base64Encoded, bool forceSaveDialog)
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

        core([m_windowController webView])->mainFrame().script().executeScript([NSString stringWithFormat:@"InspectorFrontendAPI.savedURL(\"%@\")", actualURL.absoluteString]);
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

    NSWindow *window = [[m_windowController webView] window];
    if (window)
        [panel beginSheetModalForWindow:window completionHandler:completionHandler];
    else
        completionHandler([panel runModal]);
}

void WebInspectorFrontendClient::append(const String& suggestedURL, const String& content)
{
    ASSERT(!suggestedURL.isEmpty());

    RetainPtr<NSURL> actualURL = m_suggestedToActualURLMap.get(suggestedURL);
    // do not append unless the user has already confirmed this filename in save().
    if (!actualURL)
        return;

    NSFileHandle *handle = [NSFileHandle fileHandleForWritingToURL:actualURL.get() error:NULL];
    [handle seekToEndOfFile];
    [handle writeData:[content dataUsingEncoding:NSUTF8StringEncoding]];
    [handle closeFile];

    core([m_windowController webView])->mainFrame().script().executeScript([NSString stringWithFormat:@"InspectorFrontendAPI.appendedToURL(\"%@\")", [actualURL absoluteString]]);
}

// MARK: -

@implementation WebInspectorWindowController
- (id)init
{
    if (!(self = [super initWithWindow:nil]))
        return nil;

    // Keep preferences separate from the rest of the client, making sure we are using expected preference values.

    WebPreferences *preferences = [[WebPreferences alloc] init];
    [preferences setAllowsAnimatedImages:YES];
    [preferences setAuthorAndUserStylesEnabled:YES];
    [preferences setAutosaves:NO];
    [preferences setDefaultFixedFontSize:11];
    [preferences setFixedFontFamily:@"Menlo"];
    [preferences setJavaEnabled:NO];
    [preferences setJavaScriptEnabled:YES];
    [preferences setLoadsImagesAutomatically:YES];
    [preferences setMinimumFontSize:0];
    [preferences setMinimumLogicalFontSize:9];
    [preferences setPlugInsEnabled:NO];
    [preferences setTabsToLinks:NO];
    [preferences setUserStyleSheetEnabled:NO];

    _webView = [[WebView alloc] init];
    [_webView setPreferences:preferences];
    [_webView setProhibitsMainFrameScrolling:YES];
    [_webView setUIDelegate:self];
    [_webView setPolicyDelegate:self];

#if __MAC_OS_X_VERSION_MIN_REQUIRED <= 1090
    [_webView setDrawsBackground:NO];
#endif

    [preferences release];

    [self setWindowFrameAutosaveName:@"Web Inspector 2"];
    return self;
}

- (id)initWithInspectedWebView:(WebView *)webView isUnderTest:(BOOL)isUnderTest
{
    if (!(self = [self init]))
        return nil;

    _inspectedWebView = webView;

    NSString *pagePath = isUnderTest ? [self inspectorTestPagePath] : [self inspectorPagePath];
    NSURLRequest *request = [[NSURLRequest alloc] initWithURL:[NSURL fileURLWithPath: pagePath]];
    [[_webView mainFrame] loadRequest:request];
    [request release];

    return self;
}

- (void)dealloc
{
    [_webView release];
    [super dealloc];
}

// MARK: -

- (NSString *)inspectorPagePath
{
    // Call the soft link framework function to dlopen it, then [NSBundle bundleWithIdentifier:] will work.
    WebInspectorUILibrary();

    NSString *path = [[NSBundle bundleWithIdentifier:@"com.apple.WebInspectorUI"] pathForResource:@"Main" ofType:@"html"];
    ASSERT([path length]);
    return path;
}

- (NSString *)inspectorTestPagePath
{
    // Call the soft link framework function to dlopen it, then [NSBundle bundleWithIdentifier:] will work.
    WebInspectorUILibrary();

    NSString *path = [[NSBundle bundleWithIdentifier:@"com.apple.WebInspectorUI"] pathForResource:@"Test" ofType:@"html"];

    // We might not have a Test.html in Production builds.
    if (!path)
        return nil;

    return path;
}

// MARK: -

- (WebView *)webView
{
    return _webView;
}

- (NSWindow *)window
{
    NSWindow *window = [super window];
    if (window)
        return window;

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101000
    NSUInteger styleMask = NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask | NSResizableWindowMask | NSFullSizeContentViewWindowMask;
#else
    NSUInteger styleMask = NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask | NSResizableWindowMask | NSTexturedBackgroundWindowMask;
#endif

    window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, initialWindowWidth, initialWindowHeight) styleMask:styleMask backing:NSBackingStoreBuffered defer:NO];
    [window setDelegate:self];
    [window setMinSize:NSMakeSize(minimumWindowWidth, minimumWindowHeight)];
    [window setCollectionBehavior:([window collectionBehavior] | NSWindowCollectionBehaviorFullScreenPrimary)];

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101100
    CGFloat approximatelyHalfScreenSize = (window.screen.frame.size.width / 2) - 4;
    CGFloat minimumFullScreenWidth = std::max<CGFloat>(636, approximatelyHalfScreenSize);
    [window setMinFullScreenContentSize:NSMakeSize(minimumFullScreenWidth, minimumWindowHeight)];
    [window setCollectionBehavior:([window collectionBehavior] | NSWindowCollectionBehaviorFullScreenAllowsTiling)];
#endif

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101000
    window.titlebarAppearsTransparent = YES;
#else
    [window setAutorecalculatesContentBorderThickness:NO forEdge:NSMaxYEdge];
    [window setContentBorderThickness:55. forEdge:NSMaxYEdge];
#endif

    [self setWindow:window];
    [window release];

    return window;
}

// MARK: -

- (NSRect)window:(NSWindow *)window willPositionSheet:(NSWindow *)sheet usingRect:(NSRect)rect
{
    // AppKit doesn't know about our HTML toolbar, and places the sheet just a little bit too high.
    // FIXME: It would be better to get the height of the toolbar and use it in this calculation.
    rect.origin.y -= 1;
    return rect;
}

- (BOOL)windowShouldClose:(id)sender
{
    [self destroyInspectorView:true];

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

        [_webView removeFromSuperview];

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

        [_webView removeFromSuperview];
        [_inspectedWebView.get() addSubview:_webView positioned:NSWindowBelow relativeTo:(NSView *)frameView];
        [[_inspectedWebView.get() window] makeFirstResponder:_webView];

        [_webView setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable | NSViewMaxYMargin)];
        [frameView setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable | NSViewMinYMargin)];

        _attachedToInspectedWebView = YES;
    } else {
        _attachedToInspectedWebView = NO;

        NSView *contentView = [[self window] contentView];
        [_webView setFrame:[contentView frame]];
        [_webView setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];
        [_webView removeFromSuperview];
        [contentView addSubview:_webView];

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

- (void)setFrontendClient:(WebInspectorFrontendClient*)frontendClient
{
    _frontendClient = frontendClient;
}

- (void)setInspectorClient:(WebInspectorClient*)inspectorClient
{
    _inspectorClient = inspectorClient;
}

- (WebInspectorClient*)inspectorClient
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

    [_webView setFrame:NSMakeRect(0.0, 0.0, NSWidth(frameViewRect), height)];
    [frameView setFrame:frameViewRect];
}

- (void)setDockingUnavailable:(BOOL)unavailable
{
    // Do nothing.
}

- (void)destroyInspectorView:(bool)notifyInspectorController
{
    RetainPtr<WebInspectorWindowController> protect(self);

    [[_inspectedWebView.get() inspector] releaseFrontend];
    _inspectorClient->releaseFrontend();

    if (_destroyingInspectorView)
        return;
    _destroyingInspectorView = YES;

    if (_attachedToInspectedWebView)
        [self close];

    _visible = NO;

    if (notifyInspectorController) {
        if (Page* inspectedPage = [_inspectedWebView.get() page])
            inspectedPage->inspectorController().disconnectFrontend(Inspector::DisconnectReason::InspectorDestroyed);
    }

    [_webView close];
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
        if (result == NSFileHandlingPanelCancelButton) {
            [resultListener cancel];
            return;
        }
        ASSERT(result == NSFileHandlingPanelOKButton);

        NSArray *URLs = panel.URLs;
        NSMutableArray *filenames = [NSMutableArray arrayWithCapacity:URLs.count];
        for (NSURL *URL in URLs)
            [filenames addObject:URL.path];

        [resultListener chooseFilenames:filenames];
    };

    if (_webView.window)
        [panel beginSheetModalForWindow:_webView.window completionHandler:completionHandler];
    else
        completionHandler([panel runModal]);
}

- (void)webView:(WebView *)sender frame:(WebFrame *)frame exceededDatabaseQuotaForSecurityOrigin:(WebSecurityOrigin *)origin database:(NSString *)databaseIdentifier
{
    id <WebQuotaManager> databaseQuotaManager = origin.databaseQuotaManager;
    databaseQuotaManager.quota = std::max<unsigned long long>(5 * 1024 * 1024, databaseQuotaManager.usage * 1.25);
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
