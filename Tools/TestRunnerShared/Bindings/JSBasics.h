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

#pragma once

#include <JavaScriptCore/JSRetainPtr.h>
#include <JavaScriptCore/JavaScript.h>
#include <initializer_list>
#include <optional>

namespace WTR {

std::optional<bool> toOptionalBool(JSContextRef, JSValueRef);
std::optional<double> toOptionalDouble(JSContextRef, JSValueRef);

bool isValidValue(JSContextRef, JSValueRef);

JSRetainPtr<JSStringRef> createJSString(const char* = "");
JSRetainPtr<JSStringRef> createJSString(JSContextRef, JSValueRef);

JSValueRef makeValue(JSContextRef, const char*);
JSValueRef makeValue(JSContextRef, std::optional<bool>);
JSValueRef makeValue(JSContextRef, JSStringRef);

JSValueRef property(JSContextRef, JSObjectRef, const char* name);
bool booleanProperty(JSContextRef, JSObjectRef, const char* name, bool defaultValue = false);
double numericProperty(JSContextRef, JSObjectRef, const char* name);
JSRetainPtr<JSStringRef> stringProperty(JSContextRef, JSObjectRef, const char* name);
unsigned arrayLength(JSContextRef, JSObjectRef);
JSObjectRef objectProperty(JSContextRef, JSObjectRef, const char* name);
JSObjectRef objectProperty(JSContextRef, JSObjectRef, std::initializer_list<const char*> names);

void setProperty(JSContextRef, JSObjectRef, const char* name, bool value);
void setProperty(JSContextRef, JSObjectRef, const char* name, double value);

void setGlobalObjectProperty(JSContextRef, const char* name, JSValueRef);
void setGlobalObjectProperty(JSContextRef, const char* name, double value);

JSValueRef call(JSContextRef, JSObjectRef, const char* name, std::initializer_list<JSValueRef> arguments);
JSObjectRef callConstructor(JSGlobalContextRef, const char* name, std::initializer_list<JSValueRef> arguments);

} // namespace WTR
