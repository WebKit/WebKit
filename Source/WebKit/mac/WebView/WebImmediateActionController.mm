/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#import "WebImmediateActionController.h"

#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101000

#import "DOMElementInternal.h"
#import "DOMNodeInternal.h"
#import "DOMRangeInternal.h"
#import "WebElementDictionary.h"
#import "WebFrameInternal.h"
#import "WebHTMLView.h"
#import "WebHTMLViewInternal.h"
#import "WebUIDelegatePrivate.h"
#import "WebViewInternal.h"
#import <WebCore/DataDetection.h>
#import <WebCore/DataDetectorsSPI.h>
#import <WebCore/EventHandler.h>
#import <WebCore/Frame.h>
#import <WebCore/NSMenuSPI.h>
#import <WebCore/SoftLinking.h>
#import <WebCore/TextIndicator.h>
#import <objc/objc-class.h>
#import <objc/objc.h>

SOFT_LINK_FRAMEWORK_IN_UMBRELLA(Quartz, QuickLookUI)
SOFT_LINK_CLASS(QuickLookUI, QLPreviewMenuItem)

@interface WebImmediateActionController () <QLPreviewMenuItemDelegate>
@end

using namespace WebCore;

@implementation WebImmediateActionController

- (instancetype)initWithWebView:(WebView *)webView recognizer:(NSImmediateActionGestureRecognizer *)immediateActionRecognizer
{
    if (!(self = [super init]))
        return nil;

    _webView = webView;
    _type = WebImmediateActionNone;
    _immediateActionRecognizer = immediateActionRecognizer;

    return self;
}

- (void)webViewClosed
{
    _webView = nil;
    _immediateActionRecognizer = nil;
    _currentActionContext = nil;
}

- (void)webView:(WebView *)webView willHandleMouseDown:(NSEvent *)event
{
    [self _clearImmediateActionState];
}

- (void)_cancelImmediateAction
{
    // Reset the recognizer by turning it off and on again.
    _immediateActionRecognizer.enabled = NO;
    _immediateActionRecognizer.enabled = YES;

    [self _clearImmediateActionState];
}

- (void)_clearImmediateActionState
{
    _type = WebImmediateActionNone;
    _currentActionContext = nil;
    _immediateActionRecognizer.animationController = nil;
}

- (void)performHitTestAtPoint:(NSPoint)viewPoint
{
    Frame* coreFrame = core([[[[_webView _selectedOrMainFrame] frameView] documentView] _frame]);
    if (!coreFrame)
        return;
    _hitTestResult = coreFrame->eventHandler().hitTestResultAtPoint(IntPoint(viewPoint));
}

#pragma mark NSImmediateActionGestureRecognizerDelegate

- (void)immediateActionRecognizerWillPrepare:(NSImmediateActionGestureRecognizer *)immediateActionRecognizer
{
    if (!_webView)
        return;

    if (immediateActionRecognizer != _immediateActionRecognizer)
        return;

    WebHTMLView *documentView = [[[_webView _selectedOrMainFrame] frameView] documentView];
    NSPoint locationInDocumentView = [immediateActionRecognizer locationInView:documentView];
    [self performHitTestAtPoint:locationInDocumentView];
    [self _updateImmediateActionItem];

    if (!_immediateActionRecognizer.animationController) {
        [self _cancelImmediateAction];
        return;
    }

    if (_currentActionContext) {
        _hasActivatedActionContext = YES;
        if (![getDDActionsManagerClass() shouldUseActionsWithContext:_currentActionContext.get()])
            [self _cancelImmediateAction];
    }
}

- (void)immediateActionRecognizerWillBeginAnimation:(NSImmediateActionGestureRecognizer *)immediateActionRecognizer
{
    if (immediateActionRecognizer != _immediateActionRecognizer)
        return;

    // FIXME: Add support for the types of functionality provided in Action menu's menuNeedsUpdate.
}

- (void)immediateActionRecognizerDidCancelAnimation:(NSImmediateActionGestureRecognizer *)immediateActionRecognizer
{
    if (immediateActionRecognizer != _immediateActionRecognizer)
        return;

    [self _clearImmediateActionState];
}

- (void)immediateActionRecognizerDidCompleteAnimation:(NSImmediateActionGestureRecognizer *)immediateActionRecognizer
{
    if (immediateActionRecognizer != _immediateActionRecognizer)
        return;

    // FIXME: Add support for the types of functionality provided in Action menu's willOpenMenu.
}

#pragma mark Immediate actions

- (id <NSImmediateActionAnimationController>)_defaultAnimationController
{
    NSURL *url = _hitTestResult.absoluteLinkURL();
    NSString *absoluteURLString = [url absoluteString];
    if (url && WebCore::protocolIsInHTTPFamily(absoluteURLString)) {
        _type = WebImmediateActionLinkPreview;

        RetainPtr<QLPreviewMenuItem> qlPreviewLinkItem = [NSMenuItem standardQuickLookMenuItem];
        [qlPreviewLinkItem setPreviewStyle:QLPreviewStylePopover];
        [qlPreviewLinkItem setDelegate:self];
        return (id <NSImmediateActionAnimationController>)qlPreviewLinkItem.get();
    }

    Node* node = _hitTestResult.innerNode();
    if ((node && node->isTextNode()) || _hitTestResult.isOverTextInsideFormControlElement()) {
        if (NSMenuItem *immediateActionItem = [self _menuItemForDataDetectedText]) {
            _type = WebImmediateActionDataDetectedItem;
            return (id<NSImmediateActionAnimationController>)immediateActionItem;
        }
    }

    return nil;
}

- (void)_updateImmediateActionItem
{
    _type = WebImmediateActionNone;

    id <NSImmediateActionAnimationController> defaultAnimationController = [self _defaultAnimationController];

    // Allow clients the opportunity to override the default immediate action.
    id customClientAnimationController = nil;
    if ([[_webView UIDelegate] respondsToSelector:@selector(_webView:immediateActionAnimationControllerForHitTestResult:withType:)]) {
        RetainPtr<WebElementDictionary> webHitTestResult = adoptNS([[WebElementDictionary alloc] initWithHitTestResult:_hitTestResult]);
        customClientAnimationController = [[_webView UIDelegate] _webView:_webView immediateActionAnimationControllerForHitTestResult:webHitTestResult.get() withType:_type];
    }

    if (customClientAnimationController == [NSNull null]) {
        [self _cancelImmediateAction];
        return;
    }
    if (customClientAnimationController && [customClientAnimationController conformsToProtocol:@protocol(NSImmediateActionAnimationController)])
        _immediateActionRecognizer.animationController = (id <NSImmediateActionAnimationController>)customClientAnimationController;
    else
        _immediateActionRecognizer.animationController = defaultAnimationController;
}

#pragma mark QLPreviewMenuItemDelegate implementation

- (NSView *)menuItem:(NSMenuItem *)menuItem viewAtScreenPoint:(NSPoint)screenPoint
{
    return _webView;
}

- (id<QLPreviewItem>)menuItem:(NSMenuItem *)menuItem previewItemAtPoint:(NSPoint)point
{
    if (!_webView)
        return nil;

    return _hitTestResult.absoluteLinkURL();
}

- (NSRectEdge)menuItem:(NSMenuItem *)menuItem preferredEdgeForPoint:(NSPoint)point
{
    return NSMaxYEdge;
}

#pragma mark Data Detectors actions

- (NSMenuItem *)_menuItemForDataDetectedText
{
    RefPtr<Range> detectedDataRange;
    FloatRect detectedDataBoundingBox;
    RetainPtr<DDActionContext> actionContext;

    if ([[_webView UIDelegate] respondsToSelector:@selector(_webView:actionContextForHitTestResult:range:)]) {
        RetainPtr<WebElementDictionary> hitTestDictionary = adoptNS([[WebElementDictionary alloc] initWithHitTestResult:_hitTestResult]);

        DOMRange *customDataDetectorsRange;
        actionContext = [[_webView UIDelegate] _webView:_webView actionContextForHitTestResult:hitTestDictionary.get() range:&customDataDetectorsRange];

        if (actionContext && customDataDetectorsRange)
            detectedDataRange = core(customDataDetectorsRange);
    }

    // If the client didn't give us an action context, try to scan around the hit point.
    if (!actionContext || !detectedDataRange)
        actionContext = DataDetection::detectItemAroundHitTestResult(_hitTestResult, detectedDataBoundingBox, detectedDataRange);

    if (!actionContext || !detectedDataRange)
        return nil;

    [actionContext setAltMode:YES];
    if ([[getDDActionsManagerClass() sharedManager] respondsToSelector:@selector(hasActionsForResult:actionContext:)]) {
        if (![[getDDActionsManagerClass() sharedManager] hasActionsForResult:[actionContext mainResult] actionContext:actionContext.get()])
            return nil;
    }

    _currentDetectedDataTextIndicator = TextIndicator::createWithRange(*detectedDataRange, TextIndicatorPresentationTransition::BounceAndCrossfade);

    _currentActionContext = [actionContext contextForView:_webView altMode:YES interactionStartedHandler:^() {
    } interactionChangedHandler:^() {
        [self _showTextIndicator];
    } interactionStoppedHandler:^() {
        [self _hideTextIndicator];
    }];
    _currentDetectedDataRange = detectedDataRange;

    [_currentActionContext setHighlightFrame:[_webView.window convertRectToScreen:detectedDataBoundingBox]];

    NSArray *menuItems = [[getDDActionsManagerClass() sharedManager] menuItemsForResult:[_currentActionContext mainResult] actionContext:_currentActionContext.get()];
    if (menuItems.count != 1)
        return nil;

    if (_currentDetectedDataTextIndicator)
        _currentDetectedDataTextIndicator->setPresentationTransition(TextIndicatorPresentationTransition::Bounce);
    return menuItems.lastObject;
}

#pragma mark Text Indicator

- (void)_showTextIndicator
{
    if (_isShowingTextIndicator)
        return;

    if (_type == WebImmediateActionDataDetectedItem && _currentDetectedDataTextIndicator) {
        [_webView _setTextIndicator:_currentDetectedDataTextIndicator.get() fadeOut:NO animationCompletionHandler:^ { }];
        _isShowingTextIndicator = YES;
    }
}

- (void)_hideTextIndicator
{
    if (!_isShowingTextIndicator)
        return;

    [_webView _clearTextIndicator];
    _isShowingTextIndicator = NO;
}

@end

#endif // PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101000
