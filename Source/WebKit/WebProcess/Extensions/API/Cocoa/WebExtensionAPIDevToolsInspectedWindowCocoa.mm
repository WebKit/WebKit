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

#if !__has_feature(objc_arc)
#error This file requires ARC. Add the "-fobjc-arc" compiler flag for this file.
#endif

#import "config.h"
#import "WebExtensionAPIDevToolsInspectedWindow.h"

#if ENABLE(WK_WEB_EXTENSIONS) && ENABLE(INSPECTOR_EXTENSIONS)

#import "APISerializedScriptValue.h"
#import "CocoaHelpers.h"
#import "JSWebExtensionWrapper.h"
#import "MessageSenderInlines.h"
#import "WebExtensionContextMessages.h"
#import "WebExtensionTabIdentifier.h"
#import "WebExtensionUtilities.h"
#import "WebProcess.h"

static NSString * const frameURLKey = @"frameURL";
static NSString * const ignoreCacheKey = @"ignoreCache";

static NSString * const isExceptionKey = @"isException";
static NSString * const valueKey = @"value";

namespace WebKit {

void WebExtensionAPIDevToolsInspectedWindow::eval(WebPage& page, NSString *expression, NSDictionary *options, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/devtools/inspectedWindow/eval

    static NSDictionary<NSString *, id> *types = @{
        frameURLKey: NSString.class,
    };

    if (!validateDictionary(options, @"options", nil, types, outExceptionString))
        return;

    // FIXME: <https://webkit.org/b/269349> Implement `contextSecurityOrigin` and `useContentScriptContext` options for `devtools.inspectedWindow.eval` command

    std::optional<WTF::URL> frameURL;
    if (NSString *url = options[frameURLKey]) {
        frameURL = URL { url };

        if (!frameURL.value().isValid()) {
            *outExceptionString = toErrorString(nil, frameURLKey, @"'%@' is not a valid URL", url);
            return;
        }
    }

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::DevToolsInspectedWindowEval(page.webPageProxyIdentifier(), expression, frameURL), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Expected<Expected<std::span<const uint8_t>, WebCore::ExceptionDetails>, WebExtensionError>&& result) mutable {
        if (!result) {
            callback->reportError(result.error());
            return;
        }

        auto *undefinedValue = [JSValue valueWithUndefinedInContext:[JSContext contextWithJSGlobalContextRef:callback->globalContext()]];

        if (!result.value()) {
            // If an error occurred, element 0 will be undefined, and element 1 will contain an object giving details about the error.
            callback->call(@[ undefinedValue, @{ isExceptionKey: @YES, valueKey: result.value().error().message } ]);
            return;
        }

        Ref serializedValue = API::SerializedScriptValue::createFromWireBytes(result.value().value());
        id scriptResult = API::SerializedScriptValue::deserialize(serializedValue->internalRepresentation());

        // If no error occurred, element 0 will contain the result of evaluating the expression, and element 1 will be undefined.
        callback->call(@[ scriptResult ?: undefinedValue, undefinedValue ]);
    }, extensionContext().identifier());
}

void WebExtensionAPIDevToolsInspectedWindow::reload(WebPage& page, NSDictionary *options, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/devtools/inspectedWindow/reload

    static NSDictionary<NSString *, id> *types = @{
        ignoreCacheKey: @YES.class,
    };

    if (!validateDictionary(options, @"options", nil, types, outExceptionString))
        return;

    // FIXME: <https://webkit.org/b/222328> Implement `userAgent` and `injectedScript` options for `devtools.inspectedWindow.reload` command

    std::optional<bool> ignoreCache;
    if (NSNumber *value = options[ignoreCacheKey])
        ignoreCache = value.boolValue;

    WebProcess::singleton().send(Messages::WebExtensionContext::DevToolsInspectedWindowReload(page.webPageProxyIdentifier(), ignoreCache), extensionContext().identifier());
}

double WebExtensionAPIDevToolsInspectedWindow::tabId(WebPage& page)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/devtools/inspectedWindow/tabId

    auto result = extensionContext().tabIdentifier(page);
    return toWebAPI(result ? result.value() : WebExtensionTabConstants::NoneIdentifier);
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS) && ENABLE(INSPECTOR_EXTENSIONS)
