/*
  * Copyright (C) 2017-2023 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS_FAMILY)

#import <UIKit/UIKit.h>

#if USE(APPLE_INTERNAL_SDK)

#import <UIKit/NSParagraphStyle_Private.h>
#import <UIKit/NSTextAlternatives.h>
#import <UIKit/NSTextList.h>
#import <UIKit/NSURL+UIItemProvider.h>
#import <UIKit/UIAction_Private.h>
#import <UIKit/UIApplication_Private.h>
#import <UIKit/UIBarButtonItemGroup_Private.h>
#import <UIKit/UIContextMenuInteraction_ForWebKitOnly.h>
#import <UIKit/UIDevice_Private.h>
#import <UIKit/UIEvent_Private.h>
#import <UIKit/UIKeyboardImpl.h>
#import <UIKit/UIKeyboardInputModeController.h>
#import <UIKit/UIKeyboardPreferencesController.h>
#import <UIKit/UIKeyboard_Private.h>
#import <UIKit/UIPress_Private.h>
#import <UIKit/UIResponder_Private.h>
#import <UIKit/UIScreen_Private.h>
#import <UIKit/UIScrollEvent_Private.h>
#import <UIKit/UIScrollView_ForWebKitOnly.h>
#import <UIKit/UIScrollView_Private.h>
#import <UIKit/UITapGestureRecognizer_Private.h>
#import <UIKit/UITextAutofillSuggestion.h>
#import <UIKit/UITextChecker_Private.h>
#import <UIKit/UITextEffectsWindow.h>
#import <UIKit/UITextInputMultiDocument.h>
#import <UIKit/UITextInputTraits_Private.h>
#import <UIKit/UITextInput_Private.h>
#import <UIKit/UIViewController_Private.h>
#import <UIKit/UIWKTextInteractionAssistant.h>
#import <UIKit/UIWebFormAccessory.h>
#import <UIKit/UIWindowScene_RequiresApproval.h>
#import <UIKit/UIWindow_Private.h>
#import <UIKit/_UINavigationInteractiveTransition.h>

#if !__has_include(<UIKit/UIAsyncTextInput_ForWebKitOnly.h>)
#define UITextDocumentContext UIWKDocumentContext
#define UITextDocumentRequest UIWKDocumentRequest
#endif

IGNORE_WARNINGS_BEGIN("deprecated-implementations")
#import <UIKit/UIWebBrowserView.h>
#import <UIKit/UIWebScrollView.h>
#import <UIKit/UIWebView_Private.h>
IGNORE_WARNINGS_END

#if PLATFORM(IOS) || PLATFORM(VISION)
@protocol UIDragSession;
@class UIDragInteraction;
@class UIDragItem;
#import <UIKit/NSItemProvider+UIKitAdditions_Private.h>
#import <UIKit/UIDragInteraction_Private.h>
#endif // PLATFORM(IOS) || PLATFORM(VISION)

#else // USE(APPLE_INTERNAL_SDK)

#define UITextDocumentContext UIWKDocumentContext
#define UITextDocumentRequest UIWKDocumentRequest

@interface NSTextAlternatives : NSObject
- (id)initWithPrimaryString:(NSString *)primaryString alternativeStrings:(NSArray<NSString *> *)alternativeStrings;
@property (readonly) NSString *primaryString;
@property (readonly) NSArray<NSString *> *alternativeStrings;
@property (readonly) BOOL isLowConfidence;
@end

WTF_EXTERN_C_BEGIN

typedef struct __IOHIDEvent* IOHIDEventRef;
typedef struct __GSKeyboard* GSKeyboardRef;

void UIApplicationInitialize(void);
void UIApplicationInstantiateSingleton(Class principalClass);

extern NSString * const UIWindowDidRotateNotification;
extern NSNotificationName const _UIWindowSceneEnhancedWindowingModeChanged;

WTF_EXTERN_C_END

#if HAVE(UIFINDINTERACTION)

@interface _UIFindInteraction : NSObject <UIInteraction>
@end

@interface _UIFindInteraction (Staging_84486967)

- (void)presentFindNavigatorShowingReplace:(BOOL)replaceVisible;

- (void)findNext;
- (void)findPrevious;

@end

#endif // HAVE(UIFINDINTERACTION)

@interface UIApplication ()
- (void)_enqueueHIDEvent:(IOHIDEventRef)event;
- (void)_handleHIDEvent:(IOHIDEventRef)event;
- (void)_cancelAllTouches;
- (CGFloat)statusBarHeight;
@end

@interface UIDevice ()
- (void)setOrientation:(UIDeviceOrientation)orientation animated:(BOOL)animated;
@end

@interface UITextSuggestion : NSObject
@property (nonatomic, copy) NSString *displayText;
+ (instancetype)textSuggestionWithInputText:(NSString *)inputText;
@end

@protocol UITextInputTraits_Private <NSObject, UITextInputTraits>
@property (nonatomic, readonly) UIColor *insertionPointColor;
@property (nonatomic, readonly) UIColor *selectionBarColor;
@property (nonatomic) BOOL isSingleLineDocument;
@property (nonatomic) BOOL learnsCorrections;
@end

@interface UITextInputTraits : NSObject <UITextInputTraits, UITextInputTraits_Private, NSCopying>
@end

@protocol UIDragInteractionDelegate_ForWebKitOnly <UIDragInteractionDelegate>
@optional
- (void)_dragInteraction:(UIDragInteraction *)interaction prepareForSession:(id<UIDragSession>)session completion:(void(^)(void))completion;
- (void)_dragInteraction:(UIDragInteraction *)interaction itemsForAddingToSession:(id <UIDragSession>)session withTouchAtPoint:(CGPoint)point completion:(void(^)(NSArray<UIDragItem *> *))completion;
@end

@class WebEvent;

@protocol UITextInputPrivate <UITextInput, UITextInputTraits_Private>
- (UITextInputTraits *)textInputTraits;
- (void)insertTextSuggestion:(UITextSuggestion *)textSuggestion;
- (void)handleKeyWebEvent:(WebEvent *)theEvent withCompletionHandler:(void (^)(WebEvent *, BOOL))completionHandler;
- (NSDictionary *)_autofillContext;
- (UIFont *)fontForCaretSelection;
@end

@protocol UITextInputMultiDocument <NSObject>
@optional
- (void)_preserveFocusWithToken:(id <NSCopying, NSSecureCoding>)token destructively:(BOOL)destructively;
- (BOOL)_restoreFocusWithToken:(id <NSCopying, NSSecureCoding>)token;
- (void)_clearToken:(id <NSCopying, NSSecureCoding>)token;
@end

@interface UITextAutofillSuggestion : UITextSuggestion
@property (nonatomic, assign) NSString *username;
@property (nonatomic, assign) NSString *password;
@end

@interface NSURL ()
@property (nonatomic, copy, setter=_setTitle:) NSString *_title;
@end

@interface UIKeyboard : UIView
@end

@interface _UINavigationInteractiveTransitionBase : UIPercentDrivenInteractiveTransition
@end

@interface UIWKDocumentContext : NSObject

@property (nonatomic, copy) NSObject *contextBefore;
@property (nonatomic, copy) NSObject *selectedText;
@property (nonatomic, copy) NSObject *contextAfter;
@property (nonatomic, copy) NSObject *markedText;
@property (nonatomic, assign) NSRange selectedRangeInMarkedText;
@property (nonatomic, copy) NSAttributedString *annotatedText;
@property (nonatomic, readonly) NSRange markedTextRange;

- (void)enumerateLayoutRects:(void (^)(NSRange characterRange, CGRect layoutRect, BOOL *stop))block;
- (NSArray<NSValue *> *)characterRectsForCharacterRange:(NSRange)range;

@end

typedef NS_OPTIONS(NSInteger, UIWKDocumentRequestFlags) {
    UIWKDocumentRequestNone = 0,
    UIWKDocumentRequestText = 1 << 0,
    UIWKDocumentRequestAttributed = 1 << 1,
    UIWKDocumentRequestRects = 1 << 2,
    UIWKDocumentRequestSpatial = 1 << 3,
    UIWKDocumentRequestAnnotation = 1 << 4,
    UIWKDocumentRequestMarkedTextRects = 1 << 5,
    UIWKDocumentRequestSpatialAndCurrentSelection = 1 << 6,
};

@interface UIWKDocumentRequest : NSObject
@property (nonatomic, assign) UIWKDocumentRequestFlags flags;
@property (nonatomic, assign) UITextGranularity surroundingGranularity;
@property (nonatomic, assign) NSInteger granularityCount;
@property (nonatomic, assign) CGRect documentRect;
@property (nonatomic, retain) id <NSCopying> inputElementIdentifier;
@end

@interface UIWKAutocorrectionRects : NSObject
@property (nonatomic) CGRect firstRect;
@property (nonatomic) CGRect lastRect;
@end

@interface UIWKAutocorrectionContext : NSObject
@property (nonatomic, copy) NSString *contextBeforeSelection;
@property (nonatomic, copy) NSString *selectedText;
@property (nonatomic, copy) NSString *contextAfterSelection;
@property (nonatomic, copy) NSString *markedText;
@property (nonatomic) NSRange rangeInMarkedText;
@end

typedef NS_ENUM(NSInteger, UIWKGestureType) {
    UIWKGestureLoupe
};

@class RVItem;
@protocol UIWKInteractionViewProtocol
- (void)pasteWithCompletionHandler:(void (^)(void))completionHandler;
- (void)requestAutocorrectionRectsForString:(NSString *)input withCompletionHandler:(void (^)(UIWKAutocorrectionRects *rectsForInput))completionHandler;
- (void)requestAutocorrectionContextWithCompletionHandler:(void (^)(UIWKAutocorrectionContext *autocorrectionContext))completionHandler;
- (void)selectPositionAtPoint:(CGPoint)point completionHandler:(void (^)(void))completionHandler;
- (void)selectTextWithGranularity:(UITextGranularity)granularity atPoint:(CGPoint)point completionHandler:(void (^)(void))completionHandler;
- (void)updateSelectionWithExtentPoint:(CGPoint)point completionHandler:(void (^)(BOOL selectionEndIsMoving))completionHandler;
- (void)updateSelectionWithExtentPoint:(CGPoint)point withBoundary:(UITextGranularity)granularity completionHandler:(void (^)(BOOL selectionEndIsMoving))completionHandler;
- (void)selectWordForReplacement;
- (BOOL)textInteractionGesture:(UIWKGestureType)gesture shouldBeginAtPoint:(CGPoint)point;
- (void)replaceDictatedText:(NSString *)oldText withText:(NSString *)newText;
- (NSArray<NSTextAlternatives *> *)alternativesForSelectedText;

- (void)applyAutocorrection:(NSString *)correction toString:(NSString *)input shouldUnderline:(BOOL)shouldUnderline withCompletionHandler:(void (^)(UIWKAutocorrectionRects *rectsForCorrection))completionHandler;

#if HAVE(UI_WK_DOCUMENT_CONTEXT)
- (void)requestDocumentContext:(UITextDocumentRequest *)request completionHandler:(void (^)(UITextDocumentContext *))completionHandler;
- (void)adjustSelectionWithDelta:(NSRange)deltaRange completionHandler:(void (^)(void))completionHandler;
- (void)selectPositionAtPoint:(CGPoint)point withContextRequest:(UITextDocumentRequest *)request completionHandler:(void (^)(UITextDocumentContext *))completionHandler;
#endif

@property (nonatomic, readonly) NSString *selectedText;

@optional
- (void)insertTextPlaceholderWithSize:(CGSize)size completionHandler:(void (^)(UITextPlaceholder *))completionHandler;
- (void)prepareSelectionForContextMenuWithLocationInView:(CGPoint)locationInView completionHandler:(void(^)(BOOL shouldPresentMenu, RVItem * rvItem))completionHandler;
- (void)removeTextPlaceholder:(UITextPlaceholder *)placeholder willInsertText:(BOOL)willInsertText completionHandler:(void (^)(void))completionHandler;
@end

@interface UIScreen ()
@property (nonatomic, readonly) CGRect _referenceBounds;
@end

typedef NS_ENUM(NSInteger, _UIDataOwner) {
    _UIDataOwnerUndefined,
    _UIDataOwnerUser,
    _UIDataOwnerEnterprise,
    _UIDataOwnerShared,
};

@interface UIResponder (UIKitSPI)
- (UIResponder *)firstResponder;
- (void)makeTextWritingDirectionNatural:(id)sender;
@property (nonatomic, setter=_setSuppressSoftwareKeyboard:) BOOL _suppressSoftwareKeyboard;
@property (nonatomic, setter=_setDataOwnerForCopy:) _UIDataOwner _dataOwnerForCopy;
@property (nonatomic, setter=_setDataOwnerForPaste:) _UIDataOwner _dataOwnerForPaste;
@end

@interface UIKeyboardImpl : UIView
+ (instancetype)activeInstance;
+ (instancetype)sharedInstance;
- (BOOL)isAutoShifted;
- (void)dismissKeyboard;
- (void)setCorrectionLearningAllowed:(BOOL)allowed;
@end

@interface UIScreen ()
- (void)_setScale:(CGFloat)scale;
@end

@protocol UITextInputSuggestionDelegate <UITextInputDelegate>
- (void)setSuggestions:(NSArray <UITextSuggestion*> *)suggestions;
@end

@interface UIScrollView (SPI)
@property (nonatomic, getter=_isAutomaticContentOffsetAdjustmentEnabled, setter=_setAutomaticContentOffsetAdjustmentEnabled:) BOOL isAutomaticContentOffsetAdjustmentEnabled;
@property (nonatomic, readonly, getter=_isVerticalBouncing) BOOL isVerticalBouncing;
@property (nonatomic, readonly, getter=_isHorizontalBouncing) BOOL isHorizontalBouncing;
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
@end

@interface UITextInteractionAssistant : NSObject <UIResponderStandardEditActions>
@end

@interface UIWKTextInteractionAssistant : UITextInteractionAssistant
- (void)lookup:(NSString *)textWithContext withRange:(NSRange)range fromRect:(CGRect)presentationRect;
- (void)selectionChanged;
@end

typedef NS_ENUM(NSInteger, _UITextSearchMatchMethod) {
    _UITextSearchMatchMethodContains,
    _UITextSearchMatchMethodStartsWith,
    _UITextSearchMatchMethodFullWord,
};

@protocol _UITextSearching <NSObject>
@optional
- (void)didBeginTextSearchOperation;
- (void)didEndTextSearchOperation;
- (BOOL)supportsTextReplacement;
@end

@interface UIScrollView ()
@property (nonatomic, readonly, getter=_isAnimatingZoom) BOOL isAnimatingZoom;
@property (nonatomic, readonly, getter=_isAnimatingScroll) BOOL isAnimatingScroll;
@property (nonatomic, getter=_isFirstResponderKeyboardAvoidanceEnabled, setter=_setFirstResponderKeyboardAvoidanceEnabled:) BOOL firstResponderKeyboardAvoidanceEnabled;
@end

@interface UIView ()
- (void)_removeAllAnimations:(BOOL)includeSubviews;
@end

@interface UIViewController ()
- (BOOL)isPerformingModalTransition;
@end

@interface UIApplicationRotationFollowingWindow : UIWindow
@end

@interface UIAutoRotatingWindow : UIApplicationRotationFollowingWindow
@end

@interface UIApplicationRotationFollowingController : UIViewController
@end

@interface UIApplicationRotationFollowingControllerNoTouches : UIApplicationRotationFollowingController
@end

@interface UITextEffectsWindow : UIAutoRotatingWindow
+ (UITextEffectsWindow *)sharedTextEffectsWindowForWindowScene:(UIWindowScene *)windowScene;
@end

@interface UITextChecker ()
- (instancetype)_initWithAsynchronousLoading:(BOOL)asynchronousLoading;
- (NSArray<NSTextAlternatives *> *)grammarAlternativesForString:(NSString *)string;
- (NSArray<NSTextCheckingResult *> *)checkString:(NSString *)stringToCheck range:(NSRange)range types:(NSTextCheckingTypes)checkingTypes languages:(NSArray<NSString *> *)languagesArray options:(NSDictionary<NSString *, id> *)options;
@end

@interface UITapGestureRecognizer ()
@property (nonatomic) CFTimeInterval maximumIntervalBetweenSuccessiveTaps;
@end

@interface UIWebScrollView : UIScrollView
@end

@interface UIWindow ()
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
- (void)insertText:(NSString *)text;
@property (nonatomic) BOOL inputViewObeysDOMFocus;
@end

WTF_EXTERN_C_BEGIN

IGNORE_WARNINGS_BEGIN("deprecated-implementations")

extern const float UIWebViewGrowsAndShrinksToFitHeight;
extern const float UIWebViewScalesToFitScale;
extern const float UIWebViewStandardViewportWidth;

void _UIApplicationLoadWebKit(void);

@interface UIWebView (Private)
- (UIWebBrowserView *)_browserView;
@end

IGNORE_WARNINGS_END

WTF_EXTERN_C_END

#if HAVE(UIKIT_RESIZABLE_WINDOWS)

@interface UIWindowScene ()
@property (nonatomic, readonly) BOOL _enhancedWindowingEnabled;
@end

#endif // HAVE(UIKIT_RESIZABLE_WINDOWS)

@protocol TIPreferencesControllerActions;

@interface UIKeyboardPreferencesController : NSObject
+ (UIKeyboardPreferencesController *)sharedPreferencesController;
- (void)setValue:(id)value forPreferenceKey:(NSString *)key;
- (BOOL)boolForPreferenceKey:(NSString *)key;
- (id)valueForPreferenceKey:(NSString *)key;
@property (nonatomic, readonly) UIKeyboardPreferencesController<TIPreferencesControllerActions> *preferencesActions;
@end

@interface UIContextMenuInteraction ()
- (void)_presentMenuAtLocation:(CGPoint)location;
@end

@interface UIKeyboardInputMode : UITextInputMode <NSCopying>
+ (UIKeyboardInputMode *)keyboardInputModeWithIdentifier:(NSString *)identifier;
@property (nonatomic, readonly, retain) NSArray <NSString *> *multilingualLanguages;
@property (nonatomic, readonly, retain) NSString *languageWithRegion;
@end

@interface UIKeyboardInputModeController : NSObject
+ (UIKeyboardInputModeController *)sharedInputModeController;
@property (readwrite, retain) UIKeyboardInputMode *currentInputMode;
@end

#if PLATFORM(IOS) && !defined(__IPHONE_13_4)
typedef NS_OPTIONS(NSInteger, UIEventButtonMask) {
    UIEventButtonMaskPrimary = 1 << 0,
    UIEventButtonMaskSecondary = 1 << 1,
};
#endif

typedef enum {
    kUIKeyboardInputModifierFlagsChanged   = 1 << 5,
} UIKeyboardInputFlags;

@interface UIEvent ()
- (UIEventButtonMask)_buttonMask;
@end

#if USE(BROWSERENGINEKIT)
@interface UIKeyEvent : NSObject
- (instancetype)initWithWebEvent:(WebEvent *)webEvent;
@end
#endif

#endif // USE(APPLE_INTERNAL_SDK)

// Start of UIKit IPI

@class UITextInputArrowKeyHistory;

@interface UITextAutofillSuggestion ()
+ (instancetype)autofillSuggestionWithUsername:(NSString *)username password:(NSString *)password;
@end

@interface UIKeyboard ()
+ (BOOL)isInHardwareKeyboardMode;
+ (BOOL)usesInputSystemUI;
@end

@class TIKeyboardCandidate;
@class TIKeyboardInput;

@interface UIKeyboardImpl (UIKitIPI)
- (void)prepareKeyboardInputModeFromPreferences:(UIKeyboardInputMode *)lastUsedMode;
- (BOOL)_shouldSuppressSoftwareKeyboard;
- (void)syncInputManagerToAcceptedAutocorrection:(TIKeyboardCandidate *)autocorrection forInput:(TIKeyboardInput *)inputEvent;
- (void)setInlineCompletionAsMarkedText:(NSAttributedString *)inlineCompletion selectedRange:(NSRange)selectedRange inputString:(NSString *)inputString searchString:(NSString *)searchString;
@property (nonatomic, readonly) BOOL hasInlineCompletionAsMarkedText;
@property (nonatomic, readonly) UIKeyboardInputMode *currentInputModeInPreference;
@end

#if PLATFORM(IOS) || PLATFORM(VISION)

@protocol UIDropInteractionDelegate_Private <UIDropInteractionDelegate>
- (void)_dropInteraction:(UIDropInteraction *)interaction delayedPreviewProviderForDroppingItem:(UIDragItem *)item previewProvider:(void(^)(UITargetedDragPreview *preview))previewProvider;
@end

#endif // PLATFORM(IOS) || PLATFORM(VISION)

typedef NS_ENUM(NSUInteger, _UIClickInteractionEvent) {
    _UIClickInteractionEventBegan = 0,
    _UIClickInteractionEventClickedDown,
    _UIClickInteractionEventClickedUp,
    _UIClickInteractionEventEnded,
    _UIClickInteractionEventCount
};

@protocol _UIClickInteractionDriving;
@protocol _UIClickInteractionDriverDelegate <NSObject>
- (void)clickDriver:(id<_UIClickInteractionDriving>)driver shouldBegin:(void(^)(BOOL))completion;
- (void)clickDriver:(id<_UIClickInteractionDriving>)driver didPerformEvent:(_UIClickInteractionEvent)event;
@optional
- (void)clickDriver:(id<_UIClickInteractionDriving>)driver didUpdateHighlightProgress:(CGFloat)progress;
- (BOOL)clickDriver:(id<_UIClickInteractionDriving>)driver shouldDelayGestureRecognizer:(UIGestureRecognizer *)gestureRecognizer;
@end

@protocol UITextInputInternal
- (UTF32Char)_characterInRelationToCaretSelection:(int)amount;
- (CGRect)_selectionClipRect;
- (void)moveByOffset:(NSInteger)offset;
@optional
- (void)addTextAlternatives:(NSTextAlternatives *)alternatives;
- (void)removeEmojiAlternatives;
- (UITextInputArrowKeyHistory *)_moveToEndOfParagraph:(BOOL)extending withHistory:(UITextInputArrowKeyHistory *)history;
- (UITextInputArrowKeyHistory *)_moveToStartOfParagraph:(BOOL)extending withHistory:(UITextInputArrowKeyHistory *)history;
@end

typedef NS_ENUM(NSInteger, NSTextBlockLayer) {
    NSTextBlockPadding  = -1,
    NSTextBlockBorder   =  0,
    NSTextBlockMargin   =  1
};

@interface NSTextBlock : NSObject
- (CGFloat)widthForLayer:(NSTextBlockLayer)layer edge:(CGRectEdge)edge;
@property (nonatomic, copy) UIColor *backgroundColor;
@end

@interface NSTextTable : NSTextBlock
@end

@interface NSTextTableBlock : NSTextBlock
- (NSTextTable *)table;
- (NSInteger)startingColumn;
- (NSInteger)startingRow;
- (NSUInteger)numberOfColumns;
- (NSInteger)columnSpan;
- (NSInteger)rowSpan;
@end

@interface NSParagraphStyle ()
- (NSArray<NSTextBlock *> *)textBlocks;
@end

@interface UIResponder (Internal)
- (void)_share:(id)sender;
@property (nonatomic, readonly) BOOL _requiresKeyboardWhenFirstResponder;
@end

@protocol UIWKInteractionViewProtocol_Staging_91919121 <UIWKInteractionViewProtocol>
@optional
- (void)willInsertFinalDictationResult;
- (void)didInsertFinalDictationResult;
@end

@protocol UIWKInteractionViewProtocol_Staging_95652872 <UIWKInteractionViewProtocol_Staging_91919121>
#if HAVE(UI_EDIT_MENU_INTERACTION)
- (void)requestPreferredArrowDirectionForEditMenuWithCompletionHandler:(void (^)(UIEditMenuArrowDirection))completionHandler;
#endif
@end

#if HAVE(UIFINDINTERACTION)
@interface UITextSearchOptions ()
@property (nonatomic, readwrite) UITextSearchMatchMethod wordMatchMethod;
@property (nonatomic, readwrite) NSStringCompareOptions stringCompareOptions;
@end
#endif

#if HAVE(AUTOCORRECTION_ENHANCEMENTS)
@interface UIWKDocumentContext (Staging_112795757)
@property (nonatomic, copy) NSArray<NSValue *> *autocorrectedRanges;
@end
#endif

#if USE(BROWSERENGINEKIT)
// FIXME: Replace this with BEResponderEditActions once that's in the SDK.
@interface UIResponder (Staging_121208689)
- (void)addShortcut:(id)sender;
- (void)lookup:(id)sender;
- (void)findSelected:(id)sender;
- (void)promptForReplace:(id)sender;
- (void)share:(id)sender;
- (void)translate:(id)sender;
- (void)transliterateChinese:(id)sender;
- (void)replace:(id)sender;
@end
#endif

@interface UIView (IPI)
- (void)_updateSafeAreaInsets;
@end

@interface UIPhysicalKeyboardEvent : UIPressesEvent
@end

@interface UIPhysicalKeyboardEvent ()
+ (UIPhysicalKeyboardEvent *)_eventWithInput:(NSString *)input inputFlags:(UIKeyboardInputFlags)flags;
- (void)_setHIDEvent:(IOHIDEventRef)event keyboard:(GSKeyboardRef)gsKeyboard;
@end

@class UIPressInfo;

@interface UIPress (IPI)
- (void)_loadStateFromPressInfo:(UIPressInfo *)pressInfo;
@end

@interface UIApplication (IPI)
- (UIPressInfo *)_pressInfoForPhysicalKeyboardEvent:(UIPhysicalKeyboardEvent *)physicalKeyboardEvent;
@end

#if USE(BROWSERENGINEKIT)
@interface UIKeyEvent (IPI)
@property (nonatomic, readonly) WebEvent *webEvent;
@end
#endif

#endif // PLATFORM(IOS_FAMILY)
