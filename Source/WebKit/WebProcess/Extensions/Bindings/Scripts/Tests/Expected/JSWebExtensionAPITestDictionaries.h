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

enum class WebExtensionTestMode : uint8_t {
    Fast,
    SlowMode,
};

template<> struct WebExtensionValueParser<WebExtensionTestMode> : WebExtensionStringParserBase {
    static std::expected<WebExtensionTestMode, WebExtensionValueError> parse(JSContextRef, JSValueRef, const String& path);
};

struct WebExtensionTestToggle {
    std::optional<String> path;
    std::optional<bool> enabled;
};

std::expected<WebExtensionTestToggle, WebExtensionValueError> parseWebExtensionTestToggle(JSContextRef, JSValueRef, const String& path = { });

template<> struct WebExtensionValueParser<WebExtensionTestToggle> : WebExtensionObjectParserBase {
    static std::expected<WebExtensionTestToggle, WebExtensionValueError> parse(JSContextRef context, JSValueRef value, const String& path) { return parseWebExtensionTestToggle(context, value, path); }
};

struct WebExtensionTestExclusiveBooleans {
    std::optional<bool> muted;
    std::optional<bool> audible;
};

std::expected<WebExtensionTestExclusiveBooleans, WebExtensionValueError> parseWebExtensionTestExclusiveBooleans(JSContextRef, JSValueRef, const String& path = { });

template<> struct WebExtensionValueParser<WebExtensionTestExclusiveBooleans> : WebExtensionObjectParserBase {
    static std::expected<WebExtensionTestExclusiveBooleans, WebExtensionValueError> parse(JSContextRef context, JSValueRef value, const String& path) { return parseWebExtensionTestExclusiveBooleans(context, value, path); }
};

struct WebExtensionTestTwoGroups {
    std::optional<bool> allFrames;
    std::optional<String> file;
    std::optional<String> code;
    std::optional<double> frameId;
    std::optional<String> documentId;
};

std::expected<WebExtensionTestTwoGroups, WebExtensionValueError> parseWebExtensionTestTwoGroups(JSContextRef, JSValueRef, const String& path = { });

template<> struct WebExtensionValueParser<WebExtensionTestTwoGroups> : WebExtensionObjectParserBase {
    static std::expected<WebExtensionTestTwoGroups, WebExtensionValueError> parse(JSContextRef context, JSValueRef value, const String& path) { return parseWebExtensionTestTwoGroups(context, value, path); }
};

struct WebExtensionTestBase {
    std::optional<String> label;
};

std::expected<WebExtensionTestBase, WebExtensionValueError> parseWebExtensionTestBase(JSContextRef, JSValueRef, const String& path = { });

template<> struct WebExtensionValueParser<WebExtensionTestBase> : WebExtensionObjectParserBase {
    static std::expected<WebExtensionTestBase, WebExtensionValueError> parse(JSContextRef context, JSValueRef value, const String& path) { return parseWebExtensionTestBase(context, value, path); }
};

struct WebExtensionTestOptions {
    std::optional<String> label;
    bool active;
    std::optional<Vector<WebExtensionTestBase>> bases;
    std::optional<Vector<String>> files;
#if ENABLE(WK_WEB_EXTENSIONS_TEST_FEATURE)
    std::optional<bool> experimental;
#endif
    std::optional<Protected<JSValueRef>> extra;
    std::optional<double> first;
    std::optional<Protected<JSValueRef>> func;
    uint32_t limit;
    WebExtensionTestMode mode;
    std::optional<String> name;
    std::optional<int32_t> quality;
    std::optional<Vector<Protected<JSValueRef>>> args;
    std::optional<double> second;
    std::optional<double> third;
};

std::expected<WebExtensionTestOptions, WebExtensionValueError> parseWebExtensionTestOptions(JSContextRef, JSValueRef, const String& path = { });

template<> struct WebExtensionValueParser<WebExtensionTestOptions> : WebExtensionObjectParserBase {
    static std::expected<WebExtensionTestOptions, WebExtensionValueError> parse(JSContextRef context, JSValueRef value, const String& path) { return parseWebExtensionTestOptions(context, value, path); }
};

struct WebExtensionTestNode {
    std::optional<Vector<WebExtensionTestNode>> children;
    String id;
};

std::expected<WebExtensionTestNode, WebExtensionValueError> parseWebExtensionTestNode(JSContextRef, JSValueRef, const String& path = { });

template<> struct WebExtensionValueParser<WebExtensionTestNode> : WebExtensionObjectParserBase {
    static std::expected<WebExtensionTestNode, WebExtensionValueError> parse(JSContextRef context, JSValueRef value, const String& path) { return parseWebExtensionTestNode(context, value, path); }
};

struct WebExtensionTestReport {
    String title;
    std::optional<WebExtensionTestMode> mode;
};

std::expected<WebExtensionTestReport, WebExtensionValueError> parseWebExtensionTestReport(JSContextRef, JSValueRef, const String& path = { });

template<> struct WebExtensionValueParser<WebExtensionTestReport> : WebExtensionObjectParserBase {
    static std::expected<WebExtensionTestReport, WebExtensionValueError> parse(JSContextRef context, JSValueRef value, const String& path) { return parseWebExtensionTestReport(context, value, path); }
};

class WebExtensionAPITestDictionaries;

class JSWebExtensionAPITestDictionaries : public JSWebExtensionWrapper {
public:
    static JSClassRef testDictionariesClassSingleton();
    static JSClassRef testDictionariesGlobalObjectClassSingleton();

private:
    static const JSStaticFunction* staticFunctions();
    static const JSStaticValue* staticValues();
    static const JSStaticValue* staticConstants();
    static bool hasInstance(JSContextRef, JSObjectRef constructor, JSValueRef possibleInstance, JSValueRef* exception);

    // Functions

    static JSValueRef toggle(JSContextRef, JSObjectRef, JSObjectRef, size_t, const JSValueRef[], JSValueRef*);
    static JSValueRef mute(JSContextRef, JSObjectRef, JSObjectRef, size_t, const JSValueRef[], JSValueRef*);
    static JSValueRef inject(JSContextRef, JSObjectRef, JSObjectRef, size_t, const JSValueRef[], JSValueRef*);
    static JSValueRef open(JSContextRef, JSObjectRef, JSObjectRef, size_t, const JSValueRef[], JSValueRef*);
    static JSValueRef query(JSContextRef, JSObjectRef, JSObjectRef, size_t, const JSValueRef[], JSValueRef*);
    static JSValueRef inspect(JSContextRef, JSObjectRef, JSObjectRef, size_t, const JSValueRef[], JSValueRef*);
    static JSValueRef visit(JSContextRef, JSObjectRef, JSObjectRef, size_t, const JSValueRef[], JSValueRef*);
    static JSValueRef set(JSContextRef, JSObjectRef, JSObjectRef, size_t, const JSValueRef[], JSValueRef*);
    static JSValueRef choose(JSContextRef, JSObjectRef, JSObjectRef, size_t, const JSValueRef[], JSValueRef*);
    static JSValueRef build(JSContextRef, JSObjectRef, JSObjectRef, size_t, const JSValueRef[], JSValueRef*);
    static JSValueRef seek(JSContextRef, JSObjectRef, JSObjectRef, size_t, const JSValueRef[], JSValueRef*);
};

WebExtensionAPITestDictionaries* toWebExtensionAPITestDictionaries(JSContextRef, JSValueRef);

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)

