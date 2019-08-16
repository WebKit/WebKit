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

#pragma once

#ifndef UIScriptContext_h
#define UIScriptContext_h

#include <JavaScriptCore/JSRetainPtr.h>
#include <wtf/HashMap.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
class FloatRect;
}

namespace WTR {

class UIScriptController;

class UIScriptContextDelegate {
public:
    virtual void uiScriptDidComplete(const String& result, unsigned callbackID) = 0;
};

const unsigned firstNonPersistentCallbackID = 1000;

typedef enum  {
    CallbackTypeInvalid = 0,
    CallbackTypeWillBeginZooming,
    CallbackTypeDidEndZooming,
    CallbackTypeDidShowKeyboard,
    CallbackTypeDidHideKeyboard,
    CallbackTypeDidShowMenu,
    CallbackTypeDidHideMenu,
    CallbackTypeWillPresentPopover,
    CallbackTypeDidDismissPopover,
    CallbackTypeDidEndScrolling,
    CallbackTypeDidStartFormControlInteraction,
    CallbackTypeDidEndFormControlInteraction,
    CallbackTypeDidShowForcePressPreview,
    CallbackTypeDidDismissForcePressPreview,
    CallbackTypeWillCreateNewPage,
    CallbackTypeNonPersistent = firstNonPersistentCallbackID
} CallbackType;

class UIScriptContext {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(UIScriptContext);
public:
    UIScriptContext(UIScriptContextDelegate&);
    ~UIScriptContext();

    void runUIScript(const String& script, unsigned scriptCallbackID);
    void requestUIScriptCompletion(JSStringRef);

    // For one-shot tasks callbacks.
    unsigned prepareForAsyncTask(JSValueRef taskCallback, CallbackType);
    void asyncTaskComplete(unsigned taskCallbackID);

    // For persistent callbacks.
    unsigned registerCallback(JSValueRef taskCallback, CallbackType);
    JSValueRef callbackWithID(unsigned callbackID);
    void unregisterCallback(unsigned callbackID);
    void fireCallback(unsigned callbackID);

    unsigned nextTaskCallbackID(CallbackType);

    JSObjectRef objectFromRect(const WebCore::FloatRect&) const;

    JSGlobalContextRef jsContext() const { return m_context.get(); }

private:
    JSRetainPtr<JSGlobalContextRef> m_context;
    
    bool hasOutstandingAsyncTasks() const { return !m_callbacks.isEmpty(); }
    bool currentParentCallbackIsPendingCompletion() const { return m_uiScriptResultsPendingCompletion.contains(m_currentScriptCallbackID); }
    bool currentParentCallbackHasOutstandingAsyncTasks() const;
    void tryToCompleteUIScriptForCurrentParentCallback();
    
    struct Task {
        unsigned parentScriptCallbackID { 0 };
        JSValueRef callback { nullptr };
    };
    HashMap<unsigned, Task> m_callbacks;
    HashMap<unsigned, JSStringRef> m_uiScriptResultsPendingCompletion;

    UIScriptContextDelegate& m_delegate;
    RefPtr<UIScriptController> m_controller;
    
    unsigned m_currentScriptCallbackID { 0 };
    unsigned m_nextTaskCallbackID { 0 };
};

}

#endif // UIScriptContext_h
