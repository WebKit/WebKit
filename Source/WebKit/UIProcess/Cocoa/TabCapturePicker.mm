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

#if PLATFORM(COCOA) && HAVE(SCREEN_CAPTURE_KIT)

#import "MediaPermissionUtilities.h"
#import "WKWebViewInternal.h"
#import "WKWebViewPrivate.h"
#import "WebPageProxy.h"
#import "WebProcessProxy.h"
#import <WebCore/LocalizedStrings.h>
#import <wtf/BlockPtr.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/RunLoop.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/WeakHashSet.h>

// The prompt uses the same metrics as WKCapturePreviewPromptSheet.
constexpr CGFloat promptInset = 16;
constexpr CGFloat promptTopInset = 20;
constexpr CGFloat promptSpacing = 16;
constexpr CGFloat promptButtonSpacing = 6;
constexpr CGFloat promptMinimumContentWidth = 260;
constexpr CGFloat promptMaximumContentWidth = 420;
constexpr CGFloat promptCornerRadius = 12;

static void pinToWidthOfStack(NSView *view, NSStackView *stack)
{
    // NSStackView does not stretch its views to its own width.
    [NSLayoutConstraint activateConstraints:@[
        [[view leadingAnchor] constraintEqualToAnchor:[stack leadingAnchor]],
        [[view trailingAnchor] constraintEqualToAnchor:[stack trailingAnchor]],
    ]];
}

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
    [[self layer] setBackgroundColor:[NSColor clearColor].CGColor];
    [[self layer] setBorderWidth:0];

    [self addTrackingArea:adoptNS([[NSTrackingArea alloc] initWithRect:NSZeroRect options:NSTrackingMouseEnteredAndExited | NSTrackingCursorUpdate | NSTrackingActiveInActiveApp | NSTrackingInVisibleRect owner:self userInfo:nil]).get()];

    [self _addPromptWithRequestingOriginName:requestingOriginName];

    return self;
}

- (void)_addPromptWithRequestingOriginName:(NSString *)requestingOriginName
{
    RetainPtr buttonStack = adoptNS([[NSStackView alloc] init]);
    [buttonStack setOrientation:NSUserInterfaceLayoutOrientationVertical];
    [buttonStack setDistribution:NSStackViewDistributionFill];
    [buttonStack setSpacing:promptButtonSpacing];
    [buttonStack setTranslatesAutoresizingMaskIntoConstraints:NO];

    CGFloat widestButtonWidth = 0;
    auto addButton = [&](NSString *title, SEL action, NSString *keyEquivalent) {
        RetainPtr button = [WKTabCapturePickerButton buttonWithTitle:title target:self action:action];
        [button setKeyEquivalent:keyEquivalent];
        [button setControlSize:NSControlSizeLarge];
        [button setToolTip:title];
        [button setTranslatesAutoresizingMaskIntoConstraints:NO];
        widestButtonWidth = std::max(widestButtonWidth, [button fittingSize].width);
        [buttonStack addArrangedSubview:button.get()];
        pinToWidthOfStack(button.get(), buttonStack.get());
    };
    addButton(WEB_UI_NSSTRING(@"Share This Tab", "Button title in the tab capture picker to share the selected tab"), @selector(_shareClicked:), @"\r");
    addButton(WEB_UI_NSSTRING(@"Cancel", "Cancel"), @selector(_cancelClicked:), @"\E");

    CGFloat contentWidth = std::clamp(widestButtonWidth, promptMinimumContentWidth, promptMaximumContentWidth);

    RetainPtr title = adoptNS([[NSString alloc] initWithFormat:WEB_UI_NSSTRING(@"Share this tab with “%@”?", "Title of the tab capture picker prompt"), requestingOriginName]);
    RetainPtr titleLabel = [NSTextField wrappingLabelWithString:title.get()];
    [titleLabel setFont:[NSFont boldSystemFontOfSize:[NSFont systemFontSize]]];
    [titleLabel setSelectable:NO];
    [titleLabel setPreferredMaxLayoutWidth:contentWidth];
    [titleLabel setTranslatesAutoresizingMaskIntoConstraints:NO];

    RetainPtr contentStack = adoptNS([[NSStackView alloc] init]);
    [contentStack setOrientation:NSUserInterfaceLayoutOrientationVertical];
    [contentStack setDistribution:NSStackViewDistributionFill];
    [contentStack setSpacing:promptSpacing];
    [contentStack setTranslatesAutoresizingMaskIntoConstraints:NO];
    for (NSView *view in @[ titleLabel.get(), buttonStack.get() ]) {
        [contentStack addArrangedSubview:view];
        pinToWidthOfStack(view, contentStack.get());
    }

    RetainPtr prompt = adoptNS([[NSVisualEffectView alloc] initWithFrame:NSZeroRect]);
    [prompt setMaterial:NSVisualEffectMaterialWindowBackground];
    [prompt setBlendingMode:NSVisualEffectBlendingModeWithinWindow];
    [prompt setState:NSVisualEffectStateActive];
    [prompt setWantsLayer:YES];
    [[prompt layer] setCornerRadius:promptCornerRadius];
    [[prompt layer] setMasksToBounds:YES];
    [prompt setTranslatesAutoresizingMaskIntoConstraints:NO];
    [prompt addSubview:contentStack.get()];
    [self addSubview:prompt.get()];

    [NSLayoutConstraint activateConstraints:@[
        [[contentStack topAnchor] constraintEqualToAnchor:[prompt topAnchor] constant:promptTopInset],
        [[contentStack bottomAnchor] constraintEqualToAnchor:[prompt bottomAnchor] constant:-promptInset],
        [[contentStack leadingAnchor] constraintEqualToAnchor:[prompt leadingAnchor] constant:promptInset],
        [[contentStack trailingAnchor] constraintEqualToAnchor:[prompt trailingAnchor] constant:-promptInset],
        [[contentStack widthAnchor] constraintEqualToConstant:contentWidth],
        [[prompt centerXAnchor] constraintEqualToAnchor:[self centerXAnchor]],
        [[prompt centerYAnchor] constraintEqualToAnchor:[self centerYAnchor]],
    ]];
}

- (void)setShowsCaptureTint:(BOOL)shows
{
    if (_showsCaptureTint == shows)
        return;
    _showsCaptureTint = shows;
    if (shows) {
        [[self layer] setBackgroundColor:[[NSColor systemBlueColor] colorWithAlphaComponent:0.18].CGColor];
        [[self layer] setBorderColor:[[NSColor systemBlueColor] colorWithAlphaComponent:0.9].CGColor];
        [[self layer] setBorderWidth:3];
    } else {
        [[self layer] setBackgroundColor:[NSColor clearColor].CGColor];
        [[self layer] setBorderWidth:0];
    }
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
    auto block = std::exchange(_shareBlock, nullptr);
    _cancelBlock = nullptr;
    if (block)
        block();
}

- (void)_cancelClicked:(id)sender
{
    auto block = std::exchange(_cancelBlock, nullptr);
    _shareBlock = nullptr;
    if (block)
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

static WeakHashSet<TabCapturePicker>& startedPickers()
{
    static NeverDestroyed<WeakHashSet<TabCapturePicker>> pickers;
    return pickers;
}

static bool canShowOverlayInWindow(NSWindow *window)
{
    return window && ([window isVisible] || [window isMiniaturized]);
}

// The page whose WKWebView is visible in the window, or any page of the window if none is.
static RefPtr<WebPageProxy> visiblePageInWindow(NSWindow *window)
{
    if (!window)
        return nullptr;
    RefPtr<WebPageProxy> fallback;
    for (Ref process : WebProcessProxy::allProcesses()) {
        for (Ref page : process->pages()) {
            RetainPtr view = page->cocoaView();
            if (!view || [view window] != window)
                continue;
            if (!fallback)
                fallback = page.ptr();
            if (![view isHidden] && [view superview])
                return page;
        }
    }
    return fallback;
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

void TabCapturePicker::webViewVisibilityDidChange()
{
    for (Ref picker : startedPickers())
        picker->scheduleOverlaysUpdate();
}

void TabCapturePicker::start()
{
    RefPtr requestingPage = m_requestingPage;
    RetainPtr requestingWebView = requestingPage ? requestingPage->cocoaView() : nullptr;
    if (!requestingWebView) {
        finish(nullptr);
        return;
    }

    startedPickers().add(*this);
    m_overlays = adoptNS([[NSMutableArray alloc] init]);
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

RefPtr<WebPageProxy> TabCapturePicker::requestingPage() const
{
    return m_requestingPage.get();
}

void TabCapturePicker::scheduleOverlaysUpdate()
{
    if (!m_callback || m_isOverlaysUpdateScheduled)
        return;

    // Updating asynchronously coalesces the changes of a tab switch, and lets AppKit finish moving views.
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

    // Remove the overlays that are no longer on the visible WKWebView of their window, for instance after a tab switch.
    RetainPtr windowsWithOverlay = adoptNS([[NSMutableSet alloc] init]);
    RetainPtr staleOverlays = adoptNS([[NSMutableArray alloc] init]);
    for (WKTabCapturePickerOverlay *overlay in m_overlays.get()) {
        RetainPtr window = [overlay window];
        RefPtr page = canShowOverlayInWindow(window.get()) ? visiblePageInWindow(window.get()) : nullptr;
        if (!page || [overlay superview] != page->cocoaView().get()) {
            [staleOverlays addObject:overlay];
            continue;
        }
        [windowsWithOverlay addObject:window.get()];
    }
    for (WKTabCapturePickerOverlay *overlay in staleOverlays.get())
        [overlay invalidate];
    [m_overlays removeObjectsInArray:staleOverlays.get()];

    // Add an overlay to the windows that do not have one, for instance new windows.
    for (Ref process : WebProcessProxy::allProcesses()) {
        for (Ref page : process->pages()) {
            RetainPtr webView = page->cocoaView();
            RetainPtr window = [webView window];
            if (!canShowOverlayInWindow(window.get()) || [windowsWithOverlay containsObject:window.get()])
                continue;
            [windowsWithOverlay addObject:window.get()];
            if (RefPtr visiblePage = visiblePageInWindow(window.get()))
                installOverlay(visiblePage->cocoaView().get());
        }
    }

    updateTints();
}

void TabCapturePicker::installOverlay(WKWebView *webView)
{
    RetainPtr window = [webView window];

    // Clip overlay to the intersection of the WKWebView with the window's contentLayoutRect so it
    // doesn't cover Safari's title/tab bar area (Safari uses FullSizeContentView).
    NSRect webViewInWindow = [webView convertRect:[webView bounds] toView:nil];
    NSRect visibleContentInWindow = NSIntersectionRect(webViewInWindow, [window contentLayoutRect]);
    NSRect overlayFrame = [webView convertRect:visibleContentInWindow fromView:nil];

    auto shareBlock = makeBlockPtr([weakThis = WeakPtr { *this }, weakWindow = WeakObjCPtr<NSWindow> { window.get() }] {
        RefPtr protectedThis = weakThis;
        if (!protectedThis)
            return;
        // The picked tab is the one visible in the window when the user clicks.
        RetainPtr window = weakWindow.get();
        protectedThis->finish(window ? visiblePageInWindow(window.get()) : nullptr);
    });
    auto cancelBlock = makeBlockPtr([weakThis = WeakPtr { *this }] {
        if (RefPtr protectedThis = weakThis)
            protectedThis->finish(nullptr);
    });
    auto hoverBlock = makeBlockPtr([weakThis = WeakPtr { *this }, weakWindow = WeakObjCPtr<NSWindow> { window.get() }] {
        if (RefPtr protectedThis = weakThis)
            protectedThis->selectWindow(weakWindow.get().get());
    });

    RetainPtr overlay = adoptNS([[WKTabCapturePickerOverlay alloc] initWithWebView:webView frame:overlayFrame requestingOriginName:m_requestingOriginName.get() shareBlock:shareBlock.get() cancelBlock:cancelBlock.get() hoverBlock:hoverBlock.get()]);
    [webView addSubview:overlay.get() positioned:NSWindowAbove relativeTo:nil];
    [m_overlays addObject:overlay.get()];
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
    // Only the selected window shows the capture tint. The selection stays when the mouse leaves the browser windows.
    RetainPtr selectedWindow = m_selectedWindow.get();
    for (WKTabCapturePickerOverlay *overlay in m_overlays.get())
        [overlay setShowsCaptureTint:[overlay window] == selectedWindow.get()];
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
    startedPickers().remove(*this);
    for (WKTabCapturePickerOverlay *overlay in m_overlays.get())
        [overlay invalidate];
    m_overlays = nullptr;
    if (m_windowWillCloseObserver) {
        [[NSNotificationCenter defaultCenter] removeObserver:m_windowWillCloseObserver.get()];
        m_windowWillCloseObserver = nullptr;
    }
    m_selectedWindow = nil;
}

} // namespace WebKit

#endif // PLATFORM(COCOA) && HAVE(SCREEN_CAPTURE_KIT)
