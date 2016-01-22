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

#ifndef UIScriptController_h
#define UIScriptController_h

#include "JSWrappable.h"
#include <wtf/Ref.h>

namespace WTR {

class UIScriptContext;

class UIScriptController : public JSWrappable {
public:
    static Ref<UIScriptController> create(UIScriptContext& context)
    {
        return adoptRef(*new UIScriptController(context));
    }

    void contextDestroyed();

    void makeWindowObject(JSContextRef, JSObjectRef windowObject, JSValueRef* exception);
    
    void doAsyncTask(JSValueRef callback);
    void zoomToScale(double scale, JSValueRef callback);

    void singleTapAtPoint(long x, long y, JSValueRef callback);
    void doubleTapAtPoint(long x, long y, JSValueRef callback);
    
    void typeCharacterUsingHardwareKeyboard(JSStringRef character, JSValueRef callback);
    void keyDownUsingHardwareKeyboard(JSStringRef character, JSValueRef callback);
    void keyUpUsingHardwareKeyboard(JSStringRef character, JSValueRef callback);

    void setWillBeginZoomingCallback(JSValueRef);
    JSValueRef willBeginZoomingCallback() const;

    void setDidEndZoomingCallback(JSValueRef);
    JSValueRef didEndZoomingCallback() const;

    void setDidShowKeyboardCallback(JSValueRef);
    JSValueRef didShowKeyboardCallback() const;

    void setDidHideKeyboardCallback(JSValueRef);
    JSValueRef didHideKeyboardCallback() const;

    void setDidEndScrollingCallback(JSValueRef);
    JSValueRef didEndScrollingCallback() const;

    double zoomScale() const;
    double minimumZoomScale() const;
    double maximumZoomScale() const;

    JSObjectRef contentVisibleRect() const;

    void uiScriptComplete(JSStringRef result);

private:
    UIScriptController(UIScriptContext&);
    
    void platformSetWillBeginZoomingCallback();
    void platformSetDidEndZoomingCallback();
    void platformSetDidShowKeyboardCallback();
    void platformSetDidHideKeyboardCallback();
    void platformSetDidEndScrollingCallback();
    void platformClearAllCallbacks();

    virtual JSClassRef wrapperClass() override;

    JSObjectRef objectFromRect(const WKRect&) const;

    UIScriptContext* m_context;
};

}

#endif // UIScriptController_h
