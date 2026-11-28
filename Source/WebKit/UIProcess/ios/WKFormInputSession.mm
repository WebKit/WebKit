/*
 * Copyright (C) 2012-2026 Apple Inc. All rights reserved.
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
#import "WKFormInputSession.h"

#if PLATFORM(IOS_FAMILY)

#import "APIFrameInfo.h"
#import "FocusedElementInformation.h"
#import "FrameInfoData.h"
#import "WKContentViewInteraction.h"
#import "WKFrameInfoInternal.h"
#import <pal/system/ios/UserInterfaceIdiom.h>
#import <wtf/WeakObjCPtr.h>

@implementation WKFormInputSession {
    WeakObjCPtr<WKContentView> _contentView;
    RetainPtr<WKFocusedElementInfo> _focusedElementInfo;
    RetainPtr<UIView> _customInputView;
    RetainPtr<UIView> _customInputAccessoryView;
    RetainPtr<NSArray<UITextSuggestion *>> _suggestions;
    BOOL _accessoryViewShouldNotShow;
    BOOL _forceSecureTextEntry;
    BOOL _requiresStrongPasswordAssistance;
}

- (instancetype)initWithContentView:(WKContentView *)view focusedElementInfo:(WKFocusedElementInfo *)elementInfo requiresStrongPasswordAssistance:(BOOL)requiresStrongPasswordAssistance
{
    if (!(self = [super init]))
        return nil;

    _contentView = view;
    _focusedElementInfo = elementInfo;
    _requiresStrongPasswordAssistance = requiresStrongPasswordAssistance;

    return self;
}

- (id<_WKFocusedElementInfo>)focusedElementInfo
{
    return _focusedElementInfo.get();
}

- (NSObject <NSSecureCoding> *)userObject
{
    return [_focusedElementInfo userObject];
}

- (BOOL)isValid
{
    return !!_contentView;
}

- (NSString *)accessoryViewCustomButtonTitle
{
    return [_contentView.get() formAccessoryView].autoFillButtonItem.title;
}

- (void)setAccessoryViewCustomButtonTitle:(NSString *)title
{
    if (title.length)
        [[_contentView.get() formAccessoryView] showAutoFillButtonWithTitle:title];
    else
        [[_contentView.get() formAccessoryView] hideAutoFillButton];
    if (!PAL::currentUserInterfaceIdiomIsSmallScreen())
        [_contentView.get() reloadInputViews];
}

- (BOOL)accessoryViewShouldNotShow
{
    return _accessoryViewShouldNotShow;
}

- (void)setAccessoryViewShouldNotShow:(BOOL)accessoryViewShouldNotShow
{
    if (_accessoryViewShouldNotShow == accessoryViewShouldNotShow)
        return;

    _accessoryViewShouldNotShow = accessoryViewShouldNotShow;
    [_contentView.get() reloadInputViews];
}

- (BOOL)forceSecureTextEntry
{
    return _forceSecureTextEntry;
}

- (void)setForceSecureTextEntry:(BOOL)forceSecureTextEntry
{
    if (_forceSecureTextEntry == forceSecureTextEntry)
        return;

    _forceSecureTextEntry = forceSecureTextEntry;
    [_contentView.get() reloadInputViews];
}

- (UIView *)customInputView
{
    return _customInputView.get();
}

- (void)setCustomInputView:(UIView *)customInputView
{
    if (customInputView == _customInputView)
        return;

    _customInputView = customInputView;
    [_contentView.get() reloadInputViews];
}

- (UIView *)customInputAccessoryView
{
    return _customInputAccessoryView.get();
}

- (void)setCustomInputAccessoryView:(UIView *)customInputAccessoryView
{
    if (_customInputAccessoryView == customInputAccessoryView)
        return;

    _customInputAccessoryView = customInputAccessoryView;
    [_contentView.get() reloadInputViews];
}

- (void)endEditing
{
    if ([_customInputView conformsToProtocol:@protocol(WKFormControl)])
        [(id<WKFormControl>)_customInputView.get() controlEndEditing];
}

- (NSArray<UITextSuggestion *> *)suggestions
{
    return _suggestions.get();
}

- (void)setSuggestions:(NSArray<UITextSuggestion *> *)suggestions
{
    if (suggestions == _suggestions || [suggestions isEqualToArray:_suggestions.get()])
        return;

    _suggestions = adoptNS([suggestions copy]);
    [_contentView.get() updateTextSuggestionsForInputDelegate];
}

- (BOOL)requiresStrongPasswordAssistance
{
    return _requiresStrongPasswordAssistance;
}

- (void)invalidate
{
    RetainPtr<WKContentView> contentView = _contentView.get();
    _contentView = nil;
    [contentView _provideSuggestionsToInputDelegate:nil];
}

- (void)reloadFocusedElementContextView
{
    [_contentView.get() reloadContextViewForPresentedListViewController];
}

@end

@implementation WKFocusedElementInfo {
    WKInputType _type;
    RetainPtr<NSString> _value;
    BOOL _isUserInitiated;
    RetainPtr<NSObject <NSSecureCoding>> _userObject;
    RetainPtr<NSString> _placeholder;
    RetainPtr<NSString> _label;
    RetainPtr<WKFrameInfo> _frame;
}

- (instancetype)initWithFocusedElementInformation:(const WebKit::FocusedElementInformation&)information isUserInitiated:(BOOL)isUserInitiated webView:(WKWebView *)webView userObject:(NSObject <NSSecureCoding> *)userObject
{
    if (!(self = [super init]))
        return nil;

    switch (information.elementType) {
    case WebKit::InputType::ContentEditable:
        _type = WKInputTypeContentEditable;
        break;
    case WebKit::InputType::Text:
        _type = WKInputTypeText;
        break;
    case WebKit::InputType::Password:
        _type = WKInputTypePassword;
        break;
    case WebKit::InputType::TextArea:
        _type = WKInputTypeTextArea;
        break;
    case WebKit::InputType::Search:
        _type = WKInputTypeSearch;
        break;
    case WebKit::InputType::Email:
        _type = WKInputTypeEmail;
        break;
    case WebKit::InputType::URL:
        _type = WKInputTypeURL;
        break;
    case WebKit::InputType::Phone:
        _type = WKInputTypePhone;
        break;
    case WebKit::InputType::Number:
        _type = WKInputTypeNumber;
        break;
    case WebKit::InputType::NumberPad:
        _type = WKInputTypeNumberPad;
        break;
    case WebKit::InputType::Date:
        _type = WKInputTypeDate;
        break;
    case WebKit::InputType::DateTimeLocal:
        _type = WKInputTypeDateTimeLocal;
        break;
    case WebKit::InputType::Month:
        _type = WKInputTypeMonth;
        break;
    case WebKit::InputType::Week:
        _type = WKInputTypeWeek;
        break;
    case WebKit::InputType::Time:
        _type = WKInputTypeTime;
        break;
    case WebKit::InputType::Select:
        _type = WKInputTypeSelect;
        break;
    case WebKit::InputType::Drawing:
        _type = WKInputTypeDrawing;
        break;
    case WebKit::InputType::Color:
        _type = WKInputTypeColor;
        break;
    case WebKit::InputType::None:
        _type = WKInputTypeNone;
        break;
    }
    _value = information.value.createNSString().get();
    _isUserInitiated = isUserInitiated;
    _userObject = userObject;
    _placeholder = information.placeholder.createNSString().get();
    _label = information.label.createNSString().get();
    if (information.frame)
        _frame = wrapper(API::FrameInfo::create(WebKit::FrameInfoData { *information.frame }));
    return self;
}

- (WKInputType)type
{
    return _type;
}

- (NSString *)value
{
    return _value.get();
}

- (BOOL)isUserInitiated
{
    return _isUserInitiated;
}

- (NSObject <NSSecureCoding> *)userObject
{
    return _userObject.get();
}

- (NSString *)label
{
    return _label.get();
}

- (NSString *)placeholder
{
    return _placeholder.get();
}

- (WKFrameInfo *)frame
{
    return _frame.get();
}

@end

#endif // PLATFORM(IOS_FAMILY)
