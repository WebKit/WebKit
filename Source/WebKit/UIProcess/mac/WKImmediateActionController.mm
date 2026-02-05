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
#import "MessageSenderInlines.h"
#import "WKNSURLExtras.h"
#import "WebFrameProxy.h"
#import "WebPageMessages.h"
#import "WebPageProxy.h"
#import "WebPageProxyMessages.h"
#import "WebProcessProxy.h"
#import "WebViewImpl.h"
#import <WebCore/DictionaryLookup.h>
#import <WebCore/GeometryUtilities.h>
#import <pal/spi/mac/LookupSPI.h>
#import <pal/spi/mac/NSMenuSPI.h>
#import <pal/spi/mac/NSPopoverSPI.h>
#import <wtf/URL.h>
#import <wtf/cocoa/TypeCastsCocoa.h>

#import <pal/mac/DataDetectorsSoftLink.h>
#import <pal/mac/QuickLookUISoftLink.h>

@interface WKImmediateActionController () <QLPreviewMenuItemDelegate>
@end

@interface WKAnimationController : NSObject <NSImmediateActionAnimationController>
@end

@implementation WKAnimationController
@end

@implementation WKImmediateActionController

- (instancetype)initWithPage:(std::reference_wrapper<WebKit::WebPageProxy>)page view:(NSView *)view viewImpl:(std::reference_wrapper<WebKit::WebViewImpl>)viewImpl recognizer:(NSImmediateActionGestureRecognizer *)immediateActionRecognizer
{
    self = [super init];

    if (!self)
        return nil;

    _page = page.get();
    _view = view;
    _viewImpl = viewImpl.get();
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
    
    RetainPtr<id> animationController = [_immediateActionRecognizer animationController];
    if (PAL::isQuickLookUIFrameworkAvailable() && [animationController isKindOfClass:PAL::getQLPreviewMenuItemClassSingleton()]) {
        RetainPtr menuItem = (QLPreviewMenuItem *)animationController.get();
        menuItem.get().delegate = nil;
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
        protect(_page.get())->clearTextIndicator();

    if (_currentActionContext && _hasActivatedActionContext) {
        _hasActivatedActionContext = NO;
        if (PAL::isDataDetectorsFrameworkAvailable())
            [PAL::getDDActionsManagerClassSingleton() didUseActions];
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
    protect(_page.get())->setMaintainsInactiveSelection(false);
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

    {
        CheckedPtr viewImpl = _viewImpl.get();
        viewImpl->prepareForImmediateActionAnimation();
        viewImpl->dismissContentRelativeChildWindowsWithAnimation(true);
    }

    protect(_page.get())->setMaintainsInactiveSelection(true);

    _state = WebKit::ImmediateActionState::Pending;
    immediateActionRecognizer.animationController = nil;

    if (!_page->mainFrame())
        return;

    protect(_page.get())->performImmediateActionHitTestAtLocation(_page->mainFrame()->frameID(), [immediateActionRecognizer locationInView:retainPtr(immediateActionRecognizer.view).get()]);
}

- (void)immediateActionRecognizerWillBeginAnimation:(NSImmediateActionGestureRecognizer *)immediateActionRecognizer
{
    if (immediateActionRecognizer != _immediateActionRecognizer)
        return;

    Ref mainFrameProcess = protect(_page.get())->legacyMainFrameProcess();
    if (_state == WebKit::ImmediateActionState::None || !mainFrameProcess->hasConnection())
        return;

    _hasActiveImmediateAction = YES;

    // FIXME: We need to be able to cancel this if the gesture recognizer is cancelled.
    // FIXME: Connection can be null if the process is closed; we should clean up better in that case.
    if (_state == WebKit::ImmediateActionState::Pending) {
        Ref connection = mainFrameProcess->connection();
        bool receivedReply = connection->waitForAndDispatchImmediately<Messages::WebPageProxy::DidPerformImmediateActionHitTest>(protect(_page.get())->webPageIDInMainFrameProcess(), 500_ms) == IPC::Error::NoError;
        if (!receivedReply)
            _state = WebKit::ImmediateActionState::TimedOut;
    }

    if (_state != WebKit::ImmediateActionState::Ready) {
        [self _updateImmediateActionItem];
        [self _cancelImmediateActionIfNeeded];
    }

    if (_currentActionContext) {
        _hasActivatedActionContext = YES;
        if (PAL::isDataDetectorsFrameworkAvailable()) {
            if (![PAL::getDDActionsManagerClassSingleton() shouldUseActionsWithContext:_currentActionContext.get()])
                [self _cancelImmediateAction];
        }
    }
}

- (void)immediateActionRecognizerDidUpdateAnimation:(NSImmediateActionGestureRecognizer *)immediateActionRecognizer
{
    if (immediateActionRecognizer != _immediateActionRecognizer)
        return;

    protect(_page.get())->immediateActionDidUpdate();
    if (_contentPreventsDefault)
        return;

    protect(_page.get())->setTextIndicatorAnimationProgress([immediateActionRecognizer animationProgress]);
}

- (void)immediateActionRecognizerDidCancelAnimation:(NSImmediateActionGestureRecognizer *)immediateActionRecognizer
{
    if (immediateActionRecognizer != _immediateActionRecognizer)
        return;

    protect(_page.get())->immediateActionDidCancel();

    CheckedPtr { _viewImpl.get() }->cancelImmediateActionAnimation();

    protect(_page.get())->setTextIndicatorAnimationProgress(0);
    [self _clearImmediateActionState];
    protect(_page.get())->setMaintainsInactiveSelection(false);
}

- (void)immediateActionRecognizerDidCompleteAnimation:(NSImmediateActionGestureRecognizer *)immediateActionRecognizer
{
    if (immediateActionRecognizer != _immediateActionRecognizer)
        return;

    protect(_page.get())->immediateActionDidComplete();

    CheckedPtr { _viewImpl.get() }->completeImmediateActionAnimation();

    protect(_page.get())->setTextIndicatorAnimationProgress(1);
}

- (RefPtr<API::HitTestResult>)_webHitTestResult
{
    RefPtr<API::HitTestResult> hitTestResult;
    RefPtr page = _page.get();
    if (_state == WebKit::ImmediateActionState::Ready)
        hitTestResult = API::HitTestResult::create(_hitTestResultData, page.get());
    else
        hitTestResult = page->lastMouseMoveHitTestResult();

    return hitTestResult;
}

#pragma mark Immediate actions

- (id<NSImmediateActionAnimationController>)_defaultAnimationController
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

            RetainPtr item = [NSMenuItem standardQuickLookMenuItem];
            item.get().previewStyle = QLPreviewStylePopover;
            item.get().delegate = self;
            _currentQLPreviewMenuItem = item.get();

            if (RefPtr textIndicator = _hitTestResultData.linkTextIndicator)
                protect(_page.get())->setTextIndicator(WTF::move(textIndicator), WebCore::TextIndicatorLifetime::Permanent);

            return (id<NSImmediateActionAnimationController>)item.autorelease();
        }
    }

    if (hitTestResult->isTextNode() || hitTestResult->isOverTextInsideFormControlElement()) {
        if (RetainPtr animationController = [self _animationControllerForDataDetectedText]) {
            _type = kWKImmediateActionDataDetectedItem;
            return animationController.autorelease();
        }

        if (RetainPtr animationController = [self _animationControllerForText]) {
            _type = kWKImmediateActionLookupText;
            return animationController.autorelease();
        }
    }

    return nil;
}

- (void)_updateImmediateActionItem
{
    _type = kWKImmediateActionNone;

    RetainPtr<id<NSImmediateActionAnimationController>> defaultAnimationController = [self _defaultAnimationController];

    if (_contentPreventsDefault) {
        [_immediateActionRecognizer.get() setAnimationController:defaultAnimationController.get()];
        return;
    }

    RefPtr<API::HitTestResult> hitTestResult = [self _webHitTestResult];
    if (!hitTestResult) {
        [self _cancelImmediateAction];
        return;
    }

    RetainPtr<id> customClientAnimationController = protect(_page.get())->immediateActionAnimationControllerForHitTestResult(hitTestResult, _type, _userData);
    if (customClientAnimationController.get() == [NSNull null]) {
        [self _cancelImmediateAction];
        return;
    }

    // No need to protect @protocol()
    SUPPRESS_UNCOUNTED_ARG if (customClientAnimationController && [customClientAnimationController conformsToProtocol:@protocol(NSImmediateActionAnimationController)])
        [_immediateActionRecognizer setAnimationController:(id<NSImmediateActionAnimationController>)customClientAnimationController.get()];
    else
        [_immediateActionRecognizer setAnimationController:defaultAnimationController.get()];
}

#pragma mark QLPreviewMenuItemDelegate implementation

- (NSView *)menuItem:(NSMenuItem *)menuItem viewAtScreenPoint:(NSPoint)screenPoint
{
    return _view.getAutoreleased();
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

    return [_view.get() convertRect:hitTestResult->elementBoundingBox() toView:nil];
}

- (NSSize)menuItem:(NSMenuItem *)menuItem maxSizeForPoint:(NSPoint)point
{
    if (!_view)
        return NSZeroSize;

    RetainPtr view = _view.get();
    NSSize screenSize = view.get().window.screen.frame.size;
    WebCore::FloatRect largestRect = WebCore::largestRectWithAspectRatioInsideRect(screenSize.width / screenSize.height, view.get().bounds);
    return NSMakeSize(largestRect.width() * 0.75, largestRect.height() * 0.75);
}

#pragma mark Data Detectors actions

- (id<NSImmediateActionAnimationController>)_animationControllerForDataDetectedText
{
    if (!PAL::isDataDetectorsFrameworkAvailable())
        return nil;

    auto& detectedContext = _hitTestResultData.platformData.detectedDataActionContext;
    if (!detectedContext)
        return nil;

    RetainPtr actionContext = detectedContext->context.get();
    if (!actionContext)
        return nil;

    actionContext.get().altMode = YES;
    actionContext.get().immediate = YES;
    if (![[PAL::getDDActionsManagerClassSingleton() sharedManager] hasActionsForResult:RetainPtr { actionContext.get().mainResult }.get() actionContext:actionContext.get()])
        return nil;

    RefPtr<WebKit::WebPageProxy> page = _page.get();
    RetainPtr view = _view.get();
    WebCore::PageOverlay::PageOverlayID overlayID = _hitTestResultData.platformData.detectedDataOriginatingPageOverlay;
    _currentActionContext = (WKDDActionContext *)[actionContext contextForView:view.get() altMode:YES interactionStartedHandler:^() {
        protect(page->legacyMainFrameProcess())->send(Messages::WebPage::DataDetectorsDidPresentUI(overlayID), page->webPageIDInMainFrameProcess());
    } interactionChangedHandler:^() {
        if (RefPtr detectedDataTextIndicator = _hitTestResultData.platformData.detectedDataTextIndicator)
            page->setTextIndicator(WTF::move(detectedDataTextIndicator), WebCore::TextIndicatorLifetime::Permanent);
        protect(page->legacyMainFrameProcess())->send(Messages::WebPage::DataDetectorsDidChangeUI(overlayID), page->webPageIDInMainFrameProcess());
    } interactionStoppedHandler:^() {
        protect(page->legacyMainFrameProcess())->send(Messages::WebPage::DataDetectorsDidHideUI(overlayID), page->webPageIDInMainFrameProcess());
        [self _clearImmediateActionState];
    }];

    [_currentActionContext setHighlightFrame:[retainPtr(view.get().window) convertRectToScreen:[view convertRect:_hitTestResultData.platformData.detectedDataBoundingBox toView:nil]]];

    RetainPtr menuItems = [[PAL::getDDActionsManagerClassSingleton() sharedManager] menuItemsForResult:RetainPtr { [_currentActionContext mainResult] }.get() actionContext:_currentActionContext.get()];

    if (menuItems.get().count != 1)
        return nil;

    return (id<NSImmediateActionAnimationController>)menuItems.get().lastObject;
}

- (id<NSImmediateActionAnimationController>)_animationControllerForDataDetectedLink
{
    if (!PAL::isDataDetectorsFrameworkAvailable())
        return nil;

    SUPPRESS_UNRETAINED_ARG RetainPtr actionContext = adoptNS([PAL::allocWKDDActionContextInstance() init]);
    if (!actionContext)
        return nil;

    [actionContext setAltMode:YES];
    [actionContext setImmediate:YES];

    RefPtr<WebKit::WebPageProxy> page = _page.get();
    RetainPtr view = _view.get();
    _currentActionContext = (WKDDActionContext *)[actionContext contextForView:view.get() altMode:YES interactionStartedHandler:^() {
    } interactionChangedHandler:^() {
        if (RefPtr linkTextIndicator = _hitTestResultData.linkTextIndicator)
            page->setTextIndicator(WTF::move(linkTextIndicator), WebCore::TextIndicatorLifetime::Permanent);
    } interactionStoppedHandler:^() {
        [self _clearImmediateActionState];
    }];

    [_currentActionContext setHighlightFrame:[retainPtr(view.get().window) convertRectToScreen:[view convertRect:_hitTestResultData.elementBoundingBox toView:nil]]];

    RefPtr<API::HitTestResult> hitTestResult = [self _webHitTestResult];
    if (!hitTestResult)
        return nil;

    RetainPtr menuItems = [[PAL::getDDActionsManagerClassSingleton() sharedManager] menuItemsForTargetURL:hitTestResult->absoluteLinkURL().createNSString().get() actionContext:_currentActionContext.get()];

    if (menuItems.get().count != 1)
        return nil;

    return (id<NSImmediateActionAnimationController>)menuItems.get().lastObject;
}

#pragma mark Text action

- (id<NSImmediateActionAnimationController>)_animationControllerForText
{
    if (_state != WebKit::ImmediateActionState::Ready)
        return nil;

    WebCore::DictionaryPopupInfo dictionaryPopupInfo = _hitTestResultData.dictionaryPopupInfo;

#if ENABLE(LEGACY_PDFKIT_PLUGIN)
    if (!dictionaryPopupInfo.platformData.attributedString.nsAttributedString())
        return nil;
#else
    if (!dictionaryPopupInfo.text)
        return nil;
#endif

    CheckedPtr { _viewImpl.get() }->prepareForDictionaryLookup();
    return WebCore::DictionaryLookup::animationControllerForPopup(dictionaryPopupInfo, _view.get().get(), [self](WebCore::TextIndicator& textIndicator) {
        protect(_page.get())->setTextIndicator(textIndicator, WebCore::TextIndicatorLifetime::Permanent);
    }, nullptr, [strongSelf = retainPtr(self)]() {
        protect(strongSelf->_page.get())->clearTextIndicatorWithAnimation(WebCore::TextIndicatorDismissalAnimation::None);
    });
}

@end

#endif // PLATFORM(MAC)
