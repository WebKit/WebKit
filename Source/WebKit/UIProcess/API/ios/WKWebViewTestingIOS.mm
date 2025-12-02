/*
 * Copyright (C) 2014-2023 Apple Inc. All rights reserved.
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
#import "WKWebViewPrivateForTestingIOS.h"

#if PLATFORM(IOS_FAMILY)

#import "RemoteLayerTreeDrawingAreaProxyIOS.h"
#import "RemoteLayerTreeViews.h"
#import "RemoteScrollingCoordinatorProxy.h"
#import "SystemPreviewController.h"
#import "UIKitSPI.h"
#import "WKColorExtensionView.h"
#import "WKContentViewInteraction.h"
#import "WKFullScreenWindowController.h"
#import "WKWebViewIOS.h"
#import "WebPageProxy.h"
#import "WebProcessProxy.h"
#import "_WKActivatedElementInfoInternal.h"
#import "_WKTextInputContextInternal.h"
#import <WebCore/ColorCocoa.h>
#import <WebCore/ColorSerialization.h>
#import <WebCore/ElementContext.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/SortedArrayMap.h>
#import <wtf/text/MakeString.h>
#import <wtf/text/TextStream.h>

#if HAVE(CORE_ANIMATION_SEPARATED_LAYERS)
#if USE(APPLE_INTERNAL_SDK)
#import <WebKitAdditions/SeparatedLayerAdditions.h>
#else
static void dumpSeparatedLayerProperties(TextStream&, CALayer *) { }
#endif
#endif

@implementation WKWebView (WKTestingIOS)

- (void)_requestTextInputContextsInRect:(CGRect)rect completionHandler:(void (^)(NSArray<_WKTextInputContext *> *))completionHandler
{
    // Adjust returned bounding rects to be in WKWebView coordinates.
    auto adjustedRect = [self convertRect:rect toView:_contentView.get()];
    [_contentView _requestTextInputContextsInRect:adjustedRect completionHandler:[weakSelf = WeakObjCPtr<WKWebView>(self), completionHandler = makeBlockPtr(completionHandler)](NSArray<_WKTextInputContext *> *contexts) {
        auto strongSelf = weakSelf.get();
        if (!strongSelf || !contexts.count) {
            completionHandler(@[ ]);
            return;
        }
        auto adjustedContexts = adoptNS([[NSMutableArray alloc] initWithCapacity:contexts.count]);
        for (_WKTextInputContext *context in contexts) {
            auto adjustedContext = context._textInputContext;
            adjustedContext.boundingRect = [strongSelf convertRect:adjustedContext.boundingRect fromView:strongSelf->_contentView.get()];
            [adjustedContexts addObject:adoptNS([[_WKTextInputContext alloc] _initWithTextInputContext:adjustedContext]).get()];
        }
        completionHandler(adjustedContexts.get());
    }];
}

- (void)_focusTextInputContext:(_WKTextInputContext *)context placeCaretAt:(CGPoint)point completionHandler:(void (^)(UIResponder<UITextInput> *))completionHandler
{
    auto adjustedPoint = [self convertPoint:point toView:_contentView.get()];
    [_contentView _focusTextInputContext:context placeCaretAt:adjustedPoint completionHandler:completionHandler];
}

- (void)_willBeginTextInteractionInTextInputContext:(_WKTextInputContext *)context
{
    [_contentView _willBeginTextInteractionInTextInputContext:context];
}

- (void)selectWordBackwardForTesting
{
    [_contentView selectWordBackwardForTesting];
}

- (void)_didFinishTextInteractionInTextInputContext:(_WKTextInputContext *)context
{
    [_contentView _didFinishTextInteractionInTextInputContext:context];
}

- (BOOL)_mayContainEditableElementsInRect:(CGRect)rect
{
#if ENABLE(EDITABLE_REGION)
    return WebKit::mayContainEditableElementsInRect(_contentView.get(), [self convertRect:rect toView:_contentView.get()]);
#else
    return NO;
#endif
}

- (void)keyboardAccessoryBarNext
{
    [_contentView accessoryTab:YES];
}

- (void)keyboardAccessoryBarPrevious
{
    [_contentView accessoryTab:NO];
}

- (void)dismissFormAccessoryView
{
    [_contentView dismissFormAccessoryView];
}

- (NSArray<NSString *> *)_filePickerAcceptedTypeIdentifiers
{
    return [_contentView filePickerAcceptedTypeIdentifiers];
}

- (void)_dismissFilePicker
{
    [_contentView dismissFilePicker];
}

- (void)setTimePickerValueToHour:(NSInteger)hour minute:(NSInteger)minute
{
    [_contentView setTimePickerValueToHour:hour minute:minute];
}

- (double)timePickerValueHour
{
    return [_contentView timePickerValueHour];
}

- (double)timePickerValueMinute
{
    return [_contentView timePickerValueMinute];
}

- (void)selectFormAccessoryPickerRow:(int)rowIndex
{
    [_contentView selectFormAccessoryPickerRow:rowIndex];
}

- (BOOL)selectFormAccessoryHasCheckedItemAtRow:(long)rowIndex
{
    return [_contentView selectFormAccessoryHasCheckedItemAtRow:rowIndex];
}

- (NSString *)selectFormPopoverTitle
{
    return [_contentView selectFormPopoverTitle];
}

- (void)setSelectedColorForColorPicker:(UIColor *)color
{
    [_contentView setSelectedColorForColorPicker:color];
}

- (void)_selectDataListOption:(int)optionIndex
{
    [_contentView _selectDataListOption:optionIndex];
}

- (BOOL)_isShowingDataListSuggestions
{
    return [_contentView isShowingDataListSuggestions];
}

- (NSString *)textContentTypeForTesting
{
    return [_contentView textContentTypeForTesting];
}

- (NSString *)formInputLabel
{
    return [_contentView formInputLabel];
}

- (CGRect)_inputViewBoundsInWindow
{
    return _inputViewBoundsInWindow;
}

static String allowListedClassToString(UIView *view)
{
    static constexpr ComparableASCIILiteral allowedClassesArray[] = {
        "UIView"_s,
        "WKBackdropView"_s,
        "WKCompositingView"_s,
        "WKContentView"_s,
        "WKModelView"_s,
        "WKScrollView"_s,
        "WKSeparatedImageView"_s,
        "WKSeparatedModelView"_s,
        "WKShapeView"_s,
        "WKSimpleBackdropView"_s,
        "WKTransformView"_s,
        "WKUIRemoteView"_s,
        "WKWebView"_s,
        "_UILayerHostView"_s,
    };
    static constexpr SortedArraySet allowedClasses { allowedClassesArray };

    String classString { NSStringFromClass(view.class) };
    if (allowedClasses.contains(classString))
        return classString;

    return "<class not in allowed list of classes>"_s;
}

#if HAVE(CORE_ANIMATION_SEPARATED_LAYERS)
static bool shouldDumpSeparatedDetails(UIView *view)
{
    static constexpr ComparableASCIILiteral deniedClassesArray[] = {
        "WKCompositingView"_s,
        "WKSeparatedImageView"_s,
    };
    static constexpr SortedArraySet deniedClasses { deniedClassesArray };

    String classString { NSStringFromClass(view.class) };
    if (deniedClasses.contains(classString))
        return false;

    return true;
}
#endif

static void dumpUIView(TextStream& ts, UIView *view, bool traverse)
{
    auto rectToString = [] (auto rect) {
        return makeString("[x: "_s, rect.origin.x, " y: "_s, rect.origin.y, " width: "_s, rect.size.width, " height: "_s, rect.size.height, ']');
    };

    auto pointToString = [] (auto point) {
        return makeString("[x: "_s, point.x, " y: "_s, point.x, ']');
    };


    ts << "view [class: "_s << allowListedClassToString(view) << ']';

#if ENABLE(OVERLAY_REGIONS_IN_EVENT_REGION)
    if ([view isKindOfClass:[WKBaseScrollView class]]) {
        ts.dumpProperty("scrolling behavior"_s, makeString([(WKBaseScrollView *)view _scrollingBehavior]));

        auto rects = [(WKBaseScrollView *)view overlayRegionsForTesting];
        auto overlaysAsStrings = adoptNS([[NSMutableArray alloc] initWithCapacity:rects.size()]);
        for (auto rect : rects)
            [overlaysAsStrings addObject:rectToString(CGRect(rect)).createNSString().get()];

        [overlaysAsStrings sortUsingSelector:@selector(localizedCaseInsensitiveCompare:)];
        for (NSString *overlayAsString in overlaysAsStrings.get())
            ts.dumpProperty("overlay region"_s, overlayAsString);

        auto& associatedLayers = [(WKBaseScrollView *)view overlayRegionAssociatedLayersForTesting];
        auto associatedLayersCount = associatedLayers.size();
        if (associatedLayersCount > 0)
            ts.dumpProperty("associated layers"_s, associatedLayersCount);
    }
#endif

    ts.dumpProperty("layer bounds"_s, rectToString(view.layer.bounds));
    
    if (view.layer.position.x != 0 || view.layer.position.y != 0)
        ts.dumpProperty("layer position"_s, pointToString(view.layer.position));
    
    if (view.layer.zPosition != 0)
        ts.dumpProperty("layer zPosition"_s, makeString(view.layer.zPosition));
    
    if (view.layer.anchorPoint.x != 0.5 || view.layer.anchorPoint.y != 0.5)
        ts.dumpProperty("layer anchorPoint"_s, pointToString(view.layer.anchorPoint));
    
    if (view.layer.anchorPointZ != 0)
        ts.dumpProperty("layer anchorPointZ"_s, makeString(view.layer.anchorPointZ));

    if (view.layer.cornerRadius != 0.0)
        ts.dumpProperty("layer cornerRadius"_s, makeString(view.layer.cornerRadius));

#if HAVE(CORE_ANIMATION_SEPARATED_LAYERS)
    if (view.layer.separated) {
        TextStream::GroupScope scope(ts);
        ts << "separated"_s;
        if (shouldDumpSeparatedDetails(view))
            dumpSeparatedLayerProperties(ts, view.layer);
        else
            traverse = false;
    }
#endif

    if (traverse && view.subviews.count > 0) {
        TextStream::GroupScope scope(ts);
        ts << "subviews"_s;
        for (UIView *subview in view.subviews) {
            TextStream::GroupScope scope(ts);
            dumpUIView(ts, subview, traverse);
        }
    }
}

- (NSString *)_uiViewTreeAsText
{
    return [self _uiViewTreeAsTextForView:self];
}

- (NSString *)_uiViewTreeAsTextForViewWithLayerID:(unsigned long long)layerID
{
    if (!layerID)
        return nil;
    RetainPtr view = downcast<WebKit::RemoteLayerTreeDrawingAreaProxyIOS>(_page->protectedDrawingArea())->viewWithLayerIDForTesting({ ObjectIdentifier<WebCore::PlatformLayerIdentifierType>(layerID), _page->legacyMainFrameProcess().coreProcessIdentifier() });
    if (!view)
        return nil;

    return [self _uiViewTreeAsTextForView:view.get()];
}

- (NSString *)_uiViewTreeAsTextForView:(UIView *)view
{
    TextStream ts(TextStream::LineMode::MultipleLine);

    {
        TextStream::GroupScope scope(ts);
        ts << "UIView tree root "_s;
        dumpUIView(ts, view, true);
    }

    return ts.release().createNSString().autorelease();
}

- (NSString *)_scrollbarState:(unsigned long long)rawScrollingNodeID processID:(unsigned long long)processID isVertical:(bool)isVertical
{
    std::optional<WebCore::ScrollingNodeID> scrollingNodeID;
    if (ObjectIdentifier<WebCore::ProcessIdentifierType>::isValidIdentifier(processID) && ObjectIdentifier<WebCore::ScrollingNodeIDType>::isValidIdentifier(rawScrollingNodeID))
        scrollingNodeID = WebCore::ScrollingNodeID(ObjectIdentifier<WebCore::ScrollingNodeIDType>(rawScrollingNodeID), ObjectIdentifier<WebCore::ProcessIdentifierType>(processID));

    if (_page->scrollingCoordinatorProxy()->rootScrollingNodeID() == scrollingNodeID) {
        TextStream ts(TextStream::LineMode::MultipleLine);
        {
            TextStream::GroupScope scope(ts);
            ts << ([_scrollView showsHorizontalScrollIndicator] ? ""_s : "none"_s);
#if HAVE(UIKIT_SCROLLBAR_COLOR_SPI)
            if (isVertical)
                ts << ([_scrollView _verticalScrollIndicatorColor] ? WebCore::colorFromCocoaColor([_scrollView _verticalScrollIndicatorColor]).debugDescription() :  ""_s);
            else
                ts << ([_scrollView _horizontalScrollIndicatorColor] ? WebCore::colorFromCocoaColor([_scrollView _horizontalScrollIndicatorColor]).debugDescription() :  ""_s);
#endif
        }
        return ts.release().createNSString().autorelease();
    }
    return _page->scrollbarStateForScrollingNodeID(scrollingNodeID, isVertical).createNSString().autorelease();
}

- (UIView *)_colorExtensionViewForTesting:(UIRectEdge)edge
{
#if ENABLE(CONTENT_INSET_BACKGROUND_FILL)
    switch (edge) {
    case UIRectEdgeTop:
        return _fixedColorExtensionViews.at(WebCore::BoxSide::Top).get();
    case UIRectEdgeLeft:
        return _fixedColorExtensionViews.at(WebCore::BoxSide::Left).get();
    case UIRectEdgeBottom:
        return _fixedColorExtensionViews.at(WebCore::BoxSide::Bottom).get();
    case UIRectEdgeRight:
        return _fixedColorExtensionViews.at(WebCore::BoxSide::Right).get();
    default:
        break;
    }
#endif // ENABLE(CONTENT_INSET_BACKGROUND_FILL)
    return nil;
}

- (NSNumber *)_stableStateOverride
{
    // For subclasses to override.
    return nil;
}

- (void)_doAfterReceivingEditDragSnapshotForTesting:(dispatch_block_t)action
{
    [_contentView _doAfterReceivingEditDragSnapshotForTesting:action];
}

- (CGRect)_dragCaretRect
{
#if ENABLE(DRAG_SUPPORT)
    return _page->currentDragCaretRect();
#else
    return CGRectZero;
#endif
}

- (BOOL)_isAnimatingDragCancel
{
#if ENABLE(DRAG_SUPPORT)
    return [_contentView isAnimatingDragCancel];
#else
    return NO;
#endif
}

- (CGRect)_tapHighlightViewRect
{
    return [_contentView tapHighlightViewRect];
}

- (UIGestureRecognizer *)_imageAnalysisGestureRecognizer
{
    return [_contentView imageAnalysisGestureRecognizer];
}

- (UITapGestureRecognizer *)_singleTapGestureRecognizer
{
    return [_contentView singleTapGestureRecognizer];
}

- (BOOL)_isKeyboardScrollingAnimationRunning
{
    return [_contentView isKeyboardScrollingAnimationRunning];
}

- (void)_simulateElementAction:(_WKElementActionType)actionType atLocation:(CGPoint)location
{
    [_contentView _simulateElementAction:actionType atLocation:location];
}

- (void)_simulateLongPressActionAtLocation:(CGPoint)location
{
    [_contentView _simulateLongPressActionAtLocation:location];
}

- (void)_simulateTextEntered:(NSString *)text
{
    [_contentView _simulateTextEntered:text];
}

- (void)_triggerSystemPreviewActionOnElement:(uint64_t)elementID document:(NSString*)documentID page:(uint64_t)pageID
{
#if USE(SYSTEM_PREVIEW)
    if (_page) {
        if (auto* previewController = _page->systemPreviewController())
            previewController->triggerSystemPreviewActionWithTargetForTesting(elementID, documentID, pageID);
    }
#endif
}

- (void)_setDeviceOrientationUserPermissionHandlerForTesting:(BOOL (^)())handler
{
    Function<bool()> handlerWrapper;
    if (handler)
        handlerWrapper = [handler = makeBlockPtr(handler)] { return handler(); };
    _page->setDeviceOrientationUserPermissionHandlerForTesting(WTFMove(handlerWrapper));
}

- (void)_resetObscuredInsetsForTesting
{
    if (self._haveSetObscuredInsets)
        [self _resetObscuredInsets];
}

- (BOOL)_hasResizeAssertion
{
#if HAVE(UIKIT_RESIZABLE_WINDOWS)
    if (!_resizeAssertions.isEmpty())
        return YES;
#endif
    return NO;
}

- (void)_simulateSelectionStart
{
    [_contentView _simulateSelectionStart];
}

+ (void)_resetPresentLockdownModeMessage
{
#if ENABLE(LOCKDOWN_MODE_API)
    [self _clearLockdownModeWarningNeeded];
#endif
}

- (void)_doAfterNextVisibleContentRectAndStablePresentationUpdate:(void (^)(void))updateBlock
{
    [self _doAfterNextVisibleContentRectUpdate:makeBlockPtr([strongSelf = retainPtr(self), updateBlock = makeBlockPtr(updateBlock)] {
        [strongSelf _doAfterNextStablePresentationUpdate:updateBlock.get()];
    }).get()];
}

- (void)_simulateModelInteractionPanGestureBeginAtPoint:(CGPoint)hitPoint
{
#if ENABLE(MODEL_PROCESS)
    [_contentView _simulateModelInteractionPanGestureBeginAtPoint:hitPoint];
#endif
}

- (void)_simulateModelInteractionPanGestureUpdateAtPoint:(CGPoint)hitPoint
{
#if ENABLE(MODEL_PROCESS)
    [_contentView _simulateModelInteractionPanGestureUpdateAtPoint:hitPoint];
#endif
}

- (NSDictionary *)_stageModeInfoForTesting
{
#if ENABLE(MODEL_PROCESS)
    return [_contentView _stageModeInfoForTesting];
#else
    return @{ };
#endif
}

@end

#endif // PLATFORM(IOS_FAMILY)
