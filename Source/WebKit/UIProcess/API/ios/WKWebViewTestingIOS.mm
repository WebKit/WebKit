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

#import "config.h"
#import "WKWebViewPrivateForTestingIOS.h"

#if PLATFORM(IOS_FAMILY)

#import "RemoteLayerTreeDrawingAreaProxy.h"
#import "RemoteScrollingCoordinatorProxy.h"
#import "WKFullScreenWindowController.h"
#import "WKWebViewIOS.h"
#import "WebPageProxy.h"
#import "_WKActivatedElementInfoInternal.h"

@implementation WKWebView (WKTestingIOS)

- (void)keyboardAccessoryBarNext
{
    [_contentView accessoryTab:YES];
}

- (void)keyboardAccessoryBarPrevious
{
    [_contentView accessoryTab:NO];
}

- (void)applyAutocorrection:(NSString *)newString toString:(NSString *)oldString withCompletionHandler:(void (^)(void))completionHandler
{
    [_contentView applyAutocorrection:newString toString:oldString withCompletionHandler:[capturedCompletionHandler = makeBlockPtr(completionHandler)] (UIWKAutocorrectionRects *rects) {
        capturedCompletionHandler();
    }];
}

- (void)dismissFormAccessoryView
{
    [_contentView accessoryDone];
}

- (void)_dismissFilePicker
{
    [_contentView dismissFilePicker];
}

- (void)setTimePickerValueToHour:(NSInteger)hour minute:(NSInteger)minute
{
    [_contentView setTimePickerValueToHour:hour minute:minute];
}

- (void)selectFormAccessoryPickerRow:(int)rowIndex
{
    [_contentView selectFormAccessoryPickerRow:rowIndex];
}

- (NSString *)selectFormPopoverTitle
{
    return [_contentView selectFormPopoverTitle];
}

- (NSString *)textContentTypeForTesting
{
    return [_contentView textContentTypeForTesting];
}

- (NSString *)formInputLabel
{
    return [_contentView formInputLabel];
}

- (void)_didShowContextMenu
{
    // For subclasses to override.
}

- (void)_didDismissContextMenu
{
    // For subclasses to override.
}

- (CGRect)_inputViewBounds
{
    return _inputViewBounds;
}

- (NSArray<NSValue *> *)_uiTextSelectionRects
{
    // Force the selection view to update if needed.
    [_contentView _updateChangedSelection];

    return [_contentView _uiTextSelectionRects];
}

- (NSString *)_scrollingTreeAsText
{
    WebKit::RemoteScrollingCoordinatorProxy* coordinator = _page->scrollingCoordinatorProxy();
    if (!coordinator)
        return @"";

    return coordinator->scrollingTreeAsText();
}

- (NSNumber *)_stableStateOverride
{
    // For subclasses to override.
    return nil;
}

- (NSDictionary *)_propertiesOfLayerWithID:(unsigned long long)layerID
{
    CALayer* layer = downcast<WebKit::RemoteLayerTreeDrawingAreaProxy>(*_page->drawingArea()).layerWithIDForTesting(layerID);
    if (!layer)
        return nil;

    return @{
        @"bounds" : @{
            @"x" : @(layer.bounds.origin.x),
            @"y" : @(layer.bounds.origin.x),
            @"width" : @(layer.bounds.size.width),
            @"height" : @(layer.bounds.size.height),

        },
        @"position" : @{
            @"x" : @(layer.position.x),
            @"y" : @(layer.position.y),
        },
        @"zPosition" : @(layer.zPosition),
        @"anchorPoint" : @{
            @"x" : @(layer.anchorPoint.x),
            @"y" : @(layer.anchorPoint.y),
        },
        @"anchorPointZ" : @(layer.anchorPointZ),
        @"transform" : @{
            @"m11" : @(layer.transform.m11),
            @"m12" : @(layer.transform.m12),
            @"m13" : @(layer.transform.m13),
            @"m14" : @(layer.transform.m14),

            @"m21" : @(layer.transform.m21),
            @"m22" : @(layer.transform.m22),
            @"m23" : @(layer.transform.m23),
            @"m24" : @(layer.transform.m24),

            @"m31" : @(layer.transform.m31),
            @"m32" : @(layer.transform.m32),
            @"m33" : @(layer.transform.m33),
            @"m34" : @(layer.transform.m34),

            @"m41" : @(layer.transform.m41),
            @"m42" : @(layer.transform.m42),
            @"m43" : @(layer.transform.m43),
            @"m44" : @(layer.transform.m44),
        },
        @"sublayerTransform" : @{
            @"m11" : @(layer.sublayerTransform.m11),
            @"m12" : @(layer.sublayerTransform.m12),
            @"m13" : @(layer.sublayerTransform.m13),
            @"m14" : @(layer.sublayerTransform.m14),

            @"m21" : @(layer.sublayerTransform.m21),
            @"m22" : @(layer.sublayerTransform.m22),
            @"m23" : @(layer.sublayerTransform.m23),
            @"m24" : @(layer.sublayerTransform.m24),

            @"m31" : @(layer.sublayerTransform.m31),
            @"m32" : @(layer.sublayerTransform.m32),
            @"m33" : @(layer.sublayerTransform.m33),
            @"m34" : @(layer.sublayerTransform.m34),

            @"m41" : @(layer.sublayerTransform.m41),
            @"m42" : @(layer.sublayerTransform.m42),
            @"m43" : @(layer.sublayerTransform.m43),
            @"m44" : @(layer.sublayerTransform.m44),
        },

        @"hidden" : @(layer.hidden),
        @"doubleSided" : @(layer.doubleSided),
        @"masksToBounds" : @(layer.masksToBounds),
        @"contentsScale" : @(layer.contentsScale),
        @"rasterizationScale" : @(layer.rasterizationScale),
        @"opaque" : @(layer.opaque),
        @"opacity" : @(layer.opacity),
    };
}

- (void)_doAfterResettingSingleTapGesture:(dispatch_block_t)action
{
    [_contentView _doAfterResettingSingleTapGesture:action];
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

- (void)_simulateLongPressActionAtLocation:(CGPoint)location
{
    [_contentView _simulateLongPressActionAtLocation:location];
}

- (void)_simulateTextEntered:(NSString *)text
{
    [_contentView _simulateTextEntered:text];
}

- (void)_dynamicUserInterfaceTraitDidChange
{
    if (!_page)
        return;
    _page->effectiveAppearanceDidChange();
    [self _updateScrollViewBackground];
}

- (void)_triggerSystemPreviewActionOnElement:(uint64_t)elementID document:(uint64_t)documentID page:(uint64_t)pageID
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

@end

#endif // PLATFORM(IOS_FAMILY)
