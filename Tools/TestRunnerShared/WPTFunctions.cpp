/*
 * Copyright (C) 2020-2022 Apple Inc. All rights reserved.
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
#include "WPTFunctions.h"
#include <array>
#include <wtf/URL.h>
#include <wtf/text/ASCIILiteral.h>
#include <wtf/HashMap.h>
#include <wtf/StdLibExtras.h>

#include "JSBasics.h"

namespace WTR {

void sendTestRenderedEvent(JSGlobalContextRef context)
{
    if (!context)
        return;
    auto initializer = JSObjectMake(context, nullptr, nullptr);
    setProperty(context, initializer, "bubbles", true);
    auto value = makeValue(context, "TestRendered");
    JSValueProtect(context, value);
    auto event = callConstructor(context, "Event", { value, initializer });
    auto documentElement = objectProperty(context, JSContextGetGlobalObject(context), { "document", "documentElement" });
    call(context, documentElement, "dispatchEvent", { event });
    JSValueUnprotect(context, value);
}

bool hasTestWaitAttribute(JSGlobalContextRef context)
{
    if (!context)
        return false;
    auto classList = objectProperty(context, JSContextGetGlobalObject(context), { "document", "documentElement", "classList" });
    return JSValueToBoolean(context, call(context, classList, "contains", { makeValue(context, "reftest-wait") }))
        || JSValueToBoolean(context, call(context, classList, "contains", { makeValue(context, "test-wait") }));
}

bool isWebPlatformTestURL(const URL& url)
{
    if (!url.isValid())
        return false;

    auto contains = [](const auto& collection, const auto& value) {
        return std::ranges::find(collection, value) != collection.cend();
    };

    // Sourced from imported/w3c/resources/config.json
    constexpr std::array<ASCIILiteral, 3> possibleHosts { "localhost"_s, "web-platform.test"_s, "127.0.0.1"_s };
    constexpr std::array<uint16_t, 6> possiblePorts { 8800, 8801, 9443, 9444, 9000, 49001 };
    auto host = url.host();
    auto port = url.port().value_or(0);
    return contains(possibleHosts, host) && contains(possiblePorts, port);
}

static HashMap<JSGlobalContextRef, WTF::Function<void()>>& pendingRAFCompletions()
{
    static NeverDestroyed<HashMap<JSGlobalContextRef, WTF::Function<void()>>> completions;
    return completions;
}

static JSValueRef secondRequestAnimationFrameCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    auto& map = pendingRAFCompletions();
    auto globalContext = JSContextGetGlobalContext(context);
    auto it = map.find(globalContext);
    if (it != map.end()) {
        auto completion = WTF::move(it->value);
        map.remove(it);
        completion();
    }
    return JSValueMakeUndefined(context);
}

static JSValueRef firstRequestAnimationFrameCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // First rAF callback: schedule the second rAF
    auto globalObject = JSContextGetGlobalObject(context);
    auto window = objectProperty(context, globalObject, "window");
    auto secondCallback = JSObjectMakeFunctionWithCallback(context, nullptr, secondRequestAnimationFrameCallback);
    call(context, window, "requestAnimationFrame", { secondCallback });
    return JSValueMakeUndefined(context);
}

void waitForDoubleRequestAnimationFrame(JSGlobalContextRef context, WTF::Function<void()>&& completionHandler)
{
    if (!context)
        return;

    auto& map = pendingRAFCompletions();
    map.set(context, WTF::move(completionHandler));

    auto globalObject = JSContextGetGlobalObject(context);
    auto window = objectProperty(context, globalObject, "window");
    auto firstCallback = JSObjectMakeFunctionWithCallback(context, nullptr, firstRequestAnimationFrameCallback);
    call(context, window, "requestAnimationFrame", { firstCallback });
}

void cancelPendingDoubleRequestAnimationFrameWait(JSGlobalContextRef context)
{
    if (!context)
        return;
    pendingRAFCompletions().remove(context);
}

}
