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

#import "UIScriptControllerCommon.h"

OBJC_CLASS TestRunnerWKWebView;

namespace WTR {

class UIScriptControllerCocoa : public UIScriptControllerCommon {
protected:
    explicit UIScriptControllerCocoa(UIScriptContext&);
    TestRunnerWKWebView *webView() const;

    void doAsyncTask(JSValueRef) override;

private:
    void setViewScale(double) override;
    void setMinimumEffectiveWidth(double) override;
    void becomeFirstResponder() override;
    void resignFirstResponder() override;
    void removeViewFromWindow(JSValueRef) override;
    void addViewToWindow(JSValueRef) override;
    void overridePreference(JSStringRef, JSStringRef) override;
    void findString(JSStringRef, unsigned long, unsigned long) override;
    JSObjectRef contentsOfUserInterfaceItem(JSStringRef) const override;
    void setDefaultCalendarType(JSStringRef calendarIdentifier, JSStringRef localeIdentifier) override;
    JSRetainPtr<JSStringRef> lastUndoLabel() const override;
    JSRetainPtr<JSStringRef> firstRedoLabel() const override;
    NSUndoManager *platformUndoManager() const override;

    void setDidShowMenuCallback(JSValueRef) override;
    void setDidHideMenuCallback(JSValueRef) override;
    void dismissMenu() override;
    bool isShowingMenu() const override;

    void paste() override;

    void setContinuousSpellCheckingEnabled(bool) override;

    void insertAttachmentForFilePath(JSStringRef filePath, JSStringRef contentType, JSValueRef callback) override;

    void completeTaskAsynchronouslyAfterActivityStateUpdate(unsigned callbackID);
};

} // namespace WTR
