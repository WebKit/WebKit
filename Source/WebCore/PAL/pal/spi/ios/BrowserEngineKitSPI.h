/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#if !PLATFORM(IOS_SIMULATOR) || !__has_feature(modules)

DECLARE_SYSTEM_HEADER

#if USE(BROWSERENGINEKIT)

#import <BrowserEngineKit/BrowserEngineKit.h>

#if USE(APPLE_INTERNAL_SDK)
// Note: SPI usage should be limited to testing purposes and binary compatibility with clients
// of existing WebKit SPI.
#import <BrowserEngineKit/BrowserEngineKit_Private.h>
#else

@class NSTextAlternatives;
@class UIKeyEvent;
@class UITextSuggestion;
@class UIWKDocumentContext;

@interface BEKeyEntry (ForTesting)
- (UIKeyEvent *)_uikitKeyEvent;
- (instancetype)_initWithUIKitKeyEvent:(UIKeyEvent *)keyEvent;
@end

@interface BETextAlternatives ()
@property (readonly) BOOL isLowConfidence;
- (NSTextAlternatives *)_nsTextAlternative;
- (instancetype)_initWithNSTextAlternatives:(NSTextAlternatives *)nsTextAlternatives;
@end

@interface BETextDocumentContext ()
@property (strong, nonatomic, readonly) UIWKDocumentContext *_uikitDocumentContext;
@property (nonatomic, copy) NSAttributedString *annotatedText;
@end

@interface BETextDocumentRequest ()
@property (nonatomic, assign) CGRect _documentRect;
@end

@interface BETextSuggestion ()
@property (nonatomic, readonly, strong) UITextSuggestion *_uikitTextSuggestion;
- (instancetype)_initWithUIKitTextSuggestion:(UITextSuggestion *)suggestion;
@end

#endif

#endif // USE(BROWSERENGINEKIT)

#endif // !PLATFORM(IOS_SIMULATOR) || !__has_feature(modules)
