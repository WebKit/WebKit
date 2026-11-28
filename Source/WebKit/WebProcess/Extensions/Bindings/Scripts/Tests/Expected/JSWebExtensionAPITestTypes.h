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

namespace WebKit {

using namespace WTF;

enum class WebExtensionTestItemKind : uint8_t {
    Bookmark,
    Folder,
};

template<> struct WebExtensionValueParser<WebExtensionTestItemKind> : WebExtensionStringParserBase {
    static std::expected<WebExtensionTestItemKind, WebExtensionValueError> parse(JSContextRef, JSValueRef, const String& path);
};

struct WebExtensionTestItemProperties {
    std::optional<Variant<String, Vector<String>>> urls;
    std::optional<std::optional<double>> index;
    Variant<double, String> position;
    std::optional<WebExtensionJSONValue> payload;
    std::optional<HashMap<String, String>> headers;
    std::optional<HashMap<String, Vector<String>>> formData;
    std::optional<HashMap<String, Protected<JSValueRef>>> extras;
    std::optional<WebExtensionTabIdentifier> tabId;
    std::optional<std::optional<WebExtensionWindowIdentifier>> windowId;
    std::optional<WebExtensionFrameIdentifier> frameId;
    std::optional<UUID> documentId;
    std::optional<WebExtensionCookieStoreIdentifier> storeId;
    std::optional<Ref<WebExtensionMatchPattern>> pattern;
    std::optional<WebExtensionJSONObject> rule;
    std::optional<URL> address;
    std::optional<String> identifier;
    WebExtensionTestItemKind kind;
};

std::expected<WebExtensionTestItemProperties, WebExtensionValueError> parseWebExtensionTestItemProperties(JSContextRef, JSValueRef, const String& path = { });

template<> struct WebExtensionValueParser<WebExtensionTestItemProperties> : WebExtensionObjectParserBase {
    static std::expected<WebExtensionTestItemProperties, WebExtensionValueError> parse(JSContextRef context, JSValueRef value, const String& path) { return parseWebExtensionTestItemProperties(context, value, path); }
};

class WebExtensionAPITestTypes;

class JSWebExtensionAPITestTypes : public JSWebExtensionWrapper {
public:
    static JSClassRef testTypesClassSingleton();
    static JSClassRef testTypesGlobalObjectClassSingleton();

private:
    static const JSStaticFunction* staticFunctions();
    static const JSStaticValue* staticValues();
    static const JSStaticValue* staticConstants();
    static bool hasInstance(JSContextRef, JSObjectRef constructor, JSValueRef possibleInstance, JSValueRef* exception);

    // Functions

    static JSValueRef add(JSContextRef, JSObjectRef, JSObjectRef, size_t, const JSValueRef[], JSValueRef*);
    static JSValueRef find(JSContextRef, JSObjectRef, JSObjectRef, size_t, const JSValueRef[], JSValueRef*);
    static JSValueRef tag(JSContextRef, JSObjectRef, JSObjectRef, size_t, const JSValueRef[], JSValueRef*);
    static JSValueRef select(JSContextRef, JSObjectRef, JSObjectRef, size_t, const JSValueRef[], JSValueRef*);
    static JSValueRef forget(JSContextRef, JSObjectRef, JSObjectRef, size_t, const JSValueRef[], JSValueRef*);
    static JSValueRef sync(JSContextRef, JSObjectRef, JSObjectRef, size_t, const JSValueRef[], JSValueRef*);
    static JSValueRef use(JSContextRef, JSObjectRef, JSObjectRef, size_t, const JSValueRef[], JSValueRef*);
    static JSValueRef open(JSContextRef, JSObjectRef, JSObjectRef, size_t, const JSValueRef[], JSValueRef*);
};

WebExtensionAPITestTypes* toWebExtensionAPITestTypes(JSContextRef, JSValueRef);

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)

