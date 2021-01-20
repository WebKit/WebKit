/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#if PLATFORM(WATCHOS)

#import <UIKit/UIKit.h>

typedef NS_ENUM(NSInteger, WKNumberPadButtonMode) {
    WKNumberPadButtonModeDefault,
    WKNumberPadButtonModeAlternate
};

typedef NS_ENUM(NSInteger, WKNumberPadButtonPosition) {
    WKNumberPadButtonPositionBottomLeft = -2,
    WKNumberPadButtonPositionBottomRight = -1,
    WKNumberPadButtonPosition0 = 0,
    WKNumberPadButtonPosition1 = 1,
    WKNumberPadButtonPosition2 = 2,
    WKNumberPadButtonPosition3 = 3,
    WKNumberPadButtonPosition4 = 4,
    WKNumberPadButtonPosition5 = 5,
    WKNumberPadButtonPosition6 = 6,
    WKNumberPadButtonPosition7 = 7,
    WKNumberPadButtonPosition8 = 8,
    WKNumberPadButtonPosition9 = 9
};

typedef NS_ENUM(NSInteger, WKNumberPadKey) {
    WKNumberPadKeyDash = -9,
    WKNumberPadKeyAsterisk = -8,
    WKNumberPadKeyOctothorpe = -7,
    WKNumberPadKeyClosingParenthesis = -6,
    WKNumberPadKeyOpeningParenthesis = -5,
    WKNumberPadKeyPlus = -4,
    WKNumberPadKeyAccept = -3,
    WKNumberPadKeyToggleMode = -2,
    WKNumberPadKeyNone = -1,
    WKNumberPadKey0 = WKNumberPadButtonPosition0,
    WKNumberPadKey1 = WKNumberPadButtonPosition1,
    WKNumberPadKey2 = WKNumberPadButtonPosition2,
    WKNumberPadKey3 = WKNumberPadButtonPosition3,
    WKNumberPadKey4 = WKNumberPadButtonPosition4,
    WKNumberPadKey5 = WKNumberPadButtonPosition5,
    WKNumberPadKey6 = WKNumberPadButtonPosition6,
    WKNumberPadKey7 = WKNumberPadButtonPosition7,
    WKNumberPadKey8 = WKNumberPadButtonPosition8,
    WKNumberPadKey9 = WKNumberPadButtonPosition9
};

@class WKNumberPadViewController;

@interface WKNumberPadView : UIView
- (instancetype)initWithFrame:(CGRect)frame controller:(WKNumberPadViewController *)controller NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder *)decoder NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;
@end

#endif
