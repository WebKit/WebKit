/*
 * Copyright (C) 2014-2016 Apple Inc. All rights reserved.
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
#import "WKImmediateActionController.h"

#if PLATFORM(MAC)

#import "APIHitTestResult.h"
#import "WKNSURLExtras.h"
#import "WebPageMessages.h"
#import "WebPageProxy.h"
#import "WebPageProxyMessages.h"
#import "WebProcessProxy.h"
#import "WebViewImpl.h"
#import <WebCore/DictionaryLookup.h>
#import <WebCore/GeometryUtilities.h>
#import <WebCore/TextIndicatorWindow.h>
#import <pal/spi/mac/LookupSPI.h>
#import <pal/spi/mac/NSMenuSPI.h>
#import <pal/spi/mac/NSPopoverSPI.h>
#import <wtf/URL.h>
#import <pal/mac/DataDetectorsSoftLink.h>
#import <pal/mac/QuickLookUISoftLink.h>

@interface WKImmediateActionController () <QLPreviewMenuItemDelegate>
@end

@interface WKAnimationController : NSObject <NSImmediateActionAnimationController>
@end

@implementation WKAnimationController
@end

@implementation WKImmediateActionController

- (instancetype)initWithPage:(NakedRef<WebKit::WebPageProxy>)page view:(NSView *)view viewImpl:(NakedRef<WebKit::WebViewImpl>)viewImpl recognizer:(NSImmediateActionGestureRecognizer *)immediateActionRecognizer
{
    self = [super init];

    if (!self)
        return nil;

    _page = page.ptr();
    _view = view;
    _viewImpl = viewImpl.ptr();
    _type = kWKImmediateActionNone;
    _immediateActionRecognizer = immediateActionRecognizer;
    _hasActiveImmediateAction = NO;

    return self;
}

- (void)willDestroyView:(NSView *)view
{
    _page = nullptr;
    _view = nil;
    _viewImpl = nullptr;
    _hitTestResultData = WebKit::WebHitTestResultData();
    _contentPreventsDefault = NO;
    
    id animationController = [_immediateActionRecognizer animationController];
    if (PAL::isQuickLookUIFrameworkAvailable() && [animationController isKindOfClass:PAL::getQLPreviewMenuItemClass()]) {
        QLPreviewMenuItem *menuItem = (QLPreviewMenuItem *)animationController;
        menuItem.delegate = nil;
    }

    _immediateActionRecognizer = nil;
    _currentActionContext = nil;
    _hasActiveImmediateAction = NO;
}

- (void)_cancelImmediateAction
{
    // Reset the recognizer by turning it off and on again.
    [_immediateActionRecognizer setEnabled:NO];
    [_immediateActionRecognizer setEnabled:YES];

    [self _clearImmediateActionState];
}

- (void)_cancelImmediateActionIfNeeded
{
    if (![_immediateActionRecognizer animationController])
        [self _cancelImmediateAction];
}

- (void)_clearImmediateActionState
{
    if (_page)
        _page->clearTextIndicator();

    if (_currentActionContext && _hasActivatedActionContext) {
        _hasActivatedActionContext = NO;
        if (PAL::isDataDetectorsFrameworkAvailable())
            [PAL::getDDActionsManagerClass() didUseActions];
    }

    _state = WebKit::ImmediateActionState::None;
    _hitTestResultData = WebKit::WebHitTestResultData();
    _contentPreventsDefault = NO;
    _type = kWKImmediateActionNone;
    _currentActionContext = nil;
    _userData = nil;
    _currentQLPreviewMenuItem = nil;
    _hasActiveImmediateAction = NO;
}

- (void)didPerformImmediateActionHitTest:(const WebKit::WebHitTestResultData&)hitTestResult contentPreventsDefault:(BOOL)contentPreventsDefault userData:(API::Object*)userData
{
    // If we've already given up on this gesture (either because it was canceled or the
    // willBeginAnimation timeout expired), we shouldn't build a new animationController for it.
    if (_state != WebKit::ImmediateActionState::Pending)
        return;

    // FIXME: This needs to use the WebKit2 callback mechanism to avoid out-of-order replies.
    _state = WebKit::ImmediateActionState::Ready;
    _hitTestResultData = hitTestResult;
    _contentPreventsDefault = contentPreventsDefault;
    _userData = userData;

    [self _updateImmediateActionItem];
    [self _cancelImmediateActionIfNeeded];
}

- (void)dismissContentRelativeChildWindows
{
    _page->setMaintainsInactiveSelection(false);
    [_currentQLPreviewMenuItem close];
}

- (BOOL)hasActiveImmediateAction
{
    return _hasActiveImmediateAction;
}

#pragma mark NSImmediateActionGestureRecognizerDelegate

- (void)immediateActionRecognizerWillPrepare:(NSImmediateActionGestureRecognizer *)immediateActionRecognizer
{
    if (immediateActionRecognizer != _immediateActionRecognizer)
        return;

    _viewImpl->prepareForImmediateActionAnimation();

    _viewImpl->dismissContentRelativeChildWindowsWithAnimation(true);

    _page->setMaintainsInactiveSelection(true);

    _state = WebKit::ImmediateActionState::Pending;
    immediateActionRecognizer.animationController = nil;

    _page->performImmediateActionHitTestAtLocation([immediateActionRecognizer locationInView:immediateActionRecognizer.view]);
}

- (void)immediateActionRecognizerWillBeginAnimation:(NSImmediateActionGestureRecognizer *)immediateActionRecognizer
{
    if (immediateActionRecognizer != _immediateActionRecognizer)
        return;

    if (_state == WebKit::ImmediateActionState::None)
        return;

    _hasActiveImmediateAction = YES;

    // FIXME: We need to be able to cancel this if the gesture recognizer is cancelled.
    // FIXME: Connection can be null if the process is closed; we should clean up better in that case.
    if (_state == WebKit::ImmediateActionState::Pending) {
        if (auto* connection = _page->process().connection()) {
            bool receivedReply = connection->waitForAndDispatchImmediately<Messages::WebPageProxy::DidPerformImmediateActionHitTest>(_page->webPageID(), Seconds::fromMilliseconds(500));
            if (!receivedReply)
                _state = WebKit::ImmediateActionState::TimedOut;
        }
    }

    if (_state != WebKit::ImmediateActionState::Ready) {
        [self _updateImmediateActionItem];
        [self _cancelImmediateActionIfNeeded];
    }

    if (_currentActionContext) {
        _hasActivatedActionContext = YES;
        if (PAL::isDataDetectorsFrameworkAvailable()) {
            if (![PAL::getDDActionsManagerClass() shouldUseActionsWithContext:_currentActionContext.get()])
                [self _cancelImmediateAction];
        }
    }
}

- (void)immediateActionRecognizerDidUpdateAnimation:(NSImmediateActionGestureRecognizer *)immediateActionRecognizer
{
    if (immediateActionRecognizer != _immediateActionRecognizer)
        return;

    _page->immediateActionDidUpdate();
    if (_contentPreventsDefault)
        return;

    _page->setTextIndicatorAnimationProgress([immediateActionRecognizer animationProgress]);
}

- (void)immediateActionRecognizerDidCancelAnimation:(NSImmediateActionGestureRecognizer *)immediateActionRecognizer
{
    if (immediateActionRecognizer != _immediateActionRecognizer)
        return;

    _page->immediateActionDidCancel();

    _viewImpl->cancelImmediateActionAnimation();

    _page->setTextIndicatorAnimationProgress(0);
    [self _clearImmediateActionState];
    _page->setMaintainsInactiveSelection(false);
}

- (void)immediateActionRecognizerDidCompleteAnimation:(NSImmediateActionGestureRecognizer *)immediateActionRecognizer
{
    if (immediateActionRecognizer != _immediateActionRecognizer)
        return;

    _page->immediateActionDidComplete();

    _viewImpl->completeImmediateActionAnimation();

    _page->setTextIndicatorAnimationProgress(1);
}

- (RefPtr<API::HitTestResult>)_webHitTestResult
{
    RefPtr<API::HitTestResult> hitTestResult;
    if (_state == WebKit::ImmediateActionState::Ready)
        hitTestResult = API::HitTestResult::create(_hitTestResultData);
    else
        hitTestResult = _page->lastMouseMoveHitTestResult();

    return hitTestResult;
}

#pragma mark Immediate actions

- (id <NSImmediateActionAnimationController>)_defaultAnimationController
{
    if (_contentPreventsDefault)
        return adoptNS([[WKAnimationController alloc] init]).autorelease();

    RefPtr<API::HitTestResult> hitTestResult = [self _webHitTestResult];

    if (!hitTestResult)
        return nil;

    String absoluteLinkURL = hitTestResult->absoluteLinkURL();
    if (!absoluteLinkURL.isEmpty()) {
        if (WTF::protocolIs(absoluteLinkURL, "file"_s)) {
            _type = kWKImmediateActionNone;
            return nil;
        }

        if (WTF::protocolIs(absoluteLinkURL, "mailto"_s)) {
            _type = kWKImmediateActionMailtoLink;
            return [self _animationControllerForDataDetectedLink];
        }

        if (WTF::protocolIs(absoluteLinkURL, "tel"_s)) {
            _type = kWKImmediateActionTelLink;
            return [self _animationControllerForDataDetectedLink];
        }

        if (WTF::protocolIsInHTTPFamily(absoluteLinkURL)) {
            _type = kWKImmediateActionLinkPreview;

            QLPreviewMenuItem *item = [NSMenuItem standardQuickLookMenuItem];
            item.previewStyle = QLPreviewStylePopover;
            item.delegate = self;
            _currentQLPreviewMenuItem = item;

            if (auto textIndicator = _hitTestResultData.linkTextIndicator.get())
                _page->setTextIndicator(textIndicator->data());

            return (id<NSImmediateActionAnimationController>)item;
        }
    }

    if (hitTestResult->isTextNode() || hitTestResult->isOverTextInsideFormControlElement()) {
        if (auto animationController = [self _animationControllerForDataDetectedText]) {
            _type = kWKImmediateActionDataDetectedItem;
            return animationController;
        }

        if (auto animationController = [self _animationControllerForText]) {
            _type = kWKImmediateActionLookupText;
            return animationController;
        }
    }

    return nil;
}

- (void)_updateImmediateActionItem
{
    _type = kWKImmediateActionNone;

    id <NSImmediateActionAnimationController> defaultAnimationController = [self _defaultAnimationController];

    if (_contentPreventsDefault) {
        [_immediateActionRecognizer.get() setAnimationController:defaultAnimationController];
        return;
    }

    RefPtr<API::HitTestResult> hitTestResult = [self _webHitTestResult];
    if (!hitTestResult) {
        [self _cancelImmediateAction];
        return;
    }

    id customClientAnimationController = _page->immediateActionAnimationControllerForHitTestResult(hitTestResult, _type, _userData);
    if (customClientAnimationController == [NSNull null]) {
        [self _cancelImmediateAction];
        return;
    }

    if (customClientAnimationController && [customClientAnimationController conformsToProtocol:@protocol(NSImmediateActionAnimationController)])
        [_immediateActionRecognizer setAnimationController:(id <NSImmediateActionAnimationController>)customClientAnimationController];
    else
        [_immediateActionRecognizer setAnimationController:defaultAnimationController];
}

#pragma mark QLPreviewMenuItemDelegate implementation

- (NSView *)menuItem:(NSMenuItem *)menuItem viewAtScreenPoint:(NSPoint)screenPoint
{
    return _view;
}

- (id<QLPreviewItem>)menuItem:(NSMenuItem *)menuItem previewItemAtPoint:(NSPoint)point
{
    if (!_view)
        return nil;

    RefPtr<API::HitTestResult> hitTestResult = [self _webHitTestResult];
    if (!hitTestResult)
        return nil;

    return [NSURL _web_URLWithWTFString:hitTestResult->absoluteLinkURL()];
}

- (NSRectEdge)menuItem:(NSMenuItem *)menuItem preferredEdgeForPoint:(NSPoint)point
{
    return NSMaxYEdge;
}

- (void)menuItemDidClose:(NSMenuItem *)menuItem
{
    [self _clearImmediateActionState];
}

- (NSRect)menuItem:(NSMenuItem *)menuItem itemFrameForPoint:(NSPoint)point
{
    if (!_view)
        return NSZeroRect;

    RefPtr<API::HitTestResult> hitTestResult = [self _webHitTestResult];
    if (!hitTestResult)
        return NSZeroRect;

    return [_view convertRect:hitTestResult->elementBoundingBox() toView:nil];
}

- (NSSize)menuItem:(NSMenuItem *)menuItem maxSizeForPoint:(NSPoint)point
{
    if (!_view)
        return NSZeroSize;

    NSSize screenSize = _view.window.screen.frame.size;
    WebCore::FloatRect largestRect = WebCore::largestRectWithAspectRatioInsideRect(screenSize.width / screenSize.height, _view.bounds);
    return NSMakeSize(largestRect.width() * 0.75, largestRect.height() * 0.75);
}

#pragma mark Data Detectors actions

- (id<NSImmediateActionAnimationController>)_animationControllerForDataDetectedText
{
    if (!PAL::isDataDetectorsFrameworkAvailable())
        return nil;

    DDActionContext *actionContext = _hitTestResultData.platformData.detectedDataActionContext.get();
    if (!actionContext)
        return nil;

    actionContext.altMode = YES;
    actionContext.immediate = YES;
    if (![[PAL::getDDActionsManagerClass() sharedManager] hasActionsForResult:actionContext.mainResult actionContext:actionContext])
        return nil;

    RefPtr<WebKit::WebPageProxy> page = _page.get();
    WebCore::PageOverlay::PageOverlayID overlayID = _hitTestResultData.platformData.detectedDataOriginatingPageOverlay;
    _currentActionContext = [actionContext contextForView:_view altMode:YES interactionStartedHandler:^() {
        page->send(Messages::WebPage::DataDetectorsDidPresentUI(overlayID));
    } interactionChangedHandler:^() {
        if (_hitTestResultData.platformData.detectedDataTextIndicator)
            page->setTextIndicator(_hitTestResultData.platformData.detectedDataTextIndicator->data());
        page->send(Messages::WebPage::DataDetectorsDidChangeUI(overlayID));
    } interactionStoppedHandler:^() {
        page->send(Messages::WebPage::DataDetectorsDidHideUI(overlayID));
        [self _clearImmediateActionState];
    }];

    [_currentActionContext setHighlightFrame:[_view.window convertRectToScreen:[_view convertRect:_hitTestResultData.platformData.detectedDataBoundingBox toView:nil]]];

    NSArray *menuItems = [[PAL::getDDActionsManagerClass() sharedManager] menuItemsForResult:[_currentActionContext mainResult] actionContext:_currentActionContext.get()];

    if (menuItems.count != 1)
        return nil;

    return (id<NSImmediateActionAnimationController>)menuItems.lastObject;
}

- (id<NSImmediateActionAnimationController>)_animationControllerForDataDetectedLink
{
    if (!PAL::isDataDetectorsFrameworkAvailable())
        return nil;

    auto actionContext = adoptNS([PAL::allocDDActionContextInstance() init]);

    if (!actionContext)
        return nil;

    [actionContext setAltMode:YES];
    [actionContext setImmediate:YES];

    RefPtr<WebKit::WebPageProxy> page = _page.get();
    _currentActionContext = [actionContext contextForView:_view altMode:YES interactionStartedHandler:^() {
    } interactionChangedHandler:^() {
        if (_hitTestResultData.linkTextIndicator)
            page->setTextIndicator(_hitTestResultData.linkTextIndicator->data());
    } interactionStoppedHandler:^() {
        [self _clearImmediateActionState];
    }];

    [_currentActionContext setHighlightFrame:[_view.window convertRectToScreen:[_view convertRect:_hitTestResultData.elementBoundingBox toView:nil]]];

    RefPtr<API::HitTestResult> hitTestResult = [self _webHitTestResult];
    if (!hitTestResult)
        return nil;

    NSArray *menuItems = [[PAL::getDDActionsManagerClass() sharedManager] menuItemsForTargetURL:hitTestResult->absoluteLinkURL() actionContext:_currentActionContext.get()];

    if (menuItems.count != 1)
        return nil;

    return (id<NSImmediateActionAnimationController>)menuItems.lastObject;
}

#pragma mark Text action

- (id<NSImmediateActionAnimationController>)_animationControllerForText
{
    if (_state != WebKit::ImmediateActionState::Ready)
        return nil;

    WebCore::DictionaryPopupInfo dictionaryPopupInfo = _hitTestResultData.dictionaryPopupInfo;
    if (!dictionaryPopupInfo.platformData.attributedString)
        return nil;

    _viewImpl->prepareForDictionaryLookup();

    return WebCore::DictionaryLookup::animationControllerForPopup(dictionaryPopupInfo, _view, [self](WebCore::TextIndicator& textIndicator) {
        _viewImpl->setTextIndicator(textIndicator, WebCore::TextIndicatorLifetime::Permanent);
    }, nullptr, [self]() {
        _viewImpl->clearTextIndicatorWithAnimation(WebCore::TextIndicatorDismissalAnimation::None);
    });
}

@end

#endif // PLATFORM(MAC)
