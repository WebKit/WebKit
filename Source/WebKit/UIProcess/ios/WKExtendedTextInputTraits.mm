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
}

- (instancetype)init
{
    if (!(self = [super init]))
        return nil;

#if USE(BROWSERENGINEKIT)
    self.typingAdaptationEnabled = YES;
#endif
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

#if USE(BROWSERENGINEKIT)

- (void)setSelectionHandleColor:(UIColor *)color
{
    _selectionHandleColor = color;
}

- (UIColor *)selectionHandleColor
{
    return _selectionHandleColor.get();
}

#else

- (void)setSelectionBarColor:(UIColor *)color
{
    _selectionHandleColor = color;
}

- (UIColor *)selectionBarColor
{
    return _selectionHandleColor.get();
}

#endif

- (void)setSelectionHighlightColor:(UIColor *)color
{
    _selectionHighlightColor = color;
}

- (UIColor *)selectionHighlightColor
{
    return _selectionHighlightColor.get();
}

- (void)setSelectionColorsToMatchTintColor:(UIColor *)tintColor
{
    static constexpr auto selectionHighlightAlphaComponent = 0.2;
    BOOL shouldUseTintColor = tintColor && tintColor != UIColor.systemBlueColor;
    self.insertionPointColor = shouldUseTintColor ? tintColor : nil;
#if USE(BROWSERENGINEKIT)
    self.selectionHandleColor = shouldUseTintColor ? tintColor : nil;
#else
    self.selectionBarColor = shouldUseTintColor ? tintColor : nil;
#endif
    self.selectionHighlightColor = shouldUseTintColor ? [tintColor colorWithAlphaComponent:selectionHighlightAlphaComponent] : nil;
}

@end

#endif // PLATFORM(IOS_FAMILY)
