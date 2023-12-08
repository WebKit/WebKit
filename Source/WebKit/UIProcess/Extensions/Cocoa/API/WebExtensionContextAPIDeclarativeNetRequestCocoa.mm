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
#import "_WKWebExtensionDeclarativeNetRequestSQLiteStore.h"
#import "_WKWebExtensionSQLiteStore.h"
#import <wtf/BlockPtr.h>
#import <wtf/cocoa/VectorCocoa.h>

static NSString * const declarativeNetRequestRulesetStateKey = @"DeclarativeNetRequestRulesetState";
static NSString * const displayBlockedResourceCountAsBadgeTextStateKey = @"DisplayBlockedResourceCountAsBadgeText";

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

bool WebExtensionContext::shouldDisplayBlockedResourceCountAsBadgeText()
{
    return objectForKey<NSNumber>(m_state, displayBlockedResourceCountAsBadgeTextStateKey).boolValue;
}

void WebExtensionContext::saveShouldDisplayBlockedResourceCountAsBadgeText(bool shouldDisplay)
{
    [m_state setObject:@(shouldDisplay) forKey:displayBlockedResourceCountAsBadgeTextStateKey];
    writeStateToStorage();
}

void WebExtensionContext::incrementActionCountForTab(WebExtensionTab& tab, ssize_t incrementAmount)
{
    if (!shouldDisplayBlockedResourceCountAsBadgeText())
        return;

    RefPtr tabAction = getOrCreateAction(&tab);
    tabAction->incrementBlockedResourceCount(incrementAmount);
}

void WebExtensionContext::declarativeNetRequestDisplayActionCountAsBadgeText(bool displayActionCountAsBadgeText, CompletionHandler<void(std::optional<String>)>&& completionHandler)
{
    if (shouldDisplayBlockedResourceCountAsBadgeText() == displayActionCountAsBadgeText) {
        completionHandler(std::nullopt);
        return;
    }

    saveShouldDisplayBlockedResourceCountAsBadgeText(displayActionCountAsBadgeText);
    if (!displayActionCountAsBadgeText) {
        for (auto entry : m_actionTabMap)
            entry.value->clearBlockedResourceCount();
    }

    completionHandler(std::nullopt);
}

void WebExtensionContext::declarativeNetRequestIncrementActionCount(WebExtensionTabIdentifier tabIdentifier, double increment, CompletionHandler<void(std::optional<String>)>&& completionHandler)
{
    RefPtr tab = getTab(tabIdentifier);
    if (!tab) {
        completionHandler(toErrorString(@"declarativeNetRequest.setExtensionActionOptions()", nil, @"tab not found"));
        return;
    }

    incrementActionCountForTab(*tab, increment);
    completionHandler(std::nullopt);
}

void WebExtensionContext::declarativeNetRequestGetMatchedRules(std::optional<WebExtensionTabIdentifier> tabIdentifier, std::optional<WallTime> minTimeStamp, CompletionHandler<void(std::optional<Vector<WebExtensionMatchedRuleParameters>> matchedRules, std::optional<String>)>&& completionHandler)
{
    RefPtr tab = tabIdentifier ? getTab(tabIdentifier.value()) : nullptr;

    static NSString * const apiName = @"declarativeNetRequest.getMatchedRules()";
    if (tabIdentifier && !tab) {
        completionHandler(std::nullopt, toErrorString(apiName, nil, @"tab not found"));
        return;
    }

    if (!hasPermission(_WKWebExtensionPermissionDeclarativeNetRequestFeedback)) {
        ASSERT(hasPermission(_WKWebExtensionPermissionActiveTab));

        if (!hasPermission(_WKWebExtensionPermissionTabs, tab.get())) {
            completionHandler(std::nullopt, toErrorString(apiName, nil, @"The 'activeTab' permission has not been granted by the user for the specified tab."));
            return;
        }
    }

    WallTime minTime = minTimeStamp ? minTimeStamp.value() : WallTime::nan();

    DeclarativeNetRequestMatchedRuleVector filteredMatchedRules;
    for (auto matchedRule : matchedRules()) {
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=262714 - we should be requesting permission if we don't have access to this URL.
        if (!hasPermission(matchedRule.url))
            continue;

        if (tabIdentifier && matchedRule.tabIdentifier != tabIdentifier)
            continue;

        if (minTime != WallTime::nan() && matchedRule.timeStamp <= minTime)
            continue;

        filteredMatchedRules.append(matchedRule);
    }

    completionHandler(filteredMatchedRules, std::nullopt);
}

_WKWebExtensionDeclarativeNetRequestSQLiteStore *WebExtensionContext::declarativeNetRequestDynamicRulesStore()
{
    if (!m_declarativeNetRequestDynamicRulesStore)
        m_declarativeNetRequestDynamicRulesStore = [[_WKWebExtensionDeclarativeNetRequestSQLiteStore alloc] initWithUniqueIdentifier:uniqueIdentifier() storageType:_WKWebExtensionDeclarativeNetRequestStorageType::Dynamic directory:m_storageDirectory usesInMemoryDatabase:!storageIsPersistent()];

    return m_declarativeNetRequestDynamicRulesStore.get();
}

_WKWebExtensionDeclarativeNetRequestSQLiteStore *WebExtensionContext::declarativeNetRequestSessionRulesStore()
{
    if (!m_declarativeNetRequestSessionRulesStore)
        m_declarativeNetRequestSessionRulesStore = [[_WKWebExtensionDeclarativeNetRequestSQLiteStore alloc] initWithUniqueIdentifier:uniqueIdentifier() storageType:_WKWebExtensionDeclarativeNetRequestStorageType::Session directory:m_storageDirectory usesInMemoryDatabase:YES];

    return m_declarativeNetRequestSessionRulesStore.get();
}

void WebExtensionContext::updateDeclarativeNetRequestRulesInStorage(_WKWebExtensionDeclarativeNetRequestSQLiteStore *storage, NSString *storageType, NSArray *rulesToAdd, NSArray *ruleIDsToRemove, CompletionHandler<void(std::optional<String>)>&& completionHandler)
{
    [storage createSavepointWithCompletionHandler:makeBlockPtr([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler), storage, storageType, rulesToAdd, ruleIDsToRemove](NSUUID *savepointIdentifier, NSString *errorMessage) mutable {
        if (errorMessage)
            RELEASE_LOG_ERROR(Extensions, "Unable to create %{public}@ rules savepoint for extension %{private}@. Error: %{private}@", storageType, (NSString *)uniqueIdentifier(), errorMessage);

        if (errorMessage.length) {
            completionHandler(errorMessage);
            return;
        }

        [storage updateRulesByRemovingIDs:ruleIDsToRemove addRules:rulesToAdd completionHandler:makeBlockPtr([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler), storage, storageType, savepointIdentifier](NSString *errorMessage) mutable {
            if (errorMessage)
                RELEASE_LOG_ERROR(Extensions, "Unable to update %{public}@ rules for extension %{private}@. Error: %{private}@", storageType, (NSString *)uniqueIdentifier(), errorMessage);

            if (errorMessage.length) {
                // Update was unsucessful, rollback the changes to the database.
                [storage rollbackToSavepoint:savepointIdentifier completionHandler:makeBlockPtr([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler), storageType, errorMessage](NSString *savepointErrorMessage) mutable {
                    if (savepointErrorMessage)
                        RELEASE_LOG_ERROR(Extensions, "Unable to rollback to %{public}@ rules savepoint for extension %{private}@. Error: %{private}@", storageType, (NSString *)uniqueIdentifier(), savepointErrorMessage);

                    completionHandler(errorMessage);
                }).get()];

                return;
            }

            // Update was sucessful, load the new rules.
            loadDeclarativeNetRequestRules([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler), storageType, storage, savepointIdentifier](bool success) mutable {
                if (!success) {
                    // Load was unsucessful, rollback the changes to the database.
                    [storage rollbackToSavepoint:savepointIdentifier completionHandler:makeBlockPtr([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler), storageType](NSString *savepointErrorMessage) mutable {
                        if (savepointErrorMessage)
                            RELEASE_LOG_ERROR(Extensions, "Unable to rollback to %{public}@ rules savepoint for extension %{private}@. Error: %{private}@", storageType, (NSString *)uniqueIdentifier(), savepointErrorMessage);

                        // Load the declarativeNetRequest rules again after rolling back the dynamic update.
                        loadDeclarativeNetRequestRules([completionHandler = WTFMove(completionHandler)](bool success) mutable {
                            completionHandler(success ? nil : @"Unable to load declarativeNetRequest rules.");
                        });
                    }).get()];
                }

                // Load was sucessful, commit the changes to the database.
                [storage commitSavepoint:savepointIdentifier completionHandler:makeBlockPtr([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler), storageType](NSString *savepointErrorMessage) mutable {
                    if (savepointErrorMessage)
                        RELEASE_LOG_ERROR(Extensions, "Unable to commit %{public}@ rules savepoint for extension %{private}@. Error: %{private}@", storageType, (NSString *)uniqueIdentifier(), savepointErrorMessage);

                    completionHandler(std::nullopt);
                }).get()];
            });
        }).get()];
    }).get()];
}

void WebExtensionContext::declarativeNetRequestGetDynamicRules(CompletionHandler<void(std::optional<String> rulesJSON, std::optional<String> error)>&& completionHandler)
{
    [declarativeNetRequestDynamicRulesStore() getRulesWithCompletionHandler:makeBlockPtr([protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](NSArray *rules, NSString *errorMessage) mutable {
        if (errorMessage) {
            completionHandler(std::nullopt, errorMessage);
            return;
        }

        completionHandler(encodeJSONString(rules, JSONOptions::FragmentsAllowed), std::nullopt);
    }).get()];
}

void WebExtensionContext::declarativeNetRequestUpdateDynamicRules(std::optional<String> rulesToAddJSON, std::optional<Vector<double>> ruleIDsToDeleteVector, CompletionHandler<void(std::optional<String>)>&& completionHandler)
{
    auto *ruleIDsToDelete = @[ ];
    if (ruleIDsToDeleteVector) {
        ruleIDsToDelete = createNSArray(ruleIDsToDeleteVector.value(), [](double ruleID) {
            return @(ruleID);
        }).get();
    }

    NSArray *rulesToAdd = rulesToAddJSON ? parseJSON(rulesToAddJSON.value(), JSONOptions::FragmentsAllowed) : @[ ];

    // FIXME: Make sure that adding these rules won't get us over the maximum number of dynamic + session rules.

    if (!ruleIDsToDelete.count && !rulesToAdd.count) {
        completionHandler(std::nullopt);
        return;
    }

    updateDeclarativeNetRequestRulesInStorage(declarativeNetRequestDynamicRulesStore(), @"dynamic", rulesToAdd, ruleIDsToDelete, WTFMove(completionHandler));
}

void WebExtensionContext::declarativeNetRequestGetSessionRules(CompletionHandler<void(std::optional<String> rulesJSON, std::optional<String> error)>&& completionHandler)
{
    [declarativeNetRequestSessionRulesStore() getRulesWithCompletionHandler:makeBlockPtr([protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](NSArray *rules, NSString *errorMessage) mutable {
        if (errorMessage) {
            completionHandler(std::nullopt, errorMessage);
            return;
        }

        completionHandler(encodeJSONString(rules, JSONOptions::FragmentsAllowed), std::nullopt);
    }).get()];
}

void WebExtensionContext::declarativeNetRequestUpdateSessionRules(std::optional<String> rulesToAddJSON, std::optional<Vector<double>> ruleIDsToDeleteVector, CompletionHandler<void(std::optional<String>)>&& completionHandler)
{
    auto *ruleIDsToDelete = @[ ];
    if (ruleIDsToDeleteVector) {
        ruleIDsToDelete = createNSArray(ruleIDsToDeleteVector.value(), [](double ruleID) {
            return @(ruleID);
        }).get();
    }

    NSArray *rulesToAdd = rulesToAddJSON ? parseJSON(rulesToAddJSON.value(), JSONOptions::FragmentsAllowed) : @[ ];

    // FIXME: Make sure that adding these rules won't get us over the maximum number of dynamic + session rules.

    if (!ruleIDsToDelete.count && !rulesToAdd.count) {
        completionHandler(std::nullopt);
        return;
    }

    updateDeclarativeNetRequestRulesInStorage(declarativeNetRequestSessionRulesStore(), @"session", rulesToAdd, ruleIDsToDelete, WTFMove(completionHandler));
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
