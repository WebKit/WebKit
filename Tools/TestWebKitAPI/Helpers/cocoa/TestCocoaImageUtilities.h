/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#ifdef __OBJC__

#import <CoreGraphics/CoreGraphics.h>
#import <wtf/Platform.h>

#if PLATFORM(IOS_FAMILY)
#import <UIKit/UIKit.h>
#else
#import <AppKit/AppKit.h>
#endif

NS_HEADER_AUDIT_BEGIN(nullability, sendability)

NS_SWIFT_UI_ACTOR
@interface TestCocoaImageUtilities : NSObject

+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

+ (void)performWithDarkAppearance:(BOOL)darkAppearance block:(void (NS_NOESCAPE ^)(void))block NS_SWIFT_NAME(perform(darkAppearance:block:));

#if PLATFORM(IOS_FAMILY)
+ (NSData *)pngDataWithSize:(CGSize)size color:(UIColor *)color NS_SWIFT_NAME(pngData(size:color:));
+ (nullable UIColor *)pixelColorOfImage:(UIImage *)image atPoint:(CGPoint)point NS_SWIFT_NAME(pixelColor(of:at:));
+ (BOOL)compareColor:(nullable UIColor *)color toColor:(nullable UIColor *)otherColor tolerance:(CGFloat)tolerance NS_SWIFT_NAME(compareColors(_:_:tolerance:));
#else
+ (NSData *)pngDataWithSize:(CGSize)size color:(NSColor *)color NS_SWIFT_NAME(pngData(size:color:));
+ (nullable NSColor *)pixelColorOfImage:(NSImage *)image atPoint:(CGPoint)point NS_SWIFT_NAME(pixelColor(of:at:));
+ (BOOL)compareColor:(nullable NSColor *)color toColor:(nullable NSColor *)otherColor tolerance:(CGFloat)tolerance NS_SWIFT_NAME(compareColors(_:_:tolerance:));
#endif

@end

NS_HEADER_AUDIT_END(nullability, sendability)

#endif // __OBJC__
