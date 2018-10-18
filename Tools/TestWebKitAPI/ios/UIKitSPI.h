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
#import <UIKit/UIKeyboard_Private.h>
#import <UIKit/UIResponder_Private.h>
#import <UIKit/UITextInputMultiDocument.h>
#import <UIKit/UITextInputTraits_Private.h>
#import <UIKit/UITextInput_Private.h>
#import <UIKit/UIViewController_Private.h>

#if ENABLE(DRAG_SUPPORT)
@protocol UIDragSession;
@class UIDragInteraction;
@class UIDragItem;
#import <UIKit/NSString+UIItemProvider.h>
#import <UIKit/NSURL+UIItemProvider.h>
#import <UIKit/UIDragInteraction_Private.h>
#import <UIKit/UIImage+UIItemProvider.h>
#import <UIKit/UIItemProvider.h>
#import <UIKit/UIItemProvider_Private.h>
#endif // ENABLE(DRAG_SUPPORT)

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
@end

@protocol UITextInputTraits_Private <NSObject, UITextInputTraits>
@property (nonatomic, readonly) UIColor *insertionPointColor;
@end

@class WebEvent;

@protocol UITextInputPrivate <UITextInput, UITextInputTraits_Private>
- (UITextInputTraits *)textInputTraits;
- (void)insertTextSuggestion:(UITextSuggestion *)textSuggestion;
- (void)handleKeyWebEvent:(WebEvent *)theEvent withCompletionHandler:(void (^)(WebEvent *, BOOL))completionHandler;
@end

@protocol UITextInputMultiDocument <NSObject>
@optional
- (void)_preserveFocusWithToken:(id <NSCopying, NSSecureCoding>)token destructively:(BOOL)destructively;
- (BOOL)_restoreFocusWithToken:(id <NSCopying, NSSecureCoding>)token;
@end

#if ENABLE(DRAG_SUPPORT)

@interface NSURL ()
@property (nonatomic, copy, setter=_setTitle:) NSString *_title;
@end

#define UIItemProviderRepresentationOptionsVisibilityAll NSItemProviderRepresentationVisibilityAll

@protocol UIItemProviderReading <NSItemProviderReading>

@required
- (instancetype)initWithItemProviderData:(NSData *)data typeIdentifier:(NSString *)typeIdentifier error:(NSError **)outError;

@end

@protocol UIItemProviderWriting <NSItemProviderWriting>

@required
- (NSProgress *)loadDataWithTypeIdentifier:(NSString *)typeIdentifier forItemProviderCompletionHandler:(void (^)(NSData *, NSError *))completionHandler;

@end

@interface NSAttributedString () <UIItemProviderReading, UIItemProviderWriting>
@end
@interface NSString () <UIItemProviderReading, UIItemProviderWriting>
@end
@interface NSURL () <UIItemProviderReading, UIItemProviderWriting>
@end
@interface UIImage () <UIItemProviderReading, UIItemProviderWriting>
@end

@interface UIItemProvider : NSItemProvider
@end

#endif

@interface UIKeyboard : UIView
@end

#endif

@protocol UITextInputTraits_Private_Proposed_SPI_34583628 <UITextInputPrivate>
- (NSDictionary *)_autofillContext;
@end

#if ENABLE(DRAG_SUPPORT)
@protocol UIDragInteractionDelegate_Proposed_SPI_33146803 <UIDragInteractionDelegate>
- (void)_dragInteraction:(UIDragInteraction *)interaction itemsForAddingToSession:(id <UIDragSession>)session withTouchAtPoint:(CGPoint)point completion:(void(^)(NSArray<UIDragItem *> *))completion;
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
@end

@interface UIKeyboard ()
+ (BOOL)isInHardwareKeyboardMode;
@end

#endif // PLATFORM(IOS_FAMILY)
