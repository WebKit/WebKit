/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#if ENABLE(WK_WEB_EXTENSIONS_SIDEBAR)

#include "JSWebExtensionAPISidebarAction.h"
#include "WebExtensionAPIObject.h"

OBJC_CLASS NSDictionary;
OBJC_CLASS NSString;

namespace WebKit {

using SidebarError = RetainPtr<NSString>;
// In this variant, `monostate` indicates that we have neither a window or tab identifier, but no error
using ParseResult = std::variant<std::monostate, WebExtensionTabIdentifier, WebExtensionWindowIdentifier, SidebarError>;

template<typename T, typename VARIANT_T>
struct isVariantMember;
template<typename T, typename... ALL_T>
struct isVariantMember<T, std::variant<ALL_T...>> : public std::disjunction<std::is_same<T, ALL_T>...> { };

template<typename OptType, typename... Types>
std::optional<OptType> toOptional(std::variant<Types...>& variant)
{
    if (std::holds_alternative<OptType>(variant))
        return WTFMove(std::get<OptType>(variant));
    return std::nullopt;
}

template<typename VariantType>
SidebarError indicatesError(const VariantType& variant)
{
    static_assert(isVariantMember<SidebarError, VariantType>::value);

    if (std::holds_alternative<SidebarError>(variant))
        return WTFMove(std::get<SidebarError>(variant));
    return nil;
}

class WebExtensionAPISidebarAction : public WebExtensionAPIObject, public JSWebExtensionWrappable {
    WEB_EXTENSION_DECLARE_JS_WRAPPER_CLASS(WebExtensionAPISidebarAction, sidebarAction, sidebarAction);

public:
#if PLATFORM(COCOA)
    void open(Ref<WebExtensionCallbackHandler>&&, NSString **outExceptionString);
    void close(Ref<WebExtensionCallbackHandler>&&, NSString **outExceptionString);
    void toggle(Ref<WebExtensionCallbackHandler>&&, NSString **outExceptionString);

    void isOpen(NSDictionary *details, Ref<WebExtensionCallbackHandler>&&, NSString **outExceptionString);
    void getPanel(NSDictionary *details, Ref<WebExtensionCallbackHandler>&&, NSString **outExceptionString);
    void setPanel(NSDictionary *details, Ref<WebExtensionCallbackHandler>&&, NSString **outExceptionString);
    void getTitle(NSDictionary *details, Ref<WebExtensionCallbackHandler>&&, NSString **outExceptionString);
    void setTitle(NSDictionary *details, Ref<WebExtensionCallbackHandler>&&, NSString **outExceptionString);
    void setIcon(NSDictionary *details, Ref<WebExtensionCallbackHandler>&&, NSString **outExceptionString);
#endif // PLATFORM(COCOA)
};

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS_SIDEBAR)
