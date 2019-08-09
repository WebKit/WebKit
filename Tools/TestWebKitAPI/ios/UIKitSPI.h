/*
  * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#import <UIKit/UITextInputTraits.h>

#if USE(APPLE_INTERNAL_SDK)

#import <UIKit/UIApplication_Private.h>
#import <UIKit/UIBarButtonItemGroup_Private.h>
#import <UIKit/UICalloutBar.h>
#import <UIKit/UIKeyboardImpl.h>
#import <UIKit/UIKeyboard_Private.h>
#import <UIKit/UIResponder_Private.h>
#import <UIKit/UIScreen_Private.h>
#import <UIKit/UITextInputMultiDocument.h>
#import <UIKit/UITextInputTraits_Private.h>
#import <UIKit/UITextInput_Private.h>
#import <UIKit/UIViewController_Private.h>
#import <UIKit/UIWKTextInteractionAssistant.h>
#import <UIKit/UIWebFormAccessory.h>

IGNORE_WARNINGS_BEGIN("deprecated-implementations")
#import <UIKit/UIWebBrowserView.h>
#import <UIKit/UIWebView_Private.h>
IGNORE_WARNINGS_END

#if PLATFORM(IOS)
@protocol UIDragSession;
@class UIDragInteraction;
@class UIDragItem;
#import <UIKit/NSItemProvider+UIKitAdditions_Private.h>
#import <UIKit/UIDragInteraction_Private.h>
#endif // PLATFORM(IOS)

#else

WTF_EXTERN_C_BEGIN

void UIApplicationInitialize(void);

WTF_EXTERN_C_END

@interface UITextSuggestion : NSObject

@end

@interface UITextInputTraits : NSObject <UITextInputTraits>
@end

@protocol UIDragInteractionDelegate_ForWebKitOnly <UIDragInteractionDelegate>
@optional
- (void)_dragInteraction:(UIDragInteraction *)interaction prepareForSession:(id<UIDragSession>)session completion:(void(^)(void))completion;
- (void)_dragInteraction:(UIDragInteraction *)interaction itemsForAddingToSession:(id <UIDragSession>)session withTouchAtPoint:(CGPoint)point completion:(void(^)(NSArray<UIDragItem *> *))completion;
@end

@protocol UITextInputTraits_Private <NSObject, UITextInputTraits>
@property (nonatomic, readonly) UIColor *insertionPointColor;
@property (nonatomic, readonly) UIColor *selectionBarColor;
@end

@class WebEvent;

@class UITextInputArrowKeyHistory;

@protocol UITextInputPrivate <UITextInput, UITextInputTraits_Private>
- (UITextInputTraits *)textInputTraits;
- (void)insertTextSuggestion:(UITextSuggestion *)textSuggestion;
- (void)handleKeyWebEvent:(WebEvent *)theEvent withCompletionHandler:(void (^)(WebEvent *, BOOL))completionHandler;
- (BOOL)_shouldSuppressSelectionCommands;
- (NSDictionary *)_autofillContext;
- (UIFont *)fontForCaretSelection;
@end

@interface UIWebFormAccessory : UIInputView
@end

@interface UIBarButtonItemGroup ()
@property (nonatomic, readwrite, assign, getter=_isHidden, setter=_setHidden:) BOOL hidden;
@end

@protocol UITextInputMultiDocument <NSObject>
@optional
- (void)_preserveFocusWithToken:(id <NSCopying, NSSecureCoding>)token destructively:(BOOL)destructively;
- (BOOL)_restoreFocusWithToken:(id <NSCopying, NSSecureCoding>)token;
- (void)_clearToken:(id <NSCopying, NSSecureCoding>)token;
@end

@interface NSURL ()
@property (nonatomic, copy, setter=_setTitle:) NSString *_title;
@end

@interface UIKeyboard : UIView
@end

@interface UICalloutBar : UIView
+ (UICalloutBar *)sharedCalloutBar;
@end

@interface UIWKDocumentContext : NSObject

@property (nonatomic, copy) NSObject *contextBefore;
@property (nonatomic, copy) NSObject *selectedText;
@property (nonatomic, copy) NSObject *contextAfter;
@property (nonatomic, copy) NSObject *markedText;
@property (nonatomic, assign) NSRange selectedRangeInMarkedText;
@property (nonatomic, copy) NSAttributedString *annotatedText;

- (NSArray<NSValue *> *)characterRectsForCharacterRange:(NSRange)range;

@end

typedef NS_OPTIONS(NSInteger, UIWKDocumentRequestFlags) {
    UIWKDocumentRequestNone = 0,
    UIWKDocumentRequestText = 1 << 0,
    UIWKDocumentRequestAttributed = 1 << 1,
    UIWKDocumentRequestRects = 1 << 2,
    UIWKDocumentRequestSpatial = 1 << 3,
    UIWKDocumentRequestAnnotation = 1 << 4,
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
@end

@protocol UIWKInteractionViewProtocol
- (void)requestAutocorrectionRectsForString:(NSString *)input withCompletionHandler:(void (^)(UIWKAutocorrectionRects *rectsForInput))completionHandler;
- (void)requestAutocorrectionContextWithCompletionHandler:(void (^)(UIWKAutocorrectionContext *autocorrectionContext))completionHandler;
@end

IGNORE_WARNINGS_BEGIN("deprecated-implementations")

@interface UIWebBrowserView : UIView <UIKeyInput>
@end

@interface UIWebView (Private)
- (UIWebBrowserView *)_browserView;
@end

IGNORE_WARNINGS_END

@interface UIScreen ()
@property (nonatomic, readonly) CGRect _referenceBounds;
@end

#endif

#if __has_include(<UIKit/UITextAutofillSuggestion.h>)
// FIXME: Move this import under USE(APPLE_INTERNAL_SDK) once <rdar://problem/34583628> lands in the SDK.
#import <UIKit/UITextAutofillSuggestion.h>
@interface UITextAutofillSuggestion ()
+ (instancetype)autofillSuggestionWithUsername:(NSString *)username password:(NSString *)password;
@end
#else
@interface UITextAutofillSuggestion : UITextSuggestion
@property (nonatomic, assign) NSString *username;
@property (nonatomic, assign) NSString *password;
+ (instancetype)autofillSuggestionWithUsername:(NSString *)username password:(NSString *)password;
@end
#endif

@interface NSURL (UIKitSPI)
@property (nonatomic, copy, setter=_setTitle:) NSString *_title;
@end

@interface UIViewController (UIKitSPI)
+ (UIViewController *)_viewControllerForFullScreenPresentationFromView:(UIView *)view;
@end

@interface UIResponder (UIKitSPI)
- (UIResponder *)firstResponder;
- (void)makeTextWritingDirectionNatural:(id)sender;
@end

@interface UIKeyboard ()
+ (BOOL)isInHardwareKeyboardMode;
@end

@protocol UIWKInteractionViewProtocol_Staging_49236384 <UIWKInteractionViewProtocol>
- (void)pasteWithCompletionHandler:(void (^)(void))completionHandler;
@end

@protocol UITextInputPrivate_Staging_54140418 <UITextInputPrivate>
@property (nonatomic, readonly) BOOL supportsImagePaste;
@end

@interface UIWebFormAccessory (Staging_49666643)
- (void)setNextPreviousItemsVisible:(BOOL)visible;
@end

#if PLATFORM(IOS)

@protocol UIDropInteractionDelegate_Staging_31075005 <UIDropInteractionDelegate>
- (void)_dropInteraction:(UIDropInteraction *)interaction delayedPreviewProviderForDroppingItem:(UIDragItem *)item previewProvider:(void(^)(UITargetedDragPreview *preview))previewProvider;
@end

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

#endif // PLATFORM(IOS)

@protocol UITextInputInternal
- (CGRect)_selectionClipRect;
@end

#endif // PLATFORM(IOS_FAMILY)
