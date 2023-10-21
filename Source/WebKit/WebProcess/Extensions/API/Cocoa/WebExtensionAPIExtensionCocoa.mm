/*
 * Copyright (C) 2022-2023 Apple Inc. All rights reserved.
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
#import "WebExtensionAPIExtension.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#import "MessageSenderInlines.h"
#import "WebExtensionContextMessages.h"
#import "WebPage.h"
#import "WebProcess.h"

namespace WebKit {

bool WebExtensionAPIExtension::isPropertyAllowed(ASCIILiteral name, WebPage*)
{
    // This method was removed in manifest version 3.
    if (name == "getURL"_s)
        return !extensionContext().supportsManifestVersion(3);

    ASSERT_NOT_REACHED();
    return false;
}

NSURL *WebExtensionAPIExtension::getURL(NSString *resourcePath, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/extension/getURL

    return URL { extensionContext().baseURL(), resourcePath };
}

bool WebExtensionAPIExtension::isInIncognitoContext(WebPage* page)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/extension/inIncognitoContext

    return page->usesEphemeralSession();
}

void WebExtensionAPIExtension::isAllowedFileSchemeAccess(Ref<WebExtensionCallbackHandler>&& callback)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/extension/isAllowedFileSchemeAccess
    // FIXME: rdar://problem/58428135 Consider allowing file URL access if the user opted in explicitly in some way.

    callback->call(@NO);
}

void WebExtensionAPIExtension::isAllowedIncognitoAccess(Ref<WebExtensionCallbackHandler>&& callback)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/extension/isAllowedIncognitoAccess

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::ExtensionIsAllowedIncognitoAccess(), [protectedThis = Ref { *this }, callback = WTFMove(callback)](bool result) {
        callback->call(@(result));
    }, extensionContext().identifier());
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
