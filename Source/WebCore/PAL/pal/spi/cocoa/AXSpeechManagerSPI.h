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

// FIXME: Remove the `__has_feature(modules)` condition when possible.
#if !__has_feature(modules)

DECLARE_SYSTEM_HEADER

#if PLATFORM(COCOA)

// FIXME: (rdar://167375794) Remove the `__has_feature(modules)` condition when possible.
#if USE(APPLE_INTERNAL_SDK) && !__has_feature(modules)

#include <AXSpeechManager.h>

#else

#include <AVFoundation/AVFoundation.h>

#endif // USE(APPLE_INTERNAL_SDK)

typedef void (^AVSpeechSynthesisVoiceCallbackBlock)(NSArray<AVSpeechSynthesisVoice *> *);

@interface AVSpeechSynthesisVoice (PrivateAttributes)
@property (nonatomic, readonly) BOOL isSystemVoice;
+ (nonnull NSArray<AVSpeechSynthesisVoice *> *)speechVoicesIncludingSuperCompact;
+ (void)speechVoicesIncludingSuperCompactWithCompletionHandler:(nonnull AVSpeechSynthesisVoiceCallbackBlock)completion;
@end

#endif // PLATFORM(COCOA)

#endif // !__has_feature(modules)
