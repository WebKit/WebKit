/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "AccessibilityController.h"

#import <JavaScriptCore/JSRetainPtr.h>

AccessibilityController::AccessibilityController()
{
}

AccessibilityController::~AccessibilityController()
{
}

static JSValueRef allAttributesOfFocusedElementCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    AccessibilityController* controller = reinterpret_cast<AccessibilityController*>(JSObjectGetPrivate(thisObject));
    JSRetainPtr<JSStringRef> attributes(Adopt, controller->allAttributesOfFocusedElement());
    return JSValueMakeString(context, attributes.get());
}

static JSValueRef roleOfFocusedElementCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    AccessibilityController* controller = reinterpret_cast<AccessibilityController*>(JSObjectGetPrivate(thisObject));
    JSRetainPtr<JSStringRef> role(Adopt, controller->roleOfFocusedElement());
    return JSValueMakeString(context, role.get());
}

static JSValueRef titleOfFocusedElementCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    AccessibilityController* controller = reinterpret_cast<AccessibilityController*>(JSObjectGetPrivate(thisObject));
    JSRetainPtr<JSStringRef> title(Adopt, controller->titleOfFocusedElement());
    return JSValueMakeString(context, title.get());
}

static JSValueRef descriptionOfFocusedElementCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    AccessibilityController* controller = reinterpret_cast<AccessibilityController*>(JSObjectGetPrivate(thisObject));
    JSRetainPtr<JSStringRef> description(Adopt, controller->descriptionOfFocusedElement());
    return JSValueMakeString(context, description.get());
}

// Object Creation

void AccessibilityController::makeWindowObject(JSContextRef context, JSObjectRef windowObject, JSValueRef* exception)
{
    JSRetainPtr<JSStringRef> accessibilityControllerStr(Adopt, JSStringCreateWithUTF8CString("accessibilityController"));
    JSValueRef accessibilityControllerObject = JSObjectMake(context, getJSClass(), this);
    JSObjectSetProperty(context, windowObject, accessibilityControllerStr.get(), accessibilityControllerObject, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete, exception);
}

JSClassRef AccessibilityController::getJSClass()
{
    static JSClassRef accessibilityControllerClass;

    if (!accessibilityControllerClass) {
        JSStaticFunction* staticFunctions = AccessibilityController::staticFunctions();
        JSClassDefinition classDefinition = {
            0, kJSClassAttributeNone, "AccessibilityController", 0, 0, staticFunctions,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
        };

        accessibilityControllerClass = JSClassCreate(&classDefinition);
    }

    return accessibilityControllerClass;
}

JSStaticFunction* AccessibilityController::staticFunctions()
{
    static JSStaticFunction staticFunctions[] = {
        { "allAttributesOfFocusedElement", allAttributesOfFocusedElementCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "roleOfFocusedElement", roleOfFocusedElementCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "titleOfFocusedElement", titleOfFocusedElementCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "descriptionOfFocusedElement", descriptionOfFocusedElementCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { 0, 0, 0 }
    };

    return staticFunctions;
}
