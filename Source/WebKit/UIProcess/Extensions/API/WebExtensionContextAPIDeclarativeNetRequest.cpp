/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 * Copyright (C) 2025 Igalia S.L. All rights reserved.
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
#include "WebExtensionContext.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#include "WebExtensionConstants.h"
#include "WebExtensionDeclarativeNetRequestSQLiteStore.h"
#include "WebExtensionPermission.h"
#include <wtf/JSONValues.h>

namespace WebKit {

bool WebExtensionContext::isDeclarativeNetRequestMessageAllowed(IPC::Decoder& message)
{
    return isLoadedAndPrivilegedMessage(message) && (hasPermission(WebExtensionPermission::declarativeNetRequest()) || hasPermission(WebExtensionPermission::declarativeNetRequestWithHostAccess()));
}

void WebExtensionContext::declarativeNetRequestGetEnabledRulesets(CompletionHandler<void(Vector<String>&&)>&& completionHandler)
{
    Vector<String> enabledRulesets;
    for (auto& identifier : m_enabledStaticRulesetIDs)
        enabledRulesets.append(identifier);

    completionHandler(WTFMove(enabledRulesets));
}

WebExtensionContext::DeclarativeNetRequestValidatedRulesets WebExtensionContext::declarativeNetRequestValidateRulesetIdentifiers(const Vector<String>& rulesetIdentifiers)
{
    WebExtension::DeclarativeNetRequestRulesetVector validatedRulesets;

    RefPtr extension = m_extension;
    for (auto& identifier : rulesetIdentifiers) {
        auto ruleset = extension->declarativeNetRequestRuleset(identifier);
        if (!ruleset)
            return toWebExtensionError("declarativeNetRequest.updateEnabledRulesets()"_s, nullString(), "Failed to apply rules. Invalid ruleset id: %s."_s, identifier.utf8().data());

        validatedRulesets.append(ruleset.value());
    }

    return validatedRulesets;
}

Ref<WebExtensionDeclarativeNetRequestSQLiteStore> WebExtensionContext::declarativeNetRequestDynamicRulesStore()
{
    if (!m_declarativeNetRequestDynamicRulesStore)
        m_declarativeNetRequestDynamicRulesStore = WebExtensionDeclarativeNetRequestSQLiteStore::create(uniqueIdentifier(), WebExtensionDeclarativeNetRequestStorageType::Dynamic, storageDirectory(), storageIsPersistent() ? WebExtensionDeclarativeNetRequestSQLiteStore::UsesInMemoryDatabase::No : WebExtensionDeclarativeNetRequestSQLiteStore::UsesInMemoryDatabase::Yes);

    return *m_declarativeNetRequestDynamicRulesStore;
}

Ref<WebExtensionDeclarativeNetRequestSQLiteStore> WebExtensionContext::declarativeNetRequestSessionRulesStore()
{
    if (!m_declarativeNetRequestSessionRulesStore)
        m_declarativeNetRequestSessionRulesStore = WebExtensionDeclarativeNetRequestSQLiteStore::create(uniqueIdentifier(), WebExtensionDeclarativeNetRequestStorageType::Session, storageDirectory(), WebExtensionDeclarativeNetRequestSQLiteStore::UsesInMemoryDatabase::Yes);

    return *m_declarativeNetRequestSessionRulesStore;
}

void WebExtensionContext::updateDeclarativeNetRequestRulesInStorage(RefPtr<WebExtensionDeclarativeNetRequestSQLiteStore> storage, const String& storageType, const String& apiName, Ref<JSON::Array> rulesToAdd, Vector<double> ruleIDsToRemove, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    if (storage) {
        storage->createSavepoint([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler), storage, storageType, apiName, rulesToAdd, ruleIDsToRemove](Markable<WTF::UUID> savepointIdentifier, const String& errorMessage) mutable {
            if (errorMessage.length()) {
                RELEASE_LOG_ERROR(Extensions, "Unable to create %s rules savepoint for extension %s. Error: %s", storageType.utf8().data(), uniqueIdentifier().utf8().data(), errorMessage.utf8().data());
                completionHandler(toWebExtensionError(apiName, nullString(), errorMessage));
                return;
            }

            storage->updateRulesByRemovingIDs(ruleIDsToRemove, rulesToAdd, [this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler), storage, storageType, apiName, savepointIdentifier = WTFMove(savepointIdentifier)](const String& errorMessage) mutable {
                if (errorMessage.length()) {
                    RELEASE_LOG_ERROR(Extensions, "Unable to update %s rules for extension %s. Error: %s", storageType.utf8().data(), uniqueIdentifier().utf8().data(), errorMessage.utf8().data());

                    // Update was unsucessful, rollback the changes to the database.
                    storage->rollbackToSavepoint(savepointIdentifier.value(), [this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler), storageType, apiName, errorMessage](const String& savepointErrorMessage) mutable {
                        if (savepointErrorMessage.length())
                            RELEASE_LOG_ERROR(Extensions, "Unable to rollback to %s rules savepoint for extension %s. Error: %s", storageType.utf8().data(), uniqueIdentifier().utf8().data(), savepointErrorMessage.utf8().data());

                        completionHandler(toWebExtensionError(apiName, nullString(), errorMessage));
                    });

                    return;
                }

                // Update was successful, load the new rules
                loadDeclarativeNetRequestRules([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler), storageType, apiName, storage, savepointIdentifier = WTFMove(savepointIdentifier), errorMessage](bool success) mutable {
                    if (!success) {
                        // Load was unsucessful, rollback the changes to the database.
                        storage->rollbackToSavepoint(savepointIdentifier.value(), [this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler), storageType, apiName, errorMessage](const String& savepointErrorMessage) mutable {
                            if (savepointErrorMessage.length())
                                RELEASE_LOG_ERROR(Extensions, "Unable to rollback to %s rules savepoint for extension %s. Error: %s", storageType.utf8().data(), uniqueIdentifier().utf8().data(), savepointErrorMessage.utf8().data());

                            // Load the declarativeNetRequest rules again after rolling back the dynamic update.
                            loadDeclarativeNetRequestRules([completionHandler = WTFMove(completionHandler), apiName](bool success) mutable {
                                if (!success) {
                                    completionHandler(toWebExtensionError(apiName, nullString(), "unable to load declarativeNetRequest rules"_s));
                                    return;
                                }

                                completionHandler({ });
                            });
                        });

                        return;
                    }

                    // Load was successful, commit the changes to the database.
                    storage->commitSavepoint(savepointIdentifier.value(), [this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler), storageType](const String& savepointErrorMessage) mutable {
                        if (savepointErrorMessage.length())
                            RELEASE_LOG_ERROR(Extensions, "Unable to commit %s rules savepoint for extension %s. Error: %s", storageType.utf8().data(), uniqueIdentifier().utf8().data(), savepointErrorMessage.utf8().data());

                        completionHandler({ });
                    });
                });
            });
        });
    }
}

void WebExtensionContext::declarativeNetRequestGetDynamicRules(Vector<double>&& filter, CompletionHandler<void(Expected<String, WebExtensionError>&&)>&& completionHandler)
{
    auto ruleIDs = compactMap(filter, [&](auto& ruleID) -> std::optional<double> {
        if (m_dynamicRulesIDs.contains(ruleID))
            return ruleID;

        return std::nullopt;
    });

    declarativeNetRequestDynamicRulesStore()->getRulesWithRuleIDs(ruleIDs, [protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](RefPtr<JSON::Array> rules, const String& errorMessage) mutable {
        if (errorMessage.length()) {
            completionHandler(toWebExtensionError("declarativeNetRequest.getDynamicRules()"_s, nullString(), errorMessage));
            return;
        }

        completionHandler(rules->toJSONString());
    });
}

void WebExtensionContext::declarativeNetRequestUpdateDynamicRules(String&& rulesToAddJSON, Vector<double>&& ruleIDsToDeleteVector, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    static constexpr auto apiName = "declarativeNetRequest.updateDynamicRules()"_s;

    auto ruleIDsToDelete = compactMap(ruleIDsToDeleteVector, [&](auto& ruleID) -> std::optional<double> {
        if (!m_dynamicRulesIDs.contains(ruleID))
            return std::nullopt;
        return ruleID;
    });

    Ref rulesToAdd = JSON::Array::create();
    if (!rulesToAddJSON.isEmpty()) {
        if (RefPtr parsedJSON = JSON::Value::parseJSON(rulesToAddJSON)) {
            if (RefPtr rulesArray = parsedJSON->asArray())
                rulesToAdd = *rulesArray;
        }
    }

    if (!ruleIDsToDelete.size() && !rulesToAdd->length()) {
        completionHandler({ });
        return;
    }

    auto updatedDynamicRulesCount = m_dynamicRulesIDs.size() + rulesToAdd->length() - ruleIDsToDelete.size();
    if (updatedDynamicRulesCount + m_dynamicRulesIDs.size() > webExtensionDeclarativeNetRequestMaximumNumberOfDynamicAndSessionRules) {
        completionHandler(toWebExtensionError(apiName, nullString(), "Failed to add dynamic rules. Maximum number of dynamic and session rules exceeded."_s));
        return;
    }

    updateDeclarativeNetRequestRulesInStorage(declarativeNetRequestDynamicRulesStore(), "dynamic"_s, apiName, rulesToAdd, ruleIDsToDelete, WTFMove(completionHandler));
}

void WebExtensionContext::declarativeNetRequestGetSessionRules(Vector<double>&& filter, CompletionHandler<void(Expected<String, WebExtensionError>&&)>&& completionHandler)
{
    auto ruleIDs = compactMap(filter, [&](auto& ruleID) -> std::optional<double> {
        if (m_sessionRulesIDs.contains(ruleID))
            return ruleID;

        return std::nullopt;
    });

    declarativeNetRequestSessionRulesStore()->getRulesWithRuleIDs(ruleIDs, [protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](RefPtr<JSON::Array> rules, const String& errorMessage) mutable {
        if (errorMessage.length()) {
            completionHandler(toWebExtensionError("declarativeNetRequest.getSessionRules()"_s, nullString(), errorMessage));
            return;
        }

        completionHandler(rules->toJSONString());
    });
}

void WebExtensionContext::declarativeNetRequestUpdateSessionRules(String&& rulesToAddJSON, Vector<double>&& ruleIDsToDeleteVector, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    static constexpr auto apiName = "declarativeNetRequest.updateSessionRules()"_s;

    auto ruleIDsToDelete = compactMap(ruleIDsToDeleteVector, [&](auto& ruleID) -> std::optional<double> {
        if (!m_sessionRulesIDs.contains(ruleID))
            return std::nullopt;
        return ruleID;
    });

    Ref rulesToAdd = JSON::Array::create();
    if (!rulesToAddJSON.isEmpty()) {
        if (RefPtr parsedJSON = JSON::Value::parseJSON(rulesToAddJSON)) {
            if (RefPtr rulesArray = parsedJSON->asArray())
                rulesToAdd = *rulesArray;
        }
    }

    if (!ruleIDsToDelete.size() && !rulesToAdd->length()) {
        completionHandler({ });
        return;
    }

    auto updatedSessionRulesCount = m_sessionRulesIDs.size() + rulesToAdd->length() - ruleIDsToDelete.size();
    if (updatedSessionRulesCount + m_sessionRulesIDs.size() > webExtensionDeclarativeNetRequestMaximumNumberOfDynamicAndSessionRules) {
        completionHandler(toWebExtensionError(apiName, nullString(), "Failed to add session rules. Maximum number of dynamic and session rules exceeded."_s));
        return;
    }

    updateDeclarativeNetRequestRulesInStorage(declarativeNetRequestSessionRulesStore(), "session"_s, apiName, rulesToAdd, ruleIDsToDelete, WTFMove(completionHandler));
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
