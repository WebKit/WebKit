/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#import <PrototypeTools/PrototypeTools.h>

#else

@interface PTSettings : NSObject <NSCopying>
- (void)setDefaultValues;
@end

@interface PTDomain : NSObject
+ (__kindof PTSettings *)rootSettings;
@end

@interface PTSection : NSObject
@end

@interface PTModule : NSObject
+ (instancetype)moduleWithTitle:(NSString *)title contents:(NSArray *)contents;
+ (PTSection *)sectionWithRows:(NSArray *)rows title:(NSString *)title;
+ (PTSection *)sectionWithRows:(NSArray *)rows;
@end

@interface PTRow : NSObject <NSCopying, NSSecureCoding>
+ (instancetype)rowWithTitle:(NSString *)staticTitle valueKeyPath:(NSString *)keyPath;
- (id)valueValidator:(id(^)(id proposedValue, id settings))validator;
- (id)condition:(NSPredicate *)condition;
@end

@interface PTSRow : PTRow
@end

@interface PTSwitchRow : PTSRow
@end

@interface PTSliderRow : PTSRow
- (id)minValue:(CGFloat)minValue maxValue:(CGFloat)maxValue;
@end

@interface PTEditFloatRow : PTSRow
@end

@interface PTRowAction : NSObject
@end

@interface PTRestoreDefaultSettingsRowAction : PTRowAction
+ (instancetype)action;
@end

@interface PTButtonRow : PTSRow
+ (instancetype)rowWithTitle:(NSString *)staticTitle action:(PTRowAction *)action;
@end

#endif
