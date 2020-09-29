/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

WTF_EXTERN_C_BEGIN
typedef struct __GSKeyboard* GSKeyboardRef;
WTF_EXTERN_C_END

#if USE(APPLE_INTERNAL_SDK)

#import <UIKit/NSParagraphStyle_Private.h>
#import <UIKit/NSTextAlternatives.h>
#import <UIKit/NSTextAttachment_Private.h>
#import <UIKit/NSTextList.h>
#import <UIKit/UIApplicationSceneConstants.h>
#import <UIKit/UIApplication_Private.h>
#import <UIKit/UIColor_Private.h>
#import <UIKit/UIFocusRingStyle.h>
#import <UIKit/UIFont_Private.h>
#import <UIKit/UIInterface_Private.h>
#import <UIKit/UIScreen_Private.h>
#import <UIKit/UIViewController_Private.h>
#import <UIKit/NSItemProvider+UIKitAdditions.h>
#import <UIKit/NSItemProvider+UIKitAdditions_Private.h>
#import <UIKit/NSURL+UIItemProvider.h>

@interface UIApplication ()
+ (UIApplicationSceneClassicMode)_classicMode;
- (GSKeyboardRef)_hardwareKeyboard;
- (CGFloat)_iOSMacScale;
@end


#else // USE(APPLE_INTERNAL_SDK)

#import <UIKit/UIKit.h>

#if ENABLE(DRAG_SUPPORT)
#import <UIKit/NSItemProvider+UIKitAdditions.h>
#endif

typedef NS_ENUM(NSInteger, UIApplicationSceneClassicMode) {
    UIApplicationSceneClassicModeOriginalPad = 4,
};

typedef enum {
    UIFontTraitPlain       = 0x00000000,
    UIFontTraitItalic      = 0x00000001, // 1 << 0
    UIFontTraitBold        = 0x00000002, // 1 << 1
    UIFontTraitThin        = (1 << 2),
    UIFontTraitLight       = (1 << 3),
    UIFontTraitUltraLight  = (1 << 4)
} UIFontTrait;

@interface NSParagraphStyle ()
- (NSArray *)textLists;
@end

@interface NSMutableParagraphStyle ()
- (void)setTextLists:(NSArray *)textLists;
@end

@interface NSTextAttachment ()
- (id)initWithFileWrapper:(NSFileWrapper *)fileWrapper;
@end

@interface NSTextList : NSObject
- (instancetype)initWithMarkerFormat:(NSString *)format options:(NSUInteger)mask;
@property (readonly, copy) NSString *markerFormat;
@property NSInteger startingItemNumber;
- (NSString *)markerForItemNumber:(NSInteger)itemNum;
@end

@interface NSTextAlternatives : NSObject
@property (readonly) NSArray<NSString *> *alternativeStrings;
@end

@interface UIApplication ()
- (BOOL)_isClassic;
+ (UIApplicationSceneClassicMode)_classicMode;
- (GSKeyboardRef)_hardwareKeyboard;
@end

@interface UIColor ()

+ (UIColor *)systemBlueColor;
+ (UIColor *)systemBrownColor;
+ (UIColor *)systemGrayColor;
+ (UIColor *)systemGreenColor;
+ (UIColor *)systemOrangeColor;
+ (UIColor *)systemPinkColor;
+ (UIColor *)systemPurpleColor;
+ (UIColor *)systemRedColor;
+ (UIColor *)systemYellowColor;
+ (UIColor *)systemTealColor;

+ (UIColor *)_disambiguated_due_to_CIImage_colorWithCGColor:(CGColorRef)cgColor;

- (CGFloat)alphaComponent;

@end

@interface UIFont ()

+ (UIFont *)fontWithFamilyName:(NSString *)familyName traits:(UIFontTrait)traits size:(CGFloat)fontSize;

@end

@interface UIScreen ()

@property (nonatomic, readonly) CGRect _referenceBounds;

@end

@interface UIViewController ()
+ (UIViewController *)viewControllerForView:(UIView *)view;
@end

@interface NSURL ()
@property (nonatomic, copy, setter=_setTitle:) NSString *_title;
@end

@interface UIFocusRingStyle : NSObject
+ (CGFloat)cornerRadius;
+ (CGFloat)maxAlpha;
+ (CGFloat)alphaThreshold;
@end

#endif // USE(APPLE_INTERNAL_SDK)

@interface UIColor (IPI)
+ (UIColor *)keyboardFocusIndicatorColor;
@end

#if HAVE(OS_DARK_MODE_SUPPORT)
@interface UIColor (UIColorInternal)
+ (UIColor *)tableCellDefaultSelectionTintColor;
@end
#endif

#endif // PLATFORM(IOS_FAMILY)
