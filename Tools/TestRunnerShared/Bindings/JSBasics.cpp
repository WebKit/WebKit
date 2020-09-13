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

JSValueRef JSValueMakeBooleanOrNull(JSContextRef context, Optional<bool> value)
{
    return value ? JSValueMakeBoolean(context, value.value()) : JSValueMakeNull(context);
}

Optional<bool> JSValueToNullableBoolean(JSContextRef context, JSValueRef value)
{
    return JSValueIsUndefined(context, value) || JSValueIsNull(context, value) ? WTF::nullopt : Optional<bool>(JSValueToBoolean(context, value));
}

JSValueRef JSValueMakeStringOrNull(JSContextRef context, JSStringRef stringOrNull)
{
    return stringOrNull ? JSValueMakeString(context, stringOrNull) : JSValueMakeNull(context);
}

JSRetainPtr<JSStringRef> createJSString(const char* string)
{
    return adopt(JSStringCreateWithUTF8CString(string));
}

JSValueRef makeValue(JSContextRef context, const char* string)
{
    return JSValueMakeString(context, createJSString(string).get());
}

JSObjectRef objectProperty(JSContextRef context, JSObjectRef object, const char* name)
{
    if (!object)
        return nullptr;
    auto value = JSObjectGetProperty(context, object, createJSString(name).get(), nullptr);
    if (!JSValueIsObject(context, value))
        return nullptr;
    return const_cast<JSObjectRef>(value);
}

JSObjectRef objectProperty(JSContextRef context, JSObjectRef object, std::initializer_list<const char*> names)
{
    for (auto name : names)
        object = objectProperty(context, object, name);
    return object;
}

void setProperty(JSContextRef context, JSObjectRef object, const char* name, bool value)
{
    JSObjectSetProperty(context, object, createJSString(name).get(), JSValueMakeBoolean(context, value), kJSPropertyAttributeNone, nullptr);
}

JSValueRef call(JSContextRef context, JSObjectRef object, const char* name, std::initializer_list<JSValueRef> arguments)
{
    if (!object)
        return nullptr;
    auto function = objectProperty(context, object, name);
    if (!function)
        return nullptr;
    return JSObjectCallAsFunction(context, function, object, arguments.size(), arguments.begin(), nullptr);
}

JSObjectRef callConstructor(JSGlobalContextRef context, const char* name, std::initializer_list<JSValueRef> arguments)
{
    auto constructor = objectProperty(context, JSContextGetGlobalObject(context), { name });
    if (!constructor)
        return nullptr;
    return JSObjectCallAsConstructor(context, constructor, arguments.size(), arguments.begin(), nullptr);
}

} // namespace WTR
