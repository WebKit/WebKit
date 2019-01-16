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

#if PLATFORM(IOSMAC)

WTF_EXTERN_C_BEGIN
typedef struct __GSKeyboard* GSKeyboardRef;
WTF_EXTERN_C_END

#if USE(APPLE_INTERNAL_SDK)

#import <UIKit/NSParagraphStyle_Private.h>
#import <UIKit/NSTextList.h>
#import <UIKit/UIApplicationSceneConstants.h>
#import <UIKit/UIApplication_Private.h>
#import <UIKit/UIColor_Private.h>
#import <UIKit/UIInterface_Private.h>
#import <UIKit/UIScreen_Private.h>
#import <UIKit/UIViewController_Private.h>

#if ENABLE(DATA_INTERACTION)
#import <UIKit/NSItemProvider+UIKitAdditions.h>
#import <UIKit/NSItemProvider+UIKitAdditions_Private.h>
#import <UIKit/NSURL+UIItemProvider.h>
#endif

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

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, UIApplicationSceneClassicMode) {
    UIApplicationSceneClassicModeOriginalPad = 4,
};

@interface NSParagraphStyle ()
- (NSArray *)textLists;
@end

@interface NSMutableParagraphStyle ()
- (void)setTextLists:(NSArray *)textLists;
@end

@interface NSTextList : NSObject
- (instancetype)initWithMarkerFormat:(NSString *)format options:(NSUInteger)mask;
@property (readonly, copy) NSString *markerFormat;
@property NSInteger startingItemNumber;
@end

@interface UIApplication ()
- (BOOL)_isClassic;
+ (UIApplicationSceneClassicMode)_classicMode;
- (GSKeyboardRef)_hardwareKeyboard;
@end

@interface UIColor ()

+ (UIColor *)systemBlueColor;
+ (UIColor *)systemGrayColor;
+ (UIColor *)systemGreenColor;
+ (UIColor *)systemOrangeColor;
+ (UIColor *)systemPinkColor;
+ (UIColor *)systemRedColor;
+ (UIColor *)systemYellowColor;

+ (UIColor *)_disambiguated_due_to_CIImage_colorWithCGColor:(CGColorRef)cgColor;

@end

@interface UIScreen ()

@property (nonatomic, readonly) CGRect _referenceBounds;

@end

@interface UIViewController ()
+ (UIViewController *)viewControllerForView:(UIView *)view;
@end

NS_ASSUME_NONNULL_END

#if __IPHONE_OS_VERSION_MIN_REQUIRED >= 110000
@interface NSURL ()
@property (nonatomic, copy, nullable, setter=_setTitle:) NSString *_title;
@end
#endif

#endif

#endif
