/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#import "TabCapturePicker.h"

#if HAVE(WINDOW_CAPTURE)

#import "CapturePromptUtilities.h"
#import "MediaPermissionUtilities.h"
#import "WKWebViewInternal.h"
#import "WKWebViewPrivate.h"
#import "WebInspectorUtilities.h"
#import "WebPageProxy.h"
#import "WebProcessProxy.h"
#import <WebCore/FloatRect.h>
#import <WebCore/LocalizedStrings.h>
#import <wtf/BlockPtr.h>
#import <wtf/RunLoop.h>
#import <wtf/TZoneMallocInlines.h>

constexpr CGFloat promptCornerRadius = 12;

@interface WKTabCapturePickerButton : NSButton
@end

@implementation WKTabCapturePickerButton

// The first click in a background window should press the button, not only activate the window.
- (BOOL)acceptsFirstMouse:(NSEvent *)event
{
    return YES;
}

@end

@interface WKTabCapturePickerOverlay : NSView
- (instancetype)initWithWebView:(WKWebView *)webView frame:(NSRect)frame requestingOriginName:(NSString *)requestingOriginName shareBlock:(void(^)(void))shareBlock cancelBlock:(void(^)(void))cancelBlock hoverBlock:(void(^)(void))hoverBlock;
- (void)setShowsCaptureTint:(BOOL)shows;
- (void)invalidate;
@end

@implementation WKTabCapturePickerOverlay {
    BlockPtr<void()> _shareBlock;
    BlockPtr<void()> _cancelBlock;
    BlockPtr<void()> _hoverBlock;
    WeakObjCPtr<WKWebView> _webView;
    BOOL _webViewIgnoredMouseMoveEvents;
    BOOL _showsCaptureTint;
}

- (instancetype)initWithWebView:(WKWebView *)webView frame:(NSRect)frame requestingOriginName:(NSString *)requestingOriginName shareBlock:(void(^)(void))shareBlock cancelBlock:(void(^)(void))cancelBlock hoverBlock:(void(^)(void))hoverBlock
{
    if (!(self = [super initWithFrame:frame]))
        return nil;

    _shareBlock = makeBlockPtr(shareBlock);
    _cancelBlock = makeBlockPtr(cancelBlock);
    _hoverBlock = makeBlockPtr(hoverBlock);

    _webView = webView;
    _webViewIgnoredMouseMoveEvents = [webView _ignoresMouseMoveEvents];
    [webView _setIgnoresMouseMoveEvents:YES];

    [self setWantsLayer:YES];
    [self setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    RetainPtr layer = [self layer];
    [layer setBackgroundColor:retainPtr([NSColor clearColor].CGColor)];
    [layer setBorderWidth:0];

    [self addTrackingArea:adoptNS([[NSTrackingArea alloc] initWithRect:NSZeroRect options:NSTrackingMouseEnteredAndExited | NSTrackingCursorUpdate | NSTrackingActiveInActiveApp | NSTrackingInVisibleRect owner:self userInfo:nil])];

    [self _addPromptWithRequestingOriginName:requestingOriginName];

    return self;
}

// FIXME: Keep the prompt inside the overlay, so that its buttons stay visible in very small web views, and test it.
- (void)_addPromptWithRequestingOriginName:(NSString *)requestingOriginName
{
    RetainPtr buttonStack = adoptNS([[NSStackView alloc] init]);
    [buttonStack setOrientation:NSUserInterfaceLayoutOrientationVertical];
    [buttonStack setDistribution:NSStackViewDistributionFill];
    [buttonStack setSpacing:WebKit::capturePromptButtonSpacing];
    [buttonStack setTranslatesAutoresizingMaskIntoConstraints:NO];

    CGFloat widestButtonWidth = 0;
    auto addButton = [&](NSString *title, SEL action, NSString *keyEquivalent) {
        RetainPtr button = [WKTabCapturePickerButton buttonWithTitle:title target:self action:action];
        [button setKeyEquivalent:keyEquivalent];
        [button setControlSize:NSControlSizeLarge];
        [button setToolTip:title];
        [button setTranslatesAutoresizingMaskIntoConstraints:NO];
        widestButtonWidth = std::max(widestButtonWidth, [button fittingSize].width);
        [buttonStack addArrangedSubview:button];
        WebKit::pinToWidthOfStack(button, buttonStack);
    };
    addButton(WEB_UI_NSSTRING(@"Share This Tab", "Button title in the tab capture picker to share the selected tab"), @selector(_shareClicked:), @"\r");
    addButton(WEB_UI_NSSTRING(@"Cancel", "Cancel"), @selector(_cancelClicked:), @"\E");

    CGFloat contentWidth = std::clamp(widestButtonWidth, WebKit::capturePromptMinimumContentWidth, WebKit::capturePromptMaximumContentWidth);

    RetainPtr title = adoptNS([[NSString alloc] initWithFormat:WEB_UI_NSSTRING(@"Share this tab with “%@”?", "Title of the tab capture picker prompt"), requestingOriginName]);
    RetainPtr titleLabel = [NSTextField wrappingLabelWithString:title];
    [titleLabel setFont:[NSFont boldSystemFontOfSize:[NSFont systemFontSize]]];
    [titleLabel setSelectable:NO];
    [titleLabel setPreferredMaxLayoutWidth:contentWidth];
    [titleLabel setTranslatesAutoresizingMaskIntoConstraints:NO];

    RetainPtr contentStack = adoptNS([[NSStackView alloc] init]);
    [contentStack setOrientation:NSUserInterfaceLayoutOrientationVertical];
    [contentStack setDistribution:NSStackViewDistributionFill];
    [contentStack setSpacing:WebKit::capturePromptSpacing];
    [contentStack setTranslatesAutoresizingMaskIntoConstraints:NO];
    for (NSView *view in @[ titleLabel, buttonStack ]) {
        [contentStack addArrangedSubview:view];
        WebKit::pinToWidthOfStack(view, contentStack);
    }

    RetainPtr prompt = adoptNS([[NSVisualEffectView alloc] initWithFrame:NSZeroRect]);
    [prompt setMaterial:NSVisualEffectMaterialWindowBackground];
    [prompt setBlendingMode:NSVisualEffectBlendingModeWithinWindow];
    [prompt setState:NSVisualEffectStateActive];
    [prompt setWantsLayer:YES];
    [[prompt layer] setCornerRadius:promptCornerRadius];
    [[prompt layer] setMasksToBounds:YES];
    [prompt setTranslatesAutoresizingMaskIntoConstraints:NO];
    [prompt addSubview:contentStack];
    [self addSubview:prompt];

    [NSLayoutConstraint activateConstraints:@[
        [[contentStack topAnchor] constraintEqualToAnchor:[prompt topAnchor] constant:WebKit::capturePromptTopInset],
        [[contentStack bottomAnchor] constraintEqualToAnchor:[prompt bottomAnchor] constant:-WebKit::capturePromptInset],
        [[contentStack leadingAnchor] constraintEqualToAnchor:[prompt leadingAnchor] constant:WebKit::capturePromptInset],
        [[contentStack trailingAnchor] constraintEqualToAnchor:[prompt trailingAnchor] constant:-WebKit::capturePromptInset],
        [[contentStack widthAnchor] constraintEqualToConstant:contentWidth],
        [[prompt centerXAnchor] constraintEqualToAnchor:[self centerXAnchor]],
        [[prompt centerYAnchor] constraintEqualToAnchor:[self centerYAnchor]],
    ]];
}

// FIXME: Make the overlay accessible, especially with VoiceOver.
- (void)setShowsCaptureTint:(BOOL)shows
{
    if (_showsCaptureTint == shows)
        return;
    _showsCaptureTint = shows;

    RetainPtr layer = [self layer];
    if (!shows) {
        [layer setBackgroundColor:retainPtr([NSColor clearColor].CGColor)];
        [layer setBorderWidth:0];
        return;
    }

    [layer setBackgroundColor:retainPtr([[NSColor systemBlueColor] colorWithAlphaComponent:0.18].CGColor)];
    [layer setBorderColor:retainPtr([[NSColor systemBlueColor] colorWithAlphaComponent:0.9].CGColor)];
    [layer setBorderWidth:3];
}

- (void)invalidate
{
    _shareBlock = nullptr;
    _cancelBlock = nullptr;
    _hoverBlock = nullptr;
    if (RetainPtr webView = _webView.get())
        [webView _setIgnoresMouseMoveEvents:_webViewIgnoredMouseMoveEvents];
    _webView = nil;
    [self removeFromSuperview];
}

- (void)_shareClicked:(id)sender
{
    _cancelBlock = nullptr;
    if (auto block = std::exchange(_shareBlock, nullptr))
        block();
}

- (void)_cancelClicked:(id)sender
{
    _shareBlock = nullptr;
    if (auto block = std::exchange(_cancelBlock, nullptr))
        block();
}

- (BOOL)acceptsFirstMouse:(NSEvent *)event
{
    return YES;
}

- (void)mouseEntered:(NSEvent *)event
{
    if (_hoverBlock)
        _hoverBlock();
}

- (void)mouseExited:(NSEvent *)event
{
}

- (void)cursorUpdate:(NSEvent *)event
{
    [[NSCursor arrowCursor] set];
}

// The overlay takes the mouse and zoom events so that they do not reach the page underneath.
- (void)mouseDown:(NSEvent *)event
{
}

- (void)mouseDragged:(NSEvent *)event
{
}

- (void)mouseUp:(NSEvent *)event
{
}

- (void)rightMouseDown:(NSEvent *)event
{
}

- (void)rightMouseDragged:(NSEvent *)event
{
}

- (void)rightMouseUp:(NSEvent *)event
{
}

- (void)otherMouseDown:(NSEvent *)event
{
}

- (void)otherMouseDragged:(NSEvent *)event
{
}

- (void)otherMouseUp:(NSEvent *)event
{
}

- (void)magnifyWithEvent:(NSEvent *)event
{
}

- (void)smartMagnifyWithEvent:(NSEvent *)event
{
}

@end

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(TabCapturePicker);

static NSRect overlayFrame(WebPageProxy& page, WKWebView *webView)
{
    WebCore::FloatRect frame = [webView bounds];
    frame.contract(page.pendingOrActualObscuredContentInsets());
    return frame;
}

static bool canShowOverlayInWindow(NSWindow *window)
{
    return window && ([window isVisible] || [window isMiniaturized]);
}

static bool canShowOverlay(WebPageProxy* page)
{
    RetainPtr webView = page ? page->cocoaView() : nullptr;
    return webView && canShowOverlayInWindow([webView window]) && ![webView isHiddenOrHasHiddenAncestor];
}

// FIXME: for now, we only support a window that renders one web view. We should consider the case of a window rendering multiple web views.
static RefPtr<WebPageProxy> visiblePageInWindow(NSWindow *window)
{
    if (!window)
        return nullptr;
    for (Ref process : WebProcessProxy::allProcesses()) {
        for (Ref page : process->pages()) {
            RetainPtr view = page->cocoaView();
            if (view && [view window] == window && ![view isHiddenOrHasHiddenAncestor] && !isInspectorPage(page))
                return page;
        }
    }
    return nullptr;
}

Ref<TabCapturePicker> TabCapturePicker::create(WebPageProxy& requestingPage, const WebCore::SecurityOriginData& requestingOrigin, Callback&& callback)
{
    return adoptRef(*new TabCapturePicker(requestingPage, requestingOrigin, WTF::move(callback)));
}

TabCapturePicker::TabCapturePicker(WebPageProxy& requestingPage, const WebCore::SecurityOriginData& requestingOrigin, Callback&& callback)
    : m_requestingPage(requestingPage)
    , m_requestingOriginName(applicationVisibleNameFromOrigin(requestingOrigin))
    , m_callback(WTF::move(callback))
{
    if (!m_requestingOriginName)
        m_requestingOriginName = applicationVisibleName();
}

TabCapturePicker::~TabCapturePicker()
{
    stop();
    if (m_callback)
        m_callback(nullptr);
}

void TabCapturePicker::start()
{
    RefPtr requestingPage = m_requestingPage;
    RetainPtr requestingWebView = requestingPage ? requestingPage->cocoaView() : nullptr;
    if (!requestingWebView) {
        finish(nullptr);
        return;
    }

    m_selectedWindow = [requestingWebView window];

    m_windowWillCloseObserver = [[NSNotificationCenter defaultCenter] addObserverForName:NSWindowWillCloseNotification object:nil queue:nil usingBlock:makeBlockPtr([weakThis = WeakPtr { *this }](NSNotification *) {
        if (RefPtr protectedThis = weakThis)
            protectedThis->scheduleOverlaysUpdate();
    }).get()];

    updateOverlays();
}

void TabCapturePicker::cancel()
{
    finish(nullptr);
}

void TabCapturePicker::scheduleOverlaysUpdate()
{
    if (!m_callback || m_isOverlaysUpdateScheduled)
        return;

    m_isOverlaysUpdateScheduled = true;
    RunLoop::mainSingleton().dispatch([weakThis = WeakPtr { *this }] {
        if (RefPtr protectedThis = weakThis)
            protectedThis->updateOverlays();
    });
}

void TabCapturePicker::updateOverlays()
{
    m_isOverlaysUpdateScheduled = false;
    if (!m_callback)
        return;

    RetainPtr windowsWithOverlay = adoptNS([[NSMutableSet alloc] init]);
    m_overlays.removeAllMatching([&](auto& overlay) {
        RefPtr page = overlay.page;
        RetainPtr webView = page ? page->cocoaView() : nullptr;
        if (canShowOverlay(page) && [overlay.view superview] == webView) {
            [overlay.view setFrame:overlayFrame(*page, webView)];
            [windowsWithOverlay addObject:retainPtr([webView window])];
            return false;
        }
        [overlay.view invalidate];
        return true;
    });

    for (Ref process : WebProcessProxy::allProcesses()) {
        for (Ref page : process->pages()) {
            RetainPtr webView = page->cocoaView();
            RetainPtr window = [webView window];
            if (!canShowOverlayInWindow(window) || [windowsWithOverlay containsObject:window])
                continue;
            if (RefPtr visiblePage = visiblePageInWindow(window)) {
                [windowsWithOverlay addObject:window];
                installOverlay(*visiblePage);
            }
        }
    }

    updateTints();
}

void TabCapturePicker::installOverlay(WebPageProxy& page)
{
    RetainPtr webView = page.cocoaView();
    if (!webView)
        return;
    RetainPtr window = [webView window];

    auto shareBlock = makeBlockPtr([weakThis = WeakPtr { *this }, weakPage = WeakPtr { page }] {
        if (RefPtr protectedThis = weakThis)
            protectedThis->finish(weakPage);
    });
    auto cancelBlock = makeBlockPtr([weakThis = WeakPtr { *this }] {
        if (RefPtr protectedThis = weakThis)
            protectedThis->finish(nullptr);
    });
    auto hoverBlock = makeBlockPtr([weakThis = WeakPtr { *this }, weakWindow = WeakObjCPtr<NSWindow> { window }] {
        if (RefPtr protectedThis = weakThis)
            protectedThis->selectWindow(weakWindow.get());
    });

    RetainPtr overlay = adoptNS([[WKTabCapturePickerOverlay alloc] initWithWebView:webView frame:overlayFrame(page, webView) requestingOriginName:m_requestingOriginName shareBlock:shareBlock.get() cancelBlock:cancelBlock.get() hoverBlock:hoverBlock.get()]);
    [webView addSubview:overlay positioned:NSWindowAbove relativeTo:nil];
    m_overlays.append({ WTF::move(overlay), page });
}

void TabCapturePicker::selectWindow(NSWindow *window)
{
    if (!window)
        return;
    m_selectedWindow = window;
    updateTints();
}

void TabCapturePicker::updateTints()
{
    RetainPtr selectedWindow = m_selectedWindow.get();
    for (auto& overlay : borrow(m_overlays))
        [overlay.view setShowsCaptureTint:[overlay.view window] == selectedWindow];
}

void TabCapturePicker::finish(RefPtr<WebPageProxy>&& pickedPage)
{
    if (!m_callback)
        return;
    auto callback = WTF::move(m_callback);
    stop();
    callback(WTF::move(pickedPage));
}

void TabCapturePicker::stop()
{
    for (auto& overlay : std::exchange(m_overlays, { }))
        [overlay.view invalidate];
    if (m_windowWillCloseObserver) {
        [[NSNotificationCenter defaultCenter] removeObserver:m_windowWillCloseObserver];
        m_windowWillCloseObserver = nullptr;
    }
    m_selectedWindow = nil;
}

} // namespace WebKit

#endif // HAVE(WINDOW_CAPTURE)
