/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#import "UIScriptControllerCocoa.h"

OBJC_CLASS WKWebView;

namespace WTR {

class UIScriptControllerMac : public UIScriptControllerCocoa {
public:
    explicit UIScriptControllerMac(UIScriptContext& context)
        : UIScriptControllerCocoa(context)
    {
    }

    void replaceTextAtRange(JSStringRef, int, int) override;
    void zoomToScale(double, JSValueRef) override;
    double zoomScale() const override;
    void simulateAccessibilitySettingsChangeNotification(JSValueRef) override;
    bool isShowingDateTimePicker() const override;
    double dateTimePickerValue() const override;
    bool isShowingDataListSuggestions() const override;
    void activateDataListSuggestion(unsigned index, JSValueRef callback) override;
    void beginBackSwipe(JSValueRef) override;
    void completeBackSwipe(JSValueRef) override;
    void playBackEventStream(JSStringRef, JSValueRef) override;
    void firstResponderSuppressionForWebView(bool) override;
    void makeWindowContentViewFirstResponder() override;
    bool isWindowContentViewFirstResponder() const override;
    void toggleCapsLock(JSValueRef) override;
    NSView *platformContentView() const override;
    void clearAllCallbacks() override;
    void copyText(JSStringRef) override;
    void setSpellCheckerResults(JSValueRef) override;

    void chooseMenuAction(JSStringRef, JSValueRef) override;

    void activateAtPoint(long x, long y, JSValueRef callback) override;

private:
    NSTableView *dataListSuggestionsTableView() const;
};

} // namespace WTR
