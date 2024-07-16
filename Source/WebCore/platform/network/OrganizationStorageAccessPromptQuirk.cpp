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

#include "config.h"
#include "OrganizationStorageAccessPromptQuirk.h"

#include "Logging.h"
#include "RegistrableDomain.h"
#include <wtf/Assertions.h>
#include <wtf/FileSystem.h>
#include <wtf/JSONValues.h>
#include <wtf/URL.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

Vector<OrganizationStorageAccessPromptQuirk> OrganizationStorageAccessPromptQuirk::parseOrganizationStorageAccessPromptQuirk(const String& resourcePath)
{
    if (resourcePath.isEmpty()) {
        RELEASE_LOG(Network, "OrganizationStorageAccessPromptQuirk::parseOrganizationStorageAccessPromptQuirk: Resource path was not provided. Aborting");
        return { };
    }

    constexpr auto overridesFileName = "StorageAccessPromptOrganizations.json"_s;
    auto path = FileSystem::pathByAppendingComponent(resourcePath, overridesFileName);
    auto buffer = FileSystem::readEntireFile(path);
    if (!buffer) {
        RELEASE_LOG(Network, "OrganizationStorageAccessPromptQuirk::parseOrganizationStorageAccessPromptQuirk: Failed to read file: %" PUBLIC_LOG_STRING ". Aborting", path.ascii().data());
        return { };
    }

    RefPtr value = JSON::Value::parseJSON(buffer->span());
    if (!value) {
        RELEASE_LOG(Network, "OrganizationStorageAccessPromptQuirk::parseOrganizationStorageAccessPromptQuirk: Failed to parse JSON from file. Aborting");
        return { };
    }

    RefPtr root = value->asObject();
    if (!root) {
        RELEASE_LOG(Network, "OrganizationStorageAccessPromptQuirk::parseOrganizationStorageAccessPromptQuirk: root is not an Object. Aborting");
        return { };
    }

    if (auto version = root->getInteger("version"_s)) {
        if (*version != 1) {
            RELEASE_LOG(Network, "OrganizationStorageAccessPromptQuirk::parseOrganizationStorageAccessPromptQuirk: Version number not recognized: %d. Aborting.", *version);
            return { };
        }
    } else {
        RELEASE_LOG(Network, "OrganizationStorageAccessPromptQuirk::parseOrganizationStorageAccessPromptQuirk: Version number not specified. Aborting.");
        return { };
    }

    Vector<OrganizationStorageAccessPromptQuirk> organizationQuirks;
    for (auto&& [organizationName, organization] : *root) {
        if (organizationName == "version"_s)
            continue;
        RefPtr organizationObject = organization->asObject();
        if (!organizationObject) {
            RELEASE_LOG(Network, "OrganizationStorageAccessPromptQuirk::parseOrganizationStorageAccessPromptQuirk: organization is not an Object for '%" PUBLIC_LOG_STRING "'. Skipping.", organizationName.ascii().data());
            continue;
        }
        auto quirksDomainIt = organizationObject->find("quirkDomains"_s);
        if (quirksDomainIt == organizationObject->end()) {
            RELEASE_LOG(Network, "OrganizationStorageAccessPromptQuirk::parseOrganizationStorageAccessPromptQuirk: '%" PUBLIC_LOG_STRING "' must contain a 'quirkDomains' object. Skipping.", organizationName.ascii().data());
            continue;
        }
        RefPtr quirkedDomainsObject = quirksDomainIt->value->asObject();
        if (!quirkedDomainsObject) {
            RELEASE_LOG(Network, "OrganizationStorageAccessPromptQuirk::parseOrganizationStorageAccessPromptQuirk: organization '%" PUBLIC_LOG_STRING "' does not contain a valid 'quirkDomains' object. Skipping.", organizationName.ascii().data());
            continue;
        }
        auto triggerPagesIt = organizationObject->find("triggerPages"_s);
        if (triggerPagesIt == organizationObject->end()) {
            RELEASE_LOG(Network, "OrganizationStorageAccessPromptQuirk::parseOrganizationStorageAccessPromptQuirk: '%" PUBLIC_LOG_STRING "' must contain a 'triggerPages' array. Skipping.", organizationName.ascii().data());
            continue;
        }
        RefPtr triggerPagesArray = triggerPagesIt->value->asArray();
        if (!triggerPagesArray) {
            RELEASE_LOG(Network, "OrganizationStorageAccessPromptQuirk::parseOrganizationStorageAccessPromptQuirk: organization '%" PUBLIC_LOG_STRING "' does not contain a valid 'triggerPages' array. Skipping.", organizationName.ascii().data());
            continue;
        }
        HashMap<RegistrableDomain, Vector<RegistrableDomain>> quirkedDomains;
        for (auto&& [quirkedMainFrameDomain, quirkedSubFrameDomainsValue] : *quirkedDomainsObject) {
            RefPtr quirkedSubFrameDomainsArray = quirkedSubFrameDomainsValue->asArray();
            if (!quirkedSubFrameDomainsArray) {
                RELEASE_LOG(Network, "OrganizationStorageAccessPromptQuirk::parseOrganizationStorageAccessPromptQuirk: organization '%" PUBLIC_LOG_STRING "', with main frame '%" PUBLIC_LOG_STRING "', does not contain a valid array of subframe domains. Skipping.", organizationName.ascii().data(), quirkedMainFrameDomain.ascii().data());
                continue;
            }
            if (quirkedSubFrameDomainsArray->length() < 2 && quirkedDomainsObject->size() < 2) {
                RELEASE_LOG(Network, "OrganizationStorageAccessPromptQuirk::parseOrganizationStorageAccessPromptQuirk: organization '%" PUBLIC_LOG_STRING "', with main frame '%" PUBLIC_LOG_STRING "', requires at least two domains. Skipping.", organizationName.ascii().data(), quirkedMainFrameDomain.ascii().data());
                continue;
            }
            if (quirkedSubFrameDomainsArray->length() < 1) {
                RELEASE_LOG(Network, "OrganizationStorageAccessPromptQuirk::parseOrganizationStorageAccessPromptQuirk: organization '%" PUBLIC_LOG_STRING "', with main frame '%" PUBLIC_LOG_STRING "', requires at least one domains. Skipping.", organizationName.ascii().data(), quirkedMainFrameDomain.ascii().data());
                continue;
            }

            Vector<RegistrableDomain> quirkedSubFrameDomains;
            for (auto&& quirkedSubFrameDomainValue : *quirkedSubFrameDomainsArray) {
                String quirkedSubFrameDomain = quirkedSubFrameDomainValue->asString();
                if (quirkedSubFrameDomain.isEmpty()) {
                    RELEASE_LOG(Network, "OrganizationStorageAccessPromptQuirk::parseOrganizationStorageAccessPromptQuirk: organization '%" PUBLIC_LOG_STRING "', with main frame '%" PUBLIC_LOG_STRING "', has a subframe domain entry that could not be parsed. Skipping.", organizationName.ascii().data(), quirkedMainFrameDomain.ascii().data());
                    continue;
                }
                quirkedSubFrameDomains.append(RegistrableDomain::fromRawString(WTFMove(quirkedSubFrameDomain)));
            }
            quirkedDomains.add(RegistrableDomain::fromRawString(WTFMove(quirkedMainFrameDomain)), WTFMove(quirkedSubFrameDomains));
        }
        Vector<URL> triggerPages;
        if (triggerPagesArray->length() < 1) {
            RELEASE_LOG(Network, "OrganizationStorageAccessPromptQuirk::parseOrganizationStorageAccessPromptQuirk: organization '%" PUBLIC_LOG_STRING "' requires at least one trigger page. Skipping.", organizationName.ascii().data());
            continue;
        }
        for (auto&& triggerPageValue : *triggerPagesArray) {
            String triggerPage = triggerPageValue->asString();
            URL triggerPageURL { triggerPage };
            if (!triggerPageURL.isValid()) {
                RELEASE_LOG(Network, "OrganizationStorageAccessPromptQuirk::parseOrganizationStorageAccessPromptQuirk: organization '%" PUBLIC_LOG_STRING "' trigger page '%" PUBLIC_LOG_STRING "' is not a valid URL. Skipping.", organizationName.ascii().data(), triggerPage.ascii().data());
                continue;
            }
            triggerPages.append(triggerPageURL);
        }
        organizationQuirks.append({ WTFMove(organizationName), WTFMove(quirkedDomains), WTFMove(triggerPages) });
    }
    RELEASE_LOG(Network, "OrganizationStorageAccessPromptQuirk::parseOrganizationStorageAccessPromptQuirk: Loaded quirks for %zu organizations", organizationQuirks.size());
    return organizationQuirks;
}
} // namespace WebCore
