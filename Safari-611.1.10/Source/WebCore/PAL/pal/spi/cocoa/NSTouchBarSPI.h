/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#if PLATFORM(MAC) && HAVE(TOUCH_BAR)

#if USE(APPLE_INTERNAL_SDK)

#import <AppKit/NSCandidateListTouchBarItem_Private.h>
#import <AppKit/NSTextTouchBarItemController_WebKitSPI.h>
#import <AppKit/NSTouchBar_Private.h>

#endif

NS_ASSUME_NONNULL_BEGIN

#if !USE(APPLE_INTERNAL_SDK)

@interface NSTouchBar ()
@property (readonly, copy, nullable) NSArray<NSTouchBarItem *> *items;
@property (strong, nullable) NSTouchBarItem *escapeKeyReplacementItem;
@end

@interface NSTextTouchBarItemController : NSObject

@property (readonly, strong, nullable) NSColorPickerTouchBarItem *colorPickerItem;
@property (readonly, strong, nullable) NSSegmentedControl *textStyle;
@property (readonly, strong, nullable) NSSegmentedControl *textAlignments;
@property (nullable, strong) NSViewController *textListViewController;
@property BOOL usesNarrowTextStyleItem;

- (nullable NSTouchBarItem *)itemForIdentifier:(nullable NSString *)identifier;

@end

@interface NSCandidateListTouchBarItem ()
- (void)setCandidates:(NSArray *)candidates forSelectedRange:(NSRange)selectedRange inString:(nullable NSString *)string rect:(NSRect)rect view:(nullable NSView *)view completionHandler:(nullable void (^)(id acceptedCandidate))completionBlock;
@end

#endif // !USE(APPLE_INTERNAL_SDK)

APPKIT_EXTERN NSNotificationName const NSTouchBarWillEnterCustomization API_AVAILABLE(macos(10.12.2));
APPKIT_EXTERN NSNotificationName const NSTouchBarDidEnterCustomization API_AVAILABLE(macos(10.12.2));
APPKIT_EXTERN NSNotificationName const NSTouchBarWillExitCustomization API_AVAILABLE(macos(10.12.2));
APPKIT_EXTERN NSNotificationName const NSTouchBarDidExitCustomization API_AVAILABLE(macos(10.12.2));

NS_ASSUME_NONNULL_END

#endif // PLATFORM(MAC) && HAVE(TOUCH_BAR)
