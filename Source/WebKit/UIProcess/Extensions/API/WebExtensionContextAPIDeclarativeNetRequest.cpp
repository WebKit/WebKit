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
#include <wtf/Borrow.h>
#include <wtf/JSONValues.h>

namespace WebKit {

bool WebExtensionContext::isDeclarativeNetRequestMessageAllowed(IPC::Decoder& message)
{
    return isLoadedAndPrivilegedMessage(message) && (hasPermission(WebExtensionPermission::declarativeNetRequest()) || hasPermission(WebExtensionPermission::declarativeNetRequestWithHostAccess()));
}

void WebExtensionContext::declarativeNetRequestGetEnabledRulesets(CompletionHandler<void(Vector<RefPtr<StringImpl>>&&)>&& completionHandler)
{
    Vector<RefPtr<StringImpl>> enabledRulesets;
    for (auto& identifier : m_enabledStaticRulesetIDs)
        enabledRulesets.append(identifier);

    completionHandler(WTF::move(enabledRulesets));
}

WebExtensionContext::DeclarativeNetRequestValidatedRulesets WebExtensionContext::declarativeNetRequestValidateRulesetIdentifiers(const Vector<RefPtr<StringImpl>>& rulesetIdentifiers)
{
    WebExtension::DeclarativeNetRequestRulesetVector validatedRulesets;

    RefPtr extension = m_extension;
    for (auto& identifier : borrow(rulesetIdentifiers)) {
        auto ruleset = extension->declarativeNetRequestRuleset(identifier);
        if (!ruleset)
            return toWebExtensionError("declarativeNetRequest.updateEnabledRulesets()"_si, nullString(), makeString("Failed to apply rules. Invalid ruleset id: "_s, identifier, "."_s));

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

void WebExtensionContext::updateDeclarativeNetRequestRulesInStorage(RefPtr<WebExtensionDeclarativeNetRequestSQLiteStore> storage, StringImpl* storageType, StringImpl* apiName, Ref<JSON::Array> rulesToAdd, Vector<double> ruleIDsToRemove, CompletionHandler<void(std::expected<void, WebExtensionError>&&)>&& completionHandler)
{
    if (storage) {
        storage->createSavepoint([this, protectedThis = Ref { *this }, completionHandler = WTF::move(completionHandler), storage, storageType = RefPtr { storageType }, apiName = RefPtr { apiName }, rulesToAdd, ruleIDsToRemove](Markable<WTF::UUID> savepointIdentifier, StringImpl* errorMessage) mutable {
            if (!savepointIdentifier || !isEmpty(errorMessage)) {
                RELEASE_LOG_ERROR(Extensions, "Unable to create %s rules savepoint for extension %s. Error: %s", utf8(storageType), utf8(uniqueIdentifier()), utf8(errorMessage));
                completionHandler(toWebExtensionError(apiName, nullString(), errorMessage));
                return;
            }

            storage->updateRulesByRemovingIDs(ruleIDsToRemove, rulesToAdd, [this, protectedThis = Ref { *this }, completionHandler = WTF::move(completionHandler), storage, storageType = RefPtr { storageType }, apiName = RefPtr { apiName }, savepointIdentifier = WTF::move(savepointIdentifier)](StringImpl* errorMessage) mutable {
                if (!isEmpty(errorMessage)) {
                    RELEASE_LOG_ERROR(Extensions, "Unable to update %s rules for extension %s. Error: %s", utf8(storageType), utf8(uniqueIdentifier()), utf8(errorMessage));

                    // Update was unsucessful, rollback the changes to the database.
                    storage->rollbackToSavepoint(savepointIdentifier.value(), [this, protectedThis = Ref { *this }, completionHandler = WTF::move(completionHandler), storageType = RefPtr { storageType }, apiName = RefPtr { apiName }, errorMessage = RefPtr { errorMessage }](StringImpl* savepointErrorMessage) mutable {
                        if (!isEmpty(savepointErrorMessage))
                            RELEASE_LOG_ERROR(Extensions, "Unable to rollback to %s rules savepoint for extension %s. Error: %s", utf8(storageType), utf8(uniqueIdentifier()), utf8(savepointErrorMessage));

                        completionHandler(toWebExtensionError(apiName, nullString(), errorMessage));
                    });

                    return;
                }

                // Update was successful, load the new rules
                loadDeclarativeNetRequestRules([this, protectedThis = Ref { *this }, completionHandler = WTF::move(completionHandler), storageType = RefPtr { storageType }, apiName = RefPtr { apiName }, storage, savepointIdentifier = WTF::move(savepointIdentifier), errorMessage = RefPtr { errorMessage }](bool success) mutable {
                    if (!success) {
                        // Load was unsucessful, rollback the changes to the database.
                        storage->rollbackToSavepoint(savepointIdentifier.value(), [this, protectedThis = Ref { *this }, completionHandler = WTF::move(completionHandler), storageType = RefPtr { storageType }, apiName = RefPtr { apiName }, errorMessage = RefPtr { errorMessage }](StringImpl* savepointErrorMessage) mutable {
                            if (!isEmpty(savepointErrorMessage))
                                RELEASE_LOG_ERROR(Extensions, "Unable to rollback to %s rules savepoint for extension %s. Error: %s", utf8(storageType), utf8(uniqueIdentifier()), utf8(savepointErrorMessage));

                            // Load the declarativeNetRequest rules again after rolling back the dynamic update.
                            loadDeclarativeNetRequestRules([completionHandler = WTF::move(completionHandler), apiName = RefPtr { apiName }](bool success) mutable {
                                if (!success) {
                                    completionHandler(toWebExtensionError(apiName, nullString(), "unable to load declarativeNetRequest rules"_si));
                                    return;
                                }

                                completionHandler({ });
                            });
                        });

                        return;
                    }

                    // Load was successful, commit the changes to the database.
                    storage->commitSavepoint(savepointIdentifier.value(), [this, protectedThis = Ref { *this }, completionHandler = WTF::move(completionHandler), storageType = RefPtr { storageType }](StringImpl* savepointErrorMessage) mutable {
                        if (!isEmpty(savepointErrorMessage))
                            RELEASE_LOG_ERROR(Extensions, "Unable to commit %s rules savepoint for extension %s. Error: %s", utf8(storageType), utf8(uniqueIdentifier()), utf8(savepointErrorMessage));

                        completionHandler({ });
                    });
                });
            });
        });
    }
}

void WebExtensionContext::declarativeNetRequestGetDynamicRules(Vector<double>&& filter, CompletionHandler<void(std::expected<RefPtr<StringImpl>, WebExtensionError>&&)>&& completionHandler)
{
    auto ruleIDs = compactMap(filter, [&](auto& ruleID) -> std::optional<double> {
        if (m_dynamicRulesIDs.contains(ruleID))
            return ruleID;

        return std::nullopt;
    });

    declarativeNetRequestDynamicRulesStore()->getRulesWithRuleIDs(ruleIDs, [protectedThis = Ref { *this }, completionHandler = WTF::move(completionHandler)](RefPtr<JSON::Array> rules, StringImpl* errorMessage) mutable {
        if (!isEmpty(errorMessage)) {
            completionHandler(toWebExtensionError("declarativeNetRequest.getDynamicRules()"_si, nullString(), errorMessage));
            return;
        }

        completionHandler(rules->toJSONString());
    });
}

void WebExtensionContext::declarativeNetRequestUpdateDynamicRules(RefPtr<StringImpl>&& rulesToAddJSON, Vector<double>&& ruleIDsToDeleteVector, CompletionHandler<void(std::expected<void, WebExtensionError>&&)>&& completionHandler)
{
    static constexpr auto apiName = "declarativeNetRequest.updateDynamicRules()"_s;

    HashSet<double> ruleIDsToDelete;
    for (auto ruleID : ruleIDsToDeleteVector) {
        if (m_dynamicRulesIDs.contains(ruleID))
            ruleIDsToDelete.add(ruleID);
    }

    Ref rulesToAdd = JSON::Array::create();
    if (!isEmpty(rulesToAddJSON)) {
        if (RefPtr parsedJSON = JSON::Value::parseJSON(rulesToAddJSON)) {
            if (RefPtr rulesArray = parsedJSON->asArray())
                rulesToAdd = *rulesArray;
        }
    }

    if (ruleIDsToDelete.isEmpty() && !rulesToAdd->length()) {
        completionHandler({ });
        return;
    }

    auto updatedDynamicRulesCount = m_dynamicRulesIDs.size() + rulesToAdd->length() - ruleIDsToDelete.size();
    if (updatedDynamicRulesCount + m_sessionRulesIDs.size() > webExtensionDeclarativeNetRequestMaximumNumberOfDynamicAndSessionRules) {
        completionHandler(toWebExtensionError(StringImpl::create(apiName).ptr(), nullString(), "Failed to add dynamic rules. Maximum number of dynamic and session rules exceeded."_si));
        return;
    }

    updateDeclarativeNetRequestRulesInStorage(declarativeNetRequestDynamicRulesStore(), "dynamic"_si, StringImpl::create(apiName).ptr(), rulesToAdd, copyToVector(ruleIDsToDelete), WTF::move(completionHandler));
}

void WebExtensionContext::declarativeNetRequestGetSessionRules(Vector<double>&& filter, CompletionHandler<void(std::expected<RefPtr<StringImpl>, WebExtensionError>&&)>&& completionHandler)
{
    auto ruleIDs = compactMap(filter, [&](auto& ruleID) -> std::optional<double> {
        if (m_sessionRulesIDs.contains(ruleID))
            return ruleID;

        return std::nullopt;
    });

    declarativeNetRequestSessionRulesStore()->getRulesWithRuleIDs(ruleIDs, [protectedThis = Ref { *this }, completionHandler = WTF::move(completionHandler)](RefPtr<JSON::Array> rules, StringImpl* errorMessage) mutable {
        if (!isEmpty(errorMessage)) {
            completionHandler(toWebExtensionError("declarativeNetRequest.getSessionRules()"_si, nullString(), errorMessage));
            return;
        }

        completionHandler(rules->toJSONString());
    });
}

void WebExtensionContext::declarativeNetRequestUpdateSessionRules(RefPtr<StringImpl>&& rulesToAddJSON, Vector<double>&& ruleIDsToDeleteVector, CompletionHandler<void(std::expected<void, WebExtensionError>&&)>&& completionHandler)
{
    static constexpr auto apiName = "declarativeNetRequest.updateSessionRules()"_s;

    HashSet<double> ruleIDsToDelete;
    for (auto ruleID : ruleIDsToDeleteVector) {
        if (m_sessionRulesIDs.contains(ruleID))
            ruleIDsToDelete.add(ruleID);
    }

    Ref rulesToAdd = JSON::Array::create();
    if (!isEmpty(rulesToAddJSON)) {
        if (RefPtr parsedJSON = JSON::Value::parseJSON(rulesToAddJSON)) {
            if (RefPtr rulesArray = parsedJSON->asArray())
                rulesToAdd = *rulesArray;
        }
    }

    if (ruleIDsToDelete.isEmpty() && !rulesToAdd->length()) {
        completionHandler({ });
        return;
    }

    auto updatedSessionRulesCount = m_sessionRulesIDs.size() + rulesToAdd->length() - ruleIDsToDelete.size();
    if (updatedSessionRulesCount + m_dynamicRulesIDs.size() > webExtensionDeclarativeNetRequestMaximumNumberOfDynamicAndSessionRules) {
        completionHandler(toWebExtensionError(StringImpl::create(apiName).ptr(), nullString(), "Failed to add session rules. Maximum number of dynamic and session rules exceeded."_si));
        return;
    }

    updateDeclarativeNetRequestRulesInStorage(declarativeNetRequestSessionRulesStore(), "session"_si, StringImpl::create(apiName).ptr(), rulesToAdd, copyToVector(ruleIDsToDelete), WTF::move(completionHandler));
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
