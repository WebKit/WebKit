/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if ENABLE(WK_WEB_EXTENSIONS)

#include "JSWebExtensionWrapper.h"
#include "JSWebExtensionValueParser.h"
#include "WebExtensionTestWindowState.h"

namespace WebKit {

using namespace WTF;

template<> struct WebExtensionValueParser<WebExtensionTestWindowState> : WebExtensionStringParserBase {
    static std::expected<WebExtensionTestWindowState, WebExtensionValueError> parse(JSContextRef, JSValueRef, const String& path);
};

enum class WebExtensionTestRestriction : uint8_t {
    NoRestriction,
    AskUser,
    9By9,
};

template<> struct WebExtensionValueParser<WebExtensionTestRestriction> : WebExtensionStringParserBase {
    static std::expected<WebExtensionTestRestriction, WebExtensionValueError> parse(JSContextRef, JSValueRef, const String& path);
};

class WebExtensionAPITestEnums;

class JSWebExtensionAPITestEnums : public JSWebExtensionWrapper {
public:
    static JSClassRef testEnumsClassSingleton();
    static JSClassRef testEnumsGlobalObjectClassSingleton();

private:
    static const JSStaticFunction* staticFunctions();
    static const JSStaticValue* staticValues();
    static const JSStaticValue* staticConstants();
    static bool hasInstance(JSContextRef, JSObjectRef constructor, JSValueRef possibleInstance, JSValueRef* exception);

    // Functions

    static JSValueRef setState(JSContextRef, JSObjectRef, JSObjectRef, size_t, const JSValueRef[], JSValueRef*);
    static JSValueRef restrict(JSContextRef, JSObjectRef, JSObjectRef, size_t, const JSValueRef[], JSValueRef*);

    // Attributes

    static JSValueRef WindowState(JSContextRef, JSObjectRef, JSStringRef, JSValueRef*);
    static JSValueRef Restriction(JSContextRef, JSObjectRef, JSStringRef, JSValueRef*);
};

WebExtensionAPITestEnums* toWebExtensionAPITestEnums(JSContextRef, JSValueRef);

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)

