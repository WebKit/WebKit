/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "JSBasics.h"

namespace WTR {

Optional<bool> toOptionalBool(JSContextRef context, JSValueRef value)
{
    return JSValueIsUndefined(context, value) || JSValueIsNull(context, value) ? WTF::nullopt : makeOptional(JSValueToBoolean(context, value));
}

Optional<double> toOptionalDouble(JSContextRef context, JSValueRef value)
{
    return JSValueIsUndefined(context, value) || JSValueIsNull(context, value) ? WTF::nullopt : makeOptional(JSValueToNumber(context, value, nullptr));
}

JSValueRef makeValue(JSContextRef context, Optional<bool> value)
{
    return value ? JSValueMakeBoolean(context, value.value()) : JSValueMakeNull(context);
}

JSValueRef makeValue(JSContextRef context, JSStringRef string)
{
    return string ? JSValueMakeString(context, string) : JSValueMakeNull(context);
}

JSRetainPtr<JSStringRef> createJSString(const char* string)
{
    return adopt(JSStringCreateWithUTF8CString(string));
}

JSRetainPtr<JSStringRef> createJSString(JSContextRef context, JSValueRef value)
{
    auto string = adopt(value ? JSValueToStringCopy(context, value, nullptr) : nullptr);
    return string ? string : createJSString("");
}

JSValueRef makeValue(JSContextRef context, const char* string)
{
    return JSValueMakeString(context, createJSString(string).get());
}

JSValueRef property(JSContextRef context, JSObjectRef object, const char* name)
{
    return object ? JSObjectGetProperty(context, object, createJSString(name).get(), nullptr) : nullptr;
}

JSRetainPtr<JSStringRef> stringProperty(JSContextRef context, JSObjectRef object, const char* name)
{
    return createJSString(context, property(context, object, name));
}

bool booleanProperty(JSContextRef context, JSObjectRef object, const char* name, bool defaultValue)
{
    auto value = property(context, object, name);
    return value ? JSValueToBoolean(context, value) : defaultValue;
}

double numericProperty(JSContextRef context, JSObjectRef object, const char* name)
{
    auto value = property(context, object, name);
    return value ? JSValueToNumber(context, value, nullptr) : 0;
}

JSObjectRef objectProperty(JSContextRef context, JSObjectRef object, const char* name)
{
    auto value = property(context, object, name);
    return value ? JSValueToObject(context, property(context, object, name), nullptr) : nullptr;
}

JSObjectRef objectProperty(JSContextRef context, JSObjectRef object, std::initializer_list<const char*> names)
{
    for (auto name : names)
        object = objectProperty(context, object, name);
    return object;
}

unsigned arrayLength(JSContextRef context, JSObjectRef object)
{
    return numericProperty(context, object, "length");
}

void setProperty(JSContextRef context, JSObjectRef object, const char* name, bool value)
{
    JSObjectSetProperty(context, object, createJSString(name).get(), JSValueMakeBoolean(context, value), kJSPropertyAttributeNone, nullptr);
}

void setProperty(JSContextRef context, JSObjectRef object, const char* name, double value)
{
    JSObjectSetProperty(context, object, createJSString(name).get(), JSValueMakeNumber(context, value), kJSPropertyAttributeNone, nullptr);
}

void setGlobalObjectProperty(JSContextRef context, const char* name, JSValueRef value)
{
    JSObjectSetProperty(context, JSContextGetGlobalObject(context), createJSString(name).get(), value, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete, nullptr);
}

void setGlobalObjectProperty(JSContextRef context, const char* name, double value)
{
    setGlobalObjectProperty(context, name, JSValueMakeNumber(context, value));
}

JSValueRef call(JSContextRef context, JSObjectRef object, const char* name, std::initializer_list<JSValueRef> arguments)
{
    auto function = objectProperty(context, object, name);
    return function ? JSObjectCallAsFunction(context, function, object, arguments.size(), arguments.begin(), nullptr) : nullptr;
}

JSObjectRef callConstructor(JSGlobalContextRef context, const char* name, std::initializer_list<JSValueRef> arguments)
{
    auto constructor = objectProperty(context, JSContextGetGlobalObject(context), { name });
    return constructor ? JSObjectCallAsConstructor(context, constructor, arguments.size(), arguments.begin(), nullptr) : nullptr;
}

} // namespace WTR
