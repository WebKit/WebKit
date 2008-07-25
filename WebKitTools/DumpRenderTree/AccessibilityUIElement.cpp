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

#include "AccessibilityUIElement.h"

#import <JavaScriptCore/JSRetainPtr.h>

// Static Functions

static JSValueRef allAttributesCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    AccessibilityUIElement* element = reinterpret_cast<AccessibilityUIElement*>(JSObjectGetPrivate(thisObject));
    JSRetainPtr<JSStringRef> attributes(Adopt, element->allAttributes());
    return JSValueMakeString(context, attributes.get());
}

static JSValueRef attributesOfLinkedUIElementsCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    AccessibilityUIElement* element = reinterpret_cast<AccessibilityUIElement*>(JSObjectGetPrivate(thisObject));
    JSRetainPtr<JSStringRef> linkedUIDescription(Adopt, element->attributesOfLinkedUIElements());
    return JSValueMakeString(context, linkedUIDescription.get());
}

static JSValueRef attributesOfChildrenCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    AccessibilityUIElement* element = reinterpret_cast<AccessibilityUIElement*>(JSObjectGetPrivate(thisObject));
    JSRetainPtr<JSStringRef> childrenDescription(Adopt, element->attributesOfChildren());
    return JSValueMakeString(context, childrenDescription.get());
}

static JSValueRef lineForIndexCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    int indexNumber = -1;
    if (argumentCount == 1)
        indexNumber = JSValueToNumber(context, arguments[0], exception);
    
    AccessibilityUIElement* element = reinterpret_cast<AccessibilityUIElement*>(JSObjectGetPrivate(thisObject));
    return JSValueMakeNumber(context, element->lineForIndex(indexNumber));
}

// Static Value Getters

static JSValueRef getRoleCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    AccessibilityUIElement* element = reinterpret_cast<AccessibilityUIElement*>(JSObjectGetPrivate(thisObject));
    JSRetainPtr<JSStringRef> role(Adopt, element->role());
    return JSValueMakeString(context, role.get());
}

static JSValueRef getTitleCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    AccessibilityUIElement* element = reinterpret_cast<AccessibilityUIElement*>(JSObjectGetPrivate(thisObject));
    JSRetainPtr<JSStringRef> title(Adopt, element->title());
    return JSValueMakeString(context, title.get());
}

static JSValueRef getDescriptionCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    AccessibilityUIElement* element = reinterpret_cast<AccessibilityUIElement*>(JSObjectGetPrivate(thisObject));
    JSRetainPtr<JSStringRef> description(Adopt, element->description());
    return JSValueMakeString(context, description.get());
}

static JSValueRef getWidthCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    AccessibilityUIElement* element = reinterpret_cast<AccessibilityUIElement*>(JSObjectGetPrivate(thisObject));
    return JSValueMakeNumber(context, element->width());
}

static JSValueRef getHeightCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    AccessibilityUIElement* element = reinterpret_cast<AccessibilityUIElement*>(JSObjectGetPrivate(thisObject));
    return JSValueMakeNumber(context, element->height());
}

static JSValueRef getIntValueCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    AccessibilityUIElement* element = reinterpret_cast<AccessibilityUIElement*>(JSObjectGetPrivate(thisObject));
    return JSValueMakeNumber(context, element->intValue());
}

static JSValueRef getMinValueCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    AccessibilityUIElement* element = reinterpret_cast<AccessibilityUIElement*>(JSObjectGetPrivate(thisObject));
    return JSValueMakeNumber(context, element->minValue());
}

static JSValueRef getMaxValueCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    AccessibilityUIElement* element = reinterpret_cast<AccessibilityUIElement*>(JSObjectGetPrivate(thisObject));
    return JSValueMakeNumber(context, element->maxValue());
}

static JSValueRef getInsertionPointLineNumberCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    AccessibilityUIElement* element = reinterpret_cast<AccessibilityUIElement*>(JSObjectGetPrivate(thisObject));
    return JSValueMakeNumber(context, element->insertionPointLineNumber());
}

// Destruction

static void finalize(JSObjectRef thisObject)
{
    AccessibilityUIElement* element = reinterpret_cast<AccessibilityUIElement*>(JSObjectGetPrivate(thisObject));
    delete element;
}

// Object Creation

JSObjectRef AccessibilityUIElement::makeJSAccessibilityUIElement(JSContextRef context, AccessibilityUIElement* element)
{
    return JSObjectMake(context, AccessibilityUIElement::getJSClass(), element);
}

JSClassRef AccessibilityUIElement::getJSClass()
{
    static JSStaticValue staticValues[] = {
        { "role", getRoleCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "title", getTitleCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "description", getDescriptionCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "width", getWidthCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "height", getHeightCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "intValue", getIntValueCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "minValue", getMinValueCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "maxValue", getMaxValueCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "insertionPointLineNumber", getInsertionPointLineNumberCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { 0, 0, 0, 0 }
    };

    static JSStaticFunction staticFunctions[] = {
        { "allAttributes", allAttributesCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "attributesOfLinkedUIElements", attributesOfLinkedUIElementsCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "attributesOfChildren", attributesOfChildrenCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "lineForIndex", lineForIndexCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { 0, 0, 0 }
    };

    static JSClassDefinition classDefinition = {
        0, kJSClassAttributeNone, "AccessibilityUIElement", 0, staticValues, staticFunctions,
        0, finalize, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };

    static JSClassRef accessibilityUIElementClass = JSClassCreate(&classDefinition);
    return accessibilityUIElementClass;
}
