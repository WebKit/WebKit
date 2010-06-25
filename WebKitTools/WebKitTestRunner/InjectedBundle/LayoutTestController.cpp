/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "LayoutTestController.h"

#include <JavaScriptCore/JSRetainPtr.h>

namespace WTR {

PassRefPtr<LayoutTestController> LayoutTestController::create(const std::string& testPathOrURL)
{
    return adoptRef(new LayoutTestController(testPathOrURL));
}

LayoutTestController::LayoutTestController(const std::string& testPathOrURL)
    : m_dumpAsText(false)
    , m_testPathOrURL(testPathOrURL)
{
}

LayoutTestController::~LayoutTestController()
{
}

static JSValueRef dumpAsTextCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setDumpAsText(true);
    return JSValueMakeUndefined(context);
}

// Object Finalization

static void layoutTestControllerObjectFinalize(JSObjectRef object)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(object));
    controller->deref();
}

// Object Creation

void LayoutTestController::makeWindowObject(JSContextRef context, JSObjectRef windowObject, JSValueRef* exception)
{
    JSRetainPtr<JSStringRef> layoutTestContollerStr(Adopt, JSStringCreateWithUTF8CString("layoutTestController"));
    ref();

    JSClassRef classRef = getJSClass();
    JSValueRef layoutTestContollerObject = JSObjectMake(context, classRef, this);
    JSClassRelease(classRef);

    JSObjectSetProperty(context, windowObject, layoutTestContollerStr.get(), layoutTestContollerObject, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete, exception);
}

JSClassRef LayoutTestController::getJSClass()
{
    static JSStaticFunction* staticFunctions = LayoutTestController::staticFunctions();
    static JSClassDefinition classDefinition = {
        0, kJSClassAttributeNone, "LayoutTestController", 0, 0, staticFunctions,
        0, layoutTestControllerObjectFinalize, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };

    return JSClassCreate(&classDefinition);
}

JSStaticFunction* LayoutTestController::staticFunctions()
{
    static JSStaticFunction staticFunctions[] = {
        { "dumpAsText", dumpAsTextCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { 0, 0, 0 }
    };

    return staticFunctions;
}

} // namespace WTR
