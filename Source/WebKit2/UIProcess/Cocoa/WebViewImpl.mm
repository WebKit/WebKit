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

#import "GenericCallback.h"
#import "Logging.h"
#import "NativeWebKeyboardEvent.h"
#import "WKFullScreenWindowController.h"
#import "WKViewLayoutStrategy.h"
#import "WKWebView.h"
#import "WebPageProxy.h"
#import <HIToolbox/CarbonEventsCore.h>
#import <WebCore/KeypressCommand.h>
#import <WebCore/NSWindowSPI.h>
#import <WebCore/ViewState.h>
#import <WebCore/WebCoreFullScreenPlaceholderView.h>
#import <WebCore/WebCoreFullScreenWindow.h>

namespace WebKit {

WebViewImpl::WebViewImpl(NSView <WebViewImplDelegate> *view, WebPageProxy& page)
    : m_view(view)
    , m_page(page)
    , m_intrinsicContentSize(CGSizeMake(NSViewNoInstrinsicMetric, NSViewNoInstrinsicMetric))
    , m_layoutStrategy([WKViewLayoutStrategy layoutStrategyWithPage:m_page view:m_view viewImpl:*this mode:kWKLayoutModeViewSize])
{
}

WebViewImpl::~WebViewImpl()
{
    ASSERT(!m_inSecureInputState);

    [m_layoutStrategy invalidate];
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
        m_willBecomeFirstResponderAgain = NO;
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

    m_willBecomeFirstResponderAgain = NO;
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


} // namespace WebKit

#endif // PLATFORM(MAC)
