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

#pragma once

DECLARE_SYSTEM_HEADER

#if PLATFORM(MAC)

#if USE(APPLE_INTERNAL_SDK)

#import <AppKit/NSTextChecker.h>

#else

@class NSColor;

extern NSString *NSTextCheckingInsertionPointKey;
extern NSString *NSTextCheckingSuppressInitialCapitalizationKey;
#if HAVE(INLINE_PREDICTIONS)
extern NSString *NSTextCompletionAttributeName;
#endif

@interface NSSpellChecker ()

#if HAVE(INLINE_PREDICTIONS)
@property (class, readonly, getter=isAutomaticInlineCompletionEnabled) BOOL automaticInlineCompletionEnabled;
- (NSTextCheckingResult *)completionCandidateFromCandidates:(NSArray<NSTextCheckingResult *> *)candidates;
- (void)showCompletionForCandidate:(NSTextCheckingResult *)candidate selectedRange:(NSRange)selectedRange offset:(NSUInteger)offset inString:(NSString *)string rect:(NSRect)rect view:(NSView *)view completionHandler:(void (^)(NSDictionary *resultDictionary))completionBlock;
- (void)showCompletionForCandidate:(NSTextCheckingResult *)candidate selectedRange:(NSRange)selectedRange offset:(NSUInteger)offset inString:(NSString *)string rect:(NSRect)rect view:(NSView *)view client:(id <NSTextInputClient>)client completionHandler:(void (^)(NSDictionary *resultDictionary))completionBlock;
#endif

- (NSString *)languageForWordRange:(NSRange)range inString:(NSString *)string orthography:(NSOrthography *)orthography;
- (BOOL)deletesAutospaceBeforeString:(NSString *)string language:(NSString *)language;
- (void)_preflightChosenSpellServer;

+ (BOOL)grammarCheckingEnabled;

#if HAVE(AUTOCORRECTION_ENHANCEMENTS)
+ (NSColor *)correctionIndicatorUnderlineColor;
#endif

@end

#if HAVE(INLINE_PREDICTIONS)
typedef NS_OPTIONS(uint64_t, NSTextCheckingTypeAppKitTemporary) {
#if HAVE(NS_TEXT_CHECKING_TYPE_MATH_COMPLETION)
    _NSTextCheckingTypeMathCompletion   = 1ULL << 28,
#endif
    _NSTextCheckingTypeSingleCompletion = 1ULL << 29,
};
#endif



#endif // USE(APPLE_INTERNAL_SDK)

#endif // PLATFORM(MAC)
