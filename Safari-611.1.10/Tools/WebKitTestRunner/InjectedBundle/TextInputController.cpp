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

#include "DictionaryFunctions.h"
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

JSClassRef TextInputController::wrapperClass()
{
    return JSTextInputController::textInputControllerClass();
}

void TextInputController::makeWindowObject(JSContextRef context)
{
    setGlobalObjectProperty(context, "textInputController", this);
}

static WKArrayRef createCompositionHighlightData(JSContextRef context, JSValueRef jsHighlightsValue)
{
    if (!jsHighlightsValue || !JSValueIsArray(context, jsHighlightsValue))
        return nullptr;

    auto result = WKMutableArrayCreate();
    auto array = const_cast<JSObjectRef>(jsHighlightsValue);
    unsigned length = arrayLength(context, array);
    for (unsigned i = 0; i < length; ++i) {
        auto value = JSObjectGetPropertyAtIndex(context, array, i, nullptr);
        if (!value || !JSValueIsObject(context, value))
            continue;
        auto object = const_cast<JSObjectRef>(value);
        auto dictionary = adoptWK(WKMutableDictionaryCreate());
        setValue(dictionary, "from", static_cast<uint64_t>(numericProperty(context, object, "from")));
        setValue(dictionary, "length", static_cast<uint64_t>(numericProperty(context, object, "length")));
        setValue(dictionary, "color", toWK(stringProperty(context, object, "color")));
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
