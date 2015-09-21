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

#include "config.h"
#include "UIScriptController.h"

#include "JSUIScriptController.h"
#include "UIScriptContext.h"
// #include <JavaScriptCore/JavaScriptCore.h>

namespace WTR {

UIScriptController::UIScriptController(UIScriptContext& context)
    : m_context(context)
{
}

void UIScriptController::makeWindowObject(JSContextRef context, JSObjectRef windowObject, JSValueRef* exception)
{
    setProperty(context, windowObject, "uiController", this, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete, exception);
}

JSClassRef UIScriptController::wrapperClass()
{
    return JSUIScriptController::uIScriptControllerClass();
}

#if !PLATFORM(COCOA)
void UIScriptController::doAsyncTask(JSValueRef)
{
}
#endif

#if !PLATFORM(IOS)
void UIScriptController::zoomToScale(double, JSValueRef)
{
}

double UIScriptController::zoomScale() const
{
    return 1;
}

double UIScriptController::minimumZoomScale() const
{
    return 1;
}

double UIScriptController::maximumZoomScale() const
{
    return 1;
}

JSObjectRef UIScriptController::contentVisibleRect() const
{
    return nullptr;
}
#endif

void UIScriptController::uiScriptComplete(JSStringRef result)
{
    m_context.uiScriptComplete(result);
}

}
