/*
 * Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies)
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
#include "TextInputController.h"

#include "InjectedBundle.h"
#include "InjectedBundlePage.h"
#include "JSTextInputController.h"
#include "StringFunctions.h"
#include <WebKit/WKBundleFrame.h>
#include <WebKit/WKBundlePagePrivate.h>

namespace WTR {

Ref<TextInputController> TextInputController::create()
{
    return adoptRef(*new TextInputController);
}

TextInputController::TextInputController()
{
}

TextInputController::~TextInputController()
{
}

JSClassRef TextInputController::wrapperClass()
{
    return JSTextInputController::textInputControllerClass();
}

void TextInputController::makeWindowObject(JSContextRef context, JSObjectRef windowObject, JSValueRef* exception)
{
    setProperty(context, windowObject, "textInputController", this, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete, exception);
}

static unsigned arrayLength(JSContextRef context, JSObjectRef array)
{
    auto lengthString = adopt(JSStringCreateWithUTF8CString("length"));
    if (auto lengthValue = JSObjectGetProperty(context, array, lengthString.get(), nullptr))
        return static_cast<unsigned>(JSValueToNumber(context, lengthValue, nullptr));
    return 0;
}

static WKArrayRef createCompositionHighlightData(JSContextRef context, JSValueRef jsHighlightsValue)
{
    if (!jsHighlightsValue || !JSValueIsArray(context, jsHighlightsValue))
        return nullptr;

    auto result = WKMutableArrayCreate();
    auto jsHighlightsArray = const_cast<JSObjectRef>(jsHighlightsValue);
    unsigned length = arrayLength(context, jsHighlightsArray);
    if (!length)
        return result;

    auto jsFromKey = adopt(JSStringCreateWithUTF8CString("from"));
    auto jsLengthKey = adopt(JSStringCreateWithUTF8CString("length"));
    auto jsColorKey = adopt(JSStringCreateWithUTF8CString("color"));

    auto wkFromKey = adoptWK(WKStringCreateWithUTF8CString("from"));
    auto wkLengthKey = adoptWK(WKStringCreateWithUTF8CString("length"));
    auto wkColorKey = adoptWK(WKStringCreateWithUTF8CString("color"));

    for (size_t i = 0; i < length; ++i) {
        JSValueRef exception = nullptr;
        auto jsObjectValue = JSObjectGetPropertyAtIndex(context, jsHighlightsArray, i, &exception);
        if (exception || !JSValueIsObject(context, jsObjectValue))
            continue;

        auto jsObject = const_cast<JSObjectRef>(jsObjectValue);
        auto jsFromValue = JSObjectGetProperty(context, jsObject, jsFromKey.get(), nullptr);
        if (!jsFromValue || !JSValueIsNumber(context, jsFromValue))
            continue;

        auto jsLengthValue = JSObjectGetProperty(context, jsObject, jsLengthKey.get(), nullptr);
        if (!jsLengthValue || !JSValueIsNumber(context, jsLengthValue))
            continue;

        auto jsColorValue = JSObjectGetProperty(context, jsObject, jsColorKey.get(), nullptr);
        if (!jsColorValue || !JSValueIsString(context, jsColorValue))
            continue;

        auto color = adopt(JSValueToStringCopy(context, jsColorValue, nullptr));
        auto wkColor = adoptWK(WKStringCreateWithJSString(color.get()));
        auto wkFrom = adoptWK(WKUInt64Create(lround(JSValueToNumber(context, jsFromValue, nullptr))));
        auto wkLength = adoptWK(WKUInt64Create(lround(JSValueToNumber(context, jsLengthValue, nullptr))));

        auto dictionary = adoptWK(WKMutableDictionaryCreate());
        WKDictionarySetItem(dictionary.get(), wkFromKey.get(), wkFrom.get());
        WKDictionarySetItem(dictionary.get(), wkLengthKey.get(), wkLength.get());
        WKDictionarySetItem(dictionary.get(), wkColorKey.get(), wkColor.get());
        WKArrayAppendItem(result, dictionary.get());
    }

    return result;
}

void TextInputController::setMarkedText(JSStringRef text, int from, int length, bool suppressUnderline, JSValueRef jsHighlightsValue)
{
    auto page = InjectedBundle::singleton().page()->page();
    auto highlights = adoptWK(createCompositionHighlightData(WKBundleFrameGetJavaScriptContext(WKBundlePageGetMainFrame(page)), jsHighlightsValue));
    WKBundlePageSetComposition(page, toWK(text).get(), from, length, suppressUnderline, highlights.get());
}

bool TextInputController::hasMarkedText()
{
    return WKBundlePageHasComposition(InjectedBundle::singleton().page()->page());
}

void TextInputController::unmarkText()
{
    WKBundlePageConfirmComposition(InjectedBundle::singleton().page()->page());
}

void TextInputController::insertText(JSStringRef text)
{
    WKBundlePageConfirmCompositionWithText(InjectedBundle::singleton().page()->page(), toWK(text).get());
}

} // namespace WTR
