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

#import "config.h"
#import "WKExtendedTextInputTraits.h"

#if PLATFORM(IOS_FAMILY)

#import <wtf/RetainPtr.h>

@implementation WKExtendedTextInputTraits {
    RetainPtr<UITextContentType> _textContentType;
    RetainPtr<UIColor> _insertionPointColor;
    RetainPtr<UIColor> _selectionHandleColor;
    RetainPtr<UIColor> _selectionHighlightColor;
    RetainPtr<UITextInputPasswordRules> _passwordRules;
#if HAVE(UI_CONVERSATION_CONTEXT)
    RetainPtr<UIConversationContext> _conversationContext;
#endif
}

- (instancetype)init
{
    if (!(self = [super init]))
        return nil;

    self.allowsNumberPadPopover = NO;
    self.typingAdaptationEnabled = YES;
    self.autocapitalizationType = UITextAutocapitalizationTypeSentences;
    return self;
}

- (void)setPasswordRules:(UITextInputPasswordRules *)rules
{
    _passwordRules = adoptNS(rules.copy);
}

- (UITextInputPasswordRules *)passwordRules
{
    return adoptNS([_passwordRules copy]).autorelease();
}

- (void)setTextContentType:(UITextContentType)type
{
    _textContentType = adoptNS(type.copy);
}

- (UITextContentType)textContentType
{
    return adoptNS([_textContentType copy]).autorelease();
}

- (void)setInsertionPointColor:(UIColor *)color
{
    _insertionPointColor = color;
}

- (UIColor *)insertionPointColor
{
    return _insertionPointColor.get();
}

- (void)setSelectionHandleColor:(UIColor *)color
{
    _selectionHandleColor = color;
}

- (UIColor *)selectionHandleColor
{
    return _selectionHandleColor.get();
}

- (void)setSelectionHighlightColor:(UIColor *)color
{
    _selectionHighlightColor = color;
}

- (UIColor *)selectionHighlightColor
{
    return _selectionHighlightColor.get();
}

#if HAVE(UI_CONVERSATION_CONTEXT)

- (UIConversationContext *)conversationContext
{
    return _conversationContext.get();
}

- (void)setConversationContext:(UIConversationContext *)context
{
    _conversationContext = context;
}

#endif // HAVE(UI_CONVERSATION_CONTEXT)

- (void)setSelectionColorsToMatchTintColor:(UIColor *)tintColor
{
    static constexpr auto selectionHighlightAlphaComponent = 0.2;
    BOOL shouldUseTintColor = tintColor && tintColor != UIColor.systemBlueColor;
    self.insertionPointColor = shouldUseTintColor ? tintColor : nil;
    self.selectionHandleColor = shouldUseTintColor ? tintColor : nil;
    self.selectionHighlightColor = shouldUseTintColor ? [tintColor colorWithAlphaComponent:selectionHighlightAlphaComponent] : nil;
}

- (void)restoreDefaultValues
{
    self.typingAdaptationEnabled = YES;
#if HAVE(INLINE_PREDICTIONS)
    self.inlinePredictionType = UITextInlinePredictionTypeDefault;
#endif
    self.autocapitalizationType = UITextAutocapitalizationTypeSentences;
    self.autocorrectionType = UITextAutocorrectionTypeDefault;
    self.spellCheckingType = UITextSpellCheckingTypeDefault;
    self.smartQuotesType = UITextSmartQuotesTypeDefault;
    self.smartDashesType = UITextSmartDashesTypeDefault;
    self.keyboardType = UIKeyboardTypeDefault;
    self.keyboardAppearance = UIKeyboardAppearanceDefault;
    self.allowsNumberPadPopover = NO;
    self.returnKeyType = UIReturnKeyDefault;
    self.secureTextEntry = NO;
    self.singleLineDocument = NO;
    self.textContentType = nil;
    self.passwordRules = nil;
    self.smartInsertDeleteType = UITextSmartInsertDeleteTypeDefault;
    self.enablesReturnKeyAutomatically = NO;
    self.insertionPointColor = nil;
    self.selectionHandleColor = nil;
    self.selectionHighlightColor = nil;
}

@end

#endif // PLATFORM(IOS_FAMILY)
