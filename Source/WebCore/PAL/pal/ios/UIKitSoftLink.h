/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

// FIXME: Remove the `__has_feature(modules)` condition when possible.
#if !__has_feature(modules)

#if PLATFORM(IOS_FAMILY)

#import <pal/spi/ios/UIKitSPI.h>
#import <wtf/SoftLinking.h>

@class CUICatalog;

SOFT_LINK_FRAMEWORK_FOR_HEADER_REQUIRED(PAL, UIKit)

SOFT_LINK_CONSTANT_FOR_HEADER_REQUIRED(PAL, UIKit, UIAccessibilityAnnouncementNotification, UIAccessibilityNotifications)
SOFT_LINK_CONSTANT_FOR_HEADER_REQUIRED(PAL, UIKit, UIApplicationWillResignActiveNotification, NSNotificationName)
SOFT_LINK_CONSTANT_FOR_HEADER_REQUIRED(PAL, UIKit, UIApplicationWillEnterForegroundNotification, NSNotificationName)
SOFT_LINK_CONSTANT_FOR_HEADER_REQUIRED(PAL, UIKit, UIApplicationDidBecomeActiveNotification, NSNotificationName)
SOFT_LINK_CONSTANT_FOR_HEADER_REQUIRED(PAL, UIKit, UIApplicationDidEnterBackgroundNotification, NSNotificationName)
SOFT_LINK_CONSTANT_FOR_HEADER_REQUIRED(PAL, UIKit, UIContentSizeCategoryDidChangeNotification, NSNotificationName)
SOFT_LINK_CONSTANT_FOR_HEADER_REQUIRED(PAL, UIKit, UIApplicationDidChangeStatusBarOrientationNotification, NSNotificationName)
SOFT_LINK_CONSTANT_FOR_HEADER_REQUIRED(PAL, UIKit, UIFontTextStyleCallout, UIFontTextStyle)
SOFT_LINK_CONSTANT_FOR_HEADER_REQUIRED(PAL, UIKit, UIPasteboardNameGeneral, UIPasteboardName)
SOFT_LINK_CONSTANT_FOR_HEADER_REQUIRED(PAL, UIKit, UITextEffectsBeneathStatusBarWindowLevel, UIWindowLevel)
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(PAL, NSParagraphStyle)
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(PAL, NSPresentationIntent)
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(PAL, NSShadow)
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(PAL, NSTextList)
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(PAL, UIApplication)
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(PAL, UIColor)
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(PAL, UIDevice)
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(PAL, UIDocumentInteractionController)
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(PAL, UIFocusRingStyle)
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(PAL, UIFont)
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(PAL, UIGraphicsImageRenderer)
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(PAL, UIImage)
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(PAL, UIImageSymbolConfiguration)
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(PAL, UIImageView)
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(PAL, UILabel)
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(PAL, UIPasteboard)
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(PAL, UIScreen)
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(PAL, UITapGestureRecognizer)
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(PAL, UITextEffectsWindow)
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(PAL, UITraitCollection)
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(PAL, UIView)
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(PAL, UIViewController)
SOFT_LINK_CLASS_FOR_HEADER_REQUIRED(PAL, UIWindow)
SOFT_LINK_FUNCTION_FOR_HEADER_REQUIRED(PAL, UIKit, UIAccessibilityIsGrayscaleEnabled, BOOL, (void), ())
#define UIAccessibilityIsGrayscaleEnabled PAL::softLink_UIKit_UIAccessibilityIsGrayscaleEnabled
SOFT_LINK_FUNCTION_FOR_HEADER_REQUIRED(PAL, UIKit, UIAccessibilityIsInvertColorsEnabled, BOOL, (void), ())
#define UIAccessibilityIsInvertColorsEnabled PAL::softLink_UIKit_UIAccessibilityIsInvertColorsEnabled
SOFT_LINK_FUNCTION_FOR_HEADER_REQUIRED(PAL, UIKit, UIAccessibilityIsReduceMotionEnabled, BOOL, (void), ())
#define UIAccessibilityIsReduceMotionEnabled PAL::softLink_UIKit_UIAccessibilityIsReduceMotionEnabled
SOFT_LINK_FUNCTION_FOR_HEADER_REQUIRED(PAL, UIKit, UIAccessibilityDarkerSystemColorsEnabled, BOOL, (void), ())
#define UIAccessibilityDarkerSystemColorsEnabled PAL::softLink_UIKit_UIAccessibilityDarkerSystemColorsEnabled
SOFT_LINK_FUNCTION_FOR_HEADER_REQUIRED(PAL, UIKit, UIAccessibilityIsOnOffSwitchLabelsEnabled, BOOL, (void), ())
#define UIAccessibilityIsOnOffSwitchLabelsEnabled PAL::softLink_UIKit_UIAccessibilityIsOnOffSwitchLabelsEnabled
SOFT_LINK_FUNCTION_FOR_HEADER_REQUIRED(PAL, UIKit, UIAccessibilityPostNotification, void, (UIAccessibilityNotifications n, id argument), (n, argument))
#define UIAccessibilityPostNotification PAL::softLink_UIKit_UIAccessibilityPostNotification
SOFT_LINK_FUNCTION_FOR_HEADER_REQUIRED(PAL, UIKit, UIGraphicsGetCurrentContext, CGContextRef, (void), ())
#define UIGraphicsGetCurrentContext PAL::softLink_UIKit_UIGraphicsGetCurrentContext
SOFT_LINK_FUNCTION_FOR_HEADER_REQUIRED(PAL, UIKit, UIGraphicsPopContext, void, (void), ())
#define UIGraphicsPopContext PAL::softLink_UIKit_UIGraphicsPopContext
SOFT_LINK_FUNCTION_FOR_HEADER_REQUIRED(PAL, UIKit, UIGraphicsPushContext, void, (CGContextRef context), (context))
#define UIGraphicsPushContext PAL::softLink_UIKit_UIGraphicsPushContext
SOFT_LINK_FUNCTION_FOR_HEADER_REQUIRED(PAL, UIKit, UIImagePNGRepresentation, NSData *, (UIImage *image), (image))
#define UIImagePNGRepresentation PAL::softLink_UIKit_UIImagePNGRepresentation

#endif

#endif // !__has_feature(modules)
