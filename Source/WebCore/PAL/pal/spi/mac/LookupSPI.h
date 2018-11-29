/*
 * Copyright (C) 2014 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */


#if PLATFORM(MAC)

#import <objc/runtime.h>
#import <pal/spi/mac/NSImmediateActionGestureRecognizerSPI.h>
#import <wtf/SoftLinking.h>

SOFT_LINK_PRIVATE_FRAMEWORK_OPTIONAL(Lookup)
SOFT_LINK_CLASS_OPTIONAL(Lookup, LULookupDefinitionModule)

#if USE(APPLE_INTERNAL_SDK)

#import <Lookup/Lookup.h>

#else

@interface LULookupDefinitionModule : NSObject

+ (NSRange)tokenRangeForString:(NSString *)string range:(NSRange)range options:(NSDictionary **)options;
+ (void)showDefinitionForTerm:(NSAttributedString *)term atLocation:(NSPoint)screenPoint options:(NSDictionary *)options;
+ (void)showDefinitionForTerm:(NSAttributedString *)term relativeToRect:(NSRect)positioningRect ofView:(NSView *)positioningView options:(NSDictionary *)options;
+ (void)hideDefinition;
+ (id<NSImmediateActionAnimationController>)lookupAnimationControllerForTerm:(NSAttributedString *)term atLocation:(NSPoint)screenPoint options:(NSDictionary *)options;
+ (id<NSImmediateActionAnimationController>)lookupAnimationControllerForTerm:(NSAttributedString *)term relativeToRect:(NSRect)positioningRect ofView:(NSView *)positioningView options:(NSDictionary *)options;

@end

#endif // !USE(APPLE_INTERNAL_SDK)

#endif // PLATFORM(MAC)
