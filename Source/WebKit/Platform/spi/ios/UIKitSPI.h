/*
 * Copyright (C) 2014-2022 Apple Inc. All rights reserved.
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

#pragma once

DECLARE_SYSTEM_HEADER

#import <UIKit/UIKit.h>

#if USE(APPLE_INTERNAL_SDK)

#import <CoreSVG/CGSVGDocument.h>
#import <UIKit/NSTextAlternatives.h>
#import <UIKit/UIAlertController_Private.h>
#import <UIKit/UIApplication+iOSMac_Private.h>
#import <UIKit/UIApplication_Private.h>
#import <UIKit/UIBarButtonItem_Private.h>
#import <UIKit/UIBlurEffect_Private.h>
#import <UIKit/UIClickInteraction_Private.h>
#import <UIKit/UIClickPresentationInteraction_Private.h>
#import <UIKit/UIColorEffect.h>
#import <UIKit/UIContextMenuConfiguration.h>
#import <UIKit/UIDatePicker_Private.h>
#import <UIKit/UIDevice_Private.h>
#import <UIKit/UIDocumentPasswordView.h>
#import <UIKit/UIDocumentPickerViewController_Private.h>
#import <UIKit/UIFont_Private.h>
#import <UIKit/UIGeometry_Private.h>
#import <UIKit/UIGestureRecognizer_Private.h>
#import <UIKit/UIImageAsset_Private.h>
#import <UIKit/UIImagePickerController_Private.h>
#import <UIKit/UIImage_Private.h>
#import <UIKit/UIInterface_Private.h>
#import <UIKit/UIKeyboardImpl.h>
#import <UIKit/UIKeyboardInputModeController.h>
#import <UIKit/UIKeyboardIntl.h>
#import <UIKit/UIKeyboard_Private.h>
#import <UIKit/UILongPressGestureRecognizer_Private.h>
#import <UIKit/UIMenuController_Private.h>
#import <UIKit/UIPasteboard_Private.h>
#import <UIKit/UIPeripheralHost.h>
#import <UIKit/UIPeripheralHost_Private.h>
#import <UIKit/UIPickerContentView_Private.h>
#import <UIKit/UIPickerView_Private.h>
#import <UIKit/UIPopoverPresentationController_Private.h>
#import <UIKit/UIPresentationController_Private.h>
#import <UIKit/UIPress_Private.h>
#import <UIKit/UIPrintPageRenderer_Private.h>
#import <UIKit/UIResponder_Private.h>
#import <UIKit/UIScene_Private.h>
#import <UIKit/UIScrollEvent_Private.h>
#import <UIKit/UIScrollView_ForWebKitOnly.h>
#import <UIKit/UIScrollView_Private.h>
#import <UIKit/UIStringDrawing_Private.h>
#import <UIKit/UITableViewCell_Private.h>
#import <UIKit/UITableView_Private.h>
#import <UIKit/UITapGestureRecognizer_Private.h>
#import <UIKit/UITextChecker_Private.h>
#import <UIKit/UITextEffectsWindow.h>
#import <UIKit/UITextInput_Private.h>
#import <UIKit/UITextInteractionAssistant_Private.h>
#import <UIKit/UITextInteraction_Private.h>
#import <UIKit/UITouch_Private.h>
#import <UIKit/UIViewControllerTransitioning_Private.h>
#import <UIKit/UIViewController_Private.h>
#import <UIKit/UIViewController_ViewService.h>
#import <UIKit/UIView_Private.h>
#import <UIKit/UIVisualEffect_Private.h>
#import <UIKit/UIWKTextInteractionAssistant.h>
#import <UIKit/UIWebBrowserView.h>
#import <UIKit/UIWebClip.h>
#import <UIKit/UIWebDocumentView.h>
#import <UIKit/UIWebTiledView.h>
#import <UIKit/UIWindowScene_Private.h>
#import <UIKit/UIWindow_Private.h>
#import <UIKit/_UIApplicationBSActionHandler.h>
#import <UIKit/_UIApplicationRotationFollowing.h>
#import <UIKit/_UINavigationInteractiveTransition.h>
#import <UIKit/_UINavigationParallaxTransition.h>
#import <UIKit/_UISheetPresentationController.h>
#import <UIKitServices/UISApplicationState.h>

#if HAVE(LINK_PREVIEW)
#import <UIKit/UIPreviewAction_Private.h>
#import <UIKit/UIPreviewItemController.h>
#endif

#if USE(UICONTEXTMENU)
#import <UIKit/UIContextMenuInteraction_ForSpringBoardOnly.h>
#import <UIKit/UIContextMenuInteraction_ForWebKitOnly.h>
#import <UIKit/UIContextMenuInteraction_Private.h>
#import <UIKit/UIMenu_Private.h>
#endif

#if ENABLE(DRAG_SUPPORT)
#import <UIKit/NSItemProvider+UIKitAdditions_Private.h>
#import <UIKit/UIDragInteraction.h>
#import <UIKit/UIDragInteraction_Private.h>
#import <UIKit/UIDragItem_Private.h>
#import <UIKit/UIDragPreviewParameters.h>
#import <UIKit/UIDragPreview_Private.h>
#import <UIKit/UIDragSession.h>
#import <UIKit/UIDropInteraction.h>
#import <UIKit/_UITextDragCaretView.h>
#endif

#if HAVE(UIFINDINTERACTION)
#import <UIKit/UIFindInteraction.h>
#import <UIKit/UIFindSession_Private.h>
#import <UIKit/_UITextSearching.h>
#endif

#if __has_include(<UIKit/UITargetedPreview_Private.h>)
#import <UIKit/UITargetedPreview_Private.h>
#endif

#if HAVE(UI_POINTER_INTERACTION)
#import <UIKit/UIPointerInteraction_ForWebKitOnly.h>
#endif

#if HAVE(UIKIT_RESIZABLE_WINDOWS)
#import <UIKit/UIWindowScene_RequiresApproval.h>
#import <UIKit/_UIInvalidatable.h>
#endif

#if PLATFORM(VISION)
#import <UIKit/UIActivityViewController_Private.h>
#import <UIKit/UIView+SpatialComputing.h>
#endif

#if HAVE(UITOOLTIPINTERACTION)
#import <UIKitMacHelper/UINSWindow.h>
#endif

#if PLATFORM(IOS)
@interface UIWebClip(Staging_134304426)
+ (NSString *)pathForWebClipWithIdentifier:(NSString *)identifier;
@end

@interface UIWebClip(Staging_131961097)
@property (nonatomic, readonly) NSSet<NSString *> *trustedClientBundleIdentifiers;
@end
#endif

#else // USE(APPLE_INTERNAL_SDK)

#if HAVE(UITOOLTIPINTERACTION)
@protocol UINSWindow <NSObject>
@property (nonatomic, readonly, weak) NSObject *sceneView;
@end
#endif

@interface UIWebClip : NSObject
+ (UIWebClip *)webClipWithIdentifier:(NSString *)identifier;
+ (NSArray *)webClips;
+ (NSString *)pathForWebClipWithIdentifier:(NSString *)identifier;
@property (copy) NSString *identifier;
@property (nonatomic, copy) NSString *title;
@property (nonatomic, retain) NSURL *pageURL;
@property (nonatomic, readonly) NSSet<NSString *> *trustedClientBundleIdentifiers;
@end

#if ENABLE(DRAG_SUPPORT)
#import <UIKit/NSItemProvider+UIKitAdditions.h>
#endif

typedef NS_ENUM(NSInteger, _UIDataOwner) {
    _UIDataOwnerUndefined,
    _UIDataOwnerUser,
    _UIDataOwnerEnterprise,
    _UIDataOwnerShared,
};

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
@property (nonatomic, copy, setter=_setAttributedTitle:, getter=_attributedTitle) NSAttributedString *attributedTitle;
@end

WTF_EXTERN_C_BEGIN
typedef struct __IOHIDEvent* IOHIDEventRef;
typedef struct __GSKeyboard* GSKeyboardRef;
WTF_EXTERN_C_END

@class BSAction;
@class FBSSceneTransitionContext;
@protocol _UIApplicationBSActionHandler <NSObject>
- (NSSet<BSAction *> *)_respondToApplicationActions:(NSSet<BSAction *> *)applicationActions fromTransitionContext:(FBSSceneTransitionContext *)transitionContext;
@end

@interface UIApplication ()
- (UIInterfaceOrientation)interfaceOrientation;
- (void)_cancelAllTouches;
- (BOOL)isSuspendedUnderLock;
- (void)_enqueueHIDEvent:(IOHIDEventRef)event;
- (void)_registerBSActionHandler:(id<_UIApplicationBSActionHandler>)handler;
@end

@interface UIColor ()
+ (UIColor *)systemBackgroundColor;
@end

typedef enum {
    kUIKeyboardInputRepeat                 = 1 << 0,
    kUIKeyboardInputModifierFlagsChanged   = 1 << 5,
} UIKeyboardInputFlags;

#if PLATFORM(IOS) && !defined(__IPHONE_13_4)
typedef NS_OPTIONS(NSInteger, UIEventButtonMask) {
    UIEventButtonMaskSecondary = 1 << 1,
};
#endif

@interface UIEvent ()
- (void *)_hidEvent;
- (NSString *)_unmodifiedInput;
- (NSString *)_modifiedInput;
- (BOOL)_isKeyDown;
@end

#if HAVE(UIFINDINTERACTION)

typedef NS_ENUM(NSUInteger, _UIFoundTextStyle) {
    _UIFoundTextStyleNormal,
    _UIFoundTextStyleFound,
    _UIFoundTextStyleHighlighted,
};

typedef NS_ENUM(NSInteger, _UITextSearchMatchMethod) {
    _UITextSearchMatchMethodContains,
    _UITextSearchMatchMethodStartsWith,
    _UITextSearchMatchMethodFullWord,
};

typedef id<NSCoding, NSCopying> _UITextSearchDocumentIdentifier;

@interface _UITextSearchOptions : NSObject
@property (nonatomic, readonly) _UITextSearchMatchMethod wordMatchMethod;
@property (nonatomic, readonly) NSStringCompareOptions stringCompareOptions;
@end

@protocol _UITextSearchAggregator <NSObject>
- (void)foundRange:(UITextRange *)range forSearchString:(NSString *)string inDocument:(_UITextSearchDocumentIdentifier)document;
- (void)finishedSearching;
@end

@protocol _UITextSearching <NSObject>

@property (readonly) UITextRange *selectedTextRange;

- (NSInteger)offsetFromPosition:(UITextPosition *)from toPosition:(UITextPosition *)toPosition inDocument:(_UITextSearchDocumentIdentifier)document;

- (NSComparisonResult)compareFoundRange:(UITextRange *)fromRange toRange:(UITextRange *)toRange inDocument:(_UITextSearchDocumentIdentifier)document;

- (void)performTextSearchWithQueryString:(NSString *)string usingOptions:(_UITextSearchOptions *)options resultAggregator:(id<_UITextSearchAggregator>)aggregator;

- (void)decorateFoundTextRange:(UITextRange *)range inDocument:(_UITextSearchDocumentIdentifier)document usingStyle:(_UIFoundTextStyle)style;

- (void)clearAllDecoratedFoundText;

@optional

- (BOOL)supportsTextReplacement;

@end

@interface UIFindInteraction ()
@property (class, nonatomic, copy, getter=_globalFindBuffer, setter=_setGlobalFindBuffer:) NSString *_globalFindBuffer;
@end

@interface UITextSearchingFindSession ()
@property (nonatomic, readwrite, weak) id<UITextSearching> searchableObject;
@end

#endif // HAVE(UIFINDINTERACTION)

typedef enum {
    UIFontTraitPlain = 0,
    UIFontTraitItalic = 1 << 0,
    UIFontTraitBold = 1 << 1,
} UIFontTrait;

@interface UIFont ()
+ (UIFont *)fontWithFamilyName:(NSString *)familyName traits:(UIFontTrait)traits size:(CGFloat)fontSize;
+ (UIFont *)systemFontOfSize:(CGFloat)size weight:(CGFloat)weight design:(NSString *)design;
- (BOOL)isSystemFont;
- (UIFontTrait)traits;
@end

@interface UIImagePickerController ()
@property (nonatomic, setter=_setAllowsMultipleSelection:) BOOL _allowsMultipleSelection;
@property (nonatomic, setter=_setRequiresPickingConfirmation:) BOOL _requiresPickingConfirmation;
@property (nonatomic, setter=_setShowsFileSizePicker:) BOOL _showsFileSizePicker;
@end

@interface UIImageAsset ()
+ (instancetype)_dynamicAssetNamed:(NSString *)name generator:(UIImage *(^)(UIImageAsset *, UIImageConfiguration *, UIImage *))block;
@end

typedef struct CGSVGDocument *CGSVGDocumentRef;

@interface UIImage ()
- (id)initWithCGImage:(CGImageRef)CGImage imageOrientation:(UIImageOrientation)imageOrientation;
- (UIImage *)_flatImageWithColor:(UIColor *)color;
+ (UIImage *)_systemImageNamed:(NSString *)name;
+ (UIImage *)_imageWithCGSVGDocument:(CGSVGDocumentRef)cgSVGDocument;
+ (UIImage *)_imageWithCGSVGDocument:(CGSVGDocumentRef)cgSVGDocument scale:(CGFloat)scale orientation:(UIImageOrientation)orientation;
@end

@protocol UIKeyboardImplGeometryDelegate
@property (nonatomic, readwrite, getter=isMinimized) BOOL minimized;
@end

@protocol UIKeyboardCandidateListDelegate <NSObject>
@end

@interface UIKeyboard : UIView <UIKeyboardImplGeometryDelegate>
@end

@interface UIKeyboard ()
+ (instancetype)activeKeyboard;
+ (BOOL)isInHardwareKeyboardMode;
+ (BOOL)isOnScreen;
+ (BOOL)usesInputSystemUI;
@end

@interface UIKeyboardImpl : UIView <UIKeyboardCandidateListDelegate>
+ (BOOL)smartInsertDeleteIsEnabled;
- (void)updateForChangedSelection;
@end

@interface UIKeyboardImpl ()
+ (UIKeyboardImpl *)activeInstance;
+ (UIKeyboardImpl *)sharedInstance;
- (BOOL)handleKeyTextCommandForCurrentEvent;
- (BOOL)handleKeyAppCommandForCurrentEvent;
- (BOOL)handleKeyInputMethodCommandForCurrentEvent;
- (void)addInputString:(NSString *)string withFlags:(NSUInteger)flags withInputManagerHint:(NSString *)hint;
- (BOOL)autocorrectSpellingEnabled;
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
+ (CGFloat)_checkmarkOffset;
- (CGFloat)labelWidthForBounds:(CGRect)bounds;
@property (nonatomic, getter=isChecked) BOOL checked;
@property (nonatomic, readonly) UILabel *titleLabel;
@end

@interface UIPickerView ()
+ (CGSize)defaultSizeForCurrentOrientation;
- (void)_setUsesCheckedSelection:(BOOL)usesCheckedSelection;
@property (nonatomic, setter=_setMagnifierEnabled:) BOOL _magnifierEnabled;
@end

@interface UIPrintPageRenderer ()
@property (readonly) UIPrintRenderingQuality requestedRenderingQuality;
@end

@interface UIResponder ()
- (void)_wheelChangedWithEvent:(UIEvent *)event;
- (void)_beginPinningInputViews;
- (void)_endPinningInputViews;
@property (nonatomic, setter=_setDataOwnerForCopy:) _UIDataOwner _dataOwnerForCopy;
@property (nonatomic, setter=_setDataOwnerForPaste:) _UIDataOwner _dataOwnerForPaste;
@end

@class FBSDisplayConfiguration;
@interface UIScreen ()
@property (nonatomic, readonly, retain) FBSDisplayConfiguration *displayConfiguration;
@property (nonatomic, readonly) CGRect _referenceBounds;
@end

@interface UIScrollView ()
- (void)_stopScrollingAndZoomingAnimations;
- (void)_flashScrollIndicatorsForAxes:(UIAxis)axes persistingPreviousFlashes:(BOOL)persisting;
@property (nonatomic, getter=isZoomEnabled) BOOL zoomEnabled;
@property (nonatomic, readonly, getter=_isAnimatingZoom) BOOL isAnimatingZoom;
@property (nonatomic, readonly, getter=_isAnimatingScroll) BOOL isAnimatingScroll;
@property (nonatomic, getter=_contentScrollInset, setter=_setContentScrollInset:) UIEdgeInsets contentScrollInset;
@property (nonatomic, readonly) UIEdgeInsets _systemContentInset;
@property (nonatomic, getter=_allowsAsyncScrollEvent, setter=_setAllowsAsyncScrollEvent:) BOOL _allowsAsyncScrollEvent;
@property (nonatomic, getter=_isFirstResponderKeyboardAvoidanceEnabled, setter=_setFirstResponderKeyboardAvoidanceEnabled:) BOOL firstResponderKeyboardAvoidanceEnabled;
#if !HAVE(BROWSER_ENGINE_SUPPORTING_API)
@property (nonatomic) BOOL bouncesHorizontally;
@property (nonatomic) BOOL bouncesVertically;
#endif
@property (nonatomic, setter=_setAllowsParentToBeginHorizontally:) BOOL _allowsParentToBeginHorizontally;
@property (nonatomic, setter=_setAllowsParentToBeginVertically:) BOOL _allowsParentToBeginVertically;
@property (nonatomic) BOOL tracksImmediatelyWhileDecelerating;
@property (nonatomic, getter=_avoidsJumpOnInterruptedBounce, setter=_setAvoidsJumpOnInterruptedBounce:) BOOL _avoidsJumpOnInterruptedBounce;
#if HAVE(UIKIT_SCROLLBAR_COLOR_SPI)
@property (nonatomic, nullable, setter=_setVerticalScrollIndicatorColor:) UIColor *_verticalScrollIndicatorColor;
@property (nonatomic, nullable, setter=_setHorizontalScrollIndicatorColor:) UIColor *_horizontalScrollIndicatorColor;
#endif
@end

typedef NS_ENUM(NSUInteger, UIScrollPhase) {
    UIScrollPhaseNone,
    UIScrollPhaseMayBegin,
    UIScrollPhaseBegan,
    UIScrollPhaseChanged,
    UIScrollPhaseEnded,
    UIScrollPhaseCancelled
};

@interface UIScrollEvent : UIEvent

@property (assign, readonly) UIScrollPhase phase;
- (CGPoint)locationInView:(UIView *)view;
- (CGVector)_adjustedAcceleratedDeltaInView:(UIView *)view;

@end

@interface NSString (UIKitDetails)
- (CGSize)_legacy_sizeWithFont:(UIFont *)font forWidth:(CGFloat)width lineBreakMode:(NSLineBreakMode)lineBreakMode;
- (CGSize)_legacy_sizeWithFont:(UIFont *)font minFontSize:(CGFloat)minFontSize actualFontSize:(CGFloat *)actualFontSize forWidth:(CGFloat)width lineBreakMode:(NSLineBreakMode)lineBreakMode;
@end

@interface UITableView ()
@property (nonatomic, getter=_sectionContentInsetFollowsLayoutMargins, setter=_setSectionContentInsetFollowsLayoutMargins:) BOOL sectionContentInsetFollowsLayoutMargins;
@end

@interface UITapGestureRecognizer ()
@property (nonatomic, getter=_allowableSeparation, setter=_setAllowableSeparation:) CGFloat allowableSeparation;
@property (nonatomic, readonly) CGPoint location;
@property (nonatomic) CGFloat allowableMovement;
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
@property (nonatomic) BOOL isSingleLineDocument;
@property (nonatomic) BOOL learnsCorrections;
@end

@protocol UITextInputDelegatePrivate
- (void)layoutHasChanged;
@end

@protocol UITextInputPrivate <UITextInput, UITextInputTokenizer, UITextInputTraits_Private>
@optional
- (BOOL)requiresKeyEvents;
- (UIColor *)textColorForCaretSelection;
- (UIFont *)fontForCaretSelection;
- (UIView *)automaticallySelectedOverlay;
- (void)handleKeyWebEvent:(WebEvent *)event;
- (void)insertDictationResult:(NSArray *)dictationResult withCorrectionIdentifier:(id)correctionIdentifier;
- (void)replaceRangeWithTextWithoutClosingTyping:(UITextRange *)range replacementText:(NSString *)text;
- (void)setBottomBufferHeight:(CGFloat)bottomBuffer;
- (void)modifierFlagsDidChangeFrom:(UIKeyModifierFlags)oldFlags to:(UIKeyModifierFlags)newFlags;
@property (nonatomic) UITextGranularity selectionGranularity;
@required
- (BOOL)hasContent;
- (BOOL)hasSelection;
- (void)selectAll;
@end

@protocol _UITextInputTranslationSupport <UITextInput>
@property (nonatomic, readonly, getter=isImageBacked) BOOL imageBacked;
@end

@interface UITextInputTraits : NSObject <UITextInputTraits, UITextInputTraits_Private, NSCopying>
- (void)_setColorsToMatchTintColor:(UIColor *)tintColor;
@end

@interface UITextInteractionAssistant : NSObject
- (instancetype)initWithView:(UIResponder <UITextInput> *)view;
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
@property (nonatomic, copy, readonly) NSString *displayText;
@end

@protocol UITextInputSuggestionDelegate <UITextInputDelegate>
- (void)setSuggestions:(NSArray <UITextSuggestion*> *)suggestions;
@end

@interface UIViewController (ViewService)
- (pid_t)_hostProcessIdentifier;
@property (readonly) NSString *_hostApplicationBundleIdentifier;
@end

@interface UIViewController (Private)
- (/* nullable */ UIPresentationController *)_existingPresentationControllerImmediate:(BOOL)immediate effective:(BOOL)effective;
@end

extern NSString * const UIPresentationControllerDismissalTransitionDidEndNotification;
extern NSString * const UIPresentationControllerDismissalTransitionDidEndCompletedKey;

@interface _UIViewControllerTransitionContext : NSObject <UIViewControllerContextTransitioning>
- (id <UIViewControllerTransitionCoordinator>)_transitionCoordinator;
@end

// FIXME: Separate the parts we are simply re-declaring from the ones we are overriding.
@interface _UIViewControllerTransitionContext (Details)
- (void)_setTransitionIsInFlight:(BOOL)flag;
@property (nonatomic, assign, setter=_setAnimator:) id <UIViewControllerAnimatedTransitioning> _animator;
@property (nonatomic, assign, setter=_setContainerView:) UIView *containerView;
@property (nonatomic, assign, setter=_setInteractor:) id <UIViewControllerInteractiveTransitioning> _interactor;
@property (nonatomic, copy, setter=_setCompletionHandler:)  void (^_completionHandler)(_UIViewControllerTransitionContext *context, BOOL transitionCompleted);
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
- (UINavigationControllerOperation)operation;
@optional
- (UIWindow *)window;
@end

#if PLATFORM(VISION)
@interface UIActivityViewController ()
@property (nonatomic) BOOL allowsCustomPresentationStyle;
@end
@interface UIView ()
- (void)_requestRemoteEffects:(NSArray *)effects forKey:(NSString *)key;
@end
#endif // PLATFORM(VISION)

@interface UIView ()
+ (BOOL)_isInAnimationBlock;
- (CGSize)size;
- (void)setFrameOrigin:(CGPoint)origin;
- (void)setSize:(CGSize)size;
- (void)_populateArchivedSubviews:(NSMutableSet *)encodedViews;
- (void)safeAreaInsetsDidChange;
@property (nonatomic, setter=_setContinuousCornerRadius:) CGFloat _continuousCornerRadius;
- (void)insertSubview:(UIView *)view above:(UIView *)sibling;
- (void)_didRemoveSubview:(UIView *)subview;
- (CGSize)convertSize:(CGSize)size toView:(UIView *)view;
- (NSString *)recursiveDescription;
@end

#if PLATFORM(VISION)

typedef NS_ENUM(NSInteger, _UIPlatterGroundingShadowVisibility) {
    _UIPlatterGroundingShadowVisibilityAutomatic = 0,
    _UIPlatterGroundingShadowVisibilityVisible = 1,
    _UIPlatterGroundingShadowVisibilityHidden = 2
};

@interface UIView (SpatialComputing)
@property (nonatomic, setter=_setPreferredGroundingShadowVisibility:) _UIPlatterGroundingShadowVisibility _preferredGroundingShadowVisibility;
@end

#endif

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
    UIWKSelectionFlipped = 2,
    UIWKPhraseBoundaryChanged = 4,
};

typedef NS_ENUM(NSInteger, UIWKGestureType) {
    UIWKGestureLoupe = 0,
    UIWKGestureOneFingerTap = 1,
    UIWKGestureTapAndAHalf = 2,
    UIWKGestureDoubleTap = 3,
    UIWKGestureOneFingerDoubleTap = 8,
    UIWKGestureOneFingerTripleTap = 9,
    UIWKGestureTwoFingerSingleTap = 10,
    UIWKGesturePhraseBoundary = 14,
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

@interface UITextSelectionView : UIView
@end

#if HAVE(UI_TEXT_SELECTION_RECT_CUSTOM_HANDLE_INFO)
@interface UITextSelectionRectCustomHandleInfo : NSObject
@property (nonatomic, readonly) CGPoint bottomLeft;
@property (nonatomic, readonly) CGPoint topLeft;
@property (nonatomic, readonly) CGPoint bottomRight;
@property (nonatomic, readonly) CGPoint topRight;
@end
#endif

@class UIContextMenuInteraction;
@protocol UIContextMenuInteractionDelegate;
@interface UITextInteractionAssistant (SPI)
@property (nonatomic, strong, readonly) UIContextMenuInteraction *contextMenuInteraction;
@property (nonatomic, weak, readwrite) id<UIContextMenuInteractionDelegate> externalContextMenuInteractionDelegate;
@end

@interface UIWKTextInteractionAssistant : UITextInteractionAssistant <UIResponderStandardEditActions>
@end

@interface UIWKTextInteractionAssistant ()
- (void)selectionChangedWithGestureAt:(CGPoint)point withGesture:(UIWKGestureType)gestureType withState:(UIGestureRecognizerState)gestureState withFlags:(UIWKSelectionFlags)flags;
- (void)selectionChangedWithTouchAt:(CGPoint)point withSelectionTouch:(UIWKSelectionTouch)touch withFlags:(UIWKSelectionFlags)flags;
- (void)lookup:(NSString *)textWithContext withRange:(NSRange)range fromRect:(CGRect)presentationRect;
- (void)showShareSheetFor:(NSString *)selectedTerm fromRect:(CGRect)presentationRect;
- (void)showTextServiceFor:(NSString *)selectedTerm fromRect:(CGRect)presentationRect;
- (void)scheduleReplacementsForText:(NSString *)text;
- (void)scheduleChineseTransliterationForText:(NSString *)text;
- (void)translate:(NSString *)text fromRect:(CGRect)presentationRect;
@end

@class UIWKDocumentRequest;
@class UIWKDocumentContext;

@protocol UIWKInteractionViewProtocol

- (void)changeSelectionWithGestureAt:(CGPoint)point withGesture:(UIWKGestureType)gestureType withState:(UIGestureRecognizerState)state;
- (void)changeSelectionWithTouchAt:(CGPoint)point withSelectionTouch:(UIWKSelectionTouch)touch baseIsStart:(BOOL)baseIsStart withFlags:(UIWKSelectionFlags)flags;
- (void)changeSelectionWithTouchesFrom:(CGPoint)from to:(CGPoint)to withGesture:(UIWKGestureType)gestureType withState:(UIGestureRecognizerState)gestureState;
- (CGRect)textFirstRect;
- (CGRect)textLastRect;

- (void)requestAutocorrectionContextWithCompletionHandler:(void (^)(UIWKAutocorrectionContext *autocorrectionContext))completionHandler;

- (void)requestAutocorrectionRectsForString:(NSString *)input withCompletionHandler:(void (^)(UIWKAutocorrectionRects *rectsForInput))completionHandler;

- (NSString *)markedText;
- (BOOL)hasMarkedText;

- (BOOL)hasSelectablePositionAtPoint:(CGPoint)point;
- (NSArray *)webSelectionRects;
- (void)_cancelLongPressGestureRecognizer;

@optional
- (void)insertTextPlaceholderWithSize:(CGSize)size completionHandler:(void (^)(UITextPlaceholder *))completionHandler;
- (void)removeTextPlaceholder:(UITextPlaceholder *)placeholder willInsertText:(BOOL)willInsertText completionHandler:(void (^)(void))completionHandler;

- (void)replaceDictatedText:(NSString *)oldText withText:(NSString *)newText;
- (BOOL)pointIsNearMarkedText:(CGPoint)point;
- (NSString *)selectedText;
- (NSArray<NSTextAlternatives *> *)alternativesForSelectedText;
- (void)replaceText:(NSString *)text withText:(NSString *)word;
- (void)selectWordForReplacement;
- (BOOL)isReplaceAllowed;
- (UIView *)unscaledView;
- (CGFloat)inverseScale;
- (CGRect)unobscuredContentRect;
@end

@protocol UITextAutoscrolling
- (void)startAutoscroll:(CGPoint)point;
- (void)cancelAutoscroll;
- (void)scrollSelectionToVisible:(BOOL)animated;
@end

@interface _UILookupGestureRecognizer : UIGestureRecognizer
@end

@interface _UINavigationParallaxTransition : NSObject <UIViewControllerAnimatedTransitioningEx>
@end

@interface _UINavigationParallaxTransition ()
- (instancetype)initWithCurrentOperation:(UINavigationControllerOperation)operation;
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
#if HAVE(CONTENT_SWIPE_GESTURE_RECOGNIZER)
@property (nonatomic, readonly) UIPanGestureRecognizer *contentSwipeGestureRecognizer;
#endif
@end

@protocol _UINavigationInteractiveTransitionBaseDelegate <NSObject>
- (void)startInteractiveTransition:(_UINavigationInteractiveTransitionBase *)interactiveTransition;
- (BOOL)shouldBeginInteractiveTransition:(_UINavigationInteractiveTransitionBase *)interactiveTransition;
- (BOOL)interactiveTransition:(_UINavigationInteractiveTransitionBase *)interactiveTransition gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldRecognizeSimultaneouslyWithGestureRecognizer:(UIGestureRecognizer *)otherGestureRecognizer;
- (BOOL)interactiveTransition:(_UINavigationInteractiveTransitionBase *)interactiveTransition gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldReceiveTouch:(UITouch *)touch;
- (UIPanGestureRecognizer *)gestureRecognizerForInteractiveTransition:(_UINavigationInteractiveTransitionBase *)interactiveTransition WithTarget:(id)target action:(SEL)action;
@end

@interface UIWindow ()
+ (mach_port_t)_synchronizeDrawingAcrossProcesses;
- (void)_setWindowResolution:(CGFloat)resolution displayIfChanged:(BOOL)displayIfChanged;
- (uint32_t)_contextId;
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

@interface UIDocumentPickerViewController ()
@property (nonatomic, assign, setter=_setIsContentManaged:, getter=_isContentManaged) BOOL isContentManaged;
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

WTF_EXTERN_C_END

@interface UIDragInteraction ()
@property (nonatomic, assign, getter=_liftDelay, setter=_setLiftDelay:) NSTimeInterval liftDelay;
- (void)_setAllowsPointerDragBeforeLiftDelay:(BOOL)allowsPointerDragBeforeLiftDelay;
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

@interface _UIParallaxTransitionPanGestureRecognizer : UIScreenEdgePanGestureRecognizer
@end

#endif

@interface UIAutoRotatingWindow : UIApplicationRotationFollowingWindow
@end

@interface UITextEffectsWindow : UIAutoRotatingWindow
+ (UITextEffectsWindow *)sharedTextEffectsWindow;
+ (UITextEffectsWindow *)sharedTextEffectsWindowForWindowScene:(UIWindowScene *)windowScene;
@end

@interface _UIVisualEffectLayerConfig : NSObject
@end

@interface UIWKDocumentContext : NSObject

@property (nonatomic, copy) NSObject *contextBefore;
@property (nonatomic, copy) NSObject *selectedText;
@property (nonatomic, copy) NSObject *contextAfter;
@property (nonatomic, copy) NSObject *markedText;
@property (nonatomic, assign) NSRange selectedRangeInMarkedText;
@property (nonatomic, copy) NSAttributedString *annotatedText;

- (void)addTextRect:(CGRect)rect forCharacterRange:(NSRange)range;

@end

typedef NS_OPTIONS(NSInteger, UIWKDocumentRequestFlags) {
    UIWKDocumentRequestNone = 0,
    UIWKDocumentRequestText = 1 << 0,
    UIWKDocumentRequestAttributed = 1 << 1,
    UIWKDocumentRequestRects = 1 << 2,
    UIWKDocumentRequestSpatial = 1 << 3,
    UIWKDocumentRequestAnnotation = 1 << 4,
    UIWKDocumentRequestMarkedTextRects =  1 << 5,
    UIWKDocumentRequestSpatialAndCurrentSelection =  1 << 6,
};

@interface UIWKDocumentRequest : NSObject
@property (nonatomic, assign) UIWKDocumentRequestFlags flags;
@property (nonatomic, assign) UITextGranularity surroundingGranularity;
@property (nonatomic, assign) NSInteger granularityCount;
@property (nonatomic, assign) CGRect documentRect;
@property (nonatomic, retain) id <NSCopying> inputElementIdentifier;
@end

@interface UIPreviewAction ()
@property (nonatomic, strong) UIImage *image;
@end

#if USE(UICONTEXTMENU)
@interface _UIClickInteraction : NSObject <UIInteraction>
@end

@interface _UIClickPresentationInteraction : NSObject <UIInteraction>
@end
#endif // USE(UICONTEXTMENU)

@interface NSTextAlternatives : NSObject
- (id)initWithPrimaryString:(NSString *)primaryString alternativeStrings:(NSArray<NSString *> *)alternativeStrings;
- (id)initWithPrimaryString:(NSString *)primaryString alternativeStrings:(NSArray<NSString *> *)alternativeStrings isLowConfidence:(BOOL)lowConfidence;

@property (readonly) NSString *primaryString;
@property (readonly) NSArray<NSString *> *alternativeStrings;
@property (readonly) BOOL isLowConfidence;
@end

#if PLATFORM(WATCHOS)
@interface UIStatusBar : UIView
@end
#endif

@interface UIScene ()
@property (nonatomic, readonly) NSString *_sceneIdentifier;
@end

#if HAVE(UIKIT_WITH_MOUSE_SUPPORT)
@interface UITouch ()
@property (nonatomic, readonly) BOOL _isPointerTouch;
@end
#endif

#if HAVE(UIKIT_RESIZABLE_WINDOWS)

@protocol _UIInvalidatable <NSObject>
- (void)_invalidate;
@end

@interface UIWindowScene ()
- (id<_UIInvalidatable>)_holdLiveResizeSnapshotForReason:(NSString *)reason;
@property (nonatomic, readonly) BOOL _enhancedWindowingEnabled;
@end

#endif // HAVE(UIKIT_RESIZABLE_WINDOWS)

#if HAVE(UI_WINDOW_SCENE_LIVE_RESIZE)

extern NSNotificationName const _UIWindowSceneDidBeginLiveResizeNotification;
extern NSNotificationName const _UIWindowSceneDidEndLiveResizeNotification;

#endif // HAVE(UI_WINDOW_SCENE_LIVE_RESIZE)

#if HAVE(CATALYST_USER_INTERFACE_IDIOM_AND_SCALE_FACTOR)
extern void _UIApplicationCatalystRequestViewServiceIdiomAndScaleFactor(UIUserInterfaceIdiom, CGFloat scaleFactor);
#endif

@interface UISApplicationState : NSObject
- (instancetype)initWithBundleIdentifier:(NSString *)bundleIdentifier;
@property (nonatomic, copy) id badgeValue;
@end

#endif // USE(APPLE_INTERNAL_SDK)

@interface UITextChecker (Staging_165842824)
- (void)requestProofreadingReviewOfString:(NSString *)stringToCheck range:(NSRange)range language:(NSString *)language options:(NSDictionary<NSString *, id> *)options completionHandler:(void (^)(NSArray<NSTextCheckingResult *> *results))completionHandler;
@end

#if HAVE(UITOOLTIPINTERACTION)
@interface NSObject (NSViewDynamicToolTipManager)
@property (readonly) NSObject *_dynamicToolTipManager;
- (void)windowChangedKeyState;
@end

@protocol UINSApplicationDelegate <NSObject>
- (id<UINSWindow>)hostWindowForUIWindow:(id)window;
@end

WTF_EXTERN_C_BEGIN
extern id<UINSApplicationDelegate> UINSSharedApplicationDelegate(void);
WTF_EXTERN_C_END
#endif

#if ENABLE(OVERLAY_REGIONS_IN_EVENT_REGION)
typedef NS_ENUM(NSUInteger, _UIScrollDeviceCategory) {
    _UIScrollDeviceCategoryOverlayScroll = 6
};

@interface UIScrollEvent ()
- (_UIScrollDeviceCategory)_scrollDeviceCategory;
@end
#endif

@interface UITextInteractionAssistant (IPI)
- (void)willStartScrollingOrZooming;
- (void)didEndScrollingOrZooming;
- (void)selectWord;
@end

#if USE(UICONTEXTMENU)

@interface UIContextMenuConfiguration (IPI)
@property (nonatomic, copy) UIContextMenuContentPreviewProvider previewProvider;
@property (nonatomic, copy) UIContextMenuActionProvider actionProvider;
@end

@protocol _UIClickInteractionDriverDelegate;
@protocol _UIClickInteractionDriving <NSObject>
@property (nonatomic, weak) id <_UIClickInteractionDriverDelegate> delegate;
@end

@interface _UIClickPresentationInteraction (IPI)
@property (nonatomic, strong /*, nullable */) NSArray<id<_UIClickInteractionDriving>> *overrideDrivers;
@end

@interface UIContextMenuInteraction (IPI)
@property (nonatomic, strong) _UIClickPresentationInteraction *presentationInteraction;
@end

#endif // USE(UICONTEXTMENU)

@interface UIPhysicalKeyboardEvent : UIPressesEvent
@end

@interface UIPhysicalKeyboardEvent ()
- (UIPhysicalKeyboardEvent *)_cloneEvent NS_RETURNS_RETAINED;
@property (nonatomic, readonly) UIKeyboardInputFlags _inputFlags;
@property (nonatomic, readonly) CFIndex _keyCode;
@property (nonatomic, readonly) NSInteger _gsModifierFlags;
@end

@interface UIColor (IPI)
+ (UIColor *)insertionPointColor;
@end

@interface UIImage ()
- (UIImage *)_rasterizedImage;
@end

@interface UIView (IPI)
- (UIScrollView *)_scroller;
- (CGPoint)accessibilityConvertPointFromSceneReferenceCoordinates:(CGPoint)point;
- (CGRect)accessibilityConvertRectToSceneReferenceCoordinates:(CGRect)rect;
- (void)accessibilityRelayNotification:(NSString *)notificationName notificationData:(NSData *)notificationData;
- (UIRectEdge)_edgesApplyingSafeAreaInsetsToContentInset;
@end

@interface UIScrollView (IPI)
- (void)_adjustForAutomaticKeyboardInfo:(NSDictionary *)info animated:(BOOL)animated lastAdjustment:(CGFloat*)lastAdjustment;
@end

@interface UIPeripheralHost (IPI)
- (CGFloat)getVerticalOverlapForView:(UIView *)view usingKeyboardInfo:(NSDictionary *)info;
@end

@interface UIKeyboardImpl (IPI)
- (void)setInitialDirection;
@end

@class CALayerHost;

@interface _UILayerHostView : UIView
- (instancetype)initWithFrame:(CGRect)frame pid:(pid_t)pid contextID:(uint32_t)contextID;
@property (nonatomic, readonly, retain) CALayerHost *layerHost;
@end

@interface _UIRemoteView : _UILayerHostView
- (instancetype)initWithFrame:(CGRect)frame pid:(pid_t)pid contextID:(uint32_t)contextID;
@end

@interface _UIVisibilityPropagationView : UIView
@end

#if HAVE(NON_HOSTING_VISIBILITY_PROPAGATION_VIEW)
@interface _UINonHostingVisibilityPropagationView : _UIVisibilityPropagationView
- (instancetype)initWithFrame:(CGRect)frame pid:(pid_t)pid environmentIdentifier:(NSString *)environmentIdentifier;
@end
#endif

#if __has_include(<UIKit/UITextInputMultiDocument.h>)
#import <UIKit/UITextInputMultiDocument.h>
#else
@protocol UITextInputMultiDocument <NSObject>
@optional
- (BOOL)_restoreFocusWithToken:(id <NSCopying, NSSecureCoding>)token;
- (void)_preserveFocusWithToken:(id <NSCopying, NSSecureCoding>)token destructively:(BOOL)destructively;
@end
#endif

@interface UIPasteboard ()
+ (void)_performAsDataOwner:(_UIDataOwner)dataOwner block:(void(^ NS_NOESCAPE)(void))block;
@end

@interface UIResponder ()
- (UIResponder *)firstResponder;
- (void)pasteAndMatchStyle:(id)sender;
- (void)makeTextWritingDirectionNatural:(id)sender;
@property (nonatomic, setter=_setSuppressSoftwareKeyboard:) BOOL _suppressSoftwareKeyboard;
@property (nonatomic, readonly) UITextInteractionAssistant *interactionAssistant;
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

#if ENABLE(POST_EDITING_GRAMMAR_CHECKING)
@interface UITextChecker ()
+ (BOOL)grammarCheckingEnabled;
- (NSArray<NSTextCheckingResult *> *)checkString:(NSString *)stringToCheck range:(NSRange)range types:(NSTextCheckingTypes)checkingTypes languages:(NSArray<NSString *> *)languagesArray options:(NSDictionary<NSString *, id> *)options;
@end
#endif

@protocol UITextInputInternal <UITextInputPrivate>
@optional
@property (nonatomic, readonly) CGRect _selectionClipRect;
@end

@interface UIDevice ()
@property (nonatomic, setter=_setBacklightLevel:) float _backlightLevel;
@end

@interface UIColorPickerViewController ()
@property (nonatomic, copy, setter=_setSuggestedColors:) NSArray<UIColor *> *_suggestedColors;
@end

#if HAVE(AUTOCORRECTION_ENHANCEMENTS)
@interface UIWKDocumentContext (Staging_112795757)
@property (nonatomic, copy) NSArray<NSValue *> *autocorrectedRanges;
@end
#endif

@interface UIResponder (Staging_118307086)

- (void)addShortcut:(id)sender;
- (void)lookup:(id)sender;
- (void)define:(id)sender;
- (void)promptForReplace:(id)sender;
- (void)share:(id)sender;
- (void)translate:(id)sender;
- (void)transliterateChinese:(id)sender;

#if HAVE(UIFINDINTERACTION)
- (void)findSelected:(id)sender;
#endif

@end

@interface UIResponder (Internal)
- (BOOL)_requiresKeyboardWhenFirstResponder;
- (BOOL)_requiresKeyboardResetOnReload;
- (UTF32Char)_characterInRelationToCaretSelection:(int)amount;
@end

@class UITextInputArrowKeyHistory;

@interface UIApplication (InternalBSAction)
- (void)_registerInternalBSActionHandler:(id<_UIApplicationBSActionHandler>)handler;
@end

@interface UIWindowSceneGeometry (Staging_143004359)
@property (nonatomic, readonly, getter=isInteractivelyResizing) BOOL interactivelyResizing;
@end

#if HAVE(LIQUID_GLASS)

@interface UIScrollView ()
- (void)_setPrefersSolidColorHardPocket:(BOOL)prefersSolidColorHardPocket forEdge:(UIRectEdge)edge;
- (void)_setPocketColor:(UIColor *)color forEdge:(UIRectEdge)edge;
@end

#endif // HAVE(LIQUID_GLASS)

WTF_EXTERN_C_BEGIN

BOOL UIKeyboardEnabledInputModesAllowOneToManyShortcuts(void);
BOOL UIKeyboardEnabledInputModesAllowChineseTransliterationForText(NSString *);
BOOL UIKeyboardIsRightToLeftInputModeActive(void);

extern NSString * const UIWindowDidMoveToScreenNotification;
extern NSString * const UIWindowDidRotateNotification;
extern NSString * const UIWindowNewScreenUserInfoKey;
extern NSString * const UIWindowWillRotateNotification;

extern NSString * const UIKeyboardPrivateDidRequestDismissalNotification;

extern NSString * const UIKeyboardIsLocalUserInfoKey;

BOOL _UIApplicationIsExtension(void);

void UIImageDataWriteToSavedPhotosAlbum(NSData *imageData, id completionTarget, SEL completionSelector, void *contextInfo);

extern const NSString *UIPreviewDataLink;
extern const NSString *UIPreviewDataDDResult;
extern const NSString *UIPreviewDataDDContext;

extern const NSString *UIPreviewDataAttachmentList;
extern const NSString *UIPreviewDataAttachmentIndex;

extern NSString * const UIPreviewDataAttachmentListIsContentManaged;

UIEdgeInsets UIEdgeInsetsAdd(UIEdgeInsets lhs, UIEdgeInsets rhs, UIRectEdge);

extern NSString *const UIBacklightLevelChangedNotification;

extern NSString * const NSTextEncodingNameDocumentOption;
extern NSString * const NSBaseURLDocumentOption;
extern NSString * const NSTimeoutDocumentOption;
extern NSString * const NSWebPreferencesDocumentOption;
extern NSString * const NSWebResourceLoadDelegateDocumentOption;
extern NSString * const NSTextSizeMultiplierDocumentOption;

extern NSNotificationName const _UISceneWillBeginSystemSnapshotSequence;
extern NSNotificationName const _UISceneDidCompleteSystemSnapshotSequence;
extern NSNotificationName const _UIWindowSceneEnhancedWindowingModeChanged;

extern CGRect UIRectInsetEdges(CGRect, UIRectEdge edges, CGFloat v);
extern CGRect UIRectInset(CGRect, CGFloat top, CGFloat right, CGFloat bottom, CGFloat left);

extern CGRect UIRectInsetEdges(CGRect, UIRectEdge edges, CGFloat v);
extern CGRect UIRectInset(CGRect, CGFloat top, CGFloat right, CGFloat bottom, CGFloat left);

WTF_EXTERN_C_END
