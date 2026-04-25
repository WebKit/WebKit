/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS_FAMILY)

#import "UIKitSPI.h"
#import "WKBrowserEngineDefinitions.h"
#import <pal/spi/ios/BrowserEngineKitSPI.h>

@interface WKExtendedTextInputTraits : NSObject
#if USE(BROWSERENGINEKIT)
    <BEExtendedTextInputTraits>
#endif

@property (nonatomic) UITextAutocapitalizationType autocapitalizationType;
@property (nonatomic) UITextAutocorrectionType autocorrectionType;
@property (nonatomic) UITextSpellCheckingType spellCheckingType;
@property (nonatomic) UITextSmartQuotesType smartQuotesType;
@property (nonatomic) UITextSmartDashesType smartDashesType;
#if HAVE(INLINE_PREDICTIONS)
@property (nonatomic) UITextInlinePredictionType inlinePredictionType;
#endif
@property (nonatomic) UIKeyboardType keyboardType;
@property (nonatomic) UIKeyboardAppearance keyboardAppearance;
@property (nonatomic) UIReturnKeyType returnKeyType;
@property (nonatomic, getter=isSecureTextEntry) BOOL secureTextEntry;
@property (nonatomic, getter=isSingleLineDocument) BOOL singleLineDocument;
@property (nonatomic, getter=isTypingAdaptationEnabled) BOOL typingAdaptationEnabled;
@property (nonatomic, copy) UITextContentType textContentType;
@property (nonatomic, copy) UITextInputPasswordRules *passwordRules;
@property (nonatomic) UITextSmartInsertDeleteType smartInsertDeleteType;
@property (nonatomic) BOOL enablesReturnKeyAutomatically;

@property (nonatomic, strong) UIColor *insertionPointColor;
@property (nonatomic, strong) UIColor *selectionHandleColor;
@property (nonatomic, strong) UIColor *selectionHighlightColor;

#if HAVE(UI_CONVERSATION_CONTEXT)
@property (nonatomic, strong) UIConversationContext *conversationContext;
#endif

@property (nonatomic) BOOL allowsNumberPadPopover;

- (void)setSelectionColorsToMatchTintColor:(UIColor *)tintColor;
- (void)restoreDefaultValues;

@end

#endif // PLATFORM(IOS_FAMILY)
