/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#import "WebExtensionAPILocalization.h"

#import "CocoaHelpers.h"
#import "FoundationSPI.h"
#import "Logging.h"
#import "MessageSenderInlines.h"
#import "WebExtensionContextMessages.h"
#import "WebExtensionContextProxy.h"
#import "WebExtensionLocalization.h"
#import "WebExtensionUtilities.h"
#import "WebProcess.h"
#import <JavaScriptCore/APICast.h>
#import <JavaScriptCore/ScriptCallStack.h>
#import <JavaScriptCore/ScriptCallStackFactory.h>
#import <wtf/CompletionHandler.h>
#import <wtf/cocoa/VectorCocoa.h>

#if ENABLE(WK_WEB_EXTENSIONS)

namespace WebKit {

class JSWebExtensionWrappable;

NSString *WebExtensionAPILocalization::getMessage(NSString* messageName, id substitutions)
{
    // Documentation: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/i18n/getMessage

    RefPtr localization = extensionContext().localization();
    if (!localization)
        return @"";

    NSArray<NSString *> *substitutionsArray;
    if ([substitutions isKindOfClass:NSString.class])
        substitutionsArray = @[ substitutions ];
    else if ([substitutions isKindOfClass:NSArray.class]) {
        substitutionsArray = filterObjects((NSArray *)substitutions, ^bool(id, id value) {
            return [value isKindOfClass:NSString.class];
        });
    }

    auto substitutionsVector = makeVector<String>(substitutionsArray);

    return localization->localizedStringForKey(messageName, substitutionsVector).createNSString().autorelease();
}

NSString *WebExtensionAPILocalization::getUILanguage()
{
    // Documentation: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/i18n/getUILanguage

    return toWebAPI(NSLocale.currentLocale);
}

void WebExtensionAPILocalization::getAcceptLanguages(Ref<WebExtensionCallbackHandler>&& callback)
{
    // Documentation: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/i18n/getAcceptLanguages

    NSArray<NSString *> *preferredLocaleIdentifiers = NSLocale.preferredLanguages;
    NSMutableOrderedSet<NSString *> *acceptLanguages = [NSMutableOrderedSet orderedSetWithCapacity:preferredLocaleIdentifiers.count];
    for (NSString *localeIdentifier in preferredLocaleIdentifiers) {
        [acceptLanguages addObject:localeIdentifier];
        [acceptLanguages addObject:[NSLocale localeWithLocaleIdentifier:localeIdentifier].languageCode];
    }

    callback->call(toJSValueRef(callback->globalContext(), acceptLanguages.array));
}

void WebExtensionAPILocalization::getPreferredSystemLanguages(Ref<WebExtensionCallbackHandler>&& callback)
{
    callback->call(toJSValueRef(callback->globalContext(), NSLocale.preferredLanguages));
}

void WebExtensionAPILocalization::getSystemUILanguage(Ref<WebExtensionCallbackHandler>&& callback)
{
    callback->call(toJSValueRef(callback->globalContext(), NSLocale._deviceLanguage));
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
