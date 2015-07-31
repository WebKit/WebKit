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

#import <UIKit/UIKit.h>

#if USE(APPLE_INTERNAL_SDK)

#import <UIKit/UIAlertController_Private.h>
#import <UIKit/UIApplication_Private.h>
#import <UIKit/UIBarButtonItem_Private.h>
#import <UIKit/UIDatePicker_Private.h>
#import <UIKit/UIDevice_Private.h>
#import <UIKit/UIDocumentPasswordView.h>
#import <UIKit/UIFont_Private.h>
#import <UIKit/UIGeometry_Private.h>
#import <UIKit/UIGestureRecognizer_Private.h>
#import <UIKit/UIImagePickerController_Private.h>
#import <UIKit/UIImage_Private.h>
#import <UIKit/UIInterface_Private.h>
#import <UIKit/UIKeyboardImpl.h>
#import <UIKit/UIKeyboardIntl.h>
#import <UIKit/UIKeyboard_Private.h>
#import <UIKit/UILongPressGestureRecognizer_Private.h>
#import <UIKit/UIPeripheralHost.h>
#import <UIKit/UIPeripheralHost_Private.h>
#import <UIKit/UIPickerContentView_Private.h>
#import <UIKit/UIPickerView_Private.h>
#import <UIKit/UIPresentationController_Private.h>
#import <UIKit/UIPreviewItemController.h>
#import <UIKit/UIResponder_Private.h>
#import <UIKit/UIScrollView_Private.h>
#import <UIKit/UIStringDrawing_Private.h>
#import <UIKit/UITableViewCell_Private.h>
#import <UIKit/UITapGestureRecognizer_Private.h>
#import <UIKit/UITextInput_Private.h>
#import <UIKit/UITextInteractionAssistant_Private.h>
#import <UIKit/UIViewControllerTransitioning_Private.h>
#import <UIKit/UIViewController_Private.h>
#import <UIKit/UIViewController_ViewService.h>
#import <UIKit/UIView_Private.h>
#import <UIKit/UIWKSelectionAssistant.h>
#import <UIKit/UIWKTextInteractionAssistant.h>
#import <UIKit/UIWebBrowserView.h>
#import <UIKit/UIWebDocumentView.h>
#import <UIKit/UIWebFormAccessory.h>
#import <UIKit/UIWebGeolocationPolicyDecider.h>
#import <UIKit/UIWebScrollView.h>
#import <UIKit/UIWebTiledView.h>
#import <UIKit/UIWebTouchEventsGestureRecognizer.h>
#import <UIKit/UIWindow_Private.h>
#import <UIKit/_UIBackdropView_Private.h>
#import <UIKit/_UIHighlightView.h>
#import <UIKit/_UINavigationInteractiveTransition.h>
#import <UIKit/_UINavigationParallaxTransition.h>

// FIXME: Unconditionally include this file when a new SDK is available. <rdar://problem/20150072>
#if defined(__has_include) && __has_include(<UIKit/UIDocumentMenuViewController_Private.h>)
#import <UIKit/UIDocumentMenuViewController_Private.h>
#else
@interface UIDocumentMenuViewController (Details)
@property (nonatomic, assign, setter = _setIgnoreApplicationEntitlementForImport:, getter = _ignoreApplicationEntitlementForImport) BOOL _ignoreApplicationEntitlementForImport;
@end
#endif

#else

@interface UIAlertController (Details)
- (void)_addActionWithTitle:(NSString *)title style:(UIAlertActionStyle)style handler:(void (^)(void))handler;
- (void)_addActionWithTitle:(NSString *)title style:(UIAlertActionStyle)style handler:(void (^)(void))handler shouldDismissHandler:(BOOL (^)(void))shouldDismissHandler;
@property (nonatomic) UIAlertControllerStyle preferredStyle;
@end

@interface UIApplication (Details)
- (UIInterfaceOrientation)interfaceOrientation;
- (void)_cancelAllTouches;
- (CGFloat)statusBarHeight;
- (BOOL)isSuspendedUnderLock;
@end

typedef NS_ENUM(NSInteger, UIDatePickerPrivateMode)  {
    UIDatePickerModeYearAndMonth = 4269,
};

@interface UIDatePicker (Details)
@property (nonatomic, readonly, getter=_contentWidth) CGFloat contentWidth;
@end

#define UICurrentUserInterfaceIdiomIsPad() ([[UIDevice currentDevice] userInterfaceIdiom] == UIUserInterfaceIdiomPad)

@interface UIDevice (Details)
@property (nonatomic, readonly, retain) NSString *buildVersion;
@end

typedef enum {
    kUIKeyboardInputRepeat                 = 1 << 0,
    kUIKeyboardInputPopupVariant           = 1 << 1,
    kUIKeyboardInputMultitap               = 1 << 2,
    kUIKeyboardInputSkipCandidateSelection = 1 << 3,
    kUIKeyboardInputDeadKey                = 1 << 4,
    kUIKeyboardInputModifierFlagsChanged   = 1 << 5,
    kUIKeyboardInputFlick                  = 1 << 6,
    kUIKeyboardInputPreProcessed           = 1 << 7,
} UIKeyboardInputFlags;

@interface UIEvent (Details)
@property (nonatomic, readonly) UIKeyboardInputFlags _inputFlags;
- (void *)_hidEvent;
- (NSString *)_unmodifiedInput;
- (NSString *)_modifiedInput;
- (NSInteger)_modifierFlags;
- (BOOL)_isKeyDown;
@end

typedef enum {
    UIFontTraitPlain = 0x00000000,
} UIFontTrait;

@interface UIFont (Details)
+ (UIFont *)fontWithFamilyName:(NSString *)familyName traits:(UIFontTrait)traits size:(CGFloat)fontSize;
@end

typedef enum {
    UIAllCorners = 0xFF,
} UIRectCorners;

@interface UIImagePickerController (Details)
@property (nonatomic, setter=_setAllowsMultipleSelection:) BOOL _allowsMultipleSelection;
@end

@interface UIImage (Details)
- (id)initWithCGImage:(CGImageRef)CGImage imageOrientation:(UIImageOrientation)imageOrientation;
@end

@interface UIKeyCommand (Details)
@property (nonatomic, readonly) UIEvent *_triggeringEvent;
@end

@protocol UIKeyboardImplGeometryDelegate
@property (nonatomic, readwrite, getter=isMinimized) BOOL minimized;
- (void)prepareForImplBoundsHeightChange:(CGFloat)endDelta suppressNotification:(BOOL)suppressNotification;
- (void)implBoundsHeightChangeDone:(CGFloat)endDelta suppressNotification:(BOOL)suppressNotification;
- (BOOL)shouldSaveMinimizationState;
- (BOOL)canDismiss;
- (BOOL)isActive;
@end

@protocol UIKeyboardCandidateListDelegate <NSObject>
@optional
- (void)setCandidateList:(id)candidateList updateCandidateView:(BOOL)updateCandidateView;
- (void)candidateListAcceptCandidate:(id)candidateList;
- (void)candidateListSelectionDidChange:(id)candidateList;
- (void)candidateListShouldBeDismissed:(id)candidateList;
@end

@interface UIKeyboard : UIView <UIKeyboardImplGeometryDelegate>
@end

@interface UIKeyboard (Details)
+ (CGSize)defaultSizeForInterfaceOrientation:(UIInterfaceOrientation)orientation;
- (void)activate;
- (void)geometryChangeDone:(BOOL)keyboardVisible;
- (void)prepareForGeometryChange;
@end

@interface UIKeyboardImpl : UIView <UIKeyboardCandidateListDelegate>
@end

@interface UIKeyboardImpl (Details)
+ (UIKeyboardImpl *)activeInstance;
+ (UIKeyboardImpl *)sharedInstance;
+ (CGSize)defaultSizeForInterfaceOrientation:(UIInterfaceOrientation)orientation;
- (void)addInputString:(NSString *)string withFlags:(NSUInteger)flags;
- (BOOL)autocorrectSpellingEnabled;
- (void)deleteFromInput;
- (void)deleteFromInputWithFlags:(NSUInteger)flags;
- (void)replaceText:(id)replacement;
@property (nonatomic, readwrite, retain) UIResponder <UIKeyInput> *delegate;
@end

@interface UIGestureRecognizer (Details)
- (void)requireOtherGestureToFail:(UIGestureRecognizer *)gestureRecognizer;
@end

@interface UILongPressGestureRecognizer (Details)
@property (nonatomic) CFTimeInterval delay;
@property (nonatomic, readonly) CGPoint startPoint;
@end

@interface _UIWebHighlightLongPressGestureRecognizer : UILongPressGestureRecognizer
@end

@interface _UIWebHighlightLongPressGestureRecognizer (Details)
- (void)cancel;
@end

@interface UIPeripheralHost : NSObject <UIGestureRecognizerDelegate>
@end

@class UIKeyboardRotationState;

@interface UIPeripheralHost (Details)
+ (UIPeripheralHost *)sharedInstance;
+ (UIPeripheralHost *)activeInstance;
+ (CGRect)visiblePeripheralFrame;
- (BOOL)isOnScreen;
- (UIKeyboardRotationState *)rotationState;
@end

@interface UIPickerContentView : UIView
@end

@interface UIPickerContentView (Details)
+(CGFloat)_checkmarkOffset;
-(CGFloat)labelWidthForBounds:(CGRect)bounds;
@property (nonatomic, getter=isChecked) BOOL checked;
@property (nonatomic, readonly) UILabel *titleLabel;
@end

@interface UIPickerView (Details)
+ (CGSize)defaultSizeForCurrentOrientation;
- (void)_setUsesCheckedSelection:(BOOL)usesCheckedSelection;
@property (nonatomic, setter=_setMagnifierEnabled:) BOOL _magnifierEnabled;
@end

@interface UIResponder (Details)
- (void)_handleKeyUIEvent:(UIEvent *)event;
@end

@class CADisplay;
@interface UIScreen (Details)
- (CADisplay *)_display;
@end

@interface UIScrollView (Details)
- (void)_stopScrollingAndZoomingAnimations;
- (void)_zoomToCenter:(CGPoint)center scale:(CGFloat)scale duration:(CFTimeInterval)duration force:(BOOL)force;
- (void)_zoomToCenter:(CGPoint)center scale:(CGFloat)scale duration:(CFTimeInterval)duration;
@property (nonatomic, getter=isZoomEnabled) BOOL zoomEnabled;
@property (nonatomic, readonly, getter=_isAnimatingZoom) BOOL isAnimatingZoom;
@property (nonatomic, readonly, getter=_isAnimatingScroll) BOOL isAnimatingScroll;
@property (nonatomic) CGFloat horizontalScrollDecelerationFactor;
@property (nonatomic) CGFloat verticalScrollDecelerationFactor;
@end

@interface NSString (UIKitDetails)
- (CGSize)_legacy_sizeWithFont:(UIFont *)font forWidth:(CGFloat)width lineBreakMode:(NSLineBreakMode)lineBreakMode;
- (CGSize)_legacy_sizeWithFont:(UIFont *)font minFontSize:(CGFloat)minFontSize actualFontSize:(CGFloat *)actualFontSize forWidth:(CGFloat)width lineBreakMode:(NSLineBreakMode)lineBreakMode;
@end

@interface UITapGestureRecognizer (Details)
@property (nonatomic, readonly) CGPoint location;
@end

@class WebEvent;

typedef enum {
    UITextShortcutConversionTypeDefault = 0,
    UITextShortcutConversionTypeNo = 1,
    UITextShortcutConversionTypeYes = 2,
} UITextShortcutConversionType;

@protocol UITextInputTraits_Private <NSObject, UITextInputTraits>
- (void)takeTraitsFrom:(id <UITextInputTraits>)traits;
@optional
@property (nonatomic) UITextShortcutConversionType shortcutConversionType;
@end

@class UITextInputArrowKeyHistory;

@protocol UITextInputPrivate <UITextInput, UITextInputTokenizer, UITextInputTraits_Private>
@optional
- (BOOL)requiresKeyEvents;
- (NSArray *)metadataDictionariesForDictationResults;
- (UIColor *)textColorForCaretSelection;
- (UIFont *)fontForCaretSelection;
- (UIView *)automaticallySelectedOverlay;
- (void)handleKeyWebEvent:(WebEvent *)event;
- (void)insertDictationResult:(NSArray *)dictationResult withCorrectionIdentifier:(id)correctionIdentifier;
- (void)replaceRangeWithTextWithoutClosingTyping:(UITextRange *)range replacementText:(NSString *)text;
- (void)setBottomBufferHeight:(CGFloat)bottomBuffer;
@property (nonatomic) UITextGranularity selectionGranularity;
@required
- (BOOL)hasContent;
- (BOOL)hasSelection;
- (void)selectAll;
@end

@interface UITextInputTraits : NSObject <UITextInputTraits, UITextInputTraits_Private, NSCopying>
@end

@interface UITextInteractionAssistant : NSObject
@end

@interface UITextInteractionAssistant (Details)
- (void)activateSelection;
- (void)deactivateSelection;
- (void)didEndScrollingOverflow;
- (void)selectionChanged;
- (void)setGestureRecognizers;
- (void)willStartScrollingOverflow;
@end

@interface UIViewController (Details)
+ (UIViewController *)_viewControllerForFullScreenPresentationFromView:(UIView *)view;
+ (UIViewController *)viewControllerForView:(UIView *)view;
@end

@interface UIViewController (ViewService)
- (pid_t)_hostProcessIdentifier;
@end

@protocol UIViewControllerContextTransitioningEx <UIViewControllerContextTransitioning>
- (void)__runAlongsideAnimations;
- (void)_interactivityDidChange:(BOOL)isInteractive;
@property (nonatomic, assign, setter=_setAllowUserInteraction:, getter=_allowUserInteraction) BOOL _allowUserInteraction;
@property (nonatomic, assign, setter=_setPercentOffset:) CGFloat _percentOffset;
@property (nonatomic, retain, setter=_setContainerViews:) NSArray *_containerViews;
@end

@interface _UIViewControllerTransitionContext : NSObject <UIViewControllerContextTransitioningEx>
@end

@interface _UIViewControllerTransitionContext (Details)
- (void) _setTransitionIsInFlight:(BOOL)flag;
@property (nonatomic, assign, setter=_setAllowUserInteraction:, getter=_allowUserInteraction) BOOL _allowUserInteraction;
@property (nonatomic, assign, setter=_setAnimator:) id <UIViewControllerAnimatedTransitioning> _animator;
@property (nonatomic, assign, setter=_setContainerView:) UIView *containerView;
@property (nonatomic, assign, setter=_setInteractor:) id <UIViewControllerInteractiveTransitioning> _interactor;
@property (nonatomic, assign, setter=_setPercentOffset:) CGFloat _percentOffset;
@property (nonatomic, copy, setter=_setCompletionHandler:)  void (^_completionHandler)(_UIViewControllerTransitionContext *context, BOOL transitionCompleted);
@property (nonatomic, retain, setter=_setContainerViews:) NSArray *_containerViews;
@end

@interface _UIViewControllerOneToOneTransitionContext : _UIViewControllerTransitionContext
@end

@interface _UIViewControllerOneToOneTransitionContext (Details)
@property (nonatomic, assign, setter=_setFromEndFrame:) CGRect fromEndFrame;
@property (nonatomic, assign, setter=_setFromStartFrame:) CGRect fromStartFrame;
@property (nonatomic, assign, setter=_setToEndFrame:) CGRect toEndFrame;
@property (nonatomic, assign, setter=_setToStartFrame:) CGRect toStartFrame;
@property (nonatomic, retain, setter=_setFromViewController:) UIViewController *fromViewController;
@property (nonatomic, retain, setter=_setToViewController:) UIViewController *toViewController;
@end

@protocol UIViewControllerAnimatedTransitioningEx <UIViewControllerAnimatedTransitioning>
- (BOOL)interactionAborted;
- (UINavigationControllerOperation) operation;
- (UIPercentDrivenInteractiveTransition *)interactionController;
- (void)setInteractionAborted:(BOOL)aborted;
- (void)setInteractionController:(UIPercentDrivenInteractiveTransition *)controller;
- (void)setOperation:(UINavigationControllerOperation)operation;
@optional
- (UIWindow *)window;
@end

typedef NS_ENUM (NSInteger, _UIBackdropMaskViewFlags) {
    _UIBackdropMaskViewNone = 0,
    _UIBackdropMaskViewGrayscaleTint = 1 << 0,
    _UIBackdropMaskViewColorTint = 1 << 1,
    _UIBackdropMaskViewFilters = 1 << 2,
    _UIBackdropMaskViewAll = _UIBackdropMaskViewGrayscaleTint | _UIBackdropMaskViewColorTint | _UIBackdropMaskViewFilters,
};

@interface UIView (Details)
+ (BOOL)_isInAnimationBlock;
- (CGSize)size;
- (void)setFrameOrigin:(CGPoint)origin;
- (void)setSize:(CGSize)size;
@property (nonatomic, assign, setter=_setBackdropMaskViewFlags:) NSInteger _backdropMaskViewFlags;
@end

@interface UIWebSelectionAssistant : NSObject <UIGestureRecognizerDelegate>
@end

@interface UIWebSelectionAssistant (Details)
- (BOOL)isSelectionGestureRecognizer:(UIGestureRecognizer *)gestureRecognizer;
- (id)initWithView:(UIView *)view;
- (void)clearSelection;
- (void)didEndScrollingOrZoomingPage;
- (void)didEndScrollingOverflow;
- (void)resignedFirstResponder;
- (void)selectionChanged;
- (void)setGestureRecognizers;
- (void)willStartScrollingOrZoomingPage;
- (void)willStartScrollingOverflow;
@property (nonatomic, readonly) CGRect selectionFrame;
@end

typedef NS_ENUM(NSInteger, UIWKSelectionTouch) {
    UIWKSelectionTouchStarted = 0,
    UIWKSelectionTouchMoved = 1,
    UIWKSelectionTouchEnded = 2,
    UIWKSelectionTouchEndedMovingForward = 3,
    UIWKSelectionTouchEndedMovingBackward = 4,
    UIWKSelectionTouchEndedNotMoving = 5,
};

typedef NS_ENUM(NSInteger, UIWKSelectionFlags) {
    UIWKNone = 0,
    UIWKWordIsNearTap = 1,
    UIWKIsBlockSelection = 2,
    UIWKPhraseBoundaryChanged = 4,
};

typedef NS_ENUM(NSInteger, UIWKGestureType) {
    UIWKGestureLoupe = 0,
    UIWKGestureOneFingerTap = 1,
    UIWKGestureTapAndAHalf = 2,
    UIWKGestureDoubleTap = 3,
    UIWKGestureTapAndHalf = 4,
    UIWKGestureDoubleTapInUneditable = 5,
    UIWKGestureOneFingerTapInUneditable = 6,
    UIWKGestureOneFingerTapSelectsAll = 7,
    UIWKGestureOneFingerDoubleTap = 8,
    UIWKGestureOneFingerTripleTap = 9,
    UIWKGestureTwoFingerSingleTap = 10,
    UIWKGestureTwoFingerRangedSelectGesture = 11,
    UIWKGestureTapOnLinkWithGesture = 12,
    UIWKGestureMakeWebSelection = 13,
    UIWKGesturePhraseBoundary = 14,
};

@interface UIWKSelectionAssistant : UIWebSelectionAssistant
@end

@interface UIWKSelectionAssistant (Details)
- (BOOL)shouldHandleSingleTapAtPoint:(CGPoint)point;
- (void)blockSelectionChangedWithTouch:(UIWKSelectionTouch)touch withFlags:(UIWKSelectionFlags)flags growThreshold:(CGFloat)grow shrinkThreshold:(CGFloat)shrink;
- (void)selectionChangedWithGestureAt:(CGPoint)point withGesture:(UIWKGestureType)gestureType withState:(UIGestureRecognizerState)gestureState withFlags:(UIWKSelectionFlags)flags;
- (void)selectionChangedWithTouchAt:(CGPoint)point withSelectionTouch:(UIWKSelectionTouch)touch withFlags:(UIWKSelectionFlags)flags;
- (void)selectionChangedWithTouchAt:(CGPoint)point withSelectionTouch:(UIWKSelectionTouch)touch;
- (void)showDictionaryFor:(NSString *)selectedTerm fromRect:(CGRect)presentationRect;
@property (nonatomic, readonly) UILongPressGestureRecognizer *selectionLongPressRecognizer;
@end

typedef NS_ENUM(NSInteger, UIWKHandlePosition) {
    UIWKHandleTop = 0,
    UIWKHandleRight = 1,
    UIWKHandleBottom = 2,
    UIWKHandleLeft = 3,
};

@interface UIWKAutocorrectionRects : NSObject
@end

@interface UIWKAutocorrectionRects (UIWKAutocorrectionRectsDetails)
@property (nonatomic, assign) CGRect firstRect;
@property (nonatomic, assign) CGRect lastRect;
@end

@interface UIWKAutocorrectionContext : NSObject
@end

@interface UIWKAutocorrectionContext (UIWKAutocorrectionContextDetails)
@property (nonatomic, copy) NSString *contextBeforeSelection;
@property (nonatomic, copy) NSString *selectedText;
@property (nonatomic, copy) NSString *contextAfterSelection;
@property (nonatomic, copy) NSString *markedText;
@property (nonatomic, assign) NSRange rangeInMarkedText;
@end

@interface UIWKTextInteractionAssistant : UITextInteractionAssistant
@end

@interface UIWKTextInteractionAssistant (UIWKTextInteractionAssistantDetails)
- (void)selectionChangedWithGestureAt:(CGPoint)point withGesture:(UIWKGestureType)gestureType withState:(UIGestureRecognizerState)gestureState withFlags:(UIWKSelectionFlags)flags;
- (void)showDictionaryFor:(NSString *)selectedTerm fromRect:(CGRect)presentationRect;
- (void)selectionChangedWithTouchAt:(CGPoint)point withSelectionTouch:(UIWKSelectionTouch)touch;
- (void)showTextStyleOptions;
- (void)hideTextStyleOptions;

@property (nonatomic, readonly, retain) UITapGestureRecognizer *singleTapGesture;
@property (nonatomic, readonly, retain) UILongPressGestureRecognizer *loupeGesture;
@end

@protocol UIWKInteractionViewProtocol
- (void)changeSelectionWithGestureAt:(CGPoint)point withGesture:(UIWKGestureType)gestureType withState:(UIGestureRecognizerState)state;

- (void)changeSelectionWithTouchAt:(CGPoint)point withSelectionTouch:(UIWKSelectionTouch)touch baseIsStart:(BOOL)baseIsStart;
- (void)changeSelectionWithTouchesFrom:(CGPoint)from to:(CGPoint)to withGesture:(UIWKGestureType)gestureType withState:(UIGestureRecognizerState)gestureState;
- (CGRect)textFirstRect;
- (CGRect)textLastRect;

- (void)requestAutocorrectionContextWithCompletionHandler:(void (^)(UIWKAutocorrectionContext *autocorrectionContext))completionHandler;

- (void)requestAutocorrectionRectsForString:(NSString *)input withCompletionHandler:(void (^)(UIWKAutocorrectionRects *rectsForInput))completionHandler;

- (void)applyAutocorrection:(NSString *)correction toString:(NSString *)input withCompletionHandler:(void (^)(UIWKAutocorrectionRects *rectsForCorrection))completionHandler;

- (NSString *)markedText;
- (BOOL)hasMarkedText;

- (BOOL)hasSelectablePositionAtPoint:(CGPoint)point;
- (BOOL)pointIsInAssistedNode:(CGPoint)point;
- (NSArray *)webSelectionRects;
- (void)_cancelLongPressGestureRecognizer;

@optional
- (void)changeSelectionWithTouchAt:(CGPoint)point withSelectionTouch:(UIWKSelectionTouch)touch baseIsStart:(BOOL)baseIsStart withFlags:(UIWKSelectionFlags)flags;
- (void)changeBlockSelectionWithTouchAt:(CGPoint)point withSelectionTouch:(UIWKSelectionTouch)touch forHandle:(UIWKHandlePosition)handle;
- (void)clearSelection;
- (void)replaceDictatedText:(NSString *)oldText withText:(NSString *)newText;
- (void)requestDictationContext:(void (^)(NSString *selectedText, NSString *prefixText, NSString *postfixText))completionHandler;
- (BOOL)pointIsNearMarkedText:(CGPoint)point;
- (NSString *)selectedText;
- (void)replaceText:(NSString *)text withText:(NSString *)word;
- (void)selectWordForReplacement;
- (BOOL)isReplaceAllowed;
- (void)selectWordBackward;
- (UIView *)unscaledView;
- (CGFloat)inverseScale;
- (CGRect)unobscuredContentRect;
@end

typedef enum {
    UIWebSelectionModeWeb = 0,
    UIWebSelectionModeTextOnly = 1,
} UIWebSelectionMode;

@protocol UIWebFormAccessoryDelegate;

@interface UIWebFormAccessory : UIInputView
@end

@interface UIWebFormAccessory (Details)
- (void)hideAutoFillButton;
- (void)setClearVisible:(BOOL)flag;
- (void)showAutoFillButtonWithTitle:(NSString *)title;
@property (nonatomic, retain) UIBarButtonItem *_autofill;
@property (nonatomic, assign) id <UIWebFormAccessoryDelegate> delegate;

@property (nonatomic, assign, getter=isNextEnabled) BOOL nextEnabled;
@property (nonatomic, assign, getter=isPreviousEnabled) BOOL previousEnabled;
@end

@protocol UIWebFormAccessoryDelegate
- (void)accessoryAutoFill;
- (void)accessoryClear;
- (void)accessoryDone;
- (void)accessoryTab:(BOOL)isNext;
@end

@interface UIWebGeolocationPolicyDecider : NSObject
@end

@interface UIWebGeolocationPolicyDecider (Details)
+ (instancetype)sharedPolicyDecider;
- (void)decidePolicyForGeolocationRequestFromOrigin:(id)securityOrigin requestingURL:(NSURL *)requestingURL window:(UIWindow *)window listener:(id)listener;
@end

typedef enum {
    UIWebTouchEventTouchBegin = 0,
    UIWebTouchEventTouchChange = 1,
    UIWebTouchEventTouchEnd = 2,
    UIWebTouchEventTouchCancel = 3,
} UIWebTouchEventType;

struct _UIWebTouchPoint {
    CGPoint locationInScreenCoordinates;
    CGPoint locationInDocumentCoordinates;
    unsigned identifier;
    UITouchPhase phase;
    CGFloat majorRadiusInScreenCoordinates;
};

struct _UIWebTouchEvent {
    UIWebTouchEventType type;
    NSTimeInterval timestamp;
    CGPoint locationInScreenCoordinates;
    CGPoint locationInDocumentCoordinates;
    CGFloat scale;
    CGFloat rotation;

    bool inJavaScriptGesture;

    struct _UIWebTouchPoint* touchPoints;
    unsigned touchPointCount;

#if __IPHONE_OS_VERSION_MIN_REQUIRED >= 90000
    bool isPotentialTap;
#endif
};

@protocol UIWebTouchEventsGestureRecognizerDelegate
- (BOOL)isAnyTouchOverActiveArea:(NSSet *)touches;
- (BOOL)shouldIgnoreWebTouch;
@end

@interface UIWebTouchEventsGestureRecognizer : UIGestureRecognizer
@end

@interface UIWebTouchEventsGestureRecognizer (Details)
- (id)initWithTarget:(id)target action:(SEL)action touchDelegate:(id <UIWebTouchEventsGestureRecognizerDelegate>)delegate;
@property (nonatomic, getter=isDefaultPrevented) BOOL defaultPrevented;
@property (nonatomic, readonly) BOOL inJavaScriptGesture;
@property (nonatomic, readonly) CGPoint locationInWindow;
@property (nonatomic, readonly) UIWebTouchEventType type;
@property (nonatomic, readonly) const struct _UIWebTouchEvent *lastTouchEvent;
@end

typedef NS_ENUM(NSInteger, _UIBackdropViewStylePrivate) {
    _UIBackdropViewStyle_Light = 2020,
    _UIBackdropViewStyle_Dark = 2030
};

@interface _UIBackdropView : UIView
@end

@interface _UIBackdropView (_UIBackdropViewDetails)
- (instancetype)initWithPrivateStyle:(_UIBackdropViewStylePrivate)style;
- (instancetype)initWithFrame:(CGRect)frame privateStyle:(_UIBackdropViewStylePrivate)style;
@property (nonatomic, strong, readonly) UIView *contentView;
@end

@interface _UIHighlightView : UIView
@end

@interface _UIHighlightView (Details)
- (void)setColor:(UIColor *)aColor;
- (void)setCornerRadii:(NSArray *)cornerRadii;
- (void)setCornerRadius:(CGFloat)aCornerRadius;
- (void)setFrames:(NSArray *)frames boundaryRect:(CGRect)aBoundarRect;
- (void)setQuads:(NSArray *)quads boundaryRect:(CGRect)aBoundaryRect;
@end

@interface _UINavigationParallaxTransition : NSObject <UIViewControllerAnimatedTransitioningEx>
@end

@interface _UINavigationParallaxTransition (Details)
- (instancetype) initWithCurrentOperation:(UINavigationControllerOperation)operation;
@end

@protocol _UINavigationInteractiveTransitionBaseDelegate;

@interface _UINavigationInteractiveTransitionBase : UIPercentDrivenInteractiveTransition <UIGestureRecognizerDelegate>
@end

@interface _UINavigationInteractiveTransitionBase (Details)
- (id)initWithGestureRecognizerView:(UIView *)gestureRecognizerView animator:(id<UIViewControllerAnimatedTransitioning>)animator delegate:(id<_UINavigationInteractiveTransitionBaseDelegate>)delegate;
@property (nonatomic, assign) BOOL shouldReverseTranslation;
@property (nonatomic, retain) _UINavigationParallaxTransition *animationController;
@end

@protocol _UINavigationInteractiveTransitionBaseDelegate <NSObject>
- (void)startInteractiveTransition:(_UINavigationInteractiveTransitionBase *)interactiveTransition;
- (BOOL)shouldBeginInteractiveTransition:(_UINavigationInteractiveTransitionBase *)interactiveTransition;
- (BOOL)interactiveTransition:(_UINavigationInteractiveTransitionBase *)interactiveTransition gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldRecognizeSimultaneouslyWithGestureRecognizer:(UIGestureRecognizer *)otherGestureRecognizer;
- (BOOL)interactiveTransition:(_UINavigationInteractiveTransitionBase *)interactiveTransition gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldReceiveTouch:(UITouch *)touch;
- (UIPanGestureRecognizer *)gestureRecognizerForInteractiveTransition:(_UINavigationInteractiveTransitionBase *)interactiveTransition WithTarget:(id)target action:(SEL)action;
@end

@class BKSAnimationFenceHandle;

@interface UIWindow (Details)
+ (BKSAnimationFenceHandle *)_synchronizedDrawingFence;
+ (mach_port_t)_synchronizeDrawingAcrossProcesses;
@end

@interface UIWebScrollView : UIScrollView
@end

@interface UIWebTiledView : UIView
@end

@class WAKWindow;

@interface UIWebTiledView (Details)
- (void)setWAKWindow:(WAKWindow *)window;
@end

@interface UIWebDocumentView : UIWebTiledView
@end

typedef enum {
    UIEveryDocumentMask = 0xFFFFFF,
} UIDocumentMask;

@interface UIWebDocumentView (Details)
- (void)setDelegate:(id)delegate;
- (void)setAutoresizes:(BOOL)flag;
- (void)setMinimumSize:(CGSize)aSize;
- (void)setInitialScale:(float)aScale forDocumentTypes:(UIDocumentMask)aDocumentMask;
- (void)setViewportSize:(CGSize)aSize forDocumentTypes:(UIDocumentMask)aDocumentMask;
- (void)setMinimumScale:(float)aScale forDocumentTypes:(UIDocumentMask)aDocumentMask;
- (void)setMaximumScale:(float)aScale forDocumentTypes:(UIDocumentMask)aDocumentMask;
@end

@interface UIWebBrowserView : UIWebDocumentView
@end

@class WebView;

@interface UIWebBrowserView (Details)
- (WebView *)webView;
- (void)setPaused:(BOOL)paused;
- (void)sendScrollEventIfNecessaryWasUserScroll:(BOOL)userScroll;
@property (nonatomic) BOOL inputViewObeysDOMFocus;
@end

@interface UIDocumentMenuViewController (Details)
@property (nonatomic, assign, setter = _setIgnoreApplicationEntitlementForImport:, getter = _ignoreApplicationEntitlementForImport) BOOL _ignoreApplicationEntitlementForImport;
@end

@protocol UIDocumentPasswordViewDelegate;

@interface UIDocumentPasswordView : UIView <UITextFieldDelegate>
@end

@interface UIDocumentPasswordView (Details)

- (id)initWithDocumentName:(NSString *)documentName;

@property (nonatomic, assign) NSObject<UIDocumentPasswordViewDelegate> *passwordDelegate;
@property (nonatomic, readonly) UITextField *passwordField;

@end

@protocol UIDocumentPasswordViewDelegate

@required

- (void)userDidEnterPassword:(NSString *)password forPasswordView:(UIDocumentPasswordView *)passwordView;

@optional

- (void)didBeginEditingPassword:(UITextField *)passwordField inView:(UIDocumentPasswordView *)passwordView;
- (void)didEndEditingPassword:(UITextField *)passwordField inView:(UIDocumentPasswordView *)passwordView;

@end

#endif // USE(APPLE_INTERNAL_SDK)

@interface UIView (IPI)
- (UIScrollView *)_scroller;
- (CGPoint)accessibilityConvertPointFromSceneReferenceCoordinates:(CGPoint)point;
- (CGRect)accessibilityConvertRectToSceneReferenceCoordinates:(CGRect)rect;
@end

WTF_EXTERN_C_BEGIN

BOOL UIKeyboardEnabledInputModesAllowOneToManyShortcuts();
BOOL UIKeyboardEnabledInputModesAllowChineseTransliterationForText(NSString *);
BOOL UIKeyboardCurrentInputModeAllowsChineseOrJapaneseReanalysisForText(NSString *);

extern const float UITableCellDefaultFontSize;
extern const float UITableViewCellDefaultFontSize;

extern NSString * const UIWindowDidMoveToScreenNotification;
extern NSString * const UIWindowDidRotateNotification;
extern NSString * const UIWindowNewScreenUserInfoKey;
extern NSString * const UIWindowWillRotateNotification;

extern NSString * const UIKeyboardIsLocalUserInfoKey;

extern UIApplication *UIApp;
BOOL _UIApplicationIsExtension(void);
void _UIApplicationLoadWebKit(void);
BOOL _UIApplicationUsesLegacyUI(void);

void UIImageDataWriteToSavedPhotosAlbum(NSData *imageData, id completionTarget, SEL completionSelector, void *contextInfo);

UIImage* _UIImageGetWebKitPhotoLibraryIcon(void);
UIImage* _UIImageGetWebKitTakePhotoOrVideoIcon(void);

extern const float UIWebViewGrowsAndShrinksToFitHeight;
extern const float UIWebViewScalesToFitScale;
extern const float UIWebViewStandardViewportWidth;

extern NSString *const UIKeyInputPageUp;
extern NSString *const UIKeyInputPageDown;

WTF_EXTERN_C_END
