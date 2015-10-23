/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#import "WebViewImpl.h"

#if PLATFORM(MAC)

#import "ColorSpaceData.h"
#import "GenericCallback.h"
#import "Logging.h"
#import "NativeWebKeyboardEvent.h"
#import "NativeWebMouseEvent.h"
#import "PageClient.h"
#import "WKFullScreenWindowController.h"
#import "WKImmediateActionController.h"
#import "WKViewLayoutStrategy.h"
#import "WKWebView.h"
#import "WebEditCommandProxy.h"
#import "WebPageProxy.h"
#import <HIToolbox/CarbonEventsCore.h>
#import <WebCore/AXObjectCache.h>
#import <WebCore/DataDetectorsSPI.h>
#import <WebCore/DictionaryLookup.h>
#import <WebCore/KeypressCommand.h>
#import <WebCore/LookupSPI.h>
#import <WebCore/NSImmediateActionGestureRecognizerSPI.h>
#import <WebCore/NSWindowSPI.h>
#import <WebCore/SoftLinking.h>
#import <WebCore/ViewState.h>
#import <WebCore/WebCoreFullScreenPlaceholderView.h>
#import <WebCore/WebCoreFullScreenWindow.h>
#import <WebKitSystemInterface.h>

SOFT_LINK_CONSTANT_MAY_FAIL(Lookup, LUNotificationPopoverWillClose, NSString *)

@interface WKWindowVisibilityObserver : NSObject {
    NSView *_view;
    WebKit::WebViewImpl *_impl;
}

- (instancetype)initWithView:(NSView *)view impl:(WebKit::WebViewImpl&)impl;
- (void)startObserving:(NSWindow *)window;
- (void)stopObserving:(NSWindow *)window;
- (void)startObservingFontPanel;
- (void)startObservingLookupDismissal;
@end

@implementation WKWindowVisibilityObserver

- (instancetype)initWithView:(NSView *)view impl:(WebKit::WebViewImpl&)impl
{
    self = [super init];
    if (!self)
        return nil;

    _view = view;
    _impl = &impl;

    return self;
}

- (void)dealloc
{
    if (canLoadLUNotificationPopoverWillClose())
        [[NSNotificationCenter defaultCenter] removeObserver:self name:getLUNotificationPopoverWillClose() object:nil];

    [super dealloc];
}

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101000
static void* keyValueObservingContext = &keyValueObservingContext;
#endif

- (void)startObserving:(NSWindow *)window
{
    if (!window)
        return;

    NSNotificationCenter *defaultNotificationCenter = [NSNotificationCenter defaultCenter];

    // An NSView derived object such as WKView cannot observe these notifications, because NSView itself observes them.
    [defaultNotificationCenter addObserver:self selector:@selector(_windowDidOrderOffScreen:) name:@"NSWindowDidOrderOffScreenNotification" object:window];
    [defaultNotificationCenter addObserver:self selector:@selector(_windowDidOrderOnScreen:) name:@"_NSWindowDidBecomeVisible" object:window];

    [defaultNotificationCenter addObserver:self selector:@selector(_windowDidBecomeKey:) name:NSWindowDidBecomeKeyNotification object:nil];
    [defaultNotificationCenter addObserver:self selector:@selector(_windowDidResignKey:) name:NSWindowDidResignKeyNotification object:nil];
    [defaultNotificationCenter addObserver:self selector:@selector(_windowDidMiniaturize:) name:NSWindowDidMiniaturizeNotification object:window];
    [defaultNotificationCenter addObserver:self selector:@selector(_windowDidDeminiaturize:) name:NSWindowDidDeminiaturizeNotification object:window];
    [defaultNotificationCenter addObserver:self selector:@selector(_windowDidMove:) name:NSWindowDidMoveNotification object:window];
    [defaultNotificationCenter addObserver:self selector:@selector(_windowDidResize:) name:NSWindowDidResizeNotification object:window];
    [defaultNotificationCenter addObserver:self selector:@selector(_windowDidChangeBackingProperties:) name:NSWindowDidChangeBackingPropertiesNotification object:window];
    [defaultNotificationCenter addObserver:self selector:@selector(_windowDidChangeScreen:) name:NSWindowDidChangeScreenNotification object:window];
    [defaultNotificationCenter addObserver:self selector:@selector(_windowDidChangeLayerHosting:) name:@"_NSWindowDidChangeContentsHostedInLayerSurfaceNotification" object:window];
    [defaultNotificationCenter addObserver:self selector:@selector(_windowDidChangeOcclusionState:) name:NSWindowDidChangeOcclusionStateNotification object:window];
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101000
    [window addObserver:self forKeyPath:@"contentLayoutRect" options:NSKeyValueObservingOptionInitial context:keyValueObservingContext];
    [window addObserver:self forKeyPath:@"titlebarAppearsTransparent" options:NSKeyValueObservingOptionInitial context:keyValueObservingContext];
#endif
}

- (void)stopObserving:(NSWindow *)window
{
    if (!window)
        return;

    NSNotificationCenter *defaultNotificationCenter = [NSNotificationCenter defaultCenter];

    [defaultNotificationCenter removeObserver:self name:@"NSWindowDidOrderOffScreenNotification" object:window];
    [defaultNotificationCenter removeObserver:self name:@"_NSWindowDidBecomeVisible" object:window];

    [defaultNotificationCenter removeObserver:self name:NSWindowDidBecomeKeyNotification object:nil];
    [defaultNotificationCenter removeObserver:self name:NSWindowDidResignKeyNotification object:nil];
    [defaultNotificationCenter removeObserver:self name:NSWindowDidMiniaturizeNotification object:window];
    [defaultNotificationCenter removeObserver:self name:NSWindowDidDeminiaturizeNotification object:window];
    [defaultNotificationCenter removeObserver:self name:NSWindowDidMoveNotification object:window];
    [defaultNotificationCenter removeObserver:self name:NSWindowDidResizeNotification object:window];
    [defaultNotificationCenter removeObserver:self name:NSWindowDidChangeBackingPropertiesNotification object:window];
    [defaultNotificationCenter removeObserver:self name:NSWindowDidChangeScreenNotification object:window];
    [defaultNotificationCenter removeObserver:self name:@"_NSWindowDidChangeContentsHostedInLayerSurfaceNotification" object:window];
    [defaultNotificationCenter removeObserver:self name:NSWindowDidChangeOcclusionStateNotification object:window];
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101000
    if (_impl->isEditable())
        [[NSFontPanel sharedFontPanel] removeObserver:self forKeyPath:@"visible" context:keyValueObservingContext];
    [window removeObserver:self forKeyPath:@"contentLayoutRect" context:keyValueObservingContext];
    [window removeObserver:self forKeyPath:@"titlebarAppearsTransparent" context:keyValueObservingContext];
#endif
}

- (void)startObservingFontPanel
{
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101000
    [[NSFontPanel sharedFontPanel] addObserver:self forKeyPath:@"visible" options:0 context:keyValueObservingContext];
#endif
}

- (void)startObservingLookupDismissal
{
    if (canLoadLUNotificationPopoverWillClose())
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_dictionaryLookupPopoverWillClose:) name:getLUNotificationPopoverWillClose() object:nil];
}

- (void)_windowDidOrderOnScreen:(NSNotification *)notification
{
    _impl->windowDidOrderOnScreen();
}

- (void)_windowDidOrderOffScreen:(NSNotification *)notification
{
    _impl->windowDidOrderOffScreen();
}

- (void)_windowDidBecomeKey:(NSNotification *)notification
{
    _impl->windowDidBecomeKey([notification object]);
}

- (void)_windowDidResignKey:(NSNotification *)notification
{
    _impl->windowDidResignKey([notification object]);
}

- (void)_windowDidMiniaturize:(NSNotification *)notification
{
    _impl->windowDidMiniaturize();
}

- (void)_windowDidDeminiaturize:(NSNotification *)notification
{
    _impl->windowDidDeminiaturize();
}

- (void)_windowDidMove:(NSNotification *)notification
{
    _impl->windowDidMove();
}

- (void)_windowDidResize:(NSNotification *)notification
{
    _impl->windowDidResize();
}

- (void)_windowDidChangeBackingProperties:(NSNotification *)notification
{
    CGFloat oldBackingScaleFactor = [[notification.userInfo objectForKey:NSBackingPropertyOldScaleFactorKey] doubleValue];
    _impl->windowDidChangeBackingProperties(oldBackingScaleFactor);
}

- (void)_windowDidChangeScreen:(NSNotification *)notification
{
    _impl->windowDidChangeScreen();
}

- (void)_windowDidChangeLayerHosting:(NSNotification *)notification
{
    _impl->windowDidChangeLayerHosting();
}

- (void)_windowDidChangeOcclusionState:(NSNotification *)notification
{
    _impl->windowDidChangeOcclusionState();
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101000
    if (context != keyValueObservingContext) {
        [super observeValueForKeyPath:keyPath ofObject:object change:change context:context];
        return;
    }

    if ([keyPath isEqualToString:@"visible"] && [NSFontPanel sharedFontPanelExists] && object == [NSFontPanel sharedFontPanel]) {
        _impl->updateFontPanelIfNeeded();
        return;
    }
    if ([keyPath isEqualToString:@"contentLayoutRect"] || [keyPath isEqualToString:@"titlebarAppearsTransparent"])
        _impl->updateContentInsetsIfAutomatic();
#endif
}

- (void)_dictionaryLookupPopoverWillClose:(NSNotification *)notification
{
    _impl->clearTextIndicatorWithAnimation(WebCore::TextIndicatorWindowDismissalAnimation::None);
}

@end

@interface WKEditCommandObjC : NSObject {
    RefPtr<WebKit::WebEditCommandProxy> m_command;
}
- (id)initWithWebEditCommandProxy:(RefPtr<WebKit::WebEditCommandProxy>)command;
- (WebKit::WebEditCommandProxy*)command;
@end

@interface WKEditorUndoTargetObjC : NSObject
- (void)undoEditing:(id)sender;
- (void)redoEditing:(id)sender;
@end

@implementation WKEditCommandObjC

- (id)initWithWebEditCommandProxy:(RefPtr<WebKit::WebEditCommandProxy>)command
{
    self = [super init];
    if (!self)
        return nil;

    m_command = command;
    return self;
}

- (WebKit::WebEditCommandProxy*)command
{
    return m_command.get();
}

@end

@implementation WKEditorUndoTargetObjC

- (void)undoEditing:(id)sender
{
    ASSERT([sender isKindOfClass:[WKEditCommandObjC class]]);
    [sender command]->unapply();
}

- (void)redoEditing:(id)sender
{
    ASSERT([sender isKindOfClass:[WKEditCommandObjC class]]);
    [sender command]->reapply();
}

@end

namespace WebKit {

WebViewImpl::WebViewImpl(NSView <WebViewImplDelegate> *view, WebPageProxy& page, PageClient& pageClient)
    : m_view(view)
    , m_page(page)
    , m_pageClient(pageClient)
    , m_weakPtrFactory(this)
    , m_needsViewFrameInWindowCoordinates(m_page.preferences().pluginsEnabled())
    , m_intrinsicContentSize(CGSizeMake(NSViewNoInstrinsicMetric, NSViewNoInstrinsicMetric))
    , m_layoutStrategy([WKViewLayoutStrategy layoutStrategyWithPage:m_page view:m_view viewImpl:*this mode:kWKLayoutModeViewSize])
    , m_undoTarget(adoptNS([[WKEditorUndoTargetObjC alloc] init]))
    , m_windowVisibilityObserver(adoptNS([[WKWindowVisibilityObserver alloc] initWithView:view impl:*this]))
{
    m_page.setIntrinsicDeviceScaleFactor(intrinsicDeviceScaleFactor());

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101000
    if (Class gestureClass = NSClassFromString(@"NSImmediateActionGestureRecognizer")) {
        m_immediateActionGestureRecognizer = adoptNS([(NSImmediateActionGestureRecognizer *)[gestureClass alloc] init]);
        m_immediateActionController = adoptNS([[WKImmediateActionController alloc] initWithPage:m_page view:m_view viewImpl:*this recognizer:m_immediateActionGestureRecognizer.get()]);
        [m_immediateActionGestureRecognizer setDelegate:m_immediateActionController.get()];
        [m_immediateActionGestureRecognizer setDelaysPrimaryMouseButtonEvents:NO];
    }
#endif
}

WebViewImpl::~WebViewImpl()
{
    ASSERT(!m_inSecureInputState);

    [m_layoutStrategy invalidate];

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101000
    [m_immediateActionController willDestroyView:m_view];
#endif
}

void WebViewImpl::setDrawsBackground(bool drawsBackground)
{
    m_page.setDrawsBackground(drawsBackground);
}

bool WebViewImpl::drawsBackground() const
{
    return m_page.drawsBackground();
}

void WebViewImpl::setDrawsTransparentBackground(bool drawsTransparentBackground)
{
    m_page.setDrawsTransparentBackground(drawsTransparentBackground);
}

bool WebViewImpl::drawsTransparentBackground() const
{
    return m_page.drawsTransparentBackground();
}

bool WebViewImpl::acceptsFirstResponder()
{
    return true;
}

bool WebViewImpl::becomeFirstResponder()
{
    // If we just became first responder again, there is no need to do anything,
    // since resignFirstResponder has correctly detected this situation.
    if (m_willBecomeFirstResponderAgain) {
        m_willBecomeFirstResponderAgain = false;
        return true;
    }

    NSSelectionDirection direction = [[m_view window] keyViewSelectionDirection];

    m_inBecomeFirstResponder = true;

    updateSecureInputState();
    m_page.viewStateDidChange(WebCore::ViewState::IsFocused);
    // Restore the selection in the editable region if resigning first responder cleared selection.
    m_page.restoreSelectionInFocusedEditableElement();

    m_inBecomeFirstResponder = false;

    if (direction != NSDirectSelection) {
        NSEvent *event = [NSApp currentEvent];
        NSEvent *keyboardEvent = nil;
        if ([event type] == NSKeyDown || [event type] == NSKeyUp)
            keyboardEvent = event;
        m_page.setInitialFocus(direction == NSSelectingNext, keyboardEvent != nil, NativeWebKeyboardEvent(keyboardEvent, false, Vector<WebCore::KeypressCommand>()), [](WebKit::CallbackBase::Error) { });
    }
    return true;
}

bool WebViewImpl::resignFirstResponder()
{
#if WK_API_ENABLED
    // Predict the case where we are losing first responder status only to
    // gain it back again. We want resignFirstResponder to do nothing in that case.
    id nextResponder = [[m_view window] _newFirstResponderAfterResigning];

    // FIXME: This will probably need to change once WKWebView doesn't contain a WKView.
    if ([nextResponder isKindOfClass:[WKWebView class]] && m_view.superview == nextResponder) {
        m_willBecomeFirstResponderAgain = true;
        return true;
    }
#endif

    m_willBecomeFirstResponderAgain = false;
    m_inResignFirstResponder = true;

#if USE(ASYNC_NSTEXTINPUTCLIENT)
    m_page.confirmCompositionAsync();
#else
    if (m_page.editorState().hasComposition && !m_page.editorState().shouldIgnoreCompositionSelectionChange)
        m_page.cancelComposition();
#endif

    notifyInputContextAboutDiscardedComposition();

    resetSecureInputState();

    if (!m_page.maintainsInactiveSelection())
        m_page.clearSelection();

    m_page.viewStateDidChange(WebCore::ViewState::IsFocused);
    
    m_inResignFirstResponder = false;
    
    return true;
}

bool WebViewImpl::isFocused() const
{
    if (m_inBecomeFirstResponder)
        return true;
    if (m_inResignFirstResponder)
        return false;
    return m_view.window.firstResponder == m_view;
}

void WebViewImpl::viewWillStartLiveResize()
{
    m_page.viewWillStartLiveResize();

    [m_layoutStrategy willStartLiveResize];
}

void WebViewImpl::viewDidEndLiveResize()
{
    m_page.viewWillEndLiveResize();

    [m_layoutStrategy didEndLiveResize];
}

void WebViewImpl::renewGState()
{
    if (m_textIndicatorWindow)
        dismissContentRelativeChildWindowsWithAnimation(false);

    // Update the view frame.
    if (m_view.window)
        updateWindowAndViewFrames();

    updateContentInsetsIfAutomatic();
}

void WebViewImpl::setFrameSize(CGSize)
{
    [m_layoutStrategy didChangeFrameSize];
}

void WebViewImpl::disableFrameSizeUpdates()
{
    [m_layoutStrategy disableFrameSizeUpdates];
}

void WebViewImpl::enableFrameSizeUpdates()
{
    [m_layoutStrategy enableFrameSizeUpdates];
}

bool WebViewImpl::frameSizeUpdatesDisabled() const
{
    return [m_layoutStrategy frameSizeUpdatesDisabled];
}

void WebViewImpl::setFrameAndScrollBy(CGRect frame, CGSize offset)
{
    ASSERT(CGSizeEqualToSize(m_resizeScrollOffset, CGSizeZero));

    m_resizeScrollOffset = offset;
    m_view.frame = NSRectFromCGRect(frame);
}

void WebViewImpl::updateWindowAndViewFrames()
{
    if (clipsToVisibleRect())
        updateViewExposedRect();

    if (m_didScheduleWindowAndViewFrameUpdate)
        return;

    m_didScheduleWindowAndViewFrameUpdate = true;

    auto weakThis = createWeakPtr();
    dispatch_async(dispatch_get_main_queue(), [weakThis] {
        if (!weakThis)
            return;

        weakThis->m_didScheduleWindowAndViewFrameUpdate = false;

        NSRect viewFrameInWindowCoordinates = NSZeroRect;
        NSPoint accessibilityPosition = NSZeroPoint;

        if (weakThis->m_needsViewFrameInWindowCoordinates)
            viewFrameInWindowCoordinates = [weakThis->m_view convertRect:weakThis->m_view.frame toView:nil];

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        if (WebCore::AXObjectCache::accessibilityEnabled())
            accessibilityPosition = [[weakThis->m_view accessibilityAttributeValue:NSAccessibilityPositionAttribute] pointValue];
#pragma clang diagnostic pop
        
        weakThis->m_page.windowAndViewFramesChanged(viewFrameInWindowCoordinates, accessibilityPosition);
    });
}

void WebViewImpl::setFixedLayoutSize(CGSize fixedLayoutSize)
{
    m_page.setFixedLayoutSize(WebCore::expandedIntSize(WebCore::FloatSize(fixedLayoutSize)));
}

CGSize WebViewImpl::fixedLayoutSize() const
{
    return m_page.fixedLayoutSize();
}

void WebViewImpl::setDrawingAreaSize(CGSize size)
{
    if (!m_page.drawingArea())
        return;

    m_page.drawingArea()->setSize(WebCore::IntSize(size), WebCore::IntSize(), WebCore::IntSize(m_resizeScrollOffset));
    m_resizeScrollOffset = CGSizeZero;
}

void WebViewImpl::setAutomaticallyAdjustsContentInsets(bool automaticallyAdjustsContentInsets)
{
    m_automaticallyAdjustsContentInsets = automaticallyAdjustsContentInsets;
    updateContentInsetsIfAutomatic();
}

void WebViewImpl::updateContentInsetsIfAutomatic()
{
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101000
    if (!m_automaticallyAdjustsContentInsets)
        return;

    NSWindow *window = m_view.window;
    if ((window.styleMask & NSFullSizeContentViewWindowMask) && !window.titlebarAppearsTransparent && ![m_view enclosingScrollView]) {
        NSRect contentLayoutRect = [m_view convertRect:window.contentLayoutRect fromView:nil];
        CGFloat newTopContentInset = NSMaxY(contentLayoutRect) - NSHeight(contentLayoutRect);
        if (m_topContentInset != newTopContentInset)
            setTopContentInset(newTopContentInset);
    } else
        setTopContentInset(0);
#endif
}

void WebViewImpl::setTopContentInset(CGFloat contentInset)
{
    m_topContentInset = contentInset;

    if (m_didScheduleSetTopContentInset)
        return;

    m_didScheduleSetTopContentInset = true;

    auto weakThis = createWeakPtr();
    dispatch_async(dispatch_get_main_queue(), [weakThis] {
        if (!weakThis)
            return;
        weakThis->dispatchSetTopContentInset();
    });
}

void WebViewImpl::dispatchSetTopContentInset()
{
    if (!m_didScheduleSetTopContentInset)
        return;

    m_didScheduleSetTopContentInset = false;
    m_page.setTopContentInset(m_topContentInset);
}

void WebViewImpl::setContentPreparationRect(CGRect rect)
{
    m_contentPreparationRect = rect;
    m_useContentPreparationRectForVisibleRect = true;
}

void WebViewImpl::updateViewExposedRect()
{
    CGRect exposedRect = NSRectToCGRect([m_view visibleRect]);

    if (m_useContentPreparationRectForVisibleRect)
        exposedRect = CGRectUnion(m_contentPreparationRect, exposedRect);

    if (auto drawingArea = m_page.drawingArea())
        drawingArea->setExposedRect(m_clipsToVisibleRect ? WebCore::FloatRect(exposedRect) : WebCore::FloatRect::infiniteRect());
}

void WebViewImpl::setClipsToVisibleRect(bool clipsToVisibleRect)
{
    m_clipsToVisibleRect = clipsToVisibleRect;
    updateViewExposedRect();
}

void WebViewImpl::setIntrinsicContentSize(CGSize intrinsicContentSize)
{
    // If the intrinsic content size is less than the minimum layout width, the content flowed to fit,
    // so we can report that that dimension is flexible. If not, we need to report our intrinsic width
    // so that autolayout will know to provide space for us.

    CGSize intrinsicContentSizeAcknowledgingFlexibleWidth = intrinsicContentSize;
    if (intrinsicContentSize.width < m_page.minimumLayoutSize().width())
        intrinsicContentSizeAcknowledgingFlexibleWidth.width = NSViewNoInstrinsicMetric;

    m_intrinsicContentSize = intrinsicContentSizeAcknowledgingFlexibleWidth;
    [m_view invalidateIntrinsicContentSize];
}

CGSize WebViewImpl::intrinsicContentSize() const
{
    return m_intrinsicContentSize;
}

void WebViewImpl::setViewScale(CGFloat viewScale)
{
    m_lastRequestedViewScale = viewScale;

    if (!supportsArbitraryLayoutModes() && viewScale != 1)
        return;

    if (viewScale <= 0 || isnan(viewScale) || isinf(viewScale))
        [NSException raise:NSInvalidArgumentException format:@"View scale should be a positive number"];

    m_page.scaleView(viewScale);
    [m_layoutStrategy didChangeViewScale];
}

CGFloat WebViewImpl::viewScale() const
{
    return m_page.viewScaleFactor();
}

WKLayoutMode WebViewImpl::layoutMode() const
{
    return [m_layoutStrategy layoutMode];
}

void WebViewImpl::setLayoutMode(WKLayoutMode layoutMode)
{
    m_lastRequestedLayoutMode = layoutMode;

    if (!supportsArbitraryLayoutModes() && layoutMode != kWKLayoutModeViewSize)
        return;

    if (layoutMode == [m_layoutStrategy layoutMode])
        return;

    [m_layoutStrategy willChangeLayoutStrategy];
    m_layoutStrategy = [WKViewLayoutStrategy layoutStrategyWithPage:m_page view:m_view viewImpl:*this mode:layoutMode];
}

bool WebViewImpl::supportsArbitraryLayoutModes() const
{
    if ([m_fullScreenWindowController isFullScreen])
        return false;

    WebFrameProxy* frame = m_page.mainFrame();
    if (!frame)
        return true;

    // If we have a plugin document in the main frame, avoid using custom WKLayoutModes
    // and fall back to the defaults, because there's a good chance that it won't work (e.g. with PDFPlugin).
    if (frame->containsPluginDocument())
        return false;

    return true;
}

void WebViewImpl::updateSupportsArbitraryLayoutModes()
{
    if (!supportsArbitraryLayoutModes()) {
        WKLayoutMode oldRequestedLayoutMode = m_lastRequestedLayoutMode;
        CGFloat oldRequestedViewScale = m_lastRequestedViewScale;
        setViewScale(1);
        setLayoutMode(kWKLayoutModeViewSize);

        // The 'last requested' parameters will have been overwritten by setting them above, but we don't
        // want this to count as a request (only changes from the client count), so reset them.
        m_lastRequestedLayoutMode = oldRequestedLayoutMode;
        m_lastRequestedViewScale = oldRequestedViewScale;
    } else if (m_lastRequestedLayoutMode != [m_layoutStrategy layoutMode]) {
        setViewScale(m_lastRequestedViewScale);
        setLayoutMode(m_lastRequestedLayoutMode);
    }
}

void WebViewImpl::setOverrideDeviceScaleFactor(CGFloat deviceScaleFactor)
{
    m_overrideDeviceScaleFactor = deviceScaleFactor;
    m_page.setIntrinsicDeviceScaleFactor(intrinsicDeviceScaleFactor());
}

float WebViewImpl::intrinsicDeviceScaleFactor() const
{
    if (m_overrideDeviceScaleFactor)
        return m_overrideDeviceScaleFactor;
    if (m_targetWindowForMovePreparation)
        return m_targetWindowForMovePreparation.backingScaleFactor;
    if (NSWindow *window = m_view.window)
        return window.backingScaleFactor;
    return [NSScreen mainScreen].backingScaleFactor;
}

void WebViewImpl::windowDidOrderOffScreen()
{
    m_page.viewStateDidChange(WebCore::ViewState::IsVisible | WebCore::ViewState::WindowIsActive);
}

void WebViewImpl::windowDidOrderOnScreen()
{
    m_page.viewStateDidChange(WebCore::ViewState::IsVisible | WebCore::ViewState::WindowIsActive);
}

void WebViewImpl::windowDidBecomeKey(NSWindow *keyWindow)
{
    if (keyWindow == m_view.window || keyWindow == m_view.window.attachedSheet) {
        updateSecureInputState();
        m_page.viewStateDidChange(WebCore::ViewState::WindowIsActive);
    }
}

void WebViewImpl::windowDidResignKey(NSWindow *formerKeyWindow)
{
    if (formerKeyWindow == m_view.window || formerKeyWindow == m_view.window.attachedSheet) {
        updateSecureInputState();
        m_page.viewStateDidChange(WebCore::ViewState::WindowIsActive);
    }
}

void WebViewImpl::windowDidMiniaturize()
{
    m_page.viewStateDidChange(WebCore::ViewState::IsVisible);
}

void WebViewImpl::windowDidDeminiaturize()
{
    m_page.viewStateDidChange(WebCore::ViewState::IsVisible);
}

void WebViewImpl::windowDidMove()
{
    updateWindowAndViewFrames();
}

void WebViewImpl::windowDidResize()
{
    updateWindowAndViewFrames();
}

void WebViewImpl::windowDidChangeBackingProperties(CGFloat oldBackingScaleFactor)
{
    CGFloat newBackingScaleFactor = intrinsicDeviceScaleFactor();
    if (oldBackingScaleFactor == newBackingScaleFactor)
        return;

    m_page.setIntrinsicDeviceScaleFactor(newBackingScaleFactor);
}

void WebViewImpl::windowDidChangeScreen()
{
    NSWindow *window = m_targetWindowForMovePreparation ? m_targetWindowForMovePreparation : m_view.window;
    m_page.windowScreenDidChange((PlatformDisplayID)[[[[window screen] deviceDescription] objectForKey:@"NSScreenNumber"] intValue]);
}

void WebViewImpl::windowDidChangeLayerHosting()
{
    m_page.layerHostingModeDidChange();
}

void WebViewImpl::windowDidChangeOcclusionState()
{
    m_page.viewStateDidChange(WebCore::ViewState::IsVisible);
}

void WebViewImpl::viewWillMoveToWindow(NSWindow *window)
{
    // If we're in the middle of preparing to move to a window, we should only be moved to that window.
    ASSERT(!m_targetWindowForMovePreparation || (m_targetWindowForMovePreparation == window));

    NSWindow *currentWindow = m_view.window;
    if (window == currentWindow)
        return;

    clearAllEditCommands();

    NSWindow *stopObservingWindow = m_targetWindowForMovePreparation ? m_targetWindowForMovePreparation : m_view.window;
    [m_windowVisibilityObserver stopObserving:stopObservingWindow];
    [m_windowVisibilityObserver startObserving:window];
}

void WebViewImpl::viewDidMoveToWindow()
{
    NSWindow *window = m_targetWindowForMovePreparation ? m_targetWindowForMovePreparation : m_view.window;

    if (window) {
        windowDidChangeScreen();

        WebCore::ViewState::Flags viewStateChanges = WebCore::ViewState::WindowIsActive | WebCore::ViewState::IsVisible;
        if (m_isDeferringViewInWindowChanges)
            m_viewInWindowChangeWasDeferred = true;
        else
            viewStateChanges |= WebCore::ViewState::IsInWindow;
        m_page.viewStateDidChange(viewStateChanges);

        updateWindowAndViewFrames();

        // FIXME(135509) This call becomes unnecessary once 135509 is fixed; remove.
        m_page.layerHostingModeDidChange();

        if (!m_flagsChangedEventMonitor) {
            auto weakThis = createWeakPtr();
            m_flagsChangedEventMonitor = [NSEvent addLocalMonitorForEventsMatchingMask:NSFlagsChangedMask handler:[weakThis] (NSEvent *flagsChangedEvent) {
                if (weakThis)
                    weakThis->postFakeMouseMovedEventForFlagsChangedEvent(flagsChangedEvent);
                return flagsChangedEvent;
            }];
        }

        accessibilityRegisterUIProcessTokens();

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101000
        if (m_immediateActionGestureRecognizer && ![[m_view gestureRecognizers] containsObject:m_immediateActionGestureRecognizer.get()] && !m_ignoresNonWheelEvents && m_allowsLinkPreview)
            [m_view addGestureRecognizer:m_immediateActionGestureRecognizer.get()];
#endif
    } else {
        WebCore::ViewState::Flags viewStateChanges = WebCore::ViewState::WindowIsActive | WebCore::ViewState::IsVisible;
        if (m_isDeferringViewInWindowChanges)
            m_viewInWindowChangeWasDeferred = true;
        else
            viewStateChanges |= WebCore::ViewState::IsInWindow;
        m_page.viewStateDidChange(viewStateChanges);

        [NSEvent removeMonitor:m_flagsChangedEventMonitor];
        m_flagsChangedEventMonitor = nil;

        dismissContentRelativeChildWindowsWithAnimation(false);

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101000
        if (m_immediateActionGestureRecognizer)
            [m_view removeGestureRecognizer:m_immediateActionGestureRecognizer.get()];
#endif
    }

    m_page.setIntrinsicDeviceScaleFactor(intrinsicDeviceScaleFactor());
}

void WebViewImpl::viewDidChangeBackingProperties()
{
    NSColorSpace *colorSpace = m_view.window.colorSpace;
    if ([colorSpace isEqualTo:m_colorSpace.get()])
        return;

    m_colorSpace = nullptr;
    if (DrawingAreaProxy *drawingArea = m_page.drawingArea())
        drawingArea->colorSpaceDidChange();
}

void WebViewImpl::postFakeMouseMovedEventForFlagsChangedEvent(NSEvent *flagsChangedEvent)
{
    NSEvent *fakeEvent = [NSEvent mouseEventWithType:NSMouseMoved location:flagsChangedEvent.window.mouseLocationOutsideOfEventStream
        modifierFlags:flagsChangedEvent.modifierFlags timestamp:flagsChangedEvent.timestamp windowNumber:flagsChangedEvent.windowNumber
        context:flagsChangedEvent.context eventNumber:0 clickCount:0 pressure:0];
    NativeWebMouseEvent webEvent(fakeEvent, m_lastPressureEvent.get(), m_view);
    m_page.handleMouseEvent(webEvent);
}

ColorSpaceData WebViewImpl::colorSpace()
{
    if (!m_colorSpace) {
        if (m_targetWindowForMovePreparation)
            m_colorSpace = m_targetWindowForMovePreparation.colorSpace;
        else if (NSWindow *window = m_view.window)
            m_colorSpace = window.colorSpace;
        else
            m_colorSpace = [NSScreen mainScreen].colorSpace;
    }

    ColorSpaceData colorSpaceData;
    colorSpaceData.cgColorSpace = [m_colorSpace CGColorSpace];
    
    return colorSpaceData;
}

void WebViewImpl::beginDeferringViewInWindowChanges()
{
    if (m_shouldDeferViewInWindowChanges) {
        NSLog(@"beginDeferringViewInWindowChanges was called while already deferring view-in-window changes!");
        return;
    }

    m_shouldDeferViewInWindowChanges = true;
}

void WebViewImpl::endDeferringViewInWindowChanges()
{
    if (!m_shouldDeferViewInWindowChanges) {
        NSLog(@"endDeferringViewInWindowChanges was called without beginDeferringViewInWindowChanges!");
        return;
    }

    m_shouldDeferViewInWindowChanges = false;

    if (m_viewInWindowChangeWasDeferred) {
        dispatchSetTopContentInset();
        m_page.viewStateDidChange(WebCore::ViewState::IsInWindow);
        m_viewInWindowChangeWasDeferred = false;
    }
}

void WebViewImpl::endDeferringViewInWindowChangesSync()
{
    if (!m_shouldDeferViewInWindowChanges) {
        NSLog(@"endDeferringViewInWindowChangesSync was called without beginDeferringViewInWindowChanges!");
        return;
    }

    m_shouldDeferViewInWindowChanges = false;

    if (m_viewInWindowChangeWasDeferred) {
        dispatchSetTopContentInset();
        m_page.viewStateDidChange(WebCore::ViewState::IsInWindow);
        m_viewInWindowChangeWasDeferred = false;
    }
}

void WebViewImpl::prepareForMoveToWindow(NSWindow *targetWindow, std::function<void()> completionHandler)
{
    m_shouldDeferViewInWindowChanges = true;
    viewWillMoveToWindow(targetWindow);
    m_targetWindowForMovePreparation = targetWindow;
    viewDidMoveToWindow();

    m_shouldDeferViewInWindowChanges = false;

    auto weakThis = createWeakPtr();
    m_page.installViewStateChangeCompletionHandler([weakThis, completionHandler]() {
        completionHandler();

        if (!weakThis)
            return;

        ASSERT(weakThis->m_view.window == weakThis->m_targetWindowForMovePreparation);
        weakThis->m_targetWindowForMovePreparation = nil;
    });

    dispatchSetTopContentInset();
    m_page.viewStateDidChange(WebCore::ViewState::IsInWindow, false, WebPageProxy::ViewStateChangeDispatchMode::Immediate);
    m_viewInWindowChangeWasDeferred = false;
}

void WebViewImpl::updateSecureInputState()
{
    if (![[m_view window] isKeyWindow] || !isFocused()) {
        if (m_inSecureInputState) {
            DisableSecureEventInput();
            m_inSecureInputState = false;
        }
        return;
    }
    // WKView has a single input context for all editable areas (except for plug-ins).
    NSTextInputContext *context = [m_view _superInputContext];
    bool isInPasswordField = m_page.editorState().isInPasswordField;

    if (isInPasswordField) {
        if (!m_inSecureInputState)
            EnableSecureEventInput();
        static NSArray *romanInputSources = [[NSArray alloc] initWithObjects:&NSAllRomanInputSourcesLocaleIdentifier count:1];
        LOG(TextInput, "-> setAllowedInputSourceLocales:romanInputSources");
        [context setAllowedInputSourceLocales:romanInputSources];
    } else {
        if (m_inSecureInputState)
            DisableSecureEventInput();
        LOG(TextInput, "-> setAllowedInputSourceLocales:nil");
        [context setAllowedInputSourceLocales:nil];
    }
    m_inSecureInputState = isInPasswordField;
}

void WebViewImpl::resetSecureInputState()
{
    if (m_inSecureInputState) {
        DisableSecureEventInput();
        m_inSecureInputState = false;
    }
}

void WebViewImpl::notifyInputContextAboutDiscardedComposition()
{
    // <rdar://problem/9359055>: -discardMarkedText can only be called for active contexts.
    // FIXME: We fail to ever notify the input context if something (e.g. a navigation) happens while the window is not key.
    // This is not a problem when the window is key, because we discard marked text on resigning first responder.
    if (![[m_view window] isKeyWindow] || m_view != [[m_view window] firstResponder])
        return;

    LOG(TextInput, "-> discardMarkedText");

    [[m_view _superInputContext] discardMarkedText]; // Inform the input method that we won't have an inline input area despite having been asked to.
}

void WebViewImpl::pressureChangeWithEvent(NSEvent *event)
{
#if defined(__LP64__) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 101003
    if (event == m_lastPressureEvent)
        return;

    if (m_ignoresNonWheelEvents)
        return;

    if (event.phase != NSEventPhaseChanged && event.phase != NSEventPhaseBegan && event.phase != NSEventPhaseEnded)
        return;

    NativeWebMouseEvent webEvent(event, m_lastPressureEvent.get(), m_view);
    m_page.handleMouseEvent(webEvent);

    m_lastPressureEvent = event;
#endif
}

#if ENABLE(FULLSCREEN_API)
bool WebViewImpl::hasFullScreenWindowController() const
{
    return !!m_fullScreenWindowController;
}

WKFullScreenWindowController *WebViewImpl::fullScreenWindowController()
{
    if (!m_fullScreenWindowController)
        m_fullScreenWindowController = adoptNS([[WKFullScreenWindowController alloc] initWithWindow:createFullScreenWindow() webView:m_view page:m_page]);

    return m_fullScreenWindowController.get();
}

void WebViewImpl::closeFullScreenWindowController()
{
    if (!m_fullScreenWindowController)
        return;

    [m_fullScreenWindowController close];
    m_fullScreenWindowController = nullptr;
}
#endif

NSView *WebViewImpl::fullScreenPlaceholderView()
{
#if ENABLE(FULLSCREEN_API)
    if (m_fullScreenWindowController && [m_fullScreenWindowController isFullScreen])
        return [m_fullScreenWindowController webViewPlaceholder];
#endif
    return nil;
}

NSWindow *WebViewImpl::createFullScreenWindow()
{
#if ENABLE(FULLSCREEN_API)
    return [[[WebCoreFullScreenWindow alloc] initWithContentRect:[[NSScreen mainScreen] frame] styleMask:(NSBorderlessWindowMask | NSResizableWindowMask) backing:NSBackingStoreBuffered defer:NO] autorelease];
#else
    return nil;
#endif
}

bool WebViewImpl::isEditable() const
{
    return m_page.isEditable();
}

void WebViewImpl::executeEditCommand(const String& commandName, const String& argument)
{
    m_page.executeEditCommand(commandName, argument);
}

void WebViewImpl::registerEditCommand(RefPtr<WebEditCommandProxy> prpCommand, WebPageProxy::UndoOrRedo undoOrRedo)
{
    RefPtr<WebEditCommandProxy> command = prpCommand;

    RetainPtr<WKEditCommandObjC> commandObjC = adoptNS([[WKEditCommandObjC alloc] initWithWebEditCommandProxy:command]);
    String actionName = WebEditCommandProxy::nameForEditAction(command->editAction());

    NSUndoManager *undoManager = [m_view undoManager];
    [undoManager registerUndoWithTarget:m_undoTarget.get() selector:((undoOrRedo == WebPageProxy::Undo) ? @selector(undoEditing:) : @selector(redoEditing:)) object:commandObjC.get()];
    if (!actionName.isEmpty())
        [undoManager setActionName:(NSString *)actionName];
}

void WebViewImpl::clearAllEditCommands()
{
    [[m_view undoManager] removeAllActionsWithTarget:m_undoTarget.get()];
}

bool WebViewImpl::writeSelectionToPasteboard(NSPasteboard *pasteboard, NSArray *types)
{
    size_t numTypes = types.count;
    [pasteboard declareTypes:types owner:nil];
    for (size_t i = 0; i < numTypes; ++i) {
        if ([[types objectAtIndex:i] isEqualTo:NSStringPboardType])
            [pasteboard setString:m_page.stringSelectionForPasteboard() forType:NSStringPboardType];
        else {
            RefPtr<WebCore::SharedBuffer> buffer = m_page.dataSelectionForPasteboard([types objectAtIndex:i]);
            [pasteboard setData:buffer ? buffer->createNSData().get() : nil forType:[types objectAtIndex:i]];
        }
    }
    return true;
}

void WebViewImpl::centerSelectionInVisibleArea()
{
    m_page.centerSelectionInVisibleArea();
}

void WebViewImpl::selectionDidChange()
{
    updateFontPanelIfNeeded();
}

void WebViewImpl::startObservingFontPanel()
{
    [m_windowVisibilityObserver startObservingFontPanel];
}

void WebViewImpl::updateFontPanelIfNeeded()
{
    const EditorState& editorState = m_page.editorState();
    if (editorState.selectionIsNone || !editorState.isContentEditable)
        return;
    if ([NSFontPanel sharedFontPanelExists] && [[NSFontPanel sharedFontPanel] isVisible]) {
        m_page.fontAtSelection([](const String& fontName, double fontSize, bool selectionHasMultipleFonts, WebKit::CallbackBase::Error error) {
            NSFont *font = [NSFont fontWithName:fontName size:fontSize];
            if (font)
                [[NSFontManager sharedFontManager] setSelectedFont:font isMultiple:selectionHasMultipleFonts];
        });
    }
}

void WebViewImpl::preferencesDidChange()
{
    BOOL needsViewFrameInWindowCoordinates = m_page.preferences().pluginsEnabled();

    if (!!needsViewFrameInWindowCoordinates == !!m_needsViewFrameInWindowCoordinates)
        return;

    m_needsViewFrameInWindowCoordinates = needsViewFrameInWindowCoordinates;
    if (m_view.window)
        updateWindowAndViewFrames();
}

void WebViewImpl::setTextIndicator(WebCore::TextIndicator& textIndicator, WebCore::TextIndicatorWindowLifetime lifetime)
{
    if (!m_textIndicatorWindow)
        m_textIndicatorWindow = std::make_unique<WebCore::TextIndicatorWindow>(m_view);

    NSRect textBoundingRectInScreenCoordinates = [m_view.window convertRectToScreen:[m_view convertRect:textIndicator.textBoundingRectInRootViewCoordinates() toView:nil]];
    m_textIndicatorWindow->setTextIndicator(textIndicator, NSRectToCGRect(textBoundingRectInScreenCoordinates), lifetime);
}

void WebViewImpl::clearTextIndicatorWithAnimation(WebCore::TextIndicatorWindowDismissalAnimation animation)
{
    if (m_textIndicatorWindow)
        m_textIndicatorWindow->clearTextIndicator(animation);
    m_textIndicatorWindow = nullptr;
}

void WebViewImpl::setTextIndicatorAnimationProgress(float progress)
{
    if (m_textIndicatorWindow)
        m_textIndicatorWindow->setAnimationProgress(progress);
}

void WebViewImpl::dismissContentRelativeChildWindowsWithAnimation(bool animate)
{
    [m_view _dismissContentRelativeChildWindowsWithAnimation:animate];
}

void WebViewImpl::dismissContentRelativeChildWindowsWithAnimationFromViewOnly(bool animate)
{
    // Calling _clearTextIndicatorWithAnimation here will win out over the animated clear in dismissContentRelativeChildWindowsFromViewOnly.
    // We can't invert these because clients can override (and have overridden) _dismissContentRelativeChildWindows, so it needs to be called.
    // For this same reason, this can't be moved to WebViewImpl without care.
    clearTextIndicatorWithAnimation(animate ? WebCore::TextIndicatorWindowDismissalAnimation::FadeOut : WebCore::TextIndicatorWindowDismissalAnimation::None);
    [m_view _dismissContentRelativeChildWindows];
}

void WebViewImpl::dismissContentRelativeChildWindowsFromViewOnly()
{
    bool hasActiveImmediateAction = false;
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101000
    hasActiveImmediateAction = [m_immediateActionController hasActiveImmediateAction];
#endif

    // FIXME: We don't know which panel we are dismissing, it may not even be in the current page (see <rdar://problem/13875766>).
    if (m_view.window.isKeyWindow || hasActiveImmediateAction) {
        WebCore::DictionaryLookup::hidePopup();

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101000
        DDActionsManager *actionsManager = [getDDActionsManagerClass() sharedManager];
        if ([actionsManager respondsToSelector:@selector(requestBubbleClosureUnanchorOnFailure:)])
            [actionsManager requestBubbleClosureUnanchorOnFailure:YES];
#endif
    }

    clearTextIndicatorWithAnimation(WebCore::TextIndicatorWindowDismissalAnimation::FadeOut);

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101000
    [m_immediateActionController dismissContentRelativeChildWindows];
#endif
    
    m_pageClient.dismissCorrectionPanel(WebCore::ReasonForDismissingAlternativeTextIgnored);
}

void WebViewImpl::quickLookWithEvent(NSEvent *event)
{
    if (ignoresNonWheelEvents())
        return;

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101000
    if (m_immediateActionGestureRecognizer) {
        [m_view _superQuickLookWithEvent:event];
        return;
    }
#endif

    NSPoint locationInViewCoordinates = [m_view convertPoint:[event locationInWindow] fromView:nil];
    m_page.performDictionaryLookupAtLocation(WebCore::FloatPoint(locationInViewCoordinates));
}

void WebViewImpl::prepareForDictionaryLookup()
{
    if (m_didRegisterForLookupPopoverCloseNotifications)
        return;

    m_didRegisterForLookupPopoverCloseNotifications = true;

    [m_windowVisibilityObserver startObservingLookupDismissal];
}

void WebViewImpl::setAllowsLinkPreview(bool allowsLinkPreview)
{
    if (m_allowsLinkPreview == allowsLinkPreview)
        return;

    m_allowsLinkPreview = allowsLinkPreview;

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101000
    if (!allowsLinkPreview)
        [m_view removeGestureRecognizer:m_immediateActionGestureRecognizer.get()];
    else if (NSGestureRecognizer *immediateActionRecognizer = m_immediateActionGestureRecognizer.get())
        [m_view addGestureRecognizer:immediateActionRecognizer];
#endif
}

void* WebViewImpl::immediateActionAnimationControllerForHitTestResult(WKHitTestResultRef hitTestResult, uint32_t type, WKTypeRef userData)
{
    return [m_view _immediateActionAnimationControllerForHitTestResult:hitTestResult withType:type userData:userData];
}

void* WebViewImpl::immediateActionAnimationControllerForHitTestResultFromViewOnly(WKHitTestResultRef hitTestResult, uint32_t type, WKTypeRef userData)
{
    return nil;
}

void WebViewImpl::didPerformImmediateActionHitTest(const WebHitTestResultData& result, bool contentPreventsDefault, API::Object* userData)
{
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101000
    [m_immediateActionController didPerformImmediateActionHitTest:result contentPreventsDefault:contentPreventsDefault userData:userData];
#endif
}

void WebViewImpl::prepareForImmediateActionAnimation()
{
    [m_view _prepareForImmediateActionAnimation];
}

void WebViewImpl::cancelImmediateActionAnimation()
{
    [m_view _cancelImmediateActionAnimation];
}

void WebViewImpl::completeImmediateActionAnimation()
{
    [m_view _completeImmediateActionAnimation];
}

void WebViewImpl::setIgnoresNonWheelEvents(bool ignoresNonWheelEvents)
{
    if (m_ignoresNonWheelEvents == ignoresNonWheelEvents)
        return;

    m_ignoresNonWheelEvents = ignoresNonWheelEvents;
    m_page.setShouldDispatchFakeMouseMoveEvents(!ignoresNonWheelEvents);

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101000
    if (ignoresNonWheelEvents)
        [m_view removeGestureRecognizer:m_immediateActionGestureRecognizer.get()];
    else if (NSGestureRecognizer *immediateActionRecognizer = m_immediateActionGestureRecognizer.get()) {
        if (m_allowsLinkPreview)
            [m_view addGestureRecognizer:immediateActionRecognizer];
    }
#endif
}

void WebViewImpl::setIgnoresAllEvents(bool ignoresAllEvents)
{
    m_ignoresAllEvents = ignoresAllEvents;
    setIgnoresNonWheelEvents(ignoresAllEvents);
}

void WebViewImpl::accessibilityRegisterUIProcessTokens()
{
    // Initialize remote accessibility when the window connection has been established.
    NSData *remoteElementToken = WKAXRemoteTokenForElement(m_view);
    NSData *remoteWindowToken = WKAXRemoteTokenForElement(m_view.window);
    IPC::DataReference elementToken = IPC::DataReference(reinterpret_cast<const uint8_t*>([remoteElementToken bytes]), [remoteElementToken length]);
    IPC::DataReference windowToken = IPC::DataReference(reinterpret_cast<const uint8_t*>([remoteWindowToken bytes]), [remoteWindowToken length]);
    m_page.registerUIProcessAccessibilityTokens(elementToken, windowToken);
}

} // namespace WebKit

#endif // PLATFORM(MAC)
