/*
 * Copyright (C) 2023-2024 Apple Inc. All rights reserved.
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
#import "WKWebExtensionPermission.h"
#import "WebExtensionConstants.h"
#import "WebExtensionUtilities.h"
#import "_WKWebExtensionDeclarativeNetRequestSQLiteStore.h"
#import "_WKWebExtensionSQLiteStore.h"
#import <wtf/BlockPtr.h>
#import <wtf/cocoa/VectorCocoa.h>

static NSString * const declarativeNetRequestRulesetStateKey = @"DeclarativeNetRequestRulesetState";
static NSString * const displayBlockedResourceCountAsBadgeTextStateKey = @"DisplayBlockedResourceCountAsBadgeText";

namespace WebKit {

bool WebExtensionContext::isDeclarativeNetRequestMessageAllowed()
{
    return isLoaded() && (hasPermission(WKWebExtensionPermissionDeclarativeNetRequest) || hasPermission(WKWebExtensionPermissionDeclarativeNetRequestWithHostAccess));
}

void WebExtensionContext::declarativeNetRequestGetEnabledRulesets(CompletionHandler<void(Vector<String>&&)>&& completionHandler)
{
    Vector<String> enabledRulesets;
    for (auto& identifier : m_enabledStaticRulesetIDs)
        enabledRulesets.append(identifier);

    completionHandler(WTFMove(enabledRulesets));
}

void WebExtensionContext::loadDeclarativeNetRequestRulesetStateFromStorage()
{
    m_enabledStaticRulesetIDs.clear();

    auto *savedRulesetState = objectForKey<NSDictionary>(m_state, declarativeNetRequestRulesetStateKey);
    if (!savedRulesetState.count) {
        // Populate with the default enabled state.
        for (auto& ruleset : protectedExtension()->declarativeNetRequestRulesets()) {
            if (ruleset.enabled)
                m_enabledStaticRulesetIDs.add(ruleset.rulesetID);
        }

        return;
    }

    RefPtr extension = m_extension;
    for (NSString *savedIdentifier in savedRulesetState) {
        auto ruleset = extension->declarativeNetRequestRuleset(savedIdentifier);
        if (!ruleset)
            continue;

        if (objectForKey<NSNumber>(savedRulesetState, savedIdentifier).boolValue)
            m_enabledStaticRulesetIDs.add(savedIdentifier);
        else
            m_enabledStaticRulesetIDs.remove(savedIdentifier);
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
    m_enabledStaticRulesetIDs.clear();
}

WebExtensionContext::DeclarativeNetRequestValidatedRulesets WebExtensionContext::declarativeNetRequestValidateRulesetIdentifiers(const Vector<String>& rulesetIdentifiers)
{
    WebExtension::DeclarativeNetRequestRulesetVector validatedRulesets;

    RefPtr extension = m_extension;
    for (auto& identifier : rulesetIdentifiers) {
        auto ruleset = extension->declarativeNetRequestRuleset(identifier);
        if (!ruleset)
            return toWebExtensionError(@"declarativeNetRequest.updateEnabledRulesets()", nil, @"Failed to apply rules. Invalid ruleset id: %@.", (NSString *)identifier);

        validatedRulesets.append(ruleset.value());
    }

    return WTFMove(validatedRulesets);
}

void WebExtensionContext::declarativeNetRequestToggleRulesets(const Vector<String>& rulesetIdentifiers, bool newValue, NSMutableDictionary *rulesetIdentifiersToEnabledState)
{
    RefPtr extension = m_extension;
    for (auto& identifier : rulesetIdentifiers) {
        auto ruleset = extension->declarativeNetRequestRuleset(identifier);
        if (!ruleset)
            continue;

        if (newValue)
            m_enabledStaticRulesetIDs.add(identifier);
        else
            m_enabledStaticRulesetIDs.remove(identifier);

        [rulesetIdentifiersToEnabledState setObject:@(newValue) forKey:identifier];
    }
}

void WebExtensionContext::declarativeNetRequestUpdateEnabledRulesets(const Vector<String>& rulesetIdentifiersToEnable, const Vector<String>& rulesetIdentifiersToDisable, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    if (rulesetIdentifiersToEnable.isEmpty() && rulesetIdentifiersToDisable.isEmpty()) {
        completionHandler({ });
        return;
    }

    auto validatedRulesetsToEnable = declarativeNetRequestValidateRulesetIdentifiers(rulesetIdentifiersToEnable);
    if (!validatedRulesetsToEnable) {
        completionHandler(makeUnexpected(validatedRulesetsToEnable.error()));
        return;
    }

    auto validatedRulesetsToDisable = declarativeNetRequestValidateRulesetIdentifiers(rulesetIdentifiersToDisable);
    if (!validatedRulesetsToDisable) {
        completionHandler(makeUnexpected(validatedRulesetsToDisable.error()));
        return;
    }

    if (declarativeNetRequestEnabledRulesetCount() - rulesetIdentifiersToDisable.size() + rulesetIdentifiersToEnable.size() > webExtensionDeclarativeNetRequestMaximumNumberOfEnabledRulesets) {
        completionHandler(toWebExtensionError(@"declarativeNetRequest.updateEnabledRulesets()", nil, @"The number of enabled static rulesets exceeds the limit. Only %lu rulesets can be enabled at once.", webExtensionDeclarativeNetRequestMaximumNumberOfEnabledRulesets));
        return;
    }

    NSMutableDictionary *rulesetIdentifiersToEnabledState = [NSMutableDictionary dictionary];
    declarativeNetRequestToggleRulesets(rulesetIdentifiersToDisable, false, rulesetIdentifiersToEnabledState);
    declarativeNetRequestToggleRulesets(rulesetIdentifiersToEnable, true, rulesetIdentifiersToEnabledState);

    loadDeclarativeNetRequestRules([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler), rulesetIdentifiersToEnable, rulesetIdentifiersToDisable, rulesetIdentifiersToEnabledState = RetainPtr { rulesetIdentifiersToEnabledState }](bool success) mutable {
        if (success) {
            saveDeclarativeNetRequestRulesetStateToStorage(rulesetIdentifiersToEnabledState.get());
            completionHandler({ });
            return;
        }

        // If loading the rules failed, undo the changed rulesets to get us back to a working state. We don't need to save anything to disk since
        // we only do that in the success case.
        declarativeNetRequestToggleRulesets(rulesetIdentifiersToDisable, true, rulesetIdentifiersToEnabledState.get());
        declarativeNetRequestToggleRulesets(rulesetIdentifiersToEnable, false, rulesetIdentifiersToEnabledState.get());

        completionHandler(toWebExtensionError(@"declarativeNetRequest.updateEnabledRulesets()", nil, @"Failed to apply rules."));
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

void WebExtensionContext::declarativeNetRequestDisplayActionCountAsBadgeText(bool displayActionCountAsBadgeText, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    if (shouldDisplayBlockedResourceCountAsBadgeText() == displayActionCountAsBadgeText) {
        completionHandler({ });
        return;
    }

    saveShouldDisplayBlockedResourceCountAsBadgeText(displayActionCountAsBadgeText);
    if (!displayActionCountAsBadgeText) {
        for (auto entry : m_actionTabMap)
            Ref { entry.value }->clearBlockedResourceCount();
    }

    completionHandler({ });
}

void WebExtensionContext::declarativeNetRequestIncrementActionCount(WebExtensionTabIdentifier tabIdentifier, double increment, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    RefPtr tab = getTab(tabIdentifier);
    if (!tab) {
        completionHandler(toWebExtensionError(@"declarativeNetRequest.setExtensionActionOptions()", nil, @"tab not found"));
        return;
    }

    incrementActionCountForTab(*tab, increment);
    completionHandler({ });
}

void WebExtensionContext::declarativeNetRequestGetMatchedRules(std::optional<WebExtensionTabIdentifier> tabIdentifier, std::optional<WallTime> minTimeStamp, CompletionHandler<void(Expected<Vector<WebExtensionMatchedRuleParameters>, WebExtensionError>&&)>&& completionHandler)
{
    RefPtr tab = tabIdentifier ? getTab(tabIdentifier.value()) : nullptr;

    static NSString * const apiName = @"declarativeNetRequest.getMatchedRules()";
    if (tabIdentifier && !tab) {
        completionHandler(toWebExtensionError(apiName, nil, @"tab not found"));
        return;
    }

    if (!hasPermission(WKWebExtensionPermissionDeclarativeNetRequestFeedback)) {
        if (!tab->extensionHasTemporaryPermission()) {
            completionHandler(toWebExtensionError(apiName, nil, @"the 'activeTab' permission has not been granted by the user for the tab"));
            return;
        }
    }

    WallTime minTime = minTimeStamp ? minTimeStamp.value() : WallTime::nan();

    DeclarativeNetRequestMatchedRuleVector filteredRules;

    URLVector allURLs;
    for (auto& matchedRule : matchedRules()) {
        if (tabIdentifier && matchedRule.tabIdentifier != tabIdentifier)
            continue;

        if (minTime != WallTime::nan() && matchedRule.timeStamp <= minTime)
            continue;

        filteredRules.append(matchedRule);
        allURLs.append(matchedRule.url);
    }

    requestPermissionToAccessURLs(allURLs, nullptr, [this, protectedThis = Ref { *this }, filteredRules = WTFMove(filteredRules), completionHandler = WTFMove(completionHandler)](auto&& requestedURLs, auto&& allowedURLs, auto expirationDate) mutable {
        auto result = WTF::compactMap(filteredRules, [&](auto& matchedRule) -> std::optional<WebExtensionMatchedRuleParameters> {
            return hasPermission(matchedRule.url) ? std::optional(matchedRule) : std::nullopt;
        });

        completionHandler(WTFMove(result));
    });
}

_WKWebExtensionDeclarativeNetRequestSQLiteStore *WebExtensionContext::declarativeNetRequestDynamicRulesStore()
{
    if (!m_declarativeNetRequestDynamicRulesStore)
        m_declarativeNetRequestDynamicRulesStore = [[_WKWebExtensionDeclarativeNetRequestSQLiteStore alloc] initWithUniqueIdentifier:uniqueIdentifier() storageType:_WKWebExtensionDeclarativeNetRequestStorageType::Dynamic directory:storageDirectory() usesInMemoryDatabase:!storageIsPersistent()];

    return m_declarativeNetRequestDynamicRulesStore.get();
}

_WKWebExtensionDeclarativeNetRequestSQLiteStore *WebExtensionContext::declarativeNetRequestSessionRulesStore()
{
    if (!m_declarativeNetRequestSessionRulesStore)
        m_declarativeNetRequestSessionRulesStore = [[_WKWebExtensionDeclarativeNetRequestSQLiteStore alloc] initWithUniqueIdentifier:uniqueIdentifier() storageType:_WKWebExtensionDeclarativeNetRequestStorageType::Session directory:storageDirectory() usesInMemoryDatabase:YES];

    return m_declarativeNetRequestSessionRulesStore.get();
}

void WebExtensionContext::updateDeclarativeNetRequestRulesInStorage(_WKWebExtensionDeclarativeNetRequestSQLiteStore *storage, NSString *storageType, NSString *apiName, NSArray *rulesToAdd, NSArray *ruleIDsToRemove, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    [storage createSavepointWithCompletionHandler:makeBlockPtr([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler), storage, storageType, apiName, rulesToAdd, ruleIDsToRemove](NSUUID *savepointIdentifier, NSString *errorMessage) mutable {
        if (errorMessage)
            RELEASE_LOG_ERROR(Extensions, "Unable to create %{public}@ rules savepoint for extension %{private}@. Error: %{public}@", storageType, (NSString *)uniqueIdentifier(), errorMessage);

        if (errorMessage.length) {
            completionHandler(toWebExtensionError(apiName, nil, errorMessage));
            return;
        }

        [storage updateRulesByRemovingIDs:ruleIDsToRemove addRules:rulesToAdd completionHandler:makeBlockPtr([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler), storage, storageType, apiName, savepointIdentifier](NSString *errorMessage) mutable {
            if (errorMessage)
                RELEASE_LOG_ERROR(Extensions, "Unable to update %{public}@ rules for extension %{private}@. Error: %{public}@", storageType, (NSString *)uniqueIdentifier(), errorMessage);

            if (errorMessage.length) {
                // Update was unsucessful, rollback the changes to the database.
                [storage rollbackToSavepoint:savepointIdentifier completionHandler:makeBlockPtr([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler), storageType, apiName, errorMessage](NSString *savepointErrorMessage) mutable {
                    if (savepointErrorMessage)
                        RELEASE_LOG_ERROR(Extensions, "Unable to rollback to %{public}@ rules savepoint for extension %{private}@. Error: %{public}@", storageType, (NSString *)uniqueIdentifier(), savepointErrorMessage);

                    completionHandler(toWebExtensionError(apiName, nil, errorMessage));
                }).get()];

                return;
            }

            // Update was sucessful, load the new rules.
            loadDeclarativeNetRequestRules([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler), storageType, apiName, storage, savepointIdentifier](bool success) mutable {
                if (!success) {
                    // Load was unsucessful, rollback the changes to the database.
                    [storage rollbackToSavepoint:savepointIdentifier completionHandler:makeBlockPtr([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler), storageType, apiName](NSString *savepointErrorMessage) mutable {
                        if (savepointErrorMessage)
                            RELEASE_LOG_ERROR(Extensions, "Unable to rollback to %{public}@ rules savepoint for extension %{private}@. Error: %{public}@", storageType, (NSString *)uniqueIdentifier(), savepointErrorMessage);

                        // Load the declarativeNetRequest rules again after rolling back the dynamic update.
                        loadDeclarativeNetRequestRules([completionHandler = WTFMove(completionHandler), apiName](bool success) mutable {
                            if (!success) {
                                completionHandler(toWebExtensionError(apiName, nil, @"unable to load declarativeNetRequest rules"));
                                return;
                            }

                            completionHandler({ });
                        });
                    }).get()];

                    return;
                }

                // Load was successful, commit the changes to the database.
                [storage commitSavepoint:savepointIdentifier completionHandler:makeBlockPtr([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler), storageType](NSString *savepointErrorMessage) mutable {
                    if (savepointErrorMessage)
                        RELEASE_LOG_ERROR(Extensions, "Unable to commit %{public}@ rules savepoint for extension %{private}@. Error: %{public}@", storageType, (NSString *)uniqueIdentifier(), savepointErrorMessage);

                    completionHandler({ });
                }).get()];
            });
        }).get()];
    }).get()];
}

void WebExtensionContext::declarativeNetRequestGetDynamicRules(CompletionHandler<void(Expected<String, WebExtensionError>&&)>&& completionHandler)
{
    [declarativeNetRequestDynamicRulesStore() getRulesWithCompletionHandler:makeBlockPtr([protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](NSArray *rules, NSString *errorMessage) mutable {
        if (errorMessage) {
            completionHandler(toWebExtensionError(@"declarativeNetRequest.getDynamicRules()", nil, errorMessage));
            return;
        }

        completionHandler(String(encodeJSONString(rules, JSONOptions::FragmentsAllowed)));
    }).get()];
}

void WebExtensionContext::declarativeNetRequestUpdateDynamicRules(String&& rulesToAddJSON, Vector<double>&& ruleIDsToDeleteVector, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"declarativeNetRequest.updateDynamicRules()";

    auto *ruleIDsToDelete = createNSArray(ruleIDsToDeleteVector, [this](double ruleID) -> NSNumber * {
        if (!m_dynamicRulesIDs.contains(ruleID))
            return nil;
        return @(ruleID);
    }).get();

    auto *rulesToAdd = dynamic_objc_cast<NSArray>(parseJSON(rulesToAddJSON, JSONOptions::FragmentsAllowed)) ?: @[ ];

    if (!ruleIDsToDelete.count && !rulesToAdd.count) {
        completionHandler({ });
        return;
    }

    auto updatedDynamicRulesCount = m_dynamicRulesIDs.size() + rulesToAdd.count - ruleIDsToDelete.count;
    if (updatedDynamicRulesCount + m_sessionRulesIDs.size() > webExtensionDeclarativeNetRequestMaximumNumberOfDynamicAndSessionRules) {
        completionHandler(toWebExtensionError(apiName, nil, @"Failed to add dynamic rules. Maximum number of dynamic and session rules exceeded."));
        return;
    }

    updateDeclarativeNetRequestRulesInStorage(declarativeNetRequestDynamicRulesStore(), @"dynamic", apiName, rulesToAdd, ruleIDsToDelete, WTFMove(completionHandler));
}

void WebExtensionContext::declarativeNetRequestGetSessionRules(CompletionHandler<void(Expected<String, WebExtensionError>&&)>&& completionHandler)
{
    [declarativeNetRequestSessionRulesStore() getRulesWithCompletionHandler:makeBlockPtr([protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](NSArray *rules, NSString *errorMessage) mutable {
        if (errorMessage) {
            completionHandler(toWebExtensionError(@"declarativeNetRequest.getSessionRules()", nil, errorMessage));
            return;
        }

        completionHandler(String(encodeJSONString(rules, JSONOptions::FragmentsAllowed)));
    }).get()];
}

void WebExtensionContext::declarativeNetRequestUpdateSessionRules(String&& rulesToAddJSON, Vector<double>&& ruleIDsToDeleteVector, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"declarativeNetRequest.updateSessionRules()";

    auto *ruleIDsToDelete = createNSArray(ruleIDsToDeleteVector, [this](double ruleID) -> NSNumber * {
        if (!m_sessionRulesIDs.contains(ruleID))
            return nil;
        return @(ruleID);
    }).get();

    auto *rulesToAdd = dynamic_objc_cast<NSArray>(parseJSON(rulesToAddJSON, JSONOptions::FragmentsAllowed)) ?: @[ ];

    if (!ruleIDsToDelete.count && !rulesToAdd.count) {
        completionHandler({ });
        return;
    }

    auto updatedSessionRulesCount = m_sessionRulesIDs.size() + rulesToAdd.count - ruleIDsToDelete.count;
    if (updatedSessionRulesCount + m_dynamicRulesIDs.size() > webExtensionDeclarativeNetRequestMaximumNumberOfDynamicAndSessionRules) {
        completionHandler(toWebExtensionError(apiName, nil, @"Failed to add session rules. Maximum number of dynamic and session rules exceeded."));
        return;
    }

    updateDeclarativeNetRequestRulesInStorage(declarativeNetRequestSessionRulesStore(), @"session", apiName, rulesToAdd, ruleIDsToDelete, WTFMove(completionHandler));
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
