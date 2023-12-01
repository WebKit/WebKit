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
#import "WebExtensionContext.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#import "CocoaHelpers.h"
#import "WebExtensionDeclarativeNetRequestConstants.h"
#import "WebExtensionUtilities.h"

static NSString * const declarativeNetRequestRulesetStateKey = @"DeclarativeNetRequestRulesetState";

namespace WebKit {

void WebExtensionContext::declarativeNetRequestGetEnabledRulesets(CompletionHandler<void(const Vector<String>&)>&& completionHandler)
{
    Vector<String> enabledRulesets;
    for (auto& ruleset : extension().declarativeNetRequestRulesets()) {
        if (ruleset.enabled)
            enabledRulesets.append(ruleset.rulesetID);
    }

    completionHandler(enabledRulesets);
}

void WebExtensionContext::loadDeclarativeNetRequestRulesetStateFromStorage()
{
    NSDictionary *savedRulesetState = objectForKey<NSDictionary>(m_state, declarativeNetRequestRulesetStateKey);
    for (NSString *savedIdentifier in savedRulesetState) {
        for (auto& ruleset : extension().declarativeNetRequestRulesets()) {
            if (ruleset.rulesetID == String(savedIdentifier)) {
                ruleset.enabled = objectForKey<NSNumber>(savedRulesetState, savedIdentifier).boolValue;
                break;
            }
        }
    }
}

void WebExtensionContext::saveDeclarativeNetRequestRulesetStateToStorage(NSDictionary *rulesetState)
{
    NSDictionary *savedRulesetState = objectForKey<NSDictionary>(m_state, declarativeNetRequestRulesetStateKey);
    NSMutableDictionary *updatedRulesetState = savedRulesetState ? [savedRulesetState mutableCopy] : [NSMutableDictionary dictionary];

    [updatedRulesetState addEntriesFromDictionary:rulesetState];

    [m_state setObject:[updatedRulesetState copy] forKey:declarativeNetRequestRulesetStateKey];
    writeStateToStorage();
}

void WebExtensionContext::clearDeclarativeNetRequestRulesetState()
{
    [m_state removeObjectForKey:declarativeNetRequestRulesetStateKey];
}

WebExtensionContext::DeclarativeNetRequestValidatedRulesets WebExtensionContext::declarativeNetRequestValidateRulesetIdentifiers(const Vector<String>& rulesetIdentifiers)
{
    WebExtension::DeclarativeNetRequestRulesetVector validatedRulesets;

    WebExtension::DeclarativeNetRequestRulesetVector rulesets = extension().declarativeNetRequestRulesets();

    for (auto identifier : rulesetIdentifiers) {
        bool found = false;
        for (auto ruleset : rulesets) {
            if (ruleset.rulesetID == identifier) {
                validatedRulesets.append(ruleset);
                found = true;
                break;
            }
        }

        if (!found)
            return { std::nullopt, toErrorString(@"declarativeNetRequest.updateEnabledRulesets()", nil, @"Failed to apply rules. Invalid ruleset id: %@.", (NSString *)identifier) };
    }

    return { validatedRulesets, std::nullopt };
}

size_t WebExtensionContext::declarativeNetRequestEnabledRulesetCount()
{
    size_t count = 0;

    for (auto& ruleset : extension().declarativeNetRequestRulesets()) {
        if (ruleset.enabled)
            ++count;
    }

    return count;
}

void WebExtensionContext::declarativeNetRequestToggleRulesets(const Vector<String>& rulesetIdentifiers, bool newValue, NSMutableDictionary *rulesetIdentifiersToEnabledState)
{
    for (auto& identifier : rulesetIdentifiers) {
        for (auto& ruleset : extension().declarativeNetRequestRulesets()) {
            if (ruleset.rulesetID != identifier)
                continue;

            ruleset.enabled = newValue;
            [rulesetIdentifiersToEnabledState setObject:@(newValue) forKey:ruleset.rulesetID];
            break;
        }
    }
}

void WebExtensionContext::declarativeNetRequestUpdateEnabledRulesets(const Vector<String>& rulesetIdentifiersToEnable, const Vector<String>& rulesetIdentifiersToDisable, CompletionHandler<void(std::optional<String>)>&& completionHandler)
{
    if (rulesetIdentifiersToEnable.isEmpty() && rulesetIdentifiersToDisable.isEmpty()) {
        completionHandler(std::nullopt);
        return;
    }

    auto validatedRulesetsToEnable = declarativeNetRequestValidateRulesetIdentifiers(rulesetIdentifiersToEnable);
    if (validatedRulesetsToEnable.second) {
        completionHandler(validatedRulesetsToEnable.second);
        return;
    }

    auto validatedRulesetsToDisable = declarativeNetRequestValidateRulesetIdentifiers(rulesetIdentifiersToDisable);
    if (validatedRulesetsToDisable.second) {
        completionHandler(validatedRulesetsToDisable.second);
        return;
    }

    if (declarativeNetRequestEnabledRulesetCount() - rulesetIdentifiersToDisable.size() + rulesetIdentifiersToEnable.size() > webExtensionDeclarativeNetRequestMaximumNumberOfEnabledRulesets) {
        completionHandler(toErrorString(@"declarativeNetRequest.updateEnabledRulesets()", nil, @"The number of enabled static rulesets exceeds the limit. Only %lu rulesets can be enabled at once.", webExtensionDeclarativeNetRequestMaximumNumberOfEnabledRulesets));
        return;
    }

    NSMutableDictionary *rulesetIdentifiersToEnabledState = [NSMutableDictionary dictionary];
    declarativeNetRequestToggleRulesets(rulesetIdentifiersToDisable, false, rulesetIdentifiersToEnabledState);
    declarativeNetRequestToggleRulesets(rulesetIdentifiersToEnable, true, rulesetIdentifiersToEnabledState);

    loadDeclarativeNetRequestRules([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler), rulesetIdentifiersToEnable, rulesetIdentifiersToDisable, rulesetIdentifiersToEnabledState = RetainPtr { rulesetIdentifiersToEnabledState }](bool success) mutable {
        if (success) {
            saveDeclarativeNetRequestRulesetStateToStorage(rulesetIdentifiersToEnabledState.get());
            completionHandler(std::nullopt);
            return;
        }

        // If loading the rules failed, undo the changed rulesets to get us back to a working state. We don't need to save anything to disk since
        // we only do that in the success case.
        declarativeNetRequestToggleRulesets(rulesetIdentifiersToDisable, true, rulesetIdentifiersToEnabledState.get());
        declarativeNetRequestToggleRulesets(rulesetIdentifiersToEnable, false, rulesetIdentifiersToEnabledState.get());

        completionHandler(toErrorString(@"declarativeNetRequest.updateEnabledRulesets()", nil, @"Failed to apply rules."));
    });
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
