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

#if USE(APPLE_INTERNAL_SDK)

#import <UIKit/UIApplication_Private.h>
#import <UIKit/UIInterface_Private.h>
#import <UIKit/UIPopoverPresentationController_Private.h>
#import <UIKit/UIScreen_Private.h>
#import <UIKit/UIViewController_Private.h>

#else

#import <UIKit/UIKit.h>

@interface UIApplication ()

- (BOOL)_isClassic;

@end

@interface UIColor ()

+ (UIColor *)systemBlueColor;
+ (UIColor *)systemGrayColor;
+ (UIColor *)systemGreenColor;
+ (UIColor *)systemOrangeColor;
+ (UIColor *)systemPinkColor;
+ (UIColor *)systemRedColor;
+ (UIColor *)systemYellowColor;

@end

@interface UIScreen ()

@property (nonatomic, readonly) CGRect _referenceBounds;

@end

@interface UIPopoverPresentationController()

+ (void)_setAlwaysAllowPopoverPresentations:(BOOL)flag;

@end

@interface UIViewController ()
+ (UIViewController *)viewControllerForView:(UIView *)view;
@end

#endif
