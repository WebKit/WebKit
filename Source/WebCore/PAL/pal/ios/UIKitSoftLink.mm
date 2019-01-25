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

#include "config.h"

#if PLATFORM(IOS_FAMILY)

#import <pal/spi/ios/UIKitSPI.h>
#import <wtf/SoftLinking.h>

@class CUICatalog;

SOFT_LINK_FRAMEWORK_FOR_SOURCE(PAL, UIKit)

SOFT_LINK_CONSTANT_FOR_SOURCE(PAL, UIKit, UIAccessibilityAnnouncementNotification, UIAccessibilityNotifications)
SOFT_LINK_CONSTANT_FOR_SOURCE(PAL, UIKit, UIApplicationWillResignActiveNotification, NSNotificationName)
SOFT_LINK_CONSTANT_FOR_SOURCE(PAL, UIKit, UIApplicationWillEnterForegroundNotification, NSNotificationName)
SOFT_LINK_CONSTANT_FOR_SOURCE(PAL, UIKit, UIApplicationDidBecomeActiveNotification, NSNotificationName)
SOFT_LINK_CONSTANT_FOR_SOURCE(PAL, UIKit, UIApplicationDidEnterBackgroundNotification, NSNotificationName)
SOFT_LINK_CONSTANT_FOR_SOURCE(PAL, UIKit, UIContentSizeCategoryDidChangeNotification, NSNotificationName)
SOFT_LINK_CONSTANT_FOR_SOURCE(PAL, UIKit, UIFontTextStyleCallout, UIFontTextStyle)
SOFT_LINK_CONSTANT_FOR_SOURCE(PAL, UIKit, UITextEffectsBeneathStatusBarWindowLevel, UIWindowLevel)
SOFT_LINK_CLASS_FOR_SOURCE(PAL, UIKit, NSParagraphStyle)
SOFT_LINK_CLASS_FOR_SOURCE(PAL, UIKit, NSShadow)
SOFT_LINK_CLASS_FOR_SOURCE(PAL, UIKit, NSTextList)
SOFT_LINK_CLASS_FOR_SOURCE(PAL, UIKit, UIApplication)
SOFT_LINK_CLASS_FOR_SOURCE_WITH_EXPORT(PAL, UIKit, UIColor, PAL_EXPORT)
SOFT_LINK_CLASS_FOR_SOURCE(PAL, UIKit, UIDevice)
SOFT_LINK_CLASS_FOR_SOURCE(PAL, UIKit, UIDocumentInteractionController)
SOFT_LINK_CLASS_FOR_SOURCE(PAL, UIKit, UIFont)
SOFT_LINK_CLASS_FOR_SOURCE(PAL, UIKit, UIGraphicsImageRenderer)
SOFT_LINK_CLASS_FOR_SOURCE_WITH_EXPORT(PAL, UIKit, UIImage, PAL_EXPORT)
SOFT_LINK_CLASS_FOR_SOURCE(PAL, UIKit, UILabel)
SOFT_LINK_CLASS_FOR_SOURCE(PAL, UIKit, UIPasteboard)
SOFT_LINK_CLASS_FOR_SOURCE(PAL, UIKit, UIScreen)
SOFT_LINK_CLASS_FOR_SOURCE(PAL, UIKit, UITapGestureRecognizer)
SOFT_LINK_CLASS_FOR_SOURCE(PAL, UIKit, UIView)
SOFT_LINK_CLASS_FOR_SOURCE(PAL, UIKit, UIViewController)
SOFT_LINK_CLASS_FOR_SOURCE(PAL, UIKit, UIWindow)
SOFT_LINK_FUNCTION_FOR_SOURCE(PAL, UIKit, _UIKitGetTextEffectsCatalog, CUICatalog *, (void), ())
SOFT_LINK_FUNCTION_FOR_SOURCE(PAL, UIKit, UIAccessibilityIsGrayscaleEnabled, BOOL, (void), ())
SOFT_LINK_FUNCTION_FOR_SOURCE(PAL, UIKit, UIAccessibilityIsInvertColorsEnabled, BOOL, (void), ())
SOFT_LINK_FUNCTION_FOR_SOURCE_WITH_EXPORT(PAL, UIKit, UIAccessibilityIsReduceMotionEnabled, BOOL, (void), (), PAL_EXPORT)
SOFT_LINK_FUNCTION_FOR_SOURCE(PAL, UIKit, UIAccessibilityPostNotification, void, (UIAccessibilityNotifications n, id argument), (n, argument))

#endif
