/*
 * Copyright (C) 2014-2019 Apple Inc. All rights reserved.
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

#if PLATFORM(MAC)

#import "DOMElementInternal.h"
#import "DOMNodeInternal.h"
#import "DOMRangeInternal.h"
#import "WebElementDictionary.h"
#import "WebFrameInternal.h"
#import "WebFrameView.h"
#import "WebHTMLViewInternal.h"
#import "WebUIDelegatePrivate.h"
#import "WebViewInternal.h"
#import <WebCore/DataDetection.h>
#import <WebCore/DictionaryLookup.h>
#import <WebCore/Editor.h>
#import <WebCore/EventHandler.h>
#import <WebCore/FocusController.h>
#import <WebCore/FontMetrics.h>
#import <WebCore/Frame.h>
#import <WebCore/FrameView.h>
#import <WebCore/GeometryUtilities.h>
#import <WebCore/HTMLConverter.h>
#import <WebCore/NodeRenderStyle.h>
#import <WebCore/Page.h>
#import <WebCore/RenderElement.h>
#import <WebCore/RenderObject.h>
#import <WebCore/RuntimeApplicationChecks.h>
#import <WebCore/TextIndicator.h>
#import <objc/objc-class.h>
#import <objc/objc.h>
#import <pal/spi/mac/DataDetectorsSPI.h>
#import <pal/spi/mac/LookupSPI.h>
#import <pal/spi/mac/NSMenuSPI.h>
#import <pal/spi/mac/QuickLookMacSPI.h>
#import <wtf/SoftLinking.h>

SOFT_LINK_FRAMEWORK_IN_UMBRELLA(Quartz, QuickLookUI)
SOFT_LINK_CLASS(QuickLookUI, QLPreviewMenuItem)

@interface WebImmediateActionController () <QLPreviewMenuItemDelegate>
@end

@interface WebAnimationController : NSObject <NSImmediateActionAnimationController>
@end

@implementation WebAnimationController
@end

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

    id animationController = [_immediateActionRecognizer animationController];
    if ([animationController isKindOfClass:NSClassFromString(@"QLPreviewMenuItem")]) {
        QLPreviewMenuItem *menuItem = (QLPreviewMenuItem *)animationController;
        menuItem.delegate = nil;
    }

    _immediateActionRecognizer = nil;
    _currentActionContext = nil;
}

- (void)webView:(WebView *)webView didHandleScrollWheel:(NSEvent *)event
{
    [_currentQLPreviewMenuItem close];
    [self _clearImmediateActionState];
    [_webView _clearTextIndicatorWithAnimation:WebCore::TextIndicatorWindowDismissalAnimation::None];
}

- (NSImmediateActionGestureRecognizer *)immediateActionRecognizer
{
    return _immediateActionRecognizer.get();
}

- (void)_cancelImmediateAction
{
    // Reset the recognizer by turning it off and on again.
    [_immediateActionRecognizer setEnabled:NO];
    [_immediateActionRecognizer setEnabled:YES];

    [self _clearImmediateActionState];
    [_webView _clearTextIndicatorWithAnimation:WebCore::TextIndicatorWindowDismissalAnimation::FadeOut];
}

- (void)_clearImmediateActionState
{
    if (!DataDetectorsLibrary())
        return;

    DDActionsManager *actionsManager = [getDDActionsManagerClass() sharedManager];
    [actionsManager requestBubbleClosureUnanchorOnFailure:YES];

    if (_currentActionContext && _hasActivatedActionContext) {
        _hasActivatedActionContext = NO;
        [getDDActionsManagerClass() didUseActions];
    }

    _type = WebImmediateActionNone;
    _currentActionContext = nil;
    _currentQLPreviewMenuItem = nil;
    _contentPreventsDefault = NO;
}

- (void)performHitTestAtPoint:(NSPoint)viewPoint
{
    using namespace WebCore;

    auto* coreFrame = core([_webView _selectedOrMainFrame]);
    if (!coreFrame)
        return;

    constexpr OptionSet<HitTestRequest::RequestType> hitType { HitTestRequest::ReadOnly, HitTestRequest::Active, HitTestRequest::DisallowUserAgentShadowContent, HitTestRequest::AllowChildFrameContent };
    _hitTestResult = coreFrame->eventHandler().hitTestResultAtPoint(WebCore::IntPoint(viewPoint), hitType);
    coreFrame->mainFrame().eventHandler().setImmediateActionStage(WebCore::ImmediateActionStage::PerformedHitTest);

    if (auto* element = _hitTestResult.targetElement())
        _contentPreventsDefault = element->dispatchMouseForceWillBegin();
}

#pragma mark NSImmediateActionGestureRecognizerDelegate

- (void)immediateActionRecognizerWillPrepare:(NSImmediateActionGestureRecognizer *)immediateActionRecognizer
{
    if (!_webView)
        return;

    NSView *documentView = [[[_webView _selectedOrMainFrame] frameView] documentView];
    if (![documentView isKindOfClass:[WebHTMLView class]]) {
        [self _cancelImmediateAction];
        return;
    }

    if (immediateActionRecognizer != _immediateActionRecognizer)
        return;

    [_webView _setMaintainsInactiveSelection:YES];

    NSPoint locationInDocumentView = [immediateActionRecognizer locationInView:documentView];
    [self performHitTestAtPoint:locationInDocumentView];
    [self _updateImmediateActionItem];

    if (![_immediateActionRecognizer animationController]) {
        // FIXME: We should be able to remove the dispatch_async when rdar://problem/19502927 is resolved.
        dispatch_async(dispatch_get_main_queue(), ^{
            [self _cancelImmediateAction];
        });
    }
}

- (void)immediateActionRecognizerWillBeginAnimation:(NSImmediateActionGestureRecognizer *)immediateActionRecognizer
{
    if (!DataDetectorsLibrary())
        return;

    if (immediateActionRecognizer != _immediateActionRecognizer)
        return;

    if (_currentActionContext) {
        _hasActivatedActionContext = YES;
        if (![getDDActionsManagerClass() shouldUseActionsWithContext:_currentActionContext.get()])
            [self _cancelImmediateAction];
    }
}

- (void)immediateActionRecognizerDidUpdateAnimation:(NSImmediateActionGestureRecognizer *)immediateActionRecognizer
{
    if (immediateActionRecognizer != _immediateActionRecognizer)
        return;

    if (WebCore::Frame* coreFrame = [_webView _mainCoreFrame])
        coreFrame->eventHandler().setImmediateActionStage(WebCore::ImmediateActionStage::ActionUpdated);

    if (_contentPreventsDefault)
        return;

    [_webView _setTextIndicatorAnimationProgress:[immediateActionRecognizer animationProgress]];
}

- (void)immediateActionRecognizerDidCancelAnimation:(NSImmediateActionGestureRecognizer *)immediateActionRecognizer
{
    if (immediateActionRecognizer != _immediateActionRecognizer)
        return;

    if (WebCore::Frame* coreFrame = [_webView _mainCoreFrame]) {
        WebCore::ImmediateActionStage lastStage = coreFrame->eventHandler().immediateActionStage();
        if (lastStage == WebCore::ImmediateActionStage::ActionUpdated)
            coreFrame->eventHandler().setImmediateActionStage(WebCore::ImmediateActionStage::ActionCancelledAfterUpdate);
        else
            coreFrame->eventHandler().setImmediateActionStage(WebCore::ImmediateActionStage::ActionCancelledWithoutUpdate);
    }

    [_webView _setTextIndicatorAnimationProgress:0];
    [self _clearImmediateActionState];
    [_webView _clearTextIndicatorWithAnimation:WebCore::TextIndicatorWindowDismissalAnimation::None];
    [_webView _setMaintainsInactiveSelection:NO];
}

- (void)immediateActionRecognizerDidCompleteAnimation:(NSImmediateActionGestureRecognizer *)immediateActionRecognizer
{
    if (immediateActionRecognizer != _immediateActionRecognizer)
        return;

    if (WebCore::Frame* coreFrame = [_webView _mainCoreFrame])
        coreFrame->eventHandler().setImmediateActionStage(WebCore::ImmediateActionStage::ActionCompleted);

    [_webView _setTextIndicatorAnimationProgress:1];
    [_webView _setMaintainsInactiveSelection:NO];
}

#pragma mark Immediate actions

- (id <NSImmediateActionAnimationController>)_defaultAnimationController
{
    if (_contentPreventsDefault)
        return [[[WebAnimationController alloc] init] autorelease];

    NSURL *url = _hitTestResult.absoluteLinkURL();
    String absoluteURLString = [url absoluteString];
    if (url && _hitTestResult.URLElement()) {
        if (WTF::protocolIs(absoluteURLString, "mailto")) {
            _type = WebImmediateActionMailtoLink;
            return [self _animationControllerForDataDetectedLink];
        }

        if (WTF::protocolIs(absoluteURLString, "tel")) {
            _type = WebImmediateActionTelLink;
            return [self _animationControllerForDataDetectedLink];
        }

        if (WTF::protocolIsInHTTPFamily(absoluteURLString)) {
            _type = WebImmediateActionLinkPreview;

            auto linkRange = makeRangeSelectingNodeContents(*_hitTestResult.URLElement());
            auto indicator = WebCore::TextIndicator::createWithRange(linkRange, { WebCore::TextIndicatorOption::UseBoundingRectAndPaintAllContentForComplexRanges }, WebCore::TextIndicatorPresentationTransition::FadeIn);
            if (indicator)
                [_webView _setTextIndicator:*indicator withLifetime:WebCore::TextIndicatorWindowLifetime::Permanent];

            QLPreviewMenuItem *item = [NSMenuItem standardQuickLookMenuItem];
            item.previewStyle = QLPreviewStylePopover;
            item.delegate = self;
            _currentQLPreviewMenuItem = item;
            return (id <NSImmediateActionAnimationController>)item;
        }
    }

    WebCore::Node* node = _hitTestResult.innerNode();
    if ((node && node->isTextNode()) || _hitTestResult.isOverTextInsideFormControlElement()) {
        if (auto animationController = [self _animationControllerForDataDetectedText]) {
            _type = WebImmediateActionDataDetectedItem;
            return animationController;
        }

        if (auto animationController = [self _animationControllerForText]) {
            _type = WebImmediateActionText;
            return animationController;
        }
    }

    return nil;
}

- (void)_updateImmediateActionItem
{
    _type = WebImmediateActionNone;

    id <NSImmediateActionAnimationController> defaultAnimationController = [self _defaultAnimationController];

    if (_contentPreventsDefault) {
        [_immediateActionRecognizer setAnimationController:defaultAnimationController];
        return;
    }

    // Allow clients the opportunity to override the default immediate action.
    id customClientAnimationController = nil;
    if ([[_webView UIDelegate] respondsToSelector:@selector(_webView:immediateActionAnimationControllerForHitTestResult:withType:)]) {
        RetainPtr<WebElementDictionary> webHitTestResult = adoptNS([[WebElementDictionary alloc] initWithHitTestResult:_hitTestResult]);
        customClientAnimationController = [(id)[_webView UIDelegate] _webView:_webView immediateActionAnimationControllerForHitTestResult:webHitTestResult.get() withType:_type];
    }

    if (customClientAnimationController == [NSNull null]) {
        [self _cancelImmediateAction];
        return;
    }

#if PLATFORM(MAC)
    // FIXME: We should not permanently disable this for iTunes. rdar://problem/19461358
    if (WebCore::MacApplication::isITunes()) {
        [self _cancelImmediateAction];
        return;
    }
#endif

    if (customClientAnimationController && [customClientAnimationController conformsToProtocol:@protocol(NSImmediateActionAnimationController)])
        [_immediateActionRecognizer setAnimationController:(id <NSImmediateActionAnimationController>)customClientAnimationController];
    else
        [_immediateActionRecognizer setAnimationController:defaultAnimationController];
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

- (void)menuItemDidClose:(NSMenuItem *)menuItem
{
    [self _clearImmediateActionState];
    [_webView _clearTextIndicatorWithAnimation:WebCore::TextIndicatorWindowDismissalAnimation::FadeOut];
}

static WebCore::IntRect elementBoundingBoxInWindowCoordinatesFromNode(WebCore::Node* node)
{
    if (!node)
        return { };

    WebCore::Frame* frame = node->document().frame();
    if (!frame)
        return { };

    WebCore::FrameView* view = frame->view();
    if (!view)
        return { };

    WebCore::RenderObject* renderer = node->renderer();
    if (!renderer)
        return { };

    return view->contentsToWindow(renderer->absoluteBoundingBoxRect());
}

- (NSRect)menuItem:(NSMenuItem *)menuItem itemFrameForPoint:(NSPoint)point
{
    if (!_webView)
        return NSZeroRect;

    WebCore::Node* node = _hitTestResult.innerNode();
    if (!node)
        return NSZeroRect;

    return elementBoundingBoxInWindowCoordinatesFromNode(node);
}

- (NSSize)menuItem:(NSMenuItem *)menuItem maxSizeForPoint:(NSPoint)point
{
    if (!_webView)
        return NSZeroSize;

    NSSize screenSize = _webView.window.screen.frame.size;
    WebCore::FloatRect largestRect = WebCore::largestRectWithAspectRatioInsideRect(screenSize.width / screenSize.height, _webView.bounds);
    return NSMakeSize(largestRect.width() * 0.75, largestRect.height() * 0.75);
}

#pragma mark Data Detectors actions

- (id <NSImmediateActionAnimationController>)_animationControllerForDataDetectedText
{
    if (!DataDetectorsLibrary())
        return nil;

    Optional<WebCore::DetectedItem> detectedItem;

    if ([[_webView UIDelegate] respondsToSelector:@selector(_webView:actionContextForHitTestResult:range:)]) {
        DOMRange *customDataDetectorsRange;
        auto actionContext = [(id)[_webView UIDelegate] _webView:_webView
            actionContextForHitTestResult:adoptNS([[WebElementDictionary alloc] initWithHitTestResult:_hitTestResult]).get()
            range:&customDataDetectorsRange];
        if (actionContext && customDataDetectorsRange) {
            detectedItem = { {
                actionContext,
                { }, // FIXME: Seems like an empty rect isn't really OK.
                makeSimpleRange(*core(customDataDetectorsRange))
            } };
        }
    }

    // If the client didn't give us an action context, try to scan around the hit point.
    if (!detectedItem)
        detectedItem = WebCore::DataDetection::detectItemAroundHitTestResult(_hitTestResult);

    if (!detectedItem)
        return nil;

    [detectedItem->actionContext setAltMode:YES];
    [detectedItem->actionContext setImmediate:YES];
    if (![[getDDActionsManagerClass() sharedManager] hasActionsForResult:[detectedItem->actionContext mainResult] actionContext:detectedItem->actionContext.get()])
        return nil;

    auto indicator = WebCore::TextIndicator::createWithRange(detectedItem->range, { }, WebCore::TextIndicatorPresentationTransition::FadeIn);

    _currentActionContext = [detectedItem->actionContext contextForView:_webView altMode:YES interactionStartedHandler:^() {
    } interactionChangedHandler:^() {
        if (indicator)
            [_webView _setTextIndicator:*indicator withLifetime:WebCore::TextIndicatorWindowLifetime::Permanent];
    } interactionStoppedHandler:^() {
        [_webView _clearTextIndicatorWithAnimation:WebCore::TextIndicatorWindowDismissalAnimation::FadeOut];
    }];

    [_currentActionContext setHighlightFrame:[_webView.window convertRectToScreen:detectedItem->boundingBox]];

    NSArray *menuItems = [[getDDActionsManagerClass() sharedManager] menuItemsForResult:[_currentActionContext mainResult] actionContext:_currentActionContext.get()];
    if (menuItems.count != 1)
        return nil;

    return menuItems.lastObject;
}

- (id <NSImmediateActionAnimationController>)_animationControllerForDataDetectedLink
{
    if (!DataDetectorsLibrary())
        return nil;

    RetainPtr<DDActionContext> actionContext = adoptNS([allocDDActionContextInstance() init]);

    if (!actionContext)
        return nil;

    [actionContext setAltMode:YES];
    [actionContext setImmediate:YES];

    auto linkRange = makeRangeSelectingNodeContents(*_hitTestResult.URLElement());
    auto indicator = WebCore::TextIndicator::createWithRange(linkRange, { }, WebCore::TextIndicatorPresentationTransition::FadeIn);

    _currentActionContext = [actionContext contextForView:_webView altMode:YES interactionStartedHandler:^() {
    } interactionChangedHandler:^() {
        if (indicator)
            [_webView _setTextIndicator:*indicator withLifetime:WebCore::TextIndicatorWindowLifetime::Permanent];
    } interactionStoppedHandler:^() {
        [_webView _clearTextIndicatorWithAnimation:WebCore::TextIndicatorWindowDismissalAnimation::FadeOut];
    }];

    [_currentActionContext setHighlightFrame:[_webView.window convertRectToScreen:elementBoundingBoxInWindowCoordinatesFromNode(_hitTestResult.URLElement())]];

    NSArray *menuItems = [[getDDActionsManagerClass() sharedManager] menuItemsForTargetURL:_hitTestResult.absoluteLinkURL().string() actionContext:_currentActionContext.get()];
    if (menuItems.count != 1)
        return nil;
    
    return menuItems.lastObject;
}

#pragma mark Text action

+ (WebCore::DictionaryPopupInfo)_dictionaryPopupInfoForRange:(const WebCore::SimpleRange&)range inFrame:(WebCore::Frame*)frame withLookupOptions:(NSDictionary *)lookupOptions indicatorOptions:(OptionSet<WebCore::TextIndicatorOption>)indicatorOptions transition:(WebCore::TextIndicatorPresentationTransition)presentationTransition
{
    auto& editor = frame->editor();
    editor.setIsGettingDictionaryPopupInfo(true);

    // Dictionary API will accept a whitespace-only string and display UI as if it were real text,
    // so bail out early to avoid that.
    WebCore::DictionaryPopupInfo popupInfo;
    if (plainText(range).stripWhiteSpace().isEmpty()) {
        editor.setIsGettingDictionaryPopupInfo(false);
        return popupInfo;
    }

    auto style = range.start.container->renderStyle();
    float scaledDescent = style ? style->fontMetrics().descent() * frame->page()->pageScaleFactor() : 0;

    auto quads = WebCore::RenderObject::absoluteTextQuads(range);
    if (quads.isEmpty()) {
        editor.setIsGettingDictionaryPopupInfo(false);
        return popupInfo;
    }

    auto rangeRect = frame->view()->contentsToWindow(quads[0].enclosingBoundingBox());

    popupInfo.origin = NSMakePoint(rangeRect.x(), rangeRect.y() + scaledDescent);
    popupInfo.options = lookupOptions;

    auto attributedString = editingAttributedString(range, WebCore::IncludeImages::No).string;
    auto scaledAttributedString = adoptNS([[NSMutableAttributedString alloc] initWithString:[attributedString string]]);
    NSFontManager *fontManager = [NSFontManager sharedFontManager];

    [attributedString enumerateAttributesInRange:NSMakeRange(0, [attributedString length]) options:0 usingBlock:^(NSDictionary *attributes, NSRange attributeRange, BOOL *stop) {
        RetainPtr<NSMutableDictionary> scaledAttributes = adoptNS([attributes mutableCopy]);
        NSFont *font = [scaledAttributes objectForKey:NSFontAttributeName];
        if (font)
            font = [fontManager convertFont:font toSize:font.pointSize * frame->page()->pageScaleFactor()];
        if (font)
            [scaledAttributes setObject:font forKey:NSFontAttributeName];
        [scaledAttributedString addAttributes:scaledAttributes.get() range:attributeRange];
    }];

    popupInfo.attributedString = scaledAttributedString.get();

    if (auto textIndicator = WebCore::TextIndicator::createWithRange(range, indicatorOptions, presentationTransition))
        popupInfo.textIndicator = textIndicator->data();

    editor.setIsGettingDictionaryPopupInfo(false);
    return popupInfo;
}

- (id<NSImmediateActionAnimationController>)_animationControllerForText
{
    if (!PAL::getLULookupDefinitionModuleClass())
        return nil;

    auto node = _hitTestResult.innerNode();
    if (!node)
        return nil;

    auto frame = node->document().frame();
    if (!frame)
        return nil;

    auto range = WebCore::DictionaryLookup::rangeAtHitTestResult(_hitTestResult);
    if (!range)
        return nil;

    auto [dictionaryRange, options] = WTFMove(*range);
    auto dictionaryPopupInfo = [WebImmediateActionController _dictionaryPopupInfoForRange:dictionaryRange inFrame:frame withLookupOptions:options indicatorOptions: { } transition: WebCore::TextIndicatorPresentationTransition::FadeIn];
    if (!dictionaryPopupInfo.attributedString)
        return nil;

    return [_webView _animationControllerForDictionaryLookupPopupInfo:dictionaryPopupInfo];
}

@end

#endif // PLATFORM(MAC)
