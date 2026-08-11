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

#if !__has_feature(objc_arc)
#error This file requires ARC. Add the "-fobjc-arc" compiler flag for this file.
#endif

#import "config.h"
#import "WebExtensionAPIOffscreen.h"

#if ENABLE(WK_WEB_EXTENSIONS_OFFSCREEN)

#import "MessageSenderInlines.h"
#import "WebExtensionAPIKeys.h"
#import "WebExtensionContextMessages.h"
#import "WebExtensionOffscreenDocumentParameters.h"
#import "WebProcess.h"

namespace WebKit {

void WebExtensionAPIOffscreen::createDocument(NSDictionary *details, Ref<WebExtensionCallbackHandler>&& callback, NSString **outExceptionString)
{
    // https://developer.chrome.com/docs/extensions/reference/api/offscreen#method-createDocument

    static NSArray<NSString *> *requiredKeys = @[ urlKey, justificationKey, reasonsKey ];

    static NSDictionary<NSString *, id> *types = @{
        urlKey: NSString.class,
        justificationKey: NSString.class,
        reasonsKey: @[ NSString.class ],
    };

    if (!validateDictionary(details, @"parameters", requiredKeys, types, outExceptionString))
        return;

    WebExtensionOffscreenDocumentParameters parameters;
    parameters.justification = details[justificationKey];
    parameters.url = details[urlKey];
    parameters.reasons = makeVector<String>(details[reasonsKey]);

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::OffscreenCreateDocument(parameters), [protectedThis = Ref { *this }, callback = WTF::move(callback)](Expected<void, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error().createNSString().get());
            return;
        }

        callback->call();
    }, extensionContext().identifier());
}

void WebExtensionAPIOffscreen::closeDocument(Ref<WebExtensionCallbackHandler>&& callback)
{
    // https://developer.chrome.com/docs/extensions/reference/api/offscreen#method-closeDocument

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::OffscreenCloseDocument(), [protectedThis = Ref { *this }, callback = WTF::move(callback)](Expected<void, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error().createNSString().get());
            return;
        }

        callback->call();
    }, extensionContext().identifier());
}

void WebExtensionAPIOffscreen::hasDocument(Ref<WebExtensionCallbackHandler>&& callback)
{
    // https://developer.chrome.com/docs/extensions/reference/api/offscreen#method-hasDocument

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::OffscreenHasDocument(), [protectedThis = Ref { *this }, callback = WTF::move(callback)](Expected<bool, WebExtensionError>&& result) {
        if (!result) {
            callback->reportError(result.error().createNSString().get());
            return;
        }

        callback->call(JSValueMakeBoolean(callback->globalContext(), result.value()));
    }, extensionContext().identifier());
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS_OFFSCREEN)

