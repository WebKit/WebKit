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

#if PLATFORM(WATCHOS)

#import <UIKit/UIKit.h>

#if USE(APPLE_INTERNAL_SDK)

#import <ClockKitUI/CLKUIWheelsOfTimeView.h>

#else // USE(APPLE_INTERNAL_SDK)

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSUInteger, CLKUIWheelsOfTimeStyle) {
    CLKUIWheelsOfTimeStyleTimer,
    CLKUIWheelsOfTimeStyleAlarm12,
    CLKUIWheelsOfTimeStyleOffset,
    CLKUIWheelsOfTimeStyleAlarm24,
    CLKUIWheelsOfTimeStyleDuration,
};

@protocol CLKUIWheelsOfTimeDelegate <NSObject>
@end

@interface CLKUIWheelsOfTimeView : UIView
@property (nonatomic, weak) id <CLKUIWheelsOfTimeDelegate> delegate;
@property (assign, readonly, nonatomic) NSInteger hour;
@property (assign, readonly, nonatomic) NSInteger minute;
- (instancetype)initWithFrame:(CGRect)frame style:(CLKUIWheelsOfTimeStyle)style;
- (void)setHour:(NSInteger)hour andMinute:(NSInteger)minute;
@end

NS_ASSUME_NONNULL_END

#endif // USE(APPLE_INTERNAL_SDK)

#endif // PLATFORM(WATCHOS)
