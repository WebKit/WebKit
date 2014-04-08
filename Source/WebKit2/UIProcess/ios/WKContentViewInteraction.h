/*
 * Copyright (C) 2012-2014 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS)

#import "WKContentView.h"

#import "AssistedNodeInformation.h"
#import "GestureTypes.h"
#import "InteractionInformationAtPosition.h"
#import "WKAirPlayRoutePicker.h"
#import "WKFormPeripheral.h"
#import <UIKit/UITextInput_Private.h>
#import <UIKit/UIView.h>
#import <UIKit/UIWKSelectionAssistant.h>
#import <UIKit/UIWKTextInteractionAssistant.h>
#import <UIKit/UIWebFormAccessory.h>
#import <UIKit/UIWebTouchEventsGestureRecognizer.h>
#import <wtf/Forward.h>
#import <wtf/Vector.h>
#import <wtf/text/WTFString.h>

namespace WebCore {
class Color;
class FloatQuad;
class IntSize;
}

namespace WebKit {
class NativeWebTouchEvent;
class SmartMagnificationController;
class WebPageProxy;
}

@class _UIWebHighlightLongPressGestureRecognizer;
@class _UIHighlightView;
@class WebIOSEvent;
@class WKActionSheetAssistant;

typedef void (^UIWKAutocorrectionCompletionHandler)(UIWKAutocorrectionRects *rectsForInput);
typedef void (^UIWKAutocorrectionContextHandler)(UIWKAutocorrectionContext *autocorrectionContext);
typedef void (^UIWKDictationContextHandler)(NSString *selectedText, NSString *beforeText, NSString *afterText);

namespace WebKit {
struct WKAutoCorrectionData {
    String fontName;
    CGFloat fontSize;
    uint64_t fontTraits;
    CGRect textFirstRect;
    CGRect textLastRect;
    UIWKAutocorrectionCompletionHandler autocorrectionHandler;
    UIWKAutocorrectionContextHandler autocorrectionContextHandler;
};
}

@interface WKContentView () {
    RetainPtr<UIWebTouchEventsGestureRecognizer> _touchEventGestureRecognizer;

    BOOL _canSendTouchEventsAsynchronously;
    unsigned _nativeWebTouchEventUniqueIdBeingSentSynchronously;

    RetainPtr<UITapGestureRecognizer> _singleTapGestureRecognizer;
    RetainPtr<_UIWebHighlightLongPressGestureRecognizer> _highlightLongPressGestureRecognizer;
    RetainPtr<UILongPressGestureRecognizer> _longPressGestureRecognizer;
    RetainPtr<UITapGestureRecognizer> _doubleTapGestureRecognizer;
    RetainPtr<UITapGestureRecognizer> _twoFingerDoubleTapGestureRecognizer;

    RetainPtr<UIWKTextInteractionAssistant> _textSelectionAssistant;
    RetainPtr<UIWKSelectionAssistant> _webSelectionAssistant;

    RetainPtr<UITextInputTraits> _traits;
    RetainPtr<UIWebFormAccessory> _formAccessoryView;
    RetainPtr<_UIHighlightView> _highlightView;
    RetainPtr<NSString> _markedText;
    RetainPtr<WKActionSheetAssistant> _actionSheetAssistant;
    RetainPtr<WKAirPlayRoutePicker> _airPlayRoutePicker;

    std::unique_ptr<WebKit::SmartMagnificationController> _smartMagnificationController;

    id <UITextInputDelegate> _inputDelegate;

    uint64_t _latestTapHighlightID;

    WebKit::WKAutoCorrectionData _autocorrectionData;
    WebKit::InteractionInformationAtPosition _positionInformation;
    WebKit::AssistedNodeInformation _assistedNodeInformation;
    RetainPtr<NSObject<WKFormPeripheral>> _inputPeripheral;

    BOOL _isEditable;
    BOOL _showingTextStyleOptions;
    BOOL _hasValidPositionInformation;
    BOOL _isTapHighlightIDValid;
    BOOL _selectionNeedsUpdate;
}

@end

@interface WKContentView (WKInteraction) <UIGestureRecognizerDelegate, UIWebTouchEventsGestureRecognizerDelegate, UITextInputPrivate, UIWebFormAccessoryDelegate, UIWKInteractionViewProtocol>

@property (nonatomic, readonly) BOOL isEditable;
@property (nonatomic, readonly) const WebKit::InteractionInformationAtPosition& positionInformation;
@property (nonatomic, readonly) const WebKit::WKAutoCorrectionData& autocorrectionData;
@property (nonatomic, readonly) const WebKit::AssistedNodeInformation& assistedNodeInformation;
- (void)setupInteraction;
- (void)cleanupInteraction;

- (void)_webTouchEvent:(const WebKit::NativeWebTouchEvent&)touchEvent preventsNativeGestures:(BOOL)preventsDefault;
- (void)_didGetTapHighlightForRequest:(uint64_t)requestID color:(const WebCore::Color&)color quads:(const Vector<WebCore::FloatQuad>&)highlightedQuads topLeftRadius:(const WebCore::IntSize&)topLeftRadius topRightRadius:(const WebCore::IntSize&)topRightRadius bottomLeftRadius:(const WebCore::IntSize&)bottomLeftRadius bottomRightRadius:(const WebCore::IntSize&)bottomRightRadius;

- (void)_startAssistingNode:(const WebKit::AssistedNodeInformation&)information userObject:(NSObject <NSSecureCoding> *)userObject;
- (void)_stopAssistingNode;
- (void)_selectionChanged;
- (void)_updateChangedSelection;
- (BOOL)_interpretKeyEvent:(WebIOSEvent *)theEvent isCharEvent:(BOOL)isCharEvent;
- (void)_positionInformationDidChange:(const WebKit::InteractionInformationAtPosition&)info;
- (void)_attemptClickAtLocation:(CGPoint)location;
- (void)_updatePositionInformation;
- (void)_performAction:(WebKit::SheetAction)action;
- (void)_willStartScrollingOrZooming;
- (void)_willStartUserTriggeredScrollingOrZooming;
- (void)_didEndScrollingOrZooming;
- (void)_didUpdateBlockSelectionWithTouch:(WebKit::SelectionTouch)touch withFlags:(WebKit::SelectionFlags)flags growThreshold:(CGFloat)growThreshold shrinkThreshold:(CGFloat)shrinkThreshold;
- (void)_showPlaybackTargetPicker:(BOOL)hasVideo fromRect:(const WebCore::IntRect&)elementRect;
- (void)accessoryDone;
- (Vector<WebKit::WKOptionItem>&) assistedNodeSelectOptions;
@end

#endif // PLATFORM(IOS)
