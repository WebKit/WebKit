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

#import "AppKitSPI.h"
#import "ColorSpaceData.h"
#import "GenericCallback.h"
#import "Logging.h"
#import "NativeWebGestureEvent.h"
#import "NativeWebKeyboardEvent.h"
#import "NativeWebMouseEvent.h"
#import "NativeWebWheelEvent.h"
#import "PageClient.h"
#import "PasteboardTypes.h"
#import "RemoteLayerTreeDrawingAreaProxy.h"
#import "RemoteObjectRegistry.h"
#import "RemoteObjectRegistryMessages.h"
#import "StringUtilities.h"
#import "TextChecker.h"
#import "TextCheckerState.h"
#import "TiledCoreAnimationDrawingAreaProxy.h"
#import "ViewGestureController.h"
#import "WKBrowsingContextControllerInternal.h"
#import "WKFullScreenWindowController.h"
#import "WKImmediateActionController.h"
#import "WKTextInputWindowController.h"
#import "WKViewLayoutStrategy.h"
#import "WKWebView.h"
#import "WebEditCommandProxy.h"
#import "WebInspectorProxy.h"
#import "WebPageProxy.h"
#import "WebProcessPool.h"
#import "WebProcessProxy.h"
#import "_WKRemoteObjectRegistryInternal.h"
#import "_WKThumbnailViewInternal.h"
#import <HIToolbox/CarbonEventsCore.h>
#import <WebCore/AXObjectCache.h>
#import <WebCore/ColorMac.h>
#import <WebCore/CoreGraphicsSPI.h>
#import <WebCore/DataDetectorsSPI.h>
#import <WebCore/DictionaryLookup.h>
#import <WebCore/DragData.h>
#import <WebCore/Editor.h>
#import <WebCore/KeypressCommand.h>
#import <WebCore/LocalizedStrings.h>
#import <WebCore/LookupSPI.h>
#import <WebCore/NSImmediateActionGestureRecognizerSPI.h>
#import <WebCore/NSWindowSPI.h>
#import <WebCore/PlatformEventFactoryMac.h>
#import <WebCore/SoftLinking.h>
#import <WebCore/ViewState.h>
#import <WebCore/WebActionDisablingCALayerDelegate.h>
#import <WebCore/WebCoreCALayerExtras.h>
#import <WebCore/WebCoreFullScreenPlaceholderView.h>
#import <WebCore/WebCoreFullScreenWindow.h>
#import <WebCore/WebCoreNSStringExtras.h>
#import <WebKitSystemInterface.h>
#import <sys/stat.h>

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

@interface WKFlippedView : NSView
@end

@implementation WKFlippedView

- (BOOL)isFlipped
{
    return YES;
}

@end

@interface WKResponderChainSink : NSResponder {
    NSResponder *_lastResponderInChain;
    bool _didReceiveUnhandledCommand;
}

- (id)initWithResponderChain:(NSResponder *)chain;
- (void)detach;
- (bool)didReceiveUnhandledCommand;
@end

@implementation WKResponderChainSink

- (id)initWithResponderChain:(NSResponder *)chain
{
    self = [super init];
    if (!self)
        return nil;
    _lastResponderInChain = chain;
    while (NSResponder *next = [_lastResponderInChain nextResponder])
        _lastResponderInChain = next;
    [_lastResponderInChain setNextResponder:self];
    return self;
}

- (void)detach
{
    // This assumes that the responder chain was either unmodified since
    // -initWithResponderChain: was called, or was modified in such a way
    // that _lastResponderInChain is still in the chain, and self was not
    // moved earlier in the chain than _lastResponderInChain.
    NSResponder *responderBeforeSelf = _lastResponderInChain;
    NSResponder *next = [responderBeforeSelf nextResponder];
    for (; next && next != self; next = [next nextResponder])
        responderBeforeSelf = next;

    // Nothing to be done if we are no longer in the responder chain.
    if (next != self)
        return;

    [responderBeforeSelf setNextResponder:[self nextResponder]];
    _lastResponderInChain = nil;
}

- (bool)didReceiveUnhandledCommand
{
    return _didReceiveUnhandledCommand;
}

- (void)noResponderFor:(SEL)selector
{
    _didReceiveUnhandledCommand = true;
}

- (void)doCommandBySelector:(SEL)selector
{
    _didReceiveUnhandledCommand = true;
}

- (BOOL)tryToPerform:(SEL)action with:(id)object
{
    _didReceiveUnhandledCommand = true;
    return YES;
}

@end

namespace WebKit {

static NSTrackingAreaOptions trackingAreaOptions()
{
    // Legacy style scrollbars have design details that rely on tracking the mouse all the time.
    NSTrackingAreaOptions options = NSTrackingMouseMoved | NSTrackingMouseEnteredAndExited | NSTrackingInVisibleRect | NSTrackingCursorUpdate;
    if (WKRecommendedScrollerStyle() == NSScrollerStyleLegacy)
        options |= NSTrackingActiveAlways;
    else
        options |= NSTrackingActiveInKeyWindow;
    return options;
}

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
    , m_primaryTrackingArea(adoptNS([[NSTrackingArea alloc] initWithRect:m_view.frame options:trackingAreaOptions() owner:m_view userInfo:nil]))
{
    [m_view addTrackingArea:m_primaryTrackingArea.get()];

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

#if WK_API_ENABLED
    ASSERT(!m_thumbnailView);
#endif

    [m_layoutStrategy invalidate];

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101000
    [m_immediateActionController willDestroyView:m_view];
#endif
}

NSWindow *WebViewImpl::window()
{
    return m_view.window;
}

void WebViewImpl::processDidExit()
{
    notifyInputContextAboutDiscardedComposition();

    if (m_layerHostingView)
        setAcceleratedCompositingRootLayer(nil);

    updateRemoteAccessibilityRegistration(false);

    m_gestureController = nullptr;
}

void WebViewImpl::pageClosed()
{
    updateRemoteAccessibilityRegistration(false);
}

void WebViewImpl::didRelaunchProcess()
{
    accessibilityRegisterUIProcessTokens();
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

std::unique_ptr<WebKit::DrawingAreaProxy> WebViewImpl::createDrawingAreaProxy()
{
    if ([[[NSUserDefaults standardUserDefaults] objectForKey:@"WebKit2UseRemoteLayerTreeDrawingArea"] boolValue])
        return std::make_unique<RemoteLayerTreeDrawingAreaProxy>(m_page);

    return std::make_unique<TiledCoreAnimationDrawingAreaProxy>(m_page);
}

bool WebViewImpl::isUsingUISideCompositing() const
{
    auto* drawingArea = m_page.drawingArea();
    return drawingArea && drawingArea->type() == DrawingAreaTypeRemoteLayerTree;
}

void WebViewImpl::setDrawingAreaSize(CGSize size)
{
    if (!m_page.drawingArea())
        return;

    m_page.drawingArea()->setSize(WebCore::IntSize(size), WebCore::IntSize(), WebCore::IntSize(m_resizeScrollOffset));
    m_resizeScrollOffset = CGSizeZero;
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
// This method forces a drawing area geometry update, even if frame size updates are disabled.
// The updated is performed asynchronously; we don't wait for the geometry update before returning.
// The area drawn need not match the current frame size - if it differs it will be anchored to the
// frame according to the current contentAnchor.
void WebViewImpl::forceAsyncDrawingAreaSizeUpdate(CGSize size)
{
    // This SPI is only used on 10.9 and below, and is incompatible with the fence-based drawing area size synchronization in 10.10+.
#if __MAC_OS_X_VERSION_MIN_REQUIRED <= 1090
    if (m_clipsToVisibleRect)
        updateViewExposedRect();
    setDrawingAreaSize(size);

    // If a geometry update is pending the new update won't be sent. Poll without waiting for any
    // pending did-update message now, such that the new update can be sent. We do so after setting
    // the drawing area size such that the latest update is sent.
    if (DrawingAreaProxy* drawingArea = m_page.drawingArea())
        drawingArea->waitForPossibleGeometryUpdate(std::chrono::milliseconds::zero());
#else
    ASSERT_NOT_REACHED();
#endif
}

void WebViewImpl::waitForAsyncDrawingAreaSizeUpdate()
{
    // This SPI is only used on 10.9 and below, and is incompatible with the fence-based drawing area size synchronization in 10.10+.
#if __MAC_OS_X_VERSION_MIN_REQUIRED <= 1090
    if (DrawingAreaProxy* drawingArea = m_page.drawingArea()) {
        // If a geometry update is still pending then the action of receiving the
        // first geometry update may result in another update being scheduled -
        // we should wait for this to complete too.
        drawingArea->waitForPossibleGeometryUpdate(DrawingAreaProxy::didUpdateBackingStoreStateTimeout() / 2);
        drawingArea->waitForPossibleGeometryUpdate(DrawingAreaProxy::didUpdateBackingStoreStateTimeout() / 2);
    }
#else
    ASSERT_NOT_REACHED();
#endif
}
#pragma clang diagnostic pop

void WebViewImpl::updateLayer()
{
    if (drawsBackground() && !drawsTransparentBackground())
        m_view.layer.backgroundColor = CGColorGetConstantColor(kCGColorWhite);
    else
        m_view.layer.backgroundColor = CGColorGetConstantColor(kCGColorClear);

    // If asynchronous geometry updates have been sent by forceAsyncDrawingAreaSizeUpdate,
    // then subsequent calls to setFrameSize should not result in us waiting for the did
    // udpate response if setFrameSize is called.
    if (frameSizeUpdatesDisabled())
        return;

    if (DrawingAreaProxy* drawingArea = m_page.drawingArea())
        drawingArea->waitForPossibleGeometryUpdate();
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

void WebViewImpl::setMinimumSizeForAutoLayout(CGSize minimumSizeForAutoLayout)
{
    bool expandsToFit = minimumSizeForAutoLayout.width > 0;

    m_page.setMinimumLayoutSize(WebCore::IntSize(minimumSizeForAutoLayout));
    m_page.setMainFrameIsScrollable(!expandsToFit);

    setClipsToVisibleRect(expandsToFit);
}

CGSize WebViewImpl::minimumSizeForAutoLayout() const
{
    return m_page.minimumLayoutSize();
}

void WebViewImpl::setShouldExpandToViewHeightForAutoLayout(bool shouldExpandToViewHeightForAutoLayout)
{
    m_page.setAutoSizingShouldExpandToViewHeight(shouldExpandToViewHeightForAutoLayout);
}

bool WebViewImpl::shouldExpandToViewHeightForAutoLayout() const
{
    return m_page.autoSizingShouldExpandToViewHeight();
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

void WebViewImpl::setUnderlayColor(NSColor *underlayColor)
{
    m_page.setUnderlayColor(WebCore::colorFromNSColor(underlayColor));
}

NSColor *WebViewImpl::underlayColor() const
{
    WebCore::Color webColor = m_page.underlayColor();
    if (!webColor.isValid())
        return nil;

    return WebCore::nsColor(webColor);
}

NSColor *WebViewImpl::pageExtendedBackgroundColor() const
{
    WebCore::Color color = m_page.pageExtendedBackgroundColor();
    if (!color.isValid())
        return nil;

    return WebCore::nsColor(color);
}

void WebViewImpl::setOverlayScrollbarStyle(WTF::Optional<WebCore::ScrollbarOverlayStyle> scrollbarStyle)
{
    m_page.setOverlayScrollbarStyle(scrollbarStyle);
}

WTF::Optional<WebCore::ScrollbarOverlayStyle> WebViewImpl::overlayScrollbarStyle() const
{
    return m_page.overlayScrollbarStyle();
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

void WebViewImpl::setPluginComplexTextInputState(PluginComplexTextInputState pluginComplexTextInputState)
{
    m_pluginComplexTextInputState = pluginComplexTextInputState;

    if (m_pluginComplexTextInputState != PluginComplexTextInputDisabled)
        return;

    // Send back an empty string to the plug-in. This will disable text input.
    m_page.sendComplexTextInputToPlugin(m_pluginComplexTextInputIdentifier, String());
}

void WebViewImpl::setPluginComplexTextInputStateAndIdentifier(PluginComplexTextInputState pluginComplexTextInputState, uint64_t pluginComplexTextInputIdentifier)
{
    if (pluginComplexTextInputIdentifier != m_pluginComplexTextInputIdentifier) {
        // We're asked to update the state for a plug-in that doesn't have focus.
        return;
    }

    setPluginComplexTextInputState(pluginComplexTextInputState);
}

void WebViewImpl::disableComplexTextInputIfNecessary()
{
    if (!m_pluginComplexTextInputIdentifier)
        return;

    if (m_pluginComplexTextInputState != PluginComplexTextInputEnabled)
        return;

    // Check if the text input window has been dismissed.
    if (![[WKTextInputWindowController sharedTextInputWindowController] hasMarkedText])
        setPluginComplexTextInputState(PluginComplexTextInputDisabled);
}

bool WebViewImpl::handlePluginComplexTextInputKeyDown(NSEvent *event)
{
    ASSERT(m_pluginComplexTextInputIdentifier);
    ASSERT(m_pluginComplexTextInputState != PluginComplexTextInputDisabled);

    BOOL usingLegacyCocoaTextInput = m_pluginComplexTextInputState == PluginComplexTextInputEnabledLegacy;

    NSString *string = nil;
    BOOL didHandleEvent = [[WKTextInputWindowController sharedTextInputWindowController] interpretKeyEvent:event usingLegacyCocoaTextInput:usingLegacyCocoaTextInput string:&string];

    if (string) {
        m_page.sendComplexTextInputToPlugin(m_pluginComplexTextInputIdentifier, string);

        if (!usingLegacyCocoaTextInput)
            m_pluginComplexTextInputState = PluginComplexTextInputDisabled;
    }

    return didHandleEvent;
}

bool WebViewImpl::tryHandlePluginComplexTextInputKeyDown(NSEvent *event)
{
    if (!m_pluginComplexTextInputIdentifier || m_pluginComplexTextInputState == PluginComplexTextInputDisabled)
        return NO;

    // Check if the text input window has been dismissed and let the plug-in process know.
    // This is only valid with the updated Cocoa text input spec.
    disableComplexTextInputIfNecessary();

    // Try feeding the keyboard event directly to the plug-in.
    if (m_pluginComplexTextInputState == PluginComplexTextInputEnabledLegacy)
        return handlePluginComplexTextInputKeyDown(event);

    return NO;
}

void WebViewImpl::pluginFocusOrWindowFocusChanged(bool pluginHasFocusAndWindowHasFocus, uint64_t pluginComplexTextInputIdentifier)
{
    BOOL inputSourceChanged = m_pluginComplexTextInputIdentifier;

    if (pluginHasFocusAndWindowHasFocus) {
        // Check if we're already allowing text input for this plug-in.
        if (pluginComplexTextInputIdentifier == m_pluginComplexTextInputIdentifier)
            return;

        m_pluginComplexTextInputIdentifier = pluginComplexTextInputIdentifier;

    } else {
        // Check if we got a request to unfocus a plug-in that isn't focused.
        if (pluginComplexTextInputIdentifier != m_pluginComplexTextInputIdentifier)
            return;

        m_pluginComplexTextInputIdentifier = 0;
    }

    if (inputSourceChanged) {
        // The input source changed; discard any entered text.
        [[WKTextInputWindowController sharedTextInputWindowController] unmarkText];
    }
    
    // This will force the current input context to be updated to its correct value.
    [NSApp updateWindows];
}

bool WebViewImpl::tryPostProcessPluginComplexTextInputKeyDown(NSEvent *event)
{
    if (!m_pluginComplexTextInputIdentifier || m_pluginComplexTextInputState == PluginComplexTextInputDisabled)
        return NO;

    // In the legacy text input model, the event has already been sent to the input method.
    if (m_pluginComplexTextInputState == PluginComplexTextInputEnabledLegacy)
        return NO;

    return handlePluginComplexTextInputKeyDown(event);
}

void WebViewImpl::handleAcceptedAlternativeText(const String& acceptedAlternative)
{
    m_page.handleAlternativeTextUIResult(acceptedAlternative);
}


NSInteger WebViewImpl::spellCheckerDocumentTag()
{
    if (!m_spellCheckerDocumentTag)
        m_spellCheckerDocumentTag = [NSSpellChecker uniqueSpellDocumentTag];
    return m_spellCheckerDocumentTag.value();
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

typedef HashMap<SEL, String> SelectorNameMap;

// Map selectors into Editor command names.
// This is not needed for any selectors that have the same name as the Editor command.
static const SelectorNameMap& selectorExceptionMap()
{
    static NeverDestroyed<SelectorNameMap> map;

    struct SelectorAndCommandName {
        SEL selector;
        ASCIILiteral commandName;
    };

    static const SelectorAndCommandName names[] = {
        { @selector(insertNewlineIgnoringFieldEditor:), ASCIILiteral("InsertNewline") },
        { @selector(insertParagraphSeparator:), ASCIILiteral("InsertNewline") },
        { @selector(insertTabIgnoringFieldEditor:), ASCIILiteral("InsertTab") },
        { @selector(pageDown:), ASCIILiteral("MovePageDown") },
        { @selector(pageDownAndModifySelection:), ASCIILiteral("MovePageDownAndModifySelection") },
        { @selector(pageUp:), ASCIILiteral("MovePageUp") },
        { @selector(pageUpAndModifySelection:), ASCIILiteral("MovePageUpAndModifySelection") },
        { @selector(scrollPageDown:), ASCIILiteral("ScrollPageForward") },
        { @selector(scrollPageUp:), ASCIILiteral("ScrollPageBackward") }
    };

    for (auto& name : names)
        map.get().add(name.selector, name.commandName);

    return map;
}

static String commandNameForSelector(SEL selector)
{
    // Check the exception map first.
    static const SelectorNameMap& exceptionMap = selectorExceptionMap();
    SelectorNameMap::const_iterator it = exceptionMap.find(selector);
    if (it != exceptionMap.end())
        return it->value;

    // Remove the trailing colon.
    // No need to capitalize the command name since Editor command names are
    // not case sensitive.
    const char* selectorName = sel_getName(selector);
    size_t selectorNameLength = strlen(selectorName);
    if (selectorNameLength < 2 || selectorName[selectorNameLength - 1] != ':')
        return String();
    return String(selectorName, selectorNameLength - 1);
}

bool WebViewImpl::executeSavedCommandBySelector(SEL selector)
{
    LOG(TextInput, "Executing previously saved command %s", sel_getName(selector));
    // The sink does two things: 1) Tells us if the responder went unhandled, and
    // 2) prevents any NSBeep; we don't ever want to beep here.
    RetainPtr<WKResponderChainSink> sink = adoptNS([[WKResponderChainSink alloc] initWithResponderChain:m_view]);
    [m_view _superDoCommandBySelector:selector];
    [sink detach];
    return ![sink didReceiveUnhandledCommand];
}

void WebViewImpl::executeEditCommandForSelector(SEL selector, const String& argument)
{
    m_page.executeEditCommand(commandNameForSelector(selector), argument);
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

bool WebViewImpl::readSelectionFromPasteboard(NSPasteboard *pasteboard)
{
    return m_page.readSelectionFromPasteboard([pasteboard name]);
}

id WebViewImpl::validRequestorForSendAndReturnTypes(NSString *sendType, NSString *returnType)
{
    EditorState editorState = m_page.editorState();
    bool isValidSendType = false;

    if (sendType && !editorState.selectionIsNone) {
        if (editorState.isInPlugin)
            isValidSendType = [sendType isEqualToString:NSStringPboardType];
        else
            isValidSendType = [PasteboardTypes::forSelection() containsObject:sendType];
    }

    bool isValidReturnType = false;
    if (!returnType)
        isValidReturnType = true;
    else if ([PasteboardTypes::forEditing() containsObject:returnType] && editorState.isContentEditable) {
        // We can insert strings in any editable context.  We can insert other types, like images, only in rich edit contexts.
        isValidReturnType = editorState.isContentRichlyEditable || [returnType isEqualToString:NSStringPboardType];
    }
    if (isValidSendType && isValidReturnType)
        return m_view;
    return [[m_view nextResponder] validRequestorForSendType:sendType returnType:returnType];
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

void WebViewImpl::changeFontFromFontPanel()
{
    NSFontManager *fontManager = [NSFontManager sharedFontManager];
    NSFont *font = [fontManager convertFont:fontManager.selectedFont];
    if (!font)
        return;
    m_page.setFont(font.familyName, font.pointSize, font.fontDescriptor.symbolicTraits);
}

static NSMenuItem *menuItem(id <NSValidatedUserInterfaceItem> item)
{
    if (![(NSObject *)item isKindOfClass:[NSMenuItem class]])
        return nil;
    return (NSMenuItem *)item;
}

static NSToolbarItem *toolbarItem(id <NSValidatedUserInterfaceItem> item)
{
    if (![(NSObject *)item isKindOfClass:[NSToolbarItem class]])
        return nil;
    return (NSToolbarItem *)item;
}

bool WebViewImpl::validateUserInterfaceItem(id <NSValidatedUserInterfaceItem> item)
{
    SEL action = [item action];

    if (action == @selector(showGuessPanel:)) {
        if (NSMenuItem *menuItem = WebKit::menuItem(item))
            [menuItem setTitle:WebCore::contextMenuItemTagShowSpellingPanel(![[[NSSpellChecker sharedSpellChecker] spellingPanel] isVisible])];
        return m_page.editorState().isContentEditable;
    }

    if (action == @selector(checkSpelling:) || action == @selector(changeSpelling:))
        return m_page.editorState().isContentEditable;

    if (action == @selector(toggleContinuousSpellChecking:)) {
        bool enabled = TextChecker::isContinuousSpellCheckingAllowed();
        bool checked = enabled && TextChecker::state().isContinuousSpellCheckingEnabled;
        [menuItem(item) setState:checked ? NSOnState : NSOffState];
        return enabled;
    }

    if (action == @selector(toggleGrammarChecking:)) {
        bool checked = TextChecker::state().isGrammarCheckingEnabled;
        [menuItem(item) setState:checked ? NSOnState : NSOffState];
        return true;
    }

    if (action == @selector(toggleAutomaticSpellingCorrection:)) {
        bool checked = TextChecker::state().isAutomaticSpellingCorrectionEnabled;
        [menuItem(item) setState:checked ? NSOnState : NSOffState];
        return m_page.editorState().isContentEditable;
    }

    if (action == @selector(orderFrontSubstitutionsPanel:)) {
        if (NSMenuItem *menuItem = WebKit::menuItem(item))
            [menuItem setTitle:WebCore::contextMenuItemTagShowSubstitutions(![[[NSSpellChecker sharedSpellChecker] substitutionsPanel] isVisible])];
        return m_page.editorState().isContentEditable;
    }

    if (action == @selector(toggleSmartInsertDelete:)) {
        bool checked = m_page.isSmartInsertDeleteEnabled();
        [menuItem(item) setState:checked ? NSOnState : NSOffState];
        return m_page.editorState().isContentEditable;
    }

    if (action == @selector(toggleAutomaticQuoteSubstitution:)) {
        bool checked = TextChecker::state().isAutomaticQuoteSubstitutionEnabled;
        [menuItem(item) setState:checked ? NSOnState : NSOffState];
        return m_page.editorState().isContentEditable;
    }

    if (action == @selector(toggleAutomaticDashSubstitution:)) {
        bool checked = TextChecker::state().isAutomaticDashSubstitutionEnabled;
        [menuItem(item) setState:checked ? NSOnState : NSOffState];
        return m_page.editorState().isContentEditable;
    }

    if (action == @selector(toggleAutomaticLinkDetection:)) {
        bool checked = TextChecker::state().isAutomaticLinkDetectionEnabled;
        [menuItem(item) setState:checked ? NSOnState : NSOffState];
        return m_page.editorState().isContentEditable;
    }

    if (action == @selector(toggleAutomaticTextReplacement:)) {
        bool checked = TextChecker::state().isAutomaticTextReplacementEnabled;
        [menuItem(item) setState:checked ? NSOnState : NSOffState];
        return m_page.editorState().isContentEditable;
    }

    if (action == @selector(uppercaseWord:) || action == @selector(lowercaseWord:) || action == @selector(capitalizeWord:))
        return m_page.editorState().selectionIsRange && m_page.editorState().isContentEditable;

    if (action == @selector(stopSpeaking:))
        return [NSApp isSpeaking];

    // The centerSelectionInVisibleArea: selector is enabled if there's a selection range or if there's an insertion point in an editable area.
    if (action == @selector(centerSelectionInVisibleArea:))
        return m_page.editorState().selectionIsRange || (m_page.editorState().isContentEditable && !m_page.editorState().selectionIsNone);

    // Next, handle editor commands. Start by returning true for anything that is not an editor command.
    // Returning true is the default thing to do in an AppKit validate method for any selector that is not recognized.
    String commandName = commandNameForSelector([item action]);
    if (!WebCore::Editor::commandIsSupportedFromMenuOrKeyBinding(commandName))
        return true;

    // Add this item to the vector of items for a given command that are awaiting validation.
    ValidationMap::AddResult addResult = m_validationMap.add(commandName, ValidationVector());
    addResult.iterator->value.append(item);
    if (addResult.isNewEntry) {
        // If we are not already awaiting validation for this command, start the asynchronous validation process.
        // FIXME: Theoretically, there is a race here; when we get the answer it might be old, from a previous time
        // we asked for the same command; there is no guarantee the answer is still valid.
        auto weakThis = createWeakPtr();
        m_page.validateCommand(commandName, [weakThis](const String& commandName, bool isEnabled, int32_t state, WebKit::CallbackBase::Error error) {
            if (!weakThis)
                return;

            // If the process exits before the command can be validated, we'll be called back with an error.
            if (error != WebKit::CallbackBase::Error::None)
                return;

            weakThis->setUserInterfaceItemState(commandName, isEnabled, state);
        });
    }

    // Treat as enabled until we get the result back from the web process and _setUserInterfaceItemState is called.
    // FIXME <rdar://problem/8803459>: This means disabled items will flash enabled at first for a moment.
    // But returning NO here would be worse; that would make keyboard commands such as command-C fail.
    return true;
}

void WebViewImpl::setUserInterfaceItemState(NSString *commandName, bool enabled, int state)
{
    ValidationVector items = m_validationMap.take(commandName);
    for (auto& item : items) {
        [menuItem(item.get()) setState:state];
        [menuItem(item.get()) setEnabled:enabled];
        [toolbarItem(item.get()) setEnabled:enabled];
        // FIXME <rdar://problem/8803392>: If the item is neither a menu nor toolbar item, it will be left enabled.
    }
}

void WebViewImpl::startSpeaking()
{
    m_page.getSelectionOrContentsAsString([](const String& string, WebKit::CallbackBase::Error error) {
        if (error != WebKit::CallbackBase::Error::None)
            return;
        if (!string)
            return;

        [NSApp speakString:string];
    });
}

void WebViewImpl::stopSpeaking(id sender)
{
    [NSApp stopSpeaking:sender];
}

void WebViewImpl::showGuessPanel(id sender)
{
    NSSpellChecker *checker = [NSSpellChecker sharedSpellChecker];
    if (!checker) {
        LOG_ERROR("No NSSpellChecker");
        return;
    }

    NSPanel *spellingPanel = [checker spellingPanel];
    if ([spellingPanel isVisible]) {
        [spellingPanel orderOut:sender];
        return;
    }

    m_page.advanceToNextMisspelling(true);
    [spellingPanel orderFront:sender];
}

void WebViewImpl::checkSpelling()
{
    m_page.advanceToNextMisspelling(false);
}

void WebViewImpl::changeSpelling(id sender)
{
    NSString *word = [[sender selectedCell] stringValue];

    m_page.changeSpellingToWord(word);
}

void WebViewImpl::toggleContinuousSpellChecking()
{
    bool spellCheckingEnabled = !TextChecker::state().isContinuousSpellCheckingEnabled;
    TextChecker::setContinuousSpellCheckingEnabled(spellCheckingEnabled);

    m_page.process().updateTextCheckerState();
}

bool WebViewImpl::isGrammarCheckingEnabled()
{
    return TextChecker::state().isGrammarCheckingEnabled;
}

void WebViewImpl::setGrammarCheckingEnabled(bool flag)
{
    if (static_cast<bool>(flag) == TextChecker::state().isGrammarCheckingEnabled)
        return;

    TextChecker::setGrammarCheckingEnabled(flag);
    m_page.process().updateTextCheckerState();
}

void WebViewImpl::toggleGrammarChecking()
{
    bool grammarCheckingEnabled = !TextChecker::state().isGrammarCheckingEnabled;
    TextChecker::setGrammarCheckingEnabled(grammarCheckingEnabled);

    m_page.process().updateTextCheckerState();
}

void WebViewImpl::toggleAutomaticSpellingCorrection()
{
    TextChecker::setAutomaticSpellingCorrectionEnabled(!TextChecker::state().isAutomaticSpellingCorrectionEnabled);

    m_page.process().updateTextCheckerState();
}

void WebViewImpl::orderFrontSubstitutionsPanel(id sender)
{
    NSSpellChecker *checker = [NSSpellChecker sharedSpellChecker];
    if (!checker) {
        LOG_ERROR("No NSSpellChecker");
        return;
    }

    NSPanel *substitutionsPanel = [checker substitutionsPanel];
    if ([substitutionsPanel isVisible]) {
        [substitutionsPanel orderOut:sender];
        return;
    }
    [substitutionsPanel orderFront:sender];
}

void WebViewImpl::toggleSmartInsertDelete()
{
    m_page.setSmartInsertDeleteEnabled(!m_page.isSmartInsertDeleteEnabled());
}

bool WebViewImpl::isAutomaticQuoteSubstitutionEnabled()
{
    return TextChecker::state().isAutomaticQuoteSubstitutionEnabled;
}

void WebViewImpl::setAutomaticQuoteSubstitutionEnabled(bool flag)
{
    if (static_cast<bool>(flag) == TextChecker::state().isAutomaticQuoteSubstitutionEnabled)
        return;

    TextChecker::setAutomaticQuoteSubstitutionEnabled(flag);
    m_page.process().updateTextCheckerState();
}

void WebViewImpl::toggleAutomaticQuoteSubstitution()
{
    TextChecker::setAutomaticQuoteSubstitutionEnabled(!TextChecker::state().isAutomaticQuoteSubstitutionEnabled);
    m_page.process().updateTextCheckerState();
}

bool WebViewImpl::isAutomaticDashSubstitutionEnabled()
{
    return TextChecker::state().isAutomaticDashSubstitutionEnabled;
}

void WebViewImpl::setAutomaticDashSubstitutionEnabled(bool flag)
{
    if (static_cast<bool>(flag) == TextChecker::state().isAutomaticDashSubstitutionEnabled)
        return;

    TextChecker::setAutomaticDashSubstitutionEnabled(flag);
    m_page.process().updateTextCheckerState();
}

void WebViewImpl::toggleAutomaticDashSubstitution()
{
    TextChecker::setAutomaticDashSubstitutionEnabled(!TextChecker::state().isAutomaticDashSubstitutionEnabled);
    m_page.process().updateTextCheckerState();
}

bool WebViewImpl::isAutomaticLinkDetectionEnabled()
{
    return TextChecker::state().isAutomaticLinkDetectionEnabled;
}

void WebViewImpl::setAutomaticLinkDetectionEnabled(bool flag)
{
    if (static_cast<bool>(flag) == TextChecker::state().isAutomaticLinkDetectionEnabled)
        return;

    TextChecker::setAutomaticLinkDetectionEnabled(flag);
    m_page.process().updateTextCheckerState();
}

void WebViewImpl::toggleAutomaticLinkDetection()
{
    TextChecker::setAutomaticLinkDetectionEnabled(!TextChecker::state().isAutomaticLinkDetectionEnabled);
    m_page.process().updateTextCheckerState();
}

bool WebViewImpl::isAutomaticTextReplacementEnabled()
{
    return TextChecker::state().isAutomaticTextReplacementEnabled;
}

void WebViewImpl::setAutomaticTextReplacementEnabled(bool flag)
{
    if (static_cast<bool>(flag) == TextChecker::state().isAutomaticTextReplacementEnabled)
        return;
    
    TextChecker::setAutomaticTextReplacementEnabled(flag);
    m_page.process().updateTextCheckerState();
}

void WebViewImpl::toggleAutomaticTextReplacement()
{
    TextChecker::setAutomaticTextReplacementEnabled(!TextChecker::state().isAutomaticTextReplacementEnabled);
    m_page.process().updateTextCheckerState();
}

void WebViewImpl::uppercaseWord()
{
    m_page.uppercaseWord();
}

void WebViewImpl::lowercaseWord()
{
    m_page.lowercaseWord();
}

void WebViewImpl::capitalizeWord()
{
    m_page.capitalizeWord();
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

void WebViewImpl::setIgnoresMouseDraggedEvents(bool ignoresMouseDraggedEvents)
{
    m_ignoresMouseDraggedEvents = ignoresMouseDraggedEvents;
}

void WebViewImpl::setAccessibilityWebProcessToken(NSData *data)
{
    m_remoteAccessibilityChild = WKAXRemoteElementForToken(data);
    updateRemoteAccessibilityRegistration(true);
}

void WebViewImpl::updateRemoteAccessibilityRegistration(bool registerProcess)
{
    // When the tree is connected/disconnected, the remote accessibility registration
    // needs to be updated with the pid of the remote process. If the process is going
    // away, that information is not present in WebProcess
    pid_t pid = 0;
    if (registerProcess)
        pid = m_page.process().processIdentifier();
    else if (!registerProcess) {
        pid = WKAXRemoteProcessIdentifier(m_remoteAccessibilityChild.get());
        m_remoteAccessibilityChild = nil;
    }
    if (pid)
        WKAXRegisterRemoteProcess(registerProcess, pid);
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

id WebViewImpl::accessibilityFocusedUIElement()
{
    enableAccessibilityIfNecessary();
    return m_remoteAccessibilityChild.get();
}

id WebViewImpl::accessibilityHitTest(CGPoint)
{
    return accessibilityFocusedUIElement();
}

void WebViewImpl::enableAccessibilityIfNecessary()
{
    if (WebCore::AXObjectCache::accessibilityEnabled())
        return;

    // After enabling accessibility update the window frame on the web process so that the
    // correct accessibility position is transmitted (when AX is off, that position is not calculated).
    WebCore::AXObjectCache::enableAccessibility();
    updateWindowAndViewFrames();
}

id WebViewImpl::accessibilityAttributeValue(NSString *attribute)
{
    enableAccessibilityIfNecessary();

    if ([attribute isEqualToString:NSAccessibilityChildrenAttribute]) {

        id child = nil;
        if (m_remoteAccessibilityChild)
            child = m_remoteAccessibilityChild.get();

            if (!child)
                return nil;
        return [NSArray arrayWithObject:child];
    }
    if ([attribute isEqualToString:NSAccessibilityRoleAttribute])
        return NSAccessibilityGroupRole;
    if ([attribute isEqualToString:NSAccessibilityRoleDescriptionAttribute])
        return NSAccessibilityRoleDescription(NSAccessibilityGroupRole, nil);
        if ([attribute isEqualToString:NSAccessibilityParentAttribute])
            return NSAccessibilityUnignoredAncestor([m_view superview]);
            if ([attribute isEqualToString:NSAccessibilityEnabledAttribute])
                return @YES;
    
    return [m_view _superAccessibilityAttributeValue:attribute];
}

void WebViewImpl::setPrimaryTrackingArea(NSTrackingArea *trackingArea)
{
    [m_view removeTrackingArea:m_primaryTrackingArea.get()];
    m_primaryTrackingArea = trackingArea;
    [m_view addTrackingArea:trackingArea];
}

// Any non-zero value will do, but using something recognizable might help us debug some day.
#define TRACKING_RECT_TAG 0xBADFACE

NSTrackingRectTag WebViewImpl::addTrackingRect(CGRect, id owner, void* userData, bool assumeInside)
{
    ASSERT(m_trackingRectOwner == nil);
    m_trackingRectOwner = owner;
    m_trackingRectUserData = userData;
    return TRACKING_RECT_TAG;
}

NSTrackingRectTag WebViewImpl::addTrackingRectWithTrackingNum(CGRect, id owner, void* userData, bool assumeInside, int tag)
{
    ASSERT(tag == 0 || tag == TRACKING_RECT_TAG);
    ASSERT(m_trackingRectOwner == nil);
    m_trackingRectOwner = owner;
    m_trackingRectUserData = userData;
    return TRACKING_RECT_TAG;
}

void WebViewImpl::addTrackingRectsWithTrackingNums(CGRect*, id owner, void** userDataList, bool assumeInside, NSTrackingRectTag *trackingNums, int count)
{
    ASSERT(count == 1);
    ASSERT(trackingNums[0] == 0 || trackingNums[0] == TRACKING_RECT_TAG);
    ASSERT(m_trackingRectOwner == nil);
    m_trackingRectOwner = owner;
    m_trackingRectUserData = userDataList[0];
    trackingNums[0] = TRACKING_RECT_TAG;
}

void WebViewImpl::removeTrackingRect(NSTrackingRectTag tag)
{
    if (tag == 0)
        return;

    if (tag == TRACKING_RECT_TAG) {
        m_trackingRectOwner = nil;
        return;
    }

    if (tag == m_lastToolTipTag) {
        [m_view _superRemoveTrackingRect:tag];
        m_lastToolTipTag = 0;
        return;
    }

    // If any other tracking rect is being removed, we don't know how it was created
    // and it's possible there's a leak involved (see 3500217)
    ASSERT_NOT_REACHED();
}

void WebViewImpl::removeTrackingRects(NSTrackingRectTag *tags, int count)
{
    for (int i = 0; i < count; ++i) {
        int tag = tags[i];
        if (tag == 0)
            continue;
        ASSERT(tag == TRACKING_RECT_TAG);
        m_trackingRectOwner = nil;
    }
}

void WebViewImpl::sendToolTipMouseExited()
{
    // Nothing matters except window, trackingNumber, and userData.
    NSEvent *fakeEvent = [NSEvent enterExitEventWithType:NSMouseExited
                                                location:NSMakePoint(0, 0)
                                           modifierFlags:0
                                               timestamp:0
                                            windowNumber:m_view.window.windowNumber
                                                 context:NULL
                                             eventNumber:0
                                          trackingNumber:TRACKING_RECT_TAG
                                                userData:m_trackingRectUserData];
    [m_trackingRectOwner mouseExited:fakeEvent];
}

void WebViewImpl::sendToolTipMouseEntered()
{
    // Nothing matters except window, trackingNumber, and userData.
    NSEvent *fakeEvent = [NSEvent enterExitEventWithType:NSMouseEntered
                                                location:NSMakePoint(0, 0)
                                           modifierFlags:0
                                               timestamp:0
                                            windowNumber:m_view.window.windowNumber
                                                 context:NULL
                                             eventNumber:0
                                          trackingNumber:TRACKING_RECT_TAG
                                                userData:m_trackingRectUserData];
    [m_trackingRectOwner mouseEntered:fakeEvent];
}

NSString *WebViewImpl::stringForToolTip(NSToolTipTag tag)
{
    return nsStringFromWebCoreString(m_page.toolTip());
}

void WebViewImpl::toolTipChanged(const String& oldToolTip, const String& newToolTip)
{
    if (!oldToolTip.isNull())
        sendToolTipMouseExited();
    
    if (!newToolTip.isEmpty()) {
        // See radar 3500217 for why we remove all tooltips rather than just the single one we created.
        [m_view removeAllToolTips];
        NSRect wideOpenRect = NSMakeRect(-100000, -100000, 200000, 200000);
        m_lastToolTipTag = [m_view addToolTipRect:wideOpenRect owner:m_view userData:NULL];
        sendToolTipMouseEntered();
    }
}

void WebViewImpl::setAcceleratedCompositingRootLayer(CALayer *rootLayer)
{
    [rootLayer web_disableAllActions];

    m_rootLayer = rootLayer;

#if WK_API_ENABLED
    if (m_thumbnailView) {
        updateThumbnailViewLayer();
        return;
    }
#endif

    [CATransaction begin];
    [CATransaction setDisableActions:YES];

    if (rootLayer) {
        if (!m_layerHostingView) {
            // Create an NSView that will host our layer tree.
            m_layerHostingView = adoptNS([[WKFlippedView alloc] initWithFrame:m_view.bounds]);
            [m_layerHostingView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];

            [m_view addSubview:m_layerHostingView.get() positioned:NSWindowBelow relativeTo:nil];

            // Create a root layer that will back the NSView.
            RetainPtr<CALayer> layer = adoptNS([[CALayer alloc] init]);
            [layer setDelegate:[WebActionDisablingCALayerDelegate shared]];
#ifndef NDEBUG
            [layer setName:@"Hosting root layer"];
#endif

            [m_layerHostingView setLayer:layer.get()];
            [m_layerHostingView setWantsLayer:YES];
        }

        [m_layerHostingView layer].sublayers = [NSArray arrayWithObject:rootLayer];
    } else if (m_layerHostingView) {
        [m_layerHostingView removeFromSuperview];
        [m_layerHostingView setLayer:nil];
        [m_layerHostingView setWantsLayer:NO];

        m_layerHostingView = nullptr;
    }

    [CATransaction commit];
}

#if WK_API_ENABLED
void WebViewImpl::setThumbnailView(_WKThumbnailView *thumbnailView)
{
    ASSERT(!m_thumbnailView || !thumbnailView);

    m_thumbnailView = thumbnailView;

    if (thumbnailView)
        updateThumbnailViewLayer();
    else
        setAcceleratedCompositingRootLayer(m_rootLayer.get());
}

void WebViewImpl::reparentLayerTreeInThumbnailView()
{
    m_thumbnailView._thumbnailLayer = m_rootLayer.get();
}

void WebViewImpl::updateThumbnailViewLayer()
{
    _WKThumbnailView *thumbnailView = m_thumbnailView;
    ASSERT(thumbnailView);

    if (thumbnailView._waitingForSnapshot && m_view.window)
        reparentLayerTreeInThumbnailView();
}

void WebViewImpl::setInspectorAttachmentView(NSView *newView)
{
    NSView *oldView = m_inspectorAttachmentView.get();
    if (oldView == newView)
        return;

    m_inspectorAttachmentView = newView;
    m_page.inspector()->attachmentViewDidChange(oldView ? oldView : m_view, newView ? newView : m_view);
}

NSView *WebViewImpl::inspectorAttachmentView()
{
    NSView *attachmentView = m_inspectorAttachmentView.get();
    return attachmentView ? attachmentView : m_view;
}

_WKRemoteObjectRegistry *WebViewImpl::remoteObjectRegistry()
{
    if (!m_remoteObjectRegistry) {
        m_remoteObjectRegistry = adoptNS([[_WKRemoteObjectRegistry alloc] _initWithMessageSender:m_page]);
        m_page.process().processPool().addMessageReceiver(Messages::RemoteObjectRegistry::messageReceiverName(), m_page.pageID(), [m_remoteObjectRegistry remoteObjectRegistry]);
    }

    return m_remoteObjectRegistry.get();
}

void WebViewImpl::destroyRemoteObjectRegistry()
{
    if (!m_remoteObjectRegistry)
        return;

    m_page.process().processPool().removeMessageReceiver(Messages::RemoteObjectRegistry::messageReceiverName(), m_page.pageID());
    [m_remoteObjectRegistry _invalidate];
    m_remoteObjectRegistry = nil;
}

WKBrowsingContextController *WebViewImpl::browsingContextController()
{
    if (!m_browsingContextController)
        m_browsingContextController = adoptNS([[WKBrowsingContextController alloc] _initWithPageRef:toAPI(&m_page)]);

    return m_browsingContextController.get();
}
#endif // WK_API_ENABLED

#if ENABLE(DRAG_SUPPORT)
void WebViewImpl::draggedImage(NSImage *image, CGPoint endPoint, NSDragOperation operation)
{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    NSPoint windowImageLoc = [m_view.window convertScreenToBase:NSPointFromCGPoint(endPoint)];
#pragma clang diagnostic pop
    NSPoint windowMouseLoc = windowImageLoc;

    // Prevent queued mouseDragged events from coming after the drag and fake mouseUp event.
    m_ignoresMouseDraggedEvents = true;

    m_page.dragEnded(WebCore::IntPoint(windowMouseLoc), WebCore::globalPoint(windowMouseLoc, m_view.window), operation);
}

static WebCore::DragApplicationFlags applicationFlagsForDrag(NSView *view, id <NSDraggingInfo> draggingInfo)
{
    uint32_t flags = 0;
    if ([NSApp modalWindow])
        flags = WebCore::DragApplicationIsModal;
    if (view.window.attachedSheet)
        flags |= WebCore::DragApplicationHasAttachedSheet;
    if (draggingInfo.draggingSource == view)
        flags |= WebCore::DragApplicationIsSource;
    if ([NSApp currentEvent].modifierFlags & NSAlternateKeyMask)
        flags |= WebCore::DragApplicationIsCopyKeyDown;
    return static_cast<WebCore::DragApplicationFlags>(flags);

}

NSDragOperation WebViewImpl::draggingEntered(id <NSDraggingInfo> draggingInfo)
{
    WebCore::IntPoint client([m_view convertPoint:draggingInfo.draggingLocation fromView:nil]);
    WebCore::IntPoint global(WebCore::globalPoint(draggingInfo.draggingLocation, m_view.window));
    WebCore::DragData dragData(draggingInfo, client, global, static_cast<WebCore::DragOperation>(draggingInfo.draggingSourceOperationMask), applicationFlagsForDrag(m_view, draggingInfo));

    m_page.resetCurrentDragInformation();
    m_page.dragEntered(dragData, draggingInfo.draggingPasteboard.name);
    return NSDragOperationCopy;
}

NSDragOperation WebViewImpl::draggingUpdated(id <NSDraggingInfo> draggingInfo)
{
    WebCore::IntPoint client([m_view convertPoint:draggingInfo.draggingLocation fromView:nil]);
    WebCore::IntPoint global(WebCore::globalPoint(draggingInfo.draggingLocation, m_view.window));
    WebCore::DragData dragData(draggingInfo, client, global, static_cast<WebCore::DragOperation>(draggingInfo.draggingSourceOperationMask), applicationFlagsForDrag(m_view, draggingInfo));
    m_page.dragUpdated(dragData, draggingInfo.draggingPasteboard.name);

    NSInteger numberOfValidItemsForDrop = m_page.currentDragNumberOfFilesToBeAccepted();
    NSDraggingFormation draggingFormation = NSDraggingFormationNone;
    if (m_page.currentDragIsOverFileInput() && numberOfValidItemsForDrop > 0)
        draggingFormation = NSDraggingFormationList;

    if (draggingInfo.numberOfValidItemsForDrop != numberOfValidItemsForDrop)
        [draggingInfo setNumberOfValidItemsForDrop:numberOfValidItemsForDrop];
    if (draggingInfo.draggingFormation != draggingFormation)
        [draggingInfo setDraggingFormation:draggingFormation];

    return m_page.currentDragOperation();
}

void WebViewImpl::draggingExited(id <NSDraggingInfo> draggingInfo)
{
    WebCore::IntPoint client([m_view convertPoint:draggingInfo.draggingLocation fromView:nil]);
    WebCore::IntPoint global(WebCore::globalPoint(draggingInfo.draggingLocation, m_view.window));
    WebCore::DragData dragData(draggingInfo, client, global, static_cast<WebCore::DragOperation>(draggingInfo.draggingSourceOperationMask), applicationFlagsForDrag(m_view, draggingInfo));
    m_page.dragExited(dragData, draggingInfo.draggingPasteboard.name);
    m_page.resetCurrentDragInformation();
}

bool WebViewImpl::prepareForDragOperation(id <NSDraggingInfo>)
{
    return true;
}

// FIXME: This code is more or less copied from Pasteboard::getBestURL.
// It would be nice to be able to share the code somehow.
static bool maybeCreateSandboxExtensionFromPasteboard(NSPasteboard *pasteboard, SandboxExtension::Handle& sandboxExtensionHandle)
{
    NSArray *types = pasteboard.types;
    if (![types containsObject:NSFilenamesPboardType])
        return false;

    NSArray *files = [pasteboard propertyListForType:NSFilenamesPboardType];
    if (files.count != 1)
        return false;

    NSString *file = [files objectAtIndex:0];
    BOOL isDirectory;
    if (![[NSFileManager defaultManager] fileExistsAtPath:file isDirectory:&isDirectory])
        return false;

    if (isDirectory)
        return false;

    SandboxExtension::createHandle("/", SandboxExtension::ReadOnly, sandboxExtensionHandle);
    return true;
}

static void createSandboxExtensionsForFileUpload(NSPasteboard *pasteboard, SandboxExtension::HandleArray& handles)
{
    NSArray *types = pasteboard.types;
    if (![types containsObject:NSFilenamesPboardType])
        return;

    NSArray *files = [pasteboard propertyListForType:NSFilenamesPboardType];
    handles.allocate(files.count);
    for (unsigned i = 0; i < files.count; i++) {
        NSString *file = [files objectAtIndex:i];
        if (![[NSFileManager defaultManager] fileExistsAtPath:file])
            continue;
        SandboxExtension::Handle handle;
        SandboxExtension::createHandle(file, SandboxExtension::ReadOnly, handles[i]);
    }
}

bool WebViewImpl::performDragOperation(id <NSDraggingInfo> draggingInfo)
{
    WebCore::IntPoint client([m_view convertPoint:draggingInfo.draggingLocation fromView:nil]);
    WebCore::IntPoint global(WebCore::globalPoint(draggingInfo.draggingLocation, m_view.window));
    WebCore::DragData dragData(draggingInfo, client, global, static_cast<WebCore::DragOperation>(draggingInfo.draggingSourceOperationMask), applicationFlagsForDrag(m_view, draggingInfo));

    SandboxExtension::Handle sandboxExtensionHandle;
    bool createdExtension = maybeCreateSandboxExtensionFromPasteboard(draggingInfo.draggingPasteboard, sandboxExtensionHandle);
    if (createdExtension)
        m_page.process().willAcquireUniversalFileReadSandboxExtension();

    SandboxExtension::HandleArray sandboxExtensionForUpload;
    createSandboxExtensionsForFileUpload(draggingInfo.draggingPasteboard, sandboxExtensionForUpload);

    m_page.performDragOperation(dragData, draggingInfo.draggingPasteboard.name, sandboxExtensionHandle, sandboxExtensionForUpload);

    return true;
}

NSView *WebViewImpl::hitTestForDragTypes(CGPoint point, NSSet *types)
{
    // This code is needed to support drag and drop when the drag types cannot be matched.
    // This is the case for elements that do not place content
    // in the drag pasteboard automatically when the drag start (i.e. dragging a DIV element).
    if ([[m_view superview] mouse:NSPointFromCGPoint(point) inRect:[m_view frame]])
        return m_view;
    return nil;
}

void WebViewImpl::registerDraggedTypes()
{
    auto types = adoptNS([[NSMutableSet alloc] initWithArray:PasteboardTypes::forEditing()]);
    [types addObjectsFromArray:PasteboardTypes::forURL()];
    [m_view registerForDraggedTypes:[types allObjects]];
}
#endif // ENABLE(DRAG_SUPPORT)

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 101100
void WebViewImpl::startWindowDrag()
{
    [m_view.window performWindowDragWithEvent:m_lastMouseDownEvent.get()];
}
#endif

void WebViewImpl::dragImageForView(NSView *view, NSImage *image, CGPoint clientPoint, bool linkDrag)
{
    // The call below could release the view.
    RetainPtr<NSView> protector(m_view);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    [view dragImage:image
                 at:NSPointFromCGPoint(clientPoint)
             offset:NSZeroSize
              event:linkDrag ? [NSApp currentEvent] : m_lastMouseDownEvent.get()
         pasteboard:[NSPasteboard pasteboardWithName:NSDragPboard]
             source:m_view
          slideBack:YES];
#pragma clang diagnostic pop
}

static bool matchesExtensionOrEquivalent(NSString *filename, NSString *extension)
{
    NSString *extensionAsSuffix = [@"." stringByAppendingString:extension];
    return hasCaseInsensitiveSuffix(filename, extensionAsSuffix) || (stringIsCaseInsensitiveEqualToString(extension, @"jpeg")
        && hasCaseInsensitiveSuffix(filename, @".jpg"));
}

void WebViewImpl::setFileAndURLTypes(NSString *filename, NSString *extension, NSString *title, NSString *url, NSString *visibleURL, NSPasteboard *pasteboard)
{
    if (!matchesExtensionOrEquivalent(filename, extension))
        filename = [[filename stringByAppendingString:@"."] stringByAppendingString:extension];

    [pasteboard setString:visibleURL forType:NSStringPboardType];
    [pasteboard setString:visibleURL forType:PasteboardTypes::WebURLPboardType];
    [pasteboard setString:title forType:PasteboardTypes::WebURLNamePboardType];
    [pasteboard setPropertyList:[NSArray arrayWithObjects:[NSArray arrayWithObject:visibleURL], [NSArray arrayWithObject:title], nil] forType:PasteboardTypes::WebURLsWithTitlesPboardType];
    [pasteboard setPropertyList:[NSArray arrayWithObject:extension] forType:NSFilesPromisePboardType];
    m_promisedFilename = filename;
    m_promisedURL = url;
}

void WebViewImpl::setPromisedDataForImage(WebCore::Image* image, NSString *filename, NSString *extension, NSString *title, NSString *url, NSString *visibleURL, WebCore::SharedBuffer* archiveBuffer, NSString *pasteboardName)
{
    NSPasteboard *pasteboard = [NSPasteboard pasteboardWithName:pasteboardName];
    RetainPtr<NSMutableArray> types = adoptNS([[NSMutableArray alloc] initWithObjects:NSFilesPromisePboardType, nil]);

    [types addObjectsFromArray:archiveBuffer ? PasteboardTypes::forImagesWithArchive() : PasteboardTypes::forImages()];
    [pasteboard declareTypes:types.get() owner:m_view];
    setFileAndURLTypes(filename, extension, title, url, visibleURL, pasteboard);

    if (archiveBuffer)
        [pasteboard setData:archiveBuffer->createNSData().get() forType:PasteboardTypes::WebArchivePboardType];

    m_promisedImage = image;
}

#if ENABLE(ATTACHMENT_ELEMENT)
void WebViewImpl::setPromisedDataForAttachment(NSString *filename, NSString *extension, NSString *title, NSString *url, NSString *visibleURL, NSString *pasteboardName)
{
    NSPasteboard *pasteboard = [NSPasteboard pasteboardWithName:pasteboardName];
    RetainPtr<NSMutableArray> types = adoptNS([[NSMutableArray alloc] initWithObjects:NSFilesPromisePboardType, nil]);
    [types addObjectsFromArray:PasteboardTypes::forURL()];
    [pasteboard declareTypes:types.get() owner:m_view];
    setFileAndURLTypes(filename, extension, title, url, visibleURL, pasteboard);
    
    RetainPtr<NSMutableArray> paths = adoptNS([[NSMutableArray alloc] init]);
    [paths addObject:title];
    [pasteboard setPropertyList:paths.get() forType:NSFilenamesPboardType];
    
    m_promisedImage = nullptr;
}
#endif

void WebViewImpl::pasteboardChangedOwner(NSPasteboard *pasteboard)
{
    m_promisedImage = nullptr;
    m_promisedFilename = "";
    m_promisedURL = "";
}

void WebViewImpl::provideDataForPasteboard(NSPasteboard *pasteboard, NSString *type)
{
    // FIXME: need to support NSRTFDPboardType

    if ([type isEqual:NSTIFFPboardType] && m_promisedImage) {
        [pasteboard setData:(NSData *)m_promisedImage->getTIFFRepresentation() forType:NSTIFFPboardType];
        m_promisedImage = nullptr;
    }
}

static BOOL fileExists(NSString *path)
{
    struct stat statBuffer;
    return !lstat([path fileSystemRepresentation], &statBuffer);
}

static NSString *pathWithUniqueFilenameForPath(NSString *path)
{
    // "Fix" the filename of the path.
    NSString *filename = filenameByFixingIllegalCharacters([path lastPathComponent]);
    path = [[path stringByDeletingLastPathComponent] stringByAppendingPathComponent:filename];
    
    if (fileExists(path)) {
        // Don't overwrite existing file by appending "-n", "-n.ext" or "-n.ext.ext" to the filename.
        NSString *extensions = nil;
        NSString *pathWithoutExtensions;
        NSString *lastPathComponent = [path lastPathComponent];
        NSRange periodRange = [lastPathComponent rangeOfString:@"."];
        
        if (periodRange.location == NSNotFound) {
            pathWithoutExtensions = path;
        } else {
            extensions = [lastPathComponent substringFromIndex:periodRange.location + 1];
            lastPathComponent = [lastPathComponent substringToIndex:periodRange.location];
            pathWithoutExtensions = [[path stringByDeletingLastPathComponent] stringByAppendingPathComponent:lastPathComponent];
        }
        
        for (unsigned i = 1; ; i++) {
            NSString *pathWithAppendedNumber = [NSString stringWithFormat:@"%@-%d", pathWithoutExtensions, i];
            path = [extensions length] ? [pathWithAppendedNumber stringByAppendingPathExtension:extensions] : pathWithAppendedNumber;
            if (!fileExists(path))
                break;
        }
    }
    
    return path;
}

NSArray *WebViewImpl::namesOfPromisedFilesDroppedAtDestination(NSURL *dropDestination)
{
    RetainPtr<NSFileWrapper> wrapper;
    RetainPtr<NSData> data;
    
    if (m_promisedImage) {
        data = m_promisedImage->data()->createNSData();
        wrapper = adoptNS([[NSFileWrapper alloc] initRegularFileWithContents:data.get()]);
    } else
        wrapper = adoptNS([[NSFileWrapper alloc] initWithURL:[NSURL URLWithString:m_promisedURL] options:NSFileWrapperReadingImmediate error:nil]);
    
    if (wrapper)
        [wrapper setPreferredFilename:m_promisedFilename];
    else {
        LOG_ERROR("Failed to create image file.");
        return nil;
    }
    
    // FIXME: Report an error if we fail to create a file.
    NSString *path = [[dropDestination path] stringByAppendingPathComponent:[wrapper preferredFilename]];
    path = pathWithUniqueFilenameForPath(path);
    if (![wrapper writeToURL:[NSURL fileURLWithPath:path] options:NSFileWrapperWritingWithNameUpdating originalContentsURL:nil error:nullptr])
        LOG_ERROR("Failed to create image file via -[NSFileWrapper writeToURL:options:originalContentsURL:error:]");

    if (!m_promisedURL.isEmpty())
        WebCore::setMetadataURL(m_promisedURL, "", String(path));
    
    return [NSArray arrayWithObject:[path lastPathComponent]];
}

static RetainPtr<CGImageRef> takeWindowSnapshot(CGSWindowID windowID, bool captureAtNominalResolution)
{
    CGSWindowCaptureOptions options = kCGSCaptureIgnoreGlobalClipShape;
    if (captureAtNominalResolution)
        options |= kCGSWindowCaptureNominalResolution;
    RetainPtr<CFArrayRef> windowSnapshotImages = adoptCF(CGSHWCaptureWindowList(CGSMainConnectionID(), &windowID, 1, options));

    if (windowSnapshotImages && CFArrayGetCount(windowSnapshotImages.get()))
        return (CGImageRef)CFArrayGetValueAtIndex(windowSnapshotImages.get(), 0);

    // Fall back to the non-hardware capture path if we didn't get a snapshot
    // (which usually happens if the window is fully off-screen).
    CGWindowImageOption imageOptions = kCGWindowImageBoundsIgnoreFraming | kCGWindowImageShouldBeOpaque;
    if (captureAtNominalResolution)
        imageOptions |= kCGWindowImageNominalResolution;
    return adoptCF(CGWindowListCreateImage(CGRectNull, kCGWindowListOptionIncludingWindow, windowID, imageOptions));
}

RefPtr<ViewSnapshot> WebViewImpl::takeViewSnapshot()
{
    NSWindow *window = m_view.window;

    CGSWindowID windowID = (CGSWindowID)window.windowNumber;
    if (!windowID || !window.isVisible)
        return nullptr;

    RetainPtr<CGImageRef> windowSnapshotImage = takeWindowSnapshot(windowID, false);
    if (!windowSnapshotImage)
        return nullptr;

    // Work around <rdar://problem/17084993>; re-request the snapshot at kCGWindowImageNominalResolution if it was captured at the wrong scale.
    CGFloat desiredSnapshotWidth = window.frame.size.width * window.screen.backingScaleFactor;
    if (CGImageGetWidth(windowSnapshotImage.get()) != desiredSnapshotWidth)
        windowSnapshotImage = takeWindowSnapshot(windowID, true);

    if (!windowSnapshotImage)
        return nullptr;

    ViewGestureController& gestureController = ensureGestureController();

    NSRect windowCaptureRect;
    WebCore::FloatRect boundsForCustomSwipeViews = gestureController.windowRelativeBoundsForCustomSwipeViews();
    if (!boundsForCustomSwipeViews.isEmpty())
        windowCaptureRect = boundsForCustomSwipeViews;
    else {
        NSRect unobscuredBounds = m_view.bounds;
        float topContentInset = m_page.topContentInset();
        unobscuredBounds.origin.y += topContentInset;
        unobscuredBounds.size.height -= topContentInset;
        windowCaptureRect = [m_view convertRect:unobscuredBounds toView:nil];
    }

    NSRect windowCaptureScreenRect = [window convertRectToScreen:windowCaptureRect];
    CGRect windowScreenRect;
    CGSGetScreenRectForWindow(CGSMainConnectionID(), (CGSWindowID)[window windowNumber], &windowScreenRect);

    NSRect croppedImageRect = windowCaptureRect;
    croppedImageRect.origin.y = windowScreenRect.size.height - windowCaptureScreenRect.size.height - NSMinY(windowCaptureRect);

    auto croppedSnapshotImage = adoptCF(CGImageCreateWithImageInRect(windowSnapshotImage.get(), NSRectToCGRect([window convertRectToBacking:croppedImageRect])));

    auto surface = WebCore::IOSurface::createFromImage(croppedSnapshotImage.get());
    if (!surface)
        return nullptr;
    surface->setIsVolatile(true);

    return ViewSnapshot::create(WTF::move(surface));
}

void WebViewImpl::saveBackForwardSnapshotForCurrentItem()
{
    m_page.recordNavigationSnapshot();
}

void WebViewImpl::saveBackForwardSnapshotForItem(WebBackForwardListItem& item)
{
    m_page.recordNavigationSnapshot(item);
}

ViewGestureController& WebViewImpl::ensureGestureController()
{
    if (!m_gestureController)
        m_gestureController = std::make_unique<ViewGestureController>(m_page);
    return *m_gestureController;
}

void WebViewImpl::setAllowsBackForwardNavigationGestures(bool allowsBackForwardNavigationGestures)
{
    m_allowsBackForwardNavigationGestures = allowsBackForwardNavigationGestures;
    m_page.setShouldRecordNavigationSnapshots(allowsBackForwardNavigationGestures);
    m_page.setShouldUseImplicitRubberBandControl(allowsBackForwardNavigationGestures);
}

void WebViewImpl::setAllowsMagnification(bool allowsMagnification)
{
    m_allowsMagnification = allowsMagnification;
}

void WebViewImpl::setMagnification(double magnification, CGPoint centerPoint)
{
    if (magnification <= 0 || isnan(magnification) || isinf(magnification))
        [NSException raise:NSInvalidArgumentException format:@"Magnification should be a positive number"];

    dismissContentRelativeChildWindowsWithAnimation(false);

    m_page.scalePageInViewCoordinates(magnification, WebCore::roundedIntPoint(centerPoint));
}

void WebViewImpl::setMagnification(double magnification)
{
    if (magnification <= 0 || isnan(magnification) || isinf(magnification))
        [NSException raise:NSInvalidArgumentException format:@"Magnification should be a positive number"];

    dismissContentRelativeChildWindowsWithAnimation(false);

    WebCore::FloatPoint viewCenter(NSMidX([m_view bounds]), NSMidY([m_view bounds]));
    m_page.scalePageInViewCoordinates(magnification, roundedIntPoint(viewCenter));
}

double WebViewImpl::magnification() const
{
    if (m_gestureController)
        return m_gestureController->magnification();
    return m_page.pageScaleFactor();
}

void WebViewImpl::setCustomSwipeViews(NSArray *customSwipeViews)
{
    if (!customSwipeViews.count && !m_gestureController)
        return;

    Vector<RetainPtr<NSView>> views;
    views.reserveInitialCapacity(customSwipeViews.count);
    for (NSView *view in customSwipeViews)
        views.uncheckedAppend(view);

    ensureGestureController().setCustomSwipeViews(views);
}

void WebViewImpl::setCustomSwipeViewsTopContentInset(float topContentInset)
{
    ensureGestureController().setCustomSwipeViewsTopContentInset(topContentInset);
}

bool WebViewImpl::tryToSwipeWithEvent(NSEvent *event, bool ignoringPinnedState)
{
    if (!m_allowsBackForwardNavigationGestures)
        return false;

    auto& gestureController = ensureGestureController();

    bool wasIgnoringPinnedState = gestureController.shouldIgnorePinnedState();
    gestureController.setShouldIgnorePinnedState(ignoringPinnedState);

    bool handledEvent = gestureController.handleScrollWheelEvent(event);

    gestureController.setShouldIgnorePinnedState(wasIgnoringPinnedState);
    
    return handledEvent;
}

void WebViewImpl::setDidMoveSwipeSnapshotCallback(void(^callback)(CGRect))
{
    if (!m_allowsBackForwardNavigationGestures)
        return;

    ensureGestureController().setDidMoveSwipeSnapshotCallback(callback);
}

void WebViewImpl::scrollWheel(NSEvent *event)
{
    if (m_ignoresAllEvents)
        return;

    if (event.phase == NSEventPhaseBegan)
        dismissContentRelativeChildWindowsWithAnimation(false);

    if (m_allowsBackForwardNavigationGestures && ensureGestureController().handleScrollWheelEvent(event))
        return;

    NativeWebWheelEvent webEvent = NativeWebWheelEvent(event, m_view);
    m_page.handleWheelEvent(webEvent);
}

void WebViewImpl::swipeWithEvent(NSEvent *event)
{
    if (m_ignoresNonWheelEvents)
        return;

    if (!m_allowsBackForwardNavigationGestures) {
        [m_view _superSwipeWithEvent:event];
        return;
    }

    if (event.deltaX > 0.0)
        m_page.goBack();
    else if (event.deltaX < 0.0)
        m_page.goForward();
    else
        [m_view _superSwipeWithEvent:event];
}

void WebViewImpl::magnifyWithEvent(NSEvent *event)
{
    if (!m_allowsMagnification) {
#if ENABLE(MAC_GESTURE_EVENTS)
        NativeWebGestureEvent webEvent = NativeWebGestureEvent(event, m_view);
        m_page.handleGestureEvent(webEvent);
#endif
        [m_view _superMagnifyWithEvent:event];
        return;
    }

    dismissContentRelativeChildWindowsWithAnimation(false);

    auto& gestureController = ensureGestureController();

#if ENABLE(MAC_GESTURE_EVENTS)
    if (gestureController.hasActiveMagnificationGesture()) {
        gestureController.handleMagnificationGestureEvent(event, [m_view convertPoint:event.locationInWindow fromView:nil]);
        return;
    }

    NativeWebGestureEvent webEvent = NativeWebGestureEvent(event, m_view);
    m_page.handleGestureEvent(webEvent);
#else
    gestureController.handleMagnificationGestureEvent(event, [m_view convertPoint:event.locationInWindow fromView:nil]);
#endif
}

void WebViewImpl::smartMagnifyWithEvent(NSEvent *event)
{
    if (!m_allowsMagnification) {
        [m_view _superSmartMagnifyWithEvent:event];
        return;
    }

    dismissContentRelativeChildWindowsWithAnimation(false);

    ensureGestureController().handleSmartMagnificationGesture([m_view convertPoint:event.locationInWindow fromView:nil]);
}

void WebViewImpl::setLastMouseDownEvent(NSEvent *event)
{
    ASSERT(!event || event.type == NSLeftMouseDown || event.type == NSRightMouseDown || event.type == NSOtherMouseDown);

    if (event == m_lastMouseDownEvent.get())
        return;

    m_lastMouseDownEvent = event;
}

#if ENABLE(MAC_GESTURE_EVENTS)
void WebViewImpl::rotateWithEvent(NSEvent *event)
{
    NativeWebGestureEvent webEvent = NativeWebGestureEvent(event, m_view);
    m_page.handleGestureEvent(webEvent);
}
#endif

void WebViewImpl::gestureEventWasNotHandledByWebCore(NSEvent *event)
{
    [m_view _gestureEventWasNotHandledByWebCore:event];
}

void WebViewImpl::gestureEventWasNotHandledByWebCoreFromViewOnly(NSEvent *event)
{
#if ENABLE(MAC_GESTURE_EVENTS)
    if (m_gestureController)
        m_gestureController->gestureEventWasNotHandledByWebCore(event, [m_view convertPoint:event.locationInWindow fromView:nil]);
#endif
}

} // namespace WebKit

#endif // PLATFORM(MAC)
