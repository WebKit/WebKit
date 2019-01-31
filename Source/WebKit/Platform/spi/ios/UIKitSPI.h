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

#import <UIKit/UIKit.h>

#if USE(APPLE_INTERNAL_SDK)

#import <UIKit/UIAlertController_Private.h>
#import <UIKit/UIApplication_Private.h>
#import <UIKit/UIBarButtonItem_Private.h>
#import <UIKit/UIBlurEffect_Private.h>
#import <UIKit/UICalloutBar.h>
#import <UIKit/UIColorEffect.h>
#import <UIKit/UIDatePicker_Private.h>
#import <UIKit/UIDevice_Private.h>
#import <UIKit/UIDocumentMenuViewController_Private.h>
#import <UIKit/UIDocumentPasswordView.h>
#import <UIKit/UIFont_Private.h>
#import <UIKit/UIGeometry_Private.h>
#import <UIKit/UIGestureRecognizer_Private.h>
#import <UIKit/UIImagePickerController_Private.h>
#import <UIKit/UIImage_Private.h>
#import <UIKit/UIInterface_Private.h>
#import <UIKit/UIKeyboardImpl.h>
#import <UIKit/UIKeyboardInputModeController.h>
#import <UIKit/UIKeyboardIntl.h>
#import <UIKit/UIKeyboard_Private.h>
#import <UIKit/UILongPressGestureRecognizer_Private.h>
#import <UIKit/UIPeripheralHost.h>
#import <UIKit/UIPeripheralHost_Private.h>
#import <UIKit/UIPickerContentView_Private.h>
#import <UIKit/UIPickerView_Private.h>
#import <UIKit/UIPopoverPresentationController_Private.h>
#import <UIKit/UIPresentationController_Private.h>
#import <UIKit/UIResponder_Private.h>
#import <UIKit/UIScrollView_Private.h>
#import <UIKit/UIStringDrawing_Private.h>
#import <UIKit/UITableViewCell_Private.h>
#import <UIKit/UITapGestureRecognizer_Private.h>
#import <UIKit/UITextChecker_Private.h>
#import <UIKit/UITextEffectsWindow.h>
#import <UIKit/UITextInput_Private.h>
#import <UIKit/UITextInteractionAssistant_Private.h>
#import <UIKit/UIViewControllerTransitioning_Private.h>
#import <UIKit/UIViewController_Private.h>
#import <UIKit/UIViewController_ViewService.h>
#import <UIKit/UIView_Private.h>
#import <UIKit/UIVisualEffect_Private.h>
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
#import <UIKit/_UIApplicationRotationFollowing.h>
#import <UIKit/_UIBackdropViewSettings.h>
#import <UIKit/_UIBackdropView_Private.h>
#import <UIKit/_UIHighlightView.h>
#import <UIKit/_UINavigationInteractiveTransition.h>
#import <UIKit/_UINavigationParallaxTransition.h>

#if HAVE(LINK_PREVIEW)
#import <UIKit/UIPreviewItemController.h>
#endif

#if ENABLE(DRAG_SUPPORT)
#import <UIKit/NSItemProvider+UIKitAdditions_Private.h>
#endif

#if ENABLE(DRAG_SUPPORT)
#import <UIKit/UIDragInteraction.h>
#import <UIKit/UIDragInteraction_Private.h>
#import <UIKit/UIDragItem_Private.h>
#import <UIKit/UIDragPreviewParameters.h>
#import <UIKit/UIDragPreview_Private.h>
#import <UIKit/UIDragSession.h>
#import <UIKit/UIDragging.h>
#import <UIKit/UIDropInteraction.h>
#import <UIKit/UIPreviewInteraction.h>
#import <UIKit/UIURLDragPreviewView.h>
#import <UIKit/_UITextDragCaretView.h>
#endif

#else // USE(APPLE_INTERNAL_SDK)

#if ENABLE(DRAG_SUPPORT)
#import <UIKit/NSItemProvider+UIKitAdditions.h>
#endif

#if HAVE(LINK_PREVIEW)
typedef NS_ENUM(NSInteger, UIPreviewItemType) {
    UIPreviewItemTypeNone,
    UIPreviewItemTypeClientCustom,
    UIPreviewItemTypeLink,
    UIPreviewItemTypeImage,
    UIPreviewItemTypeText,
    UIPreviewItemTypeAttachment,
};

@class UIPreviewItemController;

@protocol UIPreviewItemDelegate <NSObject>
- (NSDictionary *)_dataForPreviewItemController:(UIPreviewItemController *)controller atPosition:(CGPoint)position type:(UIPreviewItemType *)type;
@optional
- (BOOL)_interactionShouldBeginFromPreviewItemController:(UIPreviewItemController *)controller forPosition:(CGPoint)position;
- (void)_interactionStartedFromPreviewItemController:(UIPreviewItemController *)controller;
- (void)_interactionStoppedFromPreviewItemController:(UIPreviewItemController *)controller;
- (UIViewController *)_presentedViewControllerForPreviewItemController:(UIPreviewItemController *)controller;
- (void)_previewItemController:(UIPreviewItemController *)controller didDismissPreview:(UIViewController *)viewController committing:(BOOL)committing;
- (void)_previewItemController:(UIPreviewItemController *)controller commitPreview:(UIViewController *)viewController;
- (void)_previewItemControllerDidCancelPreview:(UIPreviewItemController *)controller;
- (UIImage *)_presentationSnapshotForPreviewItemController:(UIPreviewItemController *)controller;
- (NSArray *)_presentationRectsForPreviewItemController:(UIPreviewItemController *)controller;
- (CGRect)_presentationRectForPreviewItemController:(UIPreviewItemController *)controller;
@end

@interface UIPreviewItemController : NSObject
- (instancetype)initWithView:(UIView *)view;
@property (assign, nonatomic) id<UIPreviewItemDelegate> delegate;
@property (assign, nonatomic, readonly) UIPreviewItemType type;
@property (strong, nonatomic, readonly) NSDictionary *previewData;
@property (strong, nonatomic, readonly) UIGestureRecognizer *presentationGestureRecognizer;
@property (strong, nonatomic, readonly) UIGestureRecognizer *presentationSecondaryGestureRecognizer;
@end
#endif

@interface UIAlertController ()
- (void)_addActionWithTitle:(NSString *)title style:(UIAlertActionStyle)style handler:(void (^)(void))handler;
- (void)_addActionWithTitle:(NSString *)title style:(UIAlertActionStyle)style handler:(void (^)(void))handler shouldDismissHandler:(BOOL (^)(void))shouldDismissHandler;
@property (nonatomic) UIAlertControllerStyle preferredStyle;
@end

WTF_EXTERN_C_BEGIN
typedef struct __IOHIDEvent* IOHIDEventRef;
typedef struct __GSKeyboard* GSKeyboardRef;
WTF_EXTERN_C_END

@interface UIApplication ()
- (UIInterfaceOrientation)interfaceOrientation;
- (void)_cancelAllTouches;
- (CGFloat)statusBarHeight;
- (BOOL)isSuspendedUnderLock;
- (void)_enqueueHIDEvent:(IOHIDEventRef)event;
- (void)_handleHIDEvent:(IOHIDEventRef)event;
- (void)handleKeyUIEvent:(UIEvent *)event;
@end

typedef NS_ENUM(NSInteger, UIDatePickerPrivateMode)  {
    UIDatePickerModeYearAndMonth = 4269,
};

@interface UIDatePicker ()
@property (nonatomic, readonly, getter=_contentWidth) CGFloat contentWidth;
@end

@interface UIDevice ()
- (void)setOrientation:(UIDeviceOrientation)orientation animated:(BOOL)animated;
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

@interface UIEvent ()
- (void *)_hidEvent;
- (NSString *)_unmodifiedInput;
- (NSString *)_modifiedInput;
- (BOOL)_isKeyDown;
@end

typedef enum {
    UIFontTraitPlain = 0,
    UIFontTraitItalic = 1 << 0,
    UIFontTraitBold = 1 << 1,
} UIFontTrait;

@interface UIFont ()
+ (UIFont *)fontWithFamilyName:(NSString *)familyName traits:(UIFontTrait)traits size:(CGFloat)fontSize;
- (UIFontTrait)traits;
@end

typedef enum {
    UIAllCorners = 0xFF,
} UIRectCorners;

@interface UIImagePickerController ()
@property (nonatomic, setter=_setAllowsMultipleSelection:) BOOL _allowsMultipleSelection;
@end

@interface UIImage ()
- (id)initWithCGImage:(CGImageRef)CGImage imageOrientation:(UIImageOrientation)imageOrientation;
@end

@interface UIKeyCommand ()
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

@interface UIKeyboard ()
+ (CGSize)defaultSizeForInterfaceOrientation:(UIInterfaceOrientation)orientation;
- (void)activate;
- (void)geometryChangeDone:(BOOL)keyboardVisible;
- (void)prepareForGeometryChange;
+ (BOOL)isInHardwareKeyboardMode;
+ (void)removeAllDynamicDictionaries;
@end

@interface UIKeyboardImpl : UIView <UIKeyboardCandidateListDelegate>
@end

@interface UIKeyboardImpl ()
+ (UIKeyboardImpl *)activeInstance;
+ (UIKeyboardImpl *)sharedInstance;
+ (CGSize)defaultSizeForInterfaceOrientation:(UIInterfaceOrientation)orientation;
- (void)addInputString:(NSString *)string withFlags:(NSUInteger)flags;
- (void)addInputString:(NSString *)string withFlags:(NSUInteger)flags withInputManagerHint:(NSString *)hint;
- (BOOL)autocorrectSpellingEnabled;
- (void)deleteFromInput;
- (void)deleteFromInputWithFlags:(NSUInteger)flags;
- (void)replaceText:(id)replacement;
@property (nonatomic, readwrite, retain) UIResponder <UIKeyInput> *delegate;
@end

@interface UILongPressGestureRecognizer ()
@property (nonatomic) CFTimeInterval delay;
@property (nonatomic, readonly) CGPoint startPoint;
@property (nonatomic, assign, setter=_setRequiresQuietImpulse:) BOOL _requiresQuietImpulse;
@end

@interface _UIWebHighlightLongPressGestureRecognizer : UILongPressGestureRecognizer
@end

@interface _UIWebHighlightLongPressGestureRecognizer ()
- (void)cancel;
@end

@interface UIPeripheralHost : NSObject <UIGestureRecognizerDelegate>
@end

@class UIKeyboardRotationState;

@interface UIPeripheralHost ()
+ (UIPeripheralHost *)sharedInstance;
+ (UIPeripheralHost *)activeInstance;
+ (CGRect)visiblePeripheralFrame;
- (BOOL)isOnScreen;
- (BOOL)isUndocked;
- (UIKeyboardRotationState *)rotationState;
@end

@interface UIPickerContentView : UIView
@end

@interface UIPickerContentView ()
+(CGFloat)_checkmarkOffset;
-(CGFloat)labelWidthForBounds:(CGRect)bounds;
@property (nonatomic, getter=isChecked) BOOL checked;
@property (nonatomic, readonly) UILabel *titleLabel;
@end

@interface UIPickerView ()
+ (CGSize)defaultSizeForCurrentOrientation;
- (void)_setUsesCheckedSelection:(BOOL)usesCheckedSelection;
@property (nonatomic, setter=_setMagnifierEnabled:) BOOL _magnifierEnabled;
@end

@interface UIResponder ()
- (void)_handleKeyUIEvent:(UIEvent *)event;
- (void)_wheelChangedWithEvent:(UIEvent *)event;
@end

@class FBSDisplayConfiguration;
@interface UIScreen ()
- (void)_setScale:(CGFloat)scale;
@property (nonatomic, readonly, retain) FBSDisplayConfiguration *displayConfiguration;
@end

typedef NS_ENUM(NSInteger, UIScrollViewIndicatorInsetAdjustmentBehavior) {
    UIScrollViewIndicatorInsetAdjustmentAutomatic,
    UIScrollViewIndicatorInsetAdjustmentAlways,
    UIScrollViewIndicatorInsetAdjustmentNever
};

@interface UIScrollView ()
- (void)_stopScrollingAndZoomingAnimations;
- (void)_zoomToCenter:(CGPoint)center scale:(CGFloat)scale duration:(CFTimeInterval)duration force:(BOOL)force;
- (void)_zoomToCenter:(CGPoint)center scale:(CGFloat)scale duration:(CFTimeInterval)duration;
- (double)_horizontalVelocity;
- (double)_verticalVelocity;
@property (nonatomic, getter=isZoomEnabled) BOOL zoomEnabled;
@property (nonatomic, readonly, getter=_isAnimatingZoom) BOOL isAnimatingZoom;
@property (nonatomic, readonly, getter=_isAnimatingScroll) BOOL isAnimatingScroll;
@property (nonatomic) CGFloat horizontalScrollDecelerationFactor;
@property (nonatomic) CGFloat verticalScrollDecelerationFactor;
@property (nonatomic, readonly) BOOL _isInterruptingDeceleration;
@property (nonatomic, getter=_contentScrollInset, setter=_setContentScrollInset:) UIEdgeInsets contentScrollInset;
@property (nonatomic, getter=_indicatorInsetAdjustmentBehavior, setter=_setIndicatorInsetAdjustmentBehavior:) UIScrollViewIndicatorInsetAdjustmentBehavior indicatorInsetAdjustmentBehavior;
@property (nonatomic, readonly) UIEdgeInsets _systemContentInset;
@property (nonatomic, readonly) UIEdgeInsets _effectiveContentInset;
@end

@interface NSString (UIKitDetails)
- (CGSize)_legacy_sizeWithFont:(UIFont *)font forWidth:(CGFloat)width lineBreakMode:(NSLineBreakMode)lineBreakMode;
- (CGSize)_legacy_sizeWithFont:(UIFont *)font minFontSize:(CGFloat)minFontSize actualFontSize:(CGFloat *)actualFontSize forWidth:(CGFloat)width lineBreakMode:(NSLineBreakMode)lineBreakMode;
@end

@interface UITapGestureRecognizer ()
@property (nonatomic, getter=_allowableSeparation, setter=_setAllowableSeparation:) CGFloat allowableSeparation;
@property (nonatomic, readonly) CGPoint location;
@property (nonatomic) CGFloat allowableMovement;
@property (nonatomic, readonly) CGPoint centroid;
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
@property (nonatomic, retain) UIColor *insertionPointColor;
@property (nonatomic, retain) UIColor *selectionBarColor;
@property (nonatomic, retain) UIColor *selectionHighlightColor;
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
#if USE(UIKIT_KEYBOARD_ADDITIONS)
- (void)modifierFlagsDidChangeFrom:(UIKeyModifierFlags)oldFlags to:(UIKeyModifierFlags)newFlags;
#endif
@property (nonatomic) UITextGranularity selectionGranularity;
@required
- (BOOL)hasContent;
- (BOOL)hasSelection;
- (void)selectAll;
@end

@interface UITextInputTraits : NSObject <UITextInputTraits, UITextInputTraits_Private, NSCopying>
- (void)_setColorsToMatchTintColor:(UIColor *)tintColor;
@end

@interface UITextInteractionAssistant : NSObject
@end

@interface UITextInteractionAssistant ()
- (void)activateSelection;
- (void)deactivateSelection;
- (void)didEndScrollingOverflow;
- (void)selectionChanged;
- (void)setGestureRecognizers;
- (void)willStartScrollingOverflow;
@end

@interface UITextSuggestion : NSObject
+ (instancetype)textSuggestionWithInputText:(NSString *)inputText;
@property (nonatomic, copy, readonly) NSString *inputText;
@end

@protocol UITextInputSuggestionDelegate <UITextInputDelegate>
- (void)setSuggestions:(NSArray <UITextSuggestion*> *)suggestions;
@end

@interface UIViewController ()
+ (UIViewController *)_viewControllerForFullScreenPresentationFromView:(UIView *)view;
+ (UIViewController *)viewControllerForView:(UIView *)view;
@end

@interface UIViewController (ViewService)
- (pid_t)_hostProcessIdentifier;
@property (readonly) NSString *_hostApplicationBundleIdentifier;
@end

#if __IPHONE_OS_VERSION_MIN_REQUIRED >= 110000
@interface NSURL ()
@property (nonatomic, copy, setter=_setTitle:) NSString *_title;
@end
#endif

@protocol UIViewControllerContextTransitioningEx <UIViewControllerContextTransitioning>
- (void)__runAlongsideAnimations;
- (void)_interactivityDidChange:(BOOL)isInteractive;
@property (nonatomic, assign, setter=_setAllowUserInteraction:, getter=_allowUserInteraction) BOOL _allowUserInteraction;
@property (nonatomic, assign, setter=_setPercentOffset:) CGFloat _percentOffset;
@end

@interface _UIViewControllerTransitionContext : NSObject <UIViewControllerContextTransitioningEx>
@end

// FIXME: Separate the parts we are simply re-declaring from the ones we are overriding.
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

@interface _UIViewControllerOneToOneTransitionContext ()
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

@interface UIView ()
+ (BOOL)_isInAnimationBlock;
- (CGSize)size;
- (void)setFrameOrigin:(CGPoint)origin;
- (void)setSize:(CGSize)size;
@property (nonatomic, assign, setter=_setBackdropMaskViewFlags:) NSInteger _backdropMaskViewFlags;
- (void)_populateArchivedSubviews:(NSMutableSet *)encodedViews;
- (void)safeAreaInsetsDidChange;
@property (nonatomic, setter=_setContinuousCornerRadius:) CGFloat _continuousCornerRadius;
- (void)insertSubview:(UIView *)view above:(UIView *)sibling;
- (void)viewWillMoveToSuperview:(UIView *)newSuperview;
- (CGSize)convertSize:(CGSize)size toView:(UIView *)view;
- (void)_removeAllAnimations:(BOOL)includeSubviews;
- (UIColor *)_inheritedInteractionTintColor;
@end

@interface UIWebSelectionView : UIView
@end

@interface UIWebSelectionAssistant : NSObject <UIGestureRecognizerDelegate>
@end

@protocol UISelectionInteractionAssistant
- (void)showSelectionCommands;
@end

@interface UIWebSelectionAssistant ()
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
#if !PLATFORM(IOSMAC)
@property (nonatomic, retain) UIWebSelectionView *selectionView;
#endif
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

@interface UIWKSelectionAssistant ()
- (BOOL)shouldHandleSingleTapAtPoint:(CGPoint)point;
- (void)selectionChangedWithGestureAt:(CGPoint)point withGesture:(UIWKGestureType)gestureType withState:(UIGestureRecognizerState)gestureState withFlags:(UIWKSelectionFlags)flags;
- (void)selectionChangedWithTouchAt:(CGPoint)point withSelectionTouch:(UIWKSelectionTouch)touch withFlags:(UIWKSelectionFlags)flags;
- (void)showDictionaryFor:(NSString *)selectedTerm fromRect:(CGRect)presentationRect;
- (void)showShareSheetFor:(NSString *)selectedTerm fromRect:(CGRect)presentationRect;
- (void)showTextServiceFor:(NSString *)selectedTerm fromRect:(CGRect)presentationRect;
- (void)lookup:(NSString *)textWithContext withRange:(NSRange)range fromRect:(CGRect)presentationRect;
@property (nonatomic, readonly) UILongPressGestureRecognizer *selectionLongPressRecognizer;
@end

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

@interface UIWKTextInteractionAssistant : UITextInteractionAssistant <UIResponderStandardEditActions>
@end

@interface UIWKTextInteractionAssistant ()
- (void)selectionChangedWithGestureAt:(CGPoint)point withGesture:(UIWKGestureType)gestureType withState:(UIGestureRecognizerState)gestureState withFlags:(UIWKSelectionFlags)flags;
- (void)showDictionaryFor:(NSString *)selectedTerm fromRect:(CGRect)presentationRect;
- (void)selectionChangedWithTouchAt:(CGPoint)point withSelectionTouch:(UIWKSelectionTouch)touch withFlags:(UIWKSelectionFlags)flags;
- (void)showTextStyleOptions;
- (void)hideTextStyleOptions;
- (void)lookup:(NSString *)textWithContext withRange:(NSRange)range fromRect:(CGRect)presentationRect;
- (void)showShareSheetFor:(NSString *)selectedTerm fromRect:(CGRect)presentationRect;
- (void)showTextServiceFor:(NSString *)selectedTerm fromRect:(CGRect)presentationRect;
- (void)scheduleReplacementsForText:(NSString *)text;
- (void)scheduleChineseTransliterationForText:(NSString *)text;

@property (nonatomic, readonly, assign) UILongPressGestureRecognizer *forcePressGesture;
@property (nonatomic, readonly, assign) UILongPressGestureRecognizer *loupeGesture;
@property (nonatomic, readonly, assign) UITapGestureRecognizer *singleTapGesture;
@end

@protocol UIWKInteractionViewProtocol
- (void)changeSelectionWithGestureAt:(CGPoint)point withGesture:(UIWKGestureType)gestureType withState:(UIGestureRecognizerState)state;
- (void)changeSelectionWithTouchAt:(CGPoint)point withSelectionTouch:(UIWKSelectionTouch)touch baseIsStart:(BOOL)baseIsStart withFlags:(UIWKSelectionFlags)flags;
- (void)changeSelectionWithTouchesFrom:(CGPoint)from to:(CGPoint)to withGesture:(UIWKGestureType)gestureType withState:(UIGestureRecognizerState)gestureState;
- (CGRect)textFirstRect;
- (CGRect)textLastRect;

- (void)requestAutocorrectionContextWithCompletionHandler:(void (^)(UIWKAutocorrectionContext *autocorrectionContext))completionHandler;

- (void)requestAutocorrectionRectsForString:(NSString *)input withCompletionHandler:(void (^)(UIWKAutocorrectionRects *rectsForInput))completionHandler;

- (void)applyAutocorrection:(NSString *)correction toString:(NSString *)input withCompletionHandler:(void (^)(UIWKAutocorrectionRects *rectsForCorrection))completionHandler;

- (NSString *)markedText;
- (BOOL)hasMarkedText;

- (BOOL)hasSelectablePositionAtPoint:(CGPoint)point;
- (NSArray *)webSelectionRects;
- (void)_cancelLongPressGestureRecognizer;

@optional
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

@protocol UITextAutoscrolling
- (void)startAutoscroll:(CGPoint)point;
- (void)cancelAutoscroll;
- (void)scrollSelectionToVisible:(BOOL)animated;
@end


@protocol UIWebFormAccessoryDelegate;

@interface UIWebFormAccessory : UIInputView
@end

@interface UIWebFormAccessory ()
- (void)hideAutoFillButton;
- (void)setClearVisible:(BOOL)flag;
- (void)showAutoFillButtonWithTitle:(NSString *)title;
@property (nonatomic, retain) UIBarButtonItem *_autofill;
@property (nonatomic, assign) id <UIWebFormAccessoryDelegate> delegate;

@property (nonatomic, assign, getter=isNextEnabled) BOOL nextEnabled;
@property (nonatomic, assign, getter=isPreviousEnabled) BOOL previousEnabled;
- (id)initWithInputAssistantItem:(UITextInputAssistantItem *)inputAssistantItem;
@end

@protocol UIWebFormAccessoryDelegate
- (void)accessoryAutoFill;
- (void)accessoryClear;
- (void)accessoryDone;
- (void)accessoryTab:(BOOL)isNext;
@end

@interface UIWebGeolocationPolicyDecider : NSObject
@end

@interface UIWebGeolocationPolicyDecider ()
+ (instancetype)sharedPolicyDecider;
- (void)decidePolicyForGeolocationRequestFromOrigin:(id)securityOrigin requestingURL:(NSURL *)requestingURL window:(UIWindow *)window listener:(id)listener;
@end

typedef enum {
    UIWebTouchEventTouchBegin = 0,
    UIWebTouchEventTouchChange = 1,
    UIWebTouchEventTouchEnd = 2,
    UIWebTouchEventTouchCancel = 3,
} UIWebTouchEventType;

typedef enum {
    UIWebTouchPointTypeDirect = 0,
    UIWebTouchPointTypeStylus
} UIWebTouchPointType;

struct _UIWebTouchPoint {
    CGPoint locationInScreenCoordinates;
    CGPoint locationInDocumentCoordinates;
    unsigned identifier;
    UITouchPhase phase;
#if __IPHONE_OS_VERSION_MIN_REQUIRED > 100000
    CGFloat majorRadiusInScreenCoordinates;
    CGFloat force;
    CGFloat altitudeAngle;
    CGFloat azimuthAngle;
    UIWebTouchPointType touchType;
#endif
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

    bool isPotentialTap;
};

@interface _UILookupGestureRecognizer : UIGestureRecognizer
@end

@class UIWebTouchEventsGestureRecognizer;

@protocol UIWebTouchEventsGestureRecognizerDelegate <NSObject>
- (BOOL)isAnyTouchOverActiveArea:(NSSet *)touches;
@optional
- (BOOL)gestureRecognizer:(UIWebTouchEventsGestureRecognizer *)gestureRecognizer shouldIgnoreWebTouchWithEvent:(UIEvent *)event;
@end

@interface UIWebTouchEventsGestureRecognizer : UIGestureRecognizer
@end

@interface UIWebTouchEventsGestureRecognizer ()
- (id)initWithTarget:(id)target action:(SEL)action touchDelegate:(id <UIWebTouchEventsGestureRecognizerDelegate>)delegate;
- (void)cancel;
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

@interface _UIBackdropViewSettings : NSObject
@end

@interface _UIBackdropViewSettings ()
+ (_UIBackdropViewSettings *)settingsForPrivateStyle:(_UIBackdropViewStylePrivate)style;
@property (nonatomic, assign) CGFloat scale;
@end

@interface _UIBackdropView : UIView
@end

@interface _UIBackdropView ()
- (instancetype)initWithPrivateStyle:(_UIBackdropViewStylePrivate)style;
- (instancetype)initWithSettings:(_UIBackdropViewSettings *)settings;
- (instancetype)initWithFrame:(CGRect)frame privateStyle:(_UIBackdropViewStylePrivate)style;
@property (nonatomic, strong, readonly) UIView *contentView;
@end

@interface _UIHighlightView : UIView
@end

@interface _UIHighlightView ()
- (void)setColor:(UIColor *)aColor;
- (void)setCornerRadii:(NSArray *)cornerRadii;
- (void)setCornerRadius:(CGFloat)aCornerRadius;
- (void)setFrames:(NSArray *)frames boundaryRect:(CGRect)aBoundarRect;
- (void)setQuads:(NSArray *)quads boundaryRect:(CGRect)aBoundaryRect;
@end

@interface _UINavigationParallaxTransition : NSObject <UIViewControllerAnimatedTransitioningEx>
@end

@interface _UINavigationParallaxTransition ()
- (instancetype) initWithCurrentOperation:(UINavigationControllerOperation)operation;
@end

@protocol _UINavigationInteractiveTransitionBaseDelegate;

@interface _UINavigationInteractiveTransitionBase : UIPercentDrivenInteractiveTransition <UIGestureRecognizerDelegate>
@end

@interface _UINavigationInteractiveTransitionBase ()
- (id)initWithGestureRecognizerView:(UIView *)gestureRecognizerView animator:(id<UIViewControllerAnimatedTransitioning>)animator delegate:(id<_UINavigationInteractiveTransitionBaseDelegate>)delegate;
- (void)_completeStoppedInteractiveTransition;
@property (nonatomic, weak) UIPanGestureRecognizer *gestureRecognizer;
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

@interface UIWindow ()
+ (BKSAnimationFenceHandle *)_synchronizedDrawingFence;
+ (mach_port_t)_synchronizeDrawingAcrossProcesses;
- (void)_setWindowResolution:(CGFloat)resolution displayIfChanged:(BOOL)displayIfChanged;
- (uint32_t)_contextId;
@end

@interface UIWebScrollView : UIScrollView
@end

@interface UIWebTiledView : UIView
@end

@class WAKWindow;

@interface UIWebTiledView ()
- (void)setWAKWindow:(WAKWindow *)window;
@end

@interface UIWebDocumentView : UIWebTiledView
@end

typedef enum {
    UIEveryDocumentMask = 0xFFFFFF,
} UIDocumentMask;

@interface UIWebDocumentView ()
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

@interface UIWebBrowserView ()
- (WebView *)webView;
- (void)setPaused:(BOOL)paused;
- (void)sendScrollEventIfNecessaryWasUserScroll:(BOOL)userScroll;
@property (nonatomic) BOOL inputViewObeysDOMFocus;
@end

@interface UIDocumentMenuViewController ()
- (instancetype)_initIgnoringApplicationEntitlementForImportOfTypes:(NSArray *)types;
@end

@protocol UIDocumentPasswordViewDelegate;

@interface UIDocumentPasswordView : UIView <UITextFieldDelegate>
@end

@interface UIDocumentPasswordView ()

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

@interface UIViewControllerPreviewAction : NSObject <NSCopying>
@end

@interface UIViewControllerPreviewAction ()
+ (instancetype)actionWithTitle:(NSString *)title handler:(void (^)(UIViewControllerPreviewAction *action, UIViewController *previewViewController))handler;
@end

@interface UITextChecker ()
- (id)_initWithAsynchronousLoading:(BOOL)asynchronousLoading;
- (BOOL)_doneLoading;
- (NSRange)rangeOfMisspelledWordInString:(NSString *)stringToCheck range:(NSRange)range startingAt:(NSInteger)startingOffset wrap:(BOOL)wrapFlag languages:(NSArray *)languagesArray;
@end

@interface UIKeyboardInputMode : UITextInputMode <NSCopying>
+ (UIKeyboardInputMode *)keyboardInputModeWithIdentifier:(NSString *)identifier;
@property (nonatomic, readonly, retain) NSArray <NSString *> *multilingualLanguages;
@property (nonatomic, readonly, retain) NSString *languageWithRegion;
@end

@interface UIKeyboardInputModeController : NSObject
@end

@interface UIKeyboardInputModeController ()
+ (UIKeyboardInputModeController *)sharedInputModeController;
@property (readwrite, retain) UIKeyboardInputMode *currentInputMode;
@end

@interface UIApplicationRotationFollowingWindow : UIWindow
@end

@interface UIApplicationRotationFollowingController : UIViewController
@end

@interface UIApplicationRotationFollowingControllerNoTouches : UIApplicationRotationFollowingController
@end

#if ENABLE(DRAG_SUPPORT)

WTF_EXTERN_C_BEGIN

NSTimeInterval _UIDragInteractionDefaultLiftDelay(void);
CGFloat UIRoundToScreenScale(CGFloat value, UIScreen *);

WTF_EXTERN_C_END

typedef NS_OPTIONS(NSUInteger, UIDragOperation)
{
    UIDragOperationNone = 0,
    UIDragOperationEvery = NSUIntegerMax,
};

@interface UIDragInteraction ()
@property (nonatomic, assign, getter=_liftDelay, setter=_setLiftDelay:) NSTimeInterval liftDelay;
@end

@interface UIDragItem ()
@property (nonatomic, strong, setter=_setPrivateLocalContext:, getter=_privateLocalContext) id privateLocalContext;
@end

@protocol UITextInput;
@interface _UITextDragCaretView : UIView
- (instancetype)initWithTextInputView:(UIView<UITextInput> *)textInputView;
-(void)insertAtPosition:(UITextPosition *)position;
-(void)updateToPosition:(UITextPosition *)position;
-(void)remove;
@end

@interface UICalloutBar : UIView
+ (void)fadeSharedCalloutBar;
@end

@interface UIAutoRotatingWindow : UIApplicationRotationFollowingWindow
@end

@interface UITextEffectsWindow : UIAutoRotatingWindow
+ (UITextEffectsWindow *)sharedTextEffectsWindow;
@end

@interface UIURLDragPreviewView : UIView
+ (instancetype)viewWithTitle:(NSString *)title URL:(NSURL *)url;
@end

#endif

@interface _UIVisualEffectLayerConfig : NSObject
+ (instancetype)layerWithFillColor:(UIColor *)fillColor opacity:(CGFloat)opacity filterType:(NSString *)filterType;
- (void)configureLayerView:(UIView *)view;
@end

@interface _UIVisualEffectTintLayerConfig : _UIVisualEffectLayerConfig
+ (instancetype)layerWithTintColor:(UIColor *)tintColor;
+ (instancetype)layerWithTintColor:(UIColor *)tintColor filterType:(NSString *)filterType NS_AVAILABLE_IOS(9_0);
@end

@interface _UIVisualEffectConfig : NSObject
@property (nonatomic, readonly) _UIVisualEffectLayerConfig *contentConfig;
+ (_UIVisualEffectConfig *)configWithContentConfig:(_UIVisualEffectLayerConfig *)contentConfig;
@end

typedef NSInteger UICompositingMode;

@interface UIVisualEffect ()
+ (UIVisualEffect *)emptyEffect;
+ (UIVisualEffect *)effectCombiningEffects:(NSArray<UIVisualEffect *> *)effects;
+ (UIVisualEffect *)effectCompositingColor:(UIColor *)color withMode:(UICompositingMode)compositingMode alpha:(CGFloat)alpha;
@end

@interface UIColorEffect : UIVisualEffect
+ (UIColorEffect *)colorEffectSaturate:(CGFloat)saturationAmount;
@end

@interface UIBlurEffect ()
+ (UIBlurEffect *)effectWithBlurRadius:(CGFloat)blurRadius;
@end

@interface UIPopoverPresentationController ()
@property (assign, nonatomic, setter=_setCentersPopoverIfSourceViewNotSet:, getter=_centersPopoverIfSourceViewNotSet) BOOL _centersPopoverIfSourceViewNotSet;
@end

#endif // USE(APPLE_INTERNAL_SDK)

@interface UIPhysicalKeyboardEvent : UIPressesEvent
@end

@interface UIPhysicalKeyboardEvent ()
+ (UIPhysicalKeyboardEvent *)_eventWithInput:(NSString *)input inputFlags:(UIKeyboardInputFlags)flags;
- (void)_setHIDEvent:(IOHIDEventRef)event keyboard:(GSKeyboardRef)gsKeyboard;
- (UIPhysicalKeyboardEvent *)_cloneEvent NS_RETURNS_RETAINED;
@property (nonatomic, readonly) UIKeyboardInputFlags _inputFlags;
@property (nonatomic, readonly) CFIndex _keyCode;
@property (nonatomic, readonly) NSInteger _gsModifierFlags;
@end

@interface UIColor (IPI)
+ (UIColor *)insertionPointColor;
@end

@interface UIView (IPI)
- (UIScrollView *)_scroller;
- (CGPoint)accessibilityConvertPointFromSceneReferenceCoordinates:(CGPoint)point;
- (CGRect)accessibilityConvertRectToSceneReferenceCoordinates:(CGRect)rect;
- (UIRectEdge)_edgesApplyingSafeAreaInsetsToContentInset;
- (void)_updateSafeAreaInsets;
@end

@interface UIScrollView (IPI)
- (CGFloat)_rubberBandOffsetForOffset:(CGFloat)newOffset maxOffset:(CGFloat)maxOffset minOffset:(CGFloat)minOffset range:(CGFloat)range outside:(BOOL *)outside;
- (void)_adjustForAutomaticKeyboardInfo:(NSDictionary *)info animated:(BOOL)animated lastAdjustment:(CGFloat*)lastAdjustment;
- (BOOL)_isScrollingToTop;
- (CGPoint)_animatedTargetOffset;
- (BOOL)_canScrollWithoutBouncingX;
- (BOOL)_canScrollWithoutBouncingY;
- (void)_setContentOffsetWithDecelerationAnimation:(CGPoint)contentOffset;
- (CGPoint)_adjustedContentOffsetForContentOffset:(CGPoint)contentOffset;
- (void)_flashScrollIndicatorsPersistingPreviousFlashes:(BOOL)persisting;
@end

@interface UIPeripheralHost (IPI)
- (void)_beginIgnoringReloadInputViews;
- (int)_endIgnoringReloadInputViews;
- (void)forceReloadInputViews;
- (CGFloat)getVerticalOverlapForView:(UIView *)view usingKeyboardInfo:(NSDictionary *)info;
@end

@interface UIKeyboardImpl (IPI)
- (void)setInitialDirection;
- (void)prepareKeyboardInputModeFromPreferences:(UIKeyboardInputMode *)lastUsedMode;
- (BOOL)handleKeyTextCommandForCurrentEvent;
- (BOOL)handleKeyAppCommandForCurrentEvent;
@property (nonatomic, readonly) UIKeyboardInputMode *currentInputModeInPreference;
@end

@interface _UILayerHostView : UIView
@end

@interface _UIRemoteView : _UILayerHostView
- (instancetype)initWithFrame:(CGRect)frame pid:(pid_t)pid contextID:(uint32_t)contextID;
@end

#if __has_include(<UIKit/UITextInputMultiDocument.h>)
#import <UIKit/UITextInputMultiDocument.h>
#else
@protocol UITextInputMultiDocument <NSObject>
@optional
- (BOOL)_restoreFocusWithToken:(id <NSCopying, NSSecureCoding>)token;
- (void)_preserveFocusWithToken:(id <NSCopying, NSSecureCoding>)token destructively:(BOOL)destructively;
@end
#endif

@interface UIResponder ()
- (UIResponder *)firstResponder;
- (void)pasteAndMatchStyle:(id)sender;
- (void)makeTextWritingDirectionNatural:(id)sender;
@end

@interface _UINavigationInteractiveTransitionBase ()
- (void)_stopInteractiveTransition;
@end

#if __has_include(<UIKit/UITextAutofillSuggestion.h>)
#import <UIKit/UITextAutofillSuggestion.h>
#else
@interface UITextAutofillSuggestion : UITextSuggestion
@property (nonatomic, assign) NSString *username;
@property (nonatomic, assign) NSString *password;
@end
#endif

static inline bool currentUserInterfaceIdiomIsPad()
{
    // This inline function exists to thwart unreachable code
    // detection on platforms where UICurrentUserInterfaceIdiomIsPad
    // is defined directly to false.
#if USE(APPLE_INTERNAL_SDK)
    return UICurrentUserInterfaceIdiomIsPad();
#else
    return [[UIDevice currentDevice] userInterfaceIdiom] == UIUserInterfaceIdiomPad;
#endif
}

WTF_EXTERN_C_BEGIN

BOOL UIKeyboardEnabledInputModesAllowOneToManyShortcuts(void);
BOOL UIKeyboardEnabledInputModesAllowChineseTransliterationForText(NSString *);

extern const float UITableCellDefaultFontSize;
extern const float UITableViewCellDefaultFontSize;

extern NSString *const _UIApplicationDidFinishSuspensionSnapshotNotification;

extern NSString * const UIWindowDidMoveToScreenNotification;
extern NSString * const UIWindowDidRotateNotification;
extern NSString * const UIWindowNewScreenUserInfoKey;
extern NSString * const UIWindowWillRotateNotification;

extern NSString * const UIKeyboardPrivateDidRequestDismissalNotification;

extern NSString * const UIKeyboardIsLocalUserInfoKey;

extern UIApplication *UIApp;
BOOL _UIApplicationIsExtension(void);
void _UIApplicationLoadWebKit(void);

void UIImageDataWriteToSavedPhotosAlbum(NSData *imageData, id completionTarget, SEL completionSelector, void *contextInfo);

UIImage* _UIImageGetWebKitPhotoLibraryIcon(void);
UIImage* _UIImageGetWebKitTakePhotoOrVideoIcon(void);

extern const float UIWebViewGrowsAndShrinksToFitHeight;
extern const float UIWebViewScalesToFitScale;
extern const float UIWebViewStandardViewportWidth;

extern NSString *const UIKeyInputPageUp;
extern NSString *const UIKeyInputPageDown;

extern const NSString *UIPreviewDataLink;
extern const NSString *UIPreviewDataDDResult;
extern const NSString *UIPreviewDataDDContext;

extern const NSString *UIPreviewDataAttachmentList;
extern const NSString *UIPreviewDataAttachmentIndex;

#if __IPHONE_OS_VERSION_MAX_ALLOWED < 130000
extern NSString * const UIPreviewDataAttachmentListSourceIsManaged;
#else
extern NSString * const UIPreviewDataAttachmentListIsContentManaged;
#endif

UIEdgeInsets UIEdgeInsetsAdd(UIEdgeInsets lhs, UIEdgeInsets rhs, UIRectEdge);

WTF_EXTERN_C_END
