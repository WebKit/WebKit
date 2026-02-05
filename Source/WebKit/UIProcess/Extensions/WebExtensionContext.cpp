/*
 * Copyright (C) 2022-2025 Apple Inc. All rights reserved.
 * Copyright (C) 2025 Igalia, S.L. All rights reserved.
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

#include "APIContentRuleList.h"
#include "APIContentRuleListStore.h"
#include "InjectUserScriptImmediately.h"
#include "Logging.h"
#include "WebExtensionConstants.h"
#include "WebExtensionContextParameters.h"
#include "WebExtensionContextProxyMessages.h"
#include "WebExtensionController.h"
#include "WebExtensionPermission.h"
#include "WebPageProxy.h"
#include <WebCore/LocalizedStrings.h>
#include <WebCore/TextResourceDecoder.h>
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>

// This number was chosen arbitrarily based on testing with some popular extensions.
static constexpr size_t maximumCachedPermissionResults = 256;

namespace WebKit {

using namespace WebCore;

int WebExtensionContext::toAPIError(WebExtensionContext::Error error)
{
    switch (error) {
    case WebExtensionContext::Error::Unknown:
        return static_cast<int>(WebExtensionContext::APIError::Unknown);
    case WebExtensionContext::Error::AlreadyLoaded:
        return static_cast<int>(WebExtensionContext::APIError::AlreadyLoaded);
    case WebExtensionContext::Error::NotLoaded:
        return static_cast<int>(WebExtensionContext::APIError::NotLoaded);
    case WebExtensionContext::Error::BaseURLAlreadyInUse:
        return static_cast<int>(WebExtensionContext::APIError::BaseURLAlreadyInUse);
    case WebExtensionContext::Error::NoBackgroundContent:
        return static_cast<int>(WebExtensionContext::APIError::NoBackgroundContent);
    case WebExtensionContext::Error::BackgroundContentFailedToLoad:
        return static_cast<int>(WebExtensionContext::APIError::BackgroundContentFailedToLoad);
    }

    ASSERT_NOT_REACHED();
    return static_cast<int>(WebExtensionContext::APIError::Unknown);
}

Ref<API::Error> WebExtensionContext::createError(Error error, const String& customLocalizedDescription, RefPtr<API::Error> underlyingError)
{
    auto errorCode = toAPIError(error);
    String localizedDescription;

    switch (error) {
    case Error::Unknown:
        localizedDescription = WEB_UI_STRING_KEY("An unknown error has occurred.", "An unknown error has occurred. (WKWebExtensionContext)", "WKWebExtensionContextErrorUnknown description");
        break;

    case Error::AlreadyLoaded:
        localizedDescription = WEB_UI_STRING("Extension context is already loaded.", "WKWebExtensionContextErrorAlreadyLoaded description");
        break;

    case Error::NotLoaded:
        localizedDescription = WEB_UI_STRING("Extension context is not loaded.", "WKWebExtensionContextErrorNotLoaded description");
        break;

    case Error::BaseURLAlreadyInUse:
        localizedDescription = WEB_UI_STRING("Another extension context is loaded with the same base URL.", "WKWebExtensionContextErrorBaseURLAlreadyInUse description");
        break;

    case Error::NoBackgroundContent:
        localizedDescription = WEB_UI_STRING("No background content is available to load.", "WKWebExtensionContextErrorNoBackgroundContent description");
        break;

    case Error::BackgroundContentFailedToLoad:
        localizedDescription = WEB_UI_STRING("The background content failed to load due to an error.", "WKWebExtensionContextErrorBackgroundContentFailedToLoad description");
        break;
    }

    if (!customLocalizedDescription.isEmpty())
        localizedDescription = customLocalizedDescription;

    return API::Error::create({ "WKWebExtensionContextErrorDomain"_s, errorCode, { }, localizedDescription }, underlyingError);
}

Vector<Ref<API::Error>> WebExtensionContext::errors()
{
    auto array = protect(extension())->errors();
    array.appendVector(m_errors);
    return array;
}

String WebExtensionContext::stateFilePath() const
{
    if (!storageIsPersistent())
        return nullString();
    return FileSystem::pathByAppendingComponent(storageDirectory(), plistFileName());
}

void WebExtensionContext::setBaseURL(URL&& url)
{
    ASSERT(!isLoaded());
    if (isLoaded())
        return;

    if (!url.isValid())
        return;

    m_baseURL = URL { url, "/"_s };
}

bool WebExtensionContext::isURLForThisExtension(const URL& url) const
{
    return url.isValid() && protocolHostAndPortAreEqual(baseURL(), url);
}

bool WebExtensionContext::isURLForAnyExtension(const URL& url)
{
    return url.isValid() && WebExtensionMatchPattern::extensionSchemes().contains(url.protocol().toString());
}

void WebExtensionContext::setUniqueIdentifier(String&& uniqueIdentifier)
{
    ASSERT(!isLoaded());
    if (isLoaded())
        return;

    m_customUniqueIdentifier = !uniqueIdentifier.isEmpty();

    if (uniqueIdentifier.isEmpty())
        uniqueIdentifier = WTF::UUID::createVersion4().toString();

    m_uniqueIdentifier = uniqueIdentifier;
}

RefPtr<WebExtensionLocalization> WebExtensionContext::localization()
{
    if (!m_localization)
        m_localization = WebExtensionLocalization::create(protect(extension())->localization()->localizationJSON(), baseURL().host().toString());
    return m_localization;
}

RefPtr<API::Data> WebExtensionContext::localizedResourceData(const RefPtr<API::Data>& resourceData, const String& mimeType)
{
    if (!equalLettersIgnoringASCIICase(mimeType, "text/css"_s) || !resourceData)
        return resourceData;

    RefPtr decoder = WebCore::TextResourceDecoder::create(mimeType, PAL::UTF8Encoding());
    auto stylesheetContents = decoder->decode(resourceData->span());

    auto localizedString = localizedResourceString(stylesheetContents, mimeType);
    if (localizedString == stylesheetContents)
        return resourceData;

    return API::Data::create(localizedString.utf8().span());
}

String WebExtensionContext::localizedResourceString(const String& resourceContents, const String& mimeType)
{
    if (!equalLettersIgnoringASCIICase(mimeType, "text/css"_s) || resourceContents.isEmpty() || !resourceContents.contains("__MSG_"_s))
        return resourceContents;

    RefPtr localization = this->localization();
    if (!localization)
        return resourceContents;

    return localization->localizedStringForString(resourceContents);
}

void WebExtensionContext::setUnsupportedAPIs(HashSet<String>&& unsupported)
{
    ASSERT(!isLoaded());
    if (isLoaded())
        return;

    m_unsupportedAPIs = WTF::move(unsupported);
}

URL WebExtensionContext::optionsPageURL() const
{
    RefPtr extension = m_extension;
    if (!extension->hasOptionsPage())
        return { };
    return { m_baseURL, extension->optionsPagePath() };
}

URL WebExtensionContext::overrideNewTabPageURL() const
{
    RefPtr extension = m_extension;
    if (!extension->hasOverrideNewTabPage())
        return { };
    return { m_baseURL, extension->overrideNewTabPagePath() };
}

void WebExtensionContext::setHasAccessToPrivateData(bool hasAccess)
{
    if (m_hasAccessToPrivateData == hasAccess)
        return;

    m_hasAccessToPrivateData = hasAccess;

    if (!safeToInjectContent())
        return;

    if (m_hasAccessToPrivateData) {
        addDeclarativeNetRequestRulesToPrivateUserContentControllers();

        for (Ref controller : extensionController()->allPrivateUserContentControllers())
            addInjectedContent(controller);

#if ENABLE(INSPECTOR_EXTENSIONS)
        loadInspectorBackgroundPagesForPrivateBrowsing();
#endif
    } else {
        for (Ref controller : extensionController()->allPrivateUserContentControllers()) {
            removeInjectedContent(controller);
            controller->removeContentRuleList(uniqueIdentifier());
        }

#if ENABLE(INSPECTOR_EXTENSIONS)
        unloadInspectorBackgroundPagesForPrivateBrowsing();
#endif
    }
}

const WebExtensionContext::PermissionsMap& WebExtensionContext::grantedPermissions()
{
    return removeExpired(m_grantedPermissions, m_nextGrantedPermissionsExpirationDate, PermissionNotification::GrantedPermissionsWereRemoved);
}

void WebExtensionContext::setGrantedPermissions(PermissionsMap&& grantedPermissions)
{
    PermissionsSet removedPermissions;
    for (auto& entry : m_grantedPermissions)
        removedPermissions.add(entry.key);

    m_nextGrantedPermissionsExpirationDate = WallTime::nan();
    m_grantedPermissions = removeExpired(grantedPermissions, m_nextGrantedPermissionsExpirationDate);

    PermissionsSet addedPermissions;
    for (auto& entry : m_grantedPermissions) {
        if (removedPermissions.contains(entry.key)) {
            removedPermissions.remove(entry.key);
            continue;
        }

        addedPermissions.add(entry.key);
    }

    if (addedPermissions.isEmpty() && removedPermissions.isEmpty())
        return;

    removeDeniedPermissions(addedPermissions);

    permissionsDidChange(PermissionNotification::GrantedPermissionsWereRemoved, removedPermissions);
    permissionsDidChange(PermissionNotification::PermissionsWereGranted, addedPermissions);
}

const WebExtensionContext::PermissionsMap& WebExtensionContext::deniedPermissions()
{
    return removeExpired(m_deniedPermissions, m_nextDeniedPermissionsExpirationDate, PermissionNotification::DeniedPermissionsWereRemoved);
}

void WebExtensionContext::setDeniedPermissions(PermissionsMap&& deniedPermissions)
{
    PermissionsSet removedPermissions;
    for (auto& entry : m_deniedPermissions)
        removedPermissions.add(entry.key);

    m_nextDeniedPermissionsExpirationDate = WallTime::nan();
    m_deniedPermissions = removeExpired(deniedPermissions, m_nextDeniedPermissionsExpirationDate);

    PermissionsSet addedPermissions;
    for (auto& entry : m_deniedPermissions) {
        if (removedPermissions.contains(entry.key)) {
            removedPermissions.remove(entry.key);
            continue;
        }

        addedPermissions.add(entry.key);
    }

    if (addedPermissions.isEmpty() && removedPermissions.isEmpty())
        return;

    removeGrantedPermissions(addedPermissions);

    permissionsDidChange(PermissionNotification::DeniedPermissionsWereRemoved, removedPermissions);
    permissionsDidChange(PermissionNotification::PermissionsWereDenied, addedPermissions);
}

const WebExtensionContext::PermissionMatchPatternsMap& WebExtensionContext::grantedPermissionMatchPatterns()
{
    return removeExpired(m_grantedPermissionMatchPatterns, m_nextGrantedPermissionMatchPatternsExpirationDate, PermissionNotification::GrantedPermissionMatchPatternsWereRemoved);
}

void WebExtensionContext::setGrantedPermissionMatchPatterns(PermissionMatchPatternsMap&& grantedPermissionMatchPatterns, EqualityOnly equalityOnly)
{
    MatchPatternSet removedMatchPatterns;
    for (auto& entry : m_grantedPermissionMatchPatterns)
        removedMatchPatterns.add(entry.key);

    m_nextGrantedPermissionMatchPatternsExpirationDate = WallTime::nan();
    m_grantedPermissionMatchPatterns = removeExpired(grantedPermissionMatchPatterns, m_nextGrantedPermissionMatchPatternsExpirationDate);

    MatchPatternSet addedMatchPatterns;
    for (auto& entry : m_grantedPermissionMatchPatterns) {
        if (removedMatchPatterns.contains(entry.key)) {
            removedMatchPatterns.remove(entry.key);
            continue;
        }

        addedMatchPatterns.add(entry.key);
    }

    if (addedMatchPatterns.isEmpty() && removedMatchPatterns.isEmpty())
        return;

    removeDeniedPermissionMatchPatterns(addedMatchPatterns, equalityOnly);

    permissionsDidChange(PermissionNotification::GrantedPermissionMatchPatternsWereRemoved, removedMatchPatterns);
    permissionsDidChange(PermissionNotification::PermissionMatchPatternsWereGranted, addedMatchPatterns);
}

void WebExtensionContext::setDeniedPermissionMatchPatterns(PermissionMatchPatternsMap&& deniedPermissionMatchPatterns, EqualityOnly equalityOnly)
{
    MatchPatternSet removedMatchPatterns;
    for (auto& entry : m_deniedPermissionMatchPatterns)
        removedMatchPatterns.add(entry.key);

    m_nextDeniedPermissionMatchPatternsExpirationDate = WallTime::nan();
    m_deniedPermissionMatchPatterns = removeExpired(deniedPermissionMatchPatterns, m_nextDeniedPermissionMatchPatternsExpirationDate);

    MatchPatternSet addedMatchPatterns;
    for (auto& entry : m_deniedPermissionMatchPatterns) {
        if (removedMatchPatterns.contains(entry.key)) {
            removedMatchPatterns.remove(entry.key);
            continue;
        }

        addedMatchPatterns.add(entry.key);
    }

    if (addedMatchPatterns.isEmpty() && removedMatchPatterns.isEmpty())
        return;

    removeGrantedPermissionMatchPatterns(addedMatchPatterns, equalityOnly);

    permissionsDidChange(PermissionNotification::DeniedPermissionMatchPatternsWereRemoved, removedMatchPatterns);
    permissionsDidChange(PermissionNotification::PermissionMatchPatternsWereDenied, addedMatchPatterns);
}

const WebExtensionContext::PermissionMatchPatternsMap& WebExtensionContext::deniedPermissionMatchPatterns()
{
    return removeExpired(m_deniedPermissionMatchPatterns, m_nextDeniedPermissionMatchPatternsExpirationDate, PermissionNotification::DeniedPermissionMatchPatternsWereRemoved);
}

void WebExtensionContext::grantPermissions(PermissionsSet&& permissions, WallTime expirationDate)
{
    ASSERT(!expirationDate.isNaN());

    if (permissions.isEmpty())
        return;

    if (m_nextGrantedPermissionsExpirationDate > expirationDate)
        m_nextGrantedPermissionsExpirationDate = expirationDate;

    PermissionsSet addedPermissions;
    for (auto& permission : permissions) {
        if (m_grantedPermissions.add(permission, expirationDate))
            addedPermissions.addVoid(permission);
    }

    if (addedPermissions.isEmpty())
        return;

    removeDeniedPermissions(addedPermissions);

    permissionsDidChange(WebExtensionContext::PermissionNotification::PermissionsWereGranted, addedPermissions);
}

void WebExtensionContext::denyPermissions(PermissionsSet&& permissions, WallTime expirationDate)
{
    ASSERT(!expirationDate.isNaN());

    if (permissions.isEmpty())
        return;

    if (m_nextDeniedPermissionsExpirationDate > expirationDate)
        m_nextDeniedPermissionsExpirationDate = expirationDate;

    PermissionsSet addedPermissions;
    for (auto& permission : permissions) {
        if (m_deniedPermissions.add(permission, expirationDate))
            addedPermissions.addVoid(permission);
    }

    if (addedPermissions.isEmpty())
        return;

    removeGrantedPermissions(addedPermissions);

    permissionsDidChange(WebExtensionContext::PermissionNotification::PermissionsWereDenied, addedPermissions);
}

void WebExtensionContext::grantPermissionMatchPatterns(MatchPatternSet&& permissionMatchPatterns, WallTime expirationDate, EqualityOnly equalityOnly)
{
    ASSERT(!expirationDate.isNaN());

    if (permissionMatchPatterns.isEmpty())
        return;

    if (m_nextGrantedPermissionMatchPatternsExpirationDate > expirationDate)
        m_nextGrantedPermissionMatchPatternsExpirationDate = expirationDate;

    MatchPatternSet addedMatchPatterns;
    for (auto& pattern : permissionMatchPatterns) {
        if (m_grantedPermissionMatchPatterns.add(pattern, expirationDate))
            addedMatchPatterns.addVoid(pattern);
    }

    if (addedMatchPatterns.isEmpty())
        return;

    removeDeniedPermissionMatchPatterns(addedMatchPatterns, equalityOnly);

    permissionsDidChange(WebExtensionContext::PermissionNotification::PermissionMatchPatternsWereGranted, addedMatchPatterns);
}

void WebExtensionContext::denyPermissionMatchPatterns(MatchPatternSet&& permissionMatchPatterns, WallTime expirationDate, EqualityOnly equalityOnly)
{
    ASSERT(!expirationDate.isNaN());

    if (permissionMatchPatterns.isEmpty())
        return;

    if (m_nextDeniedPermissionMatchPatternsExpirationDate > expirationDate)
        m_nextDeniedPermissionMatchPatternsExpirationDate = expirationDate;

    MatchPatternSet addedMatchPatterns;
    for (auto& pattern : permissionMatchPatterns) {
        if (m_deniedPermissionMatchPatterns.add(pattern, expirationDate))
            addedMatchPatterns.addVoid(pattern);
    }

    if (addedMatchPatterns.isEmpty())
        return;

    removeGrantedPermissionMatchPatterns(addedMatchPatterns, equalityOnly);

    permissionsDidChange(WebExtensionContext::PermissionNotification::PermissionMatchPatternsWereDenied, addedMatchPatterns);
}

bool WebExtensionContext::removePermissions(PermissionsMap& permissionMap, PermissionsSet& permissionsToRemove, WallTime& nextExpirationDate, WebExtensionContext::PermissionNotification notification)
{
    if (permissionsToRemove.isEmpty())
        return false;

    nextExpirationDate = WallTime::infinity();

    PermissionsSet removedPermissions;
    permissionMap.removeIf([&](auto& entry) {
        if (permissionsToRemove.contains(entry.key)) {
            removedPermissions.add(entry.key);
            return true;
        }

        if (entry.value < nextExpirationDate)
            nextExpirationDate = entry.value;

        return false;
    });

    if (removedPermissions.isEmpty() || notification == PermissionNotification::None)
        return false;

    permissionsDidChange(notification, removedPermissions);

    return true;
}

bool WebExtensionContext::removePermissionMatchPatterns(PermissionMatchPatternsMap& matchPatternMap, MatchPatternSet& matchPatternsToRemove, EqualityOnly equalityOnly, WallTime& nextExpirationDate, WebExtensionContext::PermissionNotification notification)
{
    if (matchPatternsToRemove.isEmpty())
        return false;

    nextExpirationDate = WallTime::infinity();

    MatchPatternSet removedMatchPatterns;
    matchPatternMap.removeIf([&](auto& entry) {
        if (matchPatternsToRemove.contains(entry.key)) {
            removedMatchPatterns.add(entry.key);
            return true;
        }

        if (equalityOnly == EqualityOnly::Yes) {
            if (entry.value < nextExpirationDate)
                nextExpirationDate = entry.value;

            return false;
        }

        for (auto& patternToRemove : matchPatternsToRemove) {
            Ref pattern = entry.key;
            if (patternToRemove->matchesPattern(pattern, WebExtensionMatchPattern::Options::IgnorePaths)) {
                removedMatchPatterns.add(pattern);
                return true;
            }
        }

        if (entry.value < nextExpirationDate)
            nextExpirationDate = entry.value;

        return false;
    });

    if (removedMatchPatterns.isEmpty() || notification == PermissionNotification::None)
        return false;

    permissionsDidChange(notification, removedMatchPatterns);

    return true;
}

bool WebExtensionContext::removeGrantedPermissionMatchPatterns(MatchPatternSet& matchPatternsToRemove, EqualityOnly equalityOnly)
{
#if PLATFORM(COCOA)
    // Clear activeTab permissions if the patterns match.
    for (Ref tab : openTabs()) {
        auto temporaryPattern = tab->temporaryPermissionMatchPattern();
        if (!temporaryPattern)
            continue;

        for (auto& pattern : matchPatternsToRemove) {
            if (temporaryPattern->matchesPattern(pattern))
                tab->setTemporaryPermissionMatchPattern(nullptr);
        }
    }
#endif

    if (!removePermissionMatchPatterns(m_grantedPermissionMatchPatterns, matchPatternsToRemove, equalityOnly, m_nextGrantedPermissionMatchPatternsExpirationDate, WebExtensionContext::PermissionNotification::GrantedPermissionMatchPatternsWereRemoved))
        return false;

    removeInjectedContent(matchPatternsToRemove);

    return true;
}

bool WebExtensionContext::removeGrantedPermissions(PermissionsSet& permissionsToRemove)
{
    return removePermissions(m_grantedPermissions, permissionsToRemove, m_nextGrantedPermissionsExpirationDate, WebExtensionContext::PermissionNotification::GrantedPermissionsWereRemoved);
}

bool WebExtensionContext::removeDeniedPermissions(PermissionsSet& permissionsToRemove)
{
    return removePermissions(m_deniedPermissions, permissionsToRemove, m_nextDeniedPermissionsExpirationDate, WebExtensionContext::PermissionNotification::DeniedPermissionsWereRemoved);
}

bool WebExtensionContext::removeDeniedPermissionMatchPatterns(MatchPatternSet& matchPatternsToRemove, EqualityOnly equalityOnly)
{
    if (!removePermissionMatchPatterns(m_deniedPermissionMatchPatterns, matchPatternsToRemove, equalityOnly, m_nextDeniedPermissionMatchPatternsExpirationDate, WebExtensionContext::PermissionNotification::DeniedPermissionMatchPatternsWereRemoved))
        return false;

    updateInjectedContent();

    return true;
}

WebExtensionContext::PermissionsMap& WebExtensionContext::removeExpired(PermissionsMap& permissionMap, WallTime& nextExpirationDate, WebExtensionContext::PermissionNotification notification)
{
    WallTime currentTime = WallTime::now();

    // If the next expiration date hasn't passed yet, there is nothing to remove.
    if (nextExpirationDate != WallTime::nan() && nextExpirationDate > currentTime)
        return permissionMap;

    nextExpirationDate = WallTime::infinity();

    PermissionsSet removedPermissions;
    permissionMap.removeIf([&](auto& entry) {
        if (entry.value <= currentTime) {
            removedPermissions.add(entry.key);
            return true;
        }

        if (entry.value < nextExpirationDate)
            nextExpirationDate = entry.value;

        return false;
    });

    if (removedPermissions.isEmpty() || notification == PermissionNotification::None)
        return permissionMap;

    permissionsDidChange(notification, removedPermissions);

    return permissionMap;
}

WebExtensionContext::PermissionMatchPatternsMap& WebExtensionContext::removeExpired(PermissionMatchPatternsMap& matchPatternMap, WallTime& nextExpirationDate, WebExtensionContext::PermissionNotification notification)
{
    WallTime currentTime = WallTime::now();

    // If the next expiration date hasn't passed yet, there is nothing to remove.
    if (nextExpirationDate != WallTime::nan() && nextExpirationDate > currentTime)
        return matchPatternMap;

    nextExpirationDate = WallTime::infinity();

    MatchPatternSet removedMatchPatterns;
    matchPatternMap.removeIf([&](auto& entry) {
        if (entry.value <= currentTime) {
            removedMatchPatterns.add(entry.key);
            return true;
        }

        if (entry.value < nextExpirationDate)
            nextExpirationDate = entry.value;

        return false;
    });

    if (removedMatchPatterns.isEmpty() || notification == PermissionNotification::None)
        return matchPatternMap;

    permissionsDidChange(notification, removedMatchPatterns);

    return matchPatternMap;
}

bool WebExtensionContext::needsPermission(const String& permission, WebExtensionTab* tab, OptionSet<PermissionStateOptions> options)
{
    ASSERT(!permission.isEmpty());
    ASSERT(!options.contains(PermissionStateOptions::SkipRequestedPermissions));

    switch (permissionState(permission, tab, options)) {
    case PermissionState::Unknown:
    case PermissionState::DeniedImplicitly:
    case PermissionState::DeniedExplicitly:
    case PermissionState::GrantedImplicitly:
    case PermissionState::GrantedExplicitly:
        return false;

    case PermissionState::RequestedImplicitly:
    case PermissionState::RequestedExplicitly:
        return true;
    }

    return false;
}


bool WebExtensionContext::needsPermission(const URL& url, WebExtensionTab* tab, OptionSet<PermissionStateOptions> options)
{
    ASSERT(!options.contains(PermissionStateOptions::SkipRequestedPermissions));

    switch (permissionState(url, tab, options)) {
    case PermissionState::Unknown:
    case PermissionState::DeniedImplicitly:
    case PermissionState::DeniedExplicitly:
    case PermissionState::GrantedImplicitly:
    case PermissionState::GrantedExplicitly:
        return false;

    case PermissionState::RequestedImplicitly:
    case PermissionState::RequestedExplicitly:
        return true;
    }

    return false;
}

bool WebExtensionContext::needsPermission(const WebExtensionMatchPattern& pattern, WebExtensionTab* tab, OptionSet<PermissionStateOptions> options)
{
    ASSERT(!options.contains(PermissionStateOptions::SkipRequestedPermissions));

    switch (permissionState(pattern, tab, options)) {
    case PermissionState::Unknown:
    case PermissionState::DeniedImplicitly:
    case PermissionState::DeniedExplicitly:
    case PermissionState::GrantedImplicitly:
    case PermissionState::GrantedExplicitly:
        return false;

    case PermissionState::RequestedImplicitly:
    case PermissionState::RequestedExplicitly:
        return true;
    }

    return false;
}

bool WebExtensionContext::hasPermission(const String& permission, WebExtensionTab* tab, OptionSet<PermissionStateOptions> options)
{
    ASSERT(!permission.isEmpty());

    options.add(PermissionStateOptions::SkipRequestedPermissions);

    switch (permissionState(permission, tab, options)) {
    case PermissionState::Unknown:
    case PermissionState::DeniedImplicitly:
    case PermissionState::DeniedExplicitly:
    case PermissionState::RequestedImplicitly:
    case PermissionState::RequestedExplicitly:
        return false;

    case PermissionState::GrantedImplicitly:
    case PermissionState::GrantedExplicitly:
        return true;
    }

    return false;
}

bool WebExtensionContext::hasPermission(const URL& url, WebExtensionTab* tab, OptionSet<PermissionStateOptions> options)
{
    options.add(PermissionStateOptions::SkipRequestedPermissions);

    switch (permissionState(url, tab, options)) {
    case PermissionState::Unknown:
    case PermissionState::DeniedImplicitly:
    case PermissionState::DeniedExplicitly:
    case PermissionState::RequestedImplicitly:
    case PermissionState::RequestedExplicitly:
        return false;

    case PermissionState::GrantedImplicitly:
    case PermissionState::GrantedExplicitly:
        return true;
    }

    return false;
}

bool WebExtensionContext::hasPermission(const WebExtensionMatchPattern& pattern, WebExtensionTab* tab, OptionSet<PermissionStateOptions> options)
{
    options.add(PermissionStateOptions::SkipRequestedPermissions);

    switch (permissionState(pattern, tab, options)) {
    case PermissionState::Unknown:
    case PermissionState::DeniedImplicitly:
    case PermissionState::DeniedExplicitly:
    case PermissionState::RequestedImplicitly:
    case PermissionState::RequestedExplicitly:
        return false;

    case PermissionState::GrantedImplicitly:
    case PermissionState::GrantedExplicitly:
        return true;
    }

    return false;
}

bool WebExtensionContext::hasPermissions(PermissionsSet permissions, MatchPatternSet matchPatterns)
{
    for (auto& permission : permissions) {
        if (!m_grantedPermissions.contains(permission))
            return false;
    }

    for (auto& pattern : matchPatterns) {
        bool matchFound = false;
        for (auto& grantedPattern : currentPermissionMatchPatterns()) {
            if (grantedPattern->matchesPattern(pattern, { WebExtensionMatchPattern::Options::IgnorePaths })) {
                matchFound = true;
                break;
            }
        }

        if (!matchFound)
            return false;
    }

    return true;
}


WebExtensionContext::PermissionState WebExtensionContext::permissionState(const String& permission, WebExtensionTab* tab, OptionSet<PermissionStateOptions> options)
{
    ASSERT(!permission.isEmpty());

#if PLATFORM(COCOA)
    if (tab && permission == WebExtensionPermission::tabs()) {
        if (tab->extensionHasTemporaryPermission())
            return PermissionState::GrantedExplicitly;
    }
#endif

    if (!WebExtension::supportedPermissions().contains(permission))
        return PermissionState::Unknown;

    if (deniedPermissions().contains(permission))
        return PermissionState::DeniedExplicitly;

    if (grantedPermissions().contains(permission))
        return PermissionState::GrantedExplicitly;

    if (options.contains(PermissionStateOptions::SkipRequestedPermissions))
        return PermissionState::Unknown;

    RefPtr extension = m_extension;
    if (extension->hasRequestedPermission(permission))
        return PermissionState::RequestedExplicitly;

    if (options.contains(PermissionStateOptions::IncludeOptionalPermissions)) {
        if (extension->optionalPermissions().contains(permission))
            return PermissionState::RequestedImplicitly;
    }

    return PermissionState::Unknown;
}

WebExtensionContext::PermissionState WebExtensionContext::permissionState(const URL& url, WebExtensionTab* tab, OptionSet<PermissionStateOptions> options)
{
    if (url.isEmpty())
        return PermissionState::Unknown;

    if (isURLForThisExtension(url))
        return PermissionState::GrantedImplicitly;

    if (!WebExtensionMatchPattern::validSchemes().contains(url.protocol().toStringWithoutCopying()))
        return PermissionState::Unknown;

    if (tab) {
        auto temporaryPattern = tab->temporaryPermissionMatchPattern();
        if (temporaryPattern && temporaryPattern->matchesURL(url))
            return PermissionState::GrantedExplicitly;
    }

    bool skipRequestedPermissions = options.contains(PermissionStateOptions::SkipRequestedPermissions);

    // Access the maps here to remove any expired entries, and only do it once for this call.
    auto& grantedPermissionMatchPatterns = this->grantedPermissionMatchPatterns();
    auto& deniedPermissionMatchPatterns = this->deniedPermissionMatchPatterns();

    // If the cache still has the URL, then it has not expired.
    if (m_cachedPermissionURLs.contains(url)) {
        PermissionState cachedState = m_cachedPermissionStates.get(url);

        // We only want to return an unknown cached state if the SkippingRequestedPermissions option isn't used.
        if (cachedState != PermissionState::Unknown || skipRequestedPermissions) {
            // Move the URL to the end, so it stays in the cache longer as a recent hit.
            m_cachedPermissionURLs.appendOrMoveToLast(url);

            if ((cachedState == PermissionState::RequestedExplicitly || cachedState == PermissionState::RequestedImplicitly) && skipRequestedPermissions)
                return PermissionState::Unknown;

            return cachedState;
        }
    }

    auto cacheResultAndReturn = [&](PermissionState result) {
        m_cachedPermissionURLs.appendOrMoveToLast(url);
        m_cachedPermissionStates.set(url, result);

        ASSERT(m_cachedPermissionURLs.size() == m_cachedPermissionStates.size());

        if (m_cachedPermissionURLs.size() <= maximumCachedPermissionResults)
            return result;

        URL firstCachedURL = m_cachedPermissionURLs.takeFirst();
        m_cachedPermissionStates.remove(firstCachedURL);

        ASSERT(m_cachedPermissionURLs.size() == m_cachedPermissionStates.size());

        return result;
    };

    // First, check for patterns that are specific to certain domains, ignoring wildcard host patterns that
    // match all hosts. The order is denied, then granted. This makes sure denied takes precedence over granted.
    auto urlMatchesPatternIgnoringWildcardHostPatterns = [&](WebExtensionMatchPattern& pattern) {
        if (pattern.matchesAllHosts())
            return false;
        return pattern.matchesURL(url);
    };

    for (auto& deniedPermissionEntry : deniedPermissionMatchPatterns) {
        if (urlMatchesPatternIgnoringWildcardHostPatterns(deniedPermissionEntry.key))
            return cacheResultAndReturn(PermissionState::DeniedExplicitly);
    }

    for (auto& grantedPermissionEntry : grantedPermissionMatchPatterns) {
        if (urlMatchesPatternIgnoringWildcardHostPatterns(grantedPermissionEntry.key))
            return cacheResultAndReturn(PermissionState::GrantedExplicitly);
    }

    // Next, check for patterns that are wildcard host patterns that match all hosts (<all_urls>, *://*/*, etc),
    // also checked in denied, then granted order. Doing these wildcard patterns separately allows for blanket
    // patterns to be set as default policies while allowing for specific domains to still be granted or denied.
    auto urlMatchesWildcardHostPatterns = [&](WebExtensionMatchPattern& pattern) {
        if (!pattern.matchesAllHosts())
            return false;
        return pattern.matchesURL(url);
    };

    for (auto& deniedPermissionEntry : deniedPermissionMatchPatterns) {
        if (urlMatchesWildcardHostPatterns(deniedPermissionEntry.key))
            return cacheResultAndReturn(PermissionState::DeniedImplicitly);
    }

    for (auto& grantedPermissionEntry : grantedPermissionMatchPatterns) {
        if (urlMatchesWildcardHostPatterns(grantedPermissionEntry.key))
            return cacheResultAndReturn(PermissionState::GrantedImplicitly);
    }

    // Finally, check for requested patterns, allowing any pattern that matches. This is the default state
    // of the extension before any patterns are granted or denied, so it should always be last.

    if (skipRequestedPermissions)
        return cacheResultAndReturn(PermissionState::Unknown);

    auto requestedMatchPatterns = protect(extension())->allRequestedMatchPatterns();
    for (auto& requestedMatchPattern : requestedMatchPatterns) {
        if (urlMatchesPatternIgnoringWildcardHostPatterns(requestedMatchPattern))
            return cacheResultAndReturn(PermissionState::RequestedExplicitly);

        if (urlMatchesWildcardHostPatterns(requestedMatchPattern))
            return cacheResultAndReturn(PermissionState::RequestedImplicitly);
    }

    if (hasPermission(WebExtensionPermission::webNavigation(), tab, options))
        return cacheResultAndReturn(PermissionState::RequestedImplicitly);

    if (hasPermission(WebExtensionPermission::declarativeNetRequestFeedback(), tab, options))
        return cacheResultAndReturn(PermissionState::RequestedImplicitly);

    if (options.contains(PermissionStateOptions::RequestedWithTabsPermission) && hasPermission(WebExtensionPermission::tabs(), tab, options))
        return PermissionState::RequestedImplicitly;

    if (options.contains(PermissionStateOptions::IncludeOptionalPermissions)) {
        if (WebExtensionMatchPattern::patternsMatchURL(protect(extension())->optionalPermissionMatchPatterns(), url))
            return cacheResultAndReturn(PermissionState::RequestedImplicitly);
    }

    return cacheResultAndReturn(PermissionState::Unknown);
}

WebExtensionContext::PermissionState WebExtensionContext::permissionState(const WebExtensionMatchPattern& pattern, WebExtensionTab* tab, OptionSet<PermissionStateOptions> options)
{
    if (!pattern.isValid())
        return PermissionState::Unknown;

    if (!pattern.matchesAllURLs() && pattern.matchesURL(baseURL()))
        return PermissionState::GrantedImplicitly;

    if (!pattern.matchesAllURLs() && !WebExtensionMatchPattern::validSchemes().contains(pattern.scheme()))
        return PermissionState::Unknown;

    if (tab) {
        auto temporaryPattern = tab->temporaryPermissionMatchPattern();
        if (temporaryPattern && temporaryPattern->matchesPattern(pattern))
            return PermissionState::GrantedExplicitly;
    }

    // Access the maps here to remove any expired entries, and only do it once for this call.
    auto& grantedPermissionMatchPatterns = this->grantedPermissionMatchPatterns();
    auto& deniedPermissionMatchPatterns = this->deniedPermissionMatchPatterns();

    // First, check for patterns that are specific to certain domains, ignoring wildcard host patterns that
    // match all hosts. The order is denied, then granted. This makes sure denied takes precedence over granted.

    auto urlMatchesPatternIgnoringWildcardHostPatterns = [&](WebExtensionMatchPattern& otherPattern) {
        if (pattern.matchesAllHosts())
            return false;
        return pattern.matchesPattern(otherPattern);
    };

    for (auto& deniedPermissionEntry : deniedPermissionMatchPatterns) {
        if (urlMatchesPatternIgnoringWildcardHostPatterns(deniedPermissionEntry.key))
            return PermissionState::DeniedExplicitly;
    }

    for (auto& grantedPermissionEntry : grantedPermissionMatchPatterns) {
        if (urlMatchesPatternIgnoringWildcardHostPatterns(grantedPermissionEntry.key))
            return PermissionState::GrantedExplicitly;
    }

    // Next, check for patterns that are wildcard host patterns that match all hosts (<all_urls>, *://*/*, etc),
    // also checked in denied, then granted order. Doing these wildcard patterns separately allows for blanket
    // patterns to be set as default policies while allowing for specific domains to still be granted or denied.

    auto urlMatchesWildcardHostPatterns = [&](WebExtensionMatchPattern& otherPattern) {
        if (!pattern.matchesAllHosts())
            return false;
        return pattern.matchesPattern(otherPattern);
    };

    for (auto& deniedPermissionEntry : deniedPermissionMatchPatterns) {
        if (urlMatchesWildcardHostPatterns(deniedPermissionEntry.key))
            return PermissionState::DeniedImplicitly;
    }

    for (auto& grantedPermissionEntry : grantedPermissionMatchPatterns) {
        if (urlMatchesWildcardHostPatterns(grantedPermissionEntry.key))
            return PermissionState::GrantedImplicitly;
    }

    // Finally, check for requested patterns, allowing any pattern that matches. This is the default state
    // of the extension before any patterns are granted or denied, so it should always be last.

    if (options.contains(PermissionStateOptions::SkipRequestedPermissions))
        return PermissionState::Unknown;

    auto requestedMatchPatterns = protect(extension())->allRequestedMatchPatterns();
    for (auto& requestedMatchPattern : requestedMatchPatterns) {
        if (urlMatchesPatternIgnoringWildcardHostPatterns(requestedMatchPattern))
            return PermissionState::RequestedExplicitly;

        if (urlMatchesWildcardHostPatterns(requestedMatchPattern))
            return PermissionState::RequestedImplicitly;
    }

    if (options.contains(PermissionStateOptions::RequestedWithTabsPermission) && hasPermission(WebExtensionPermission::tabs(), tab, options))
        return PermissionState::RequestedImplicitly;

    if (options.contains(PermissionStateOptions::IncludeOptionalPermissions)) {
        if (WebExtensionMatchPattern::patternsMatchPattern(protect(extension())->optionalPermissionMatchPatterns(), pattern))
            return PermissionState::RequestedImplicitly;
    }

    return PermissionState::Unknown;
}

void WebExtensionContext::setPermissionState(PermissionState state, const String& permission, WallTime expirationDate)
{
    ASSERT(!permission.isEmpty());
    ASSERT(!expirationDate.isNaN());

    auto permissions = PermissionsSet { permission };

    switch (state) {
    case PermissionState::DeniedExplicitly:
        denyPermissions(WTF::move(permissions), expirationDate);
        break;

    case PermissionState::Unknown: {
        removeGrantedPermissions(permissions);
        removeDeniedPermissions(permissions);
        break;
    }

    case PermissionState::GrantedExplicitly:
        grantPermissions(WTF::move(permissions), expirationDate);
        break;

    case PermissionState::DeniedImplicitly:
    case PermissionState::RequestedImplicitly:
    case PermissionState::RequestedExplicitly:
    case PermissionState::GrantedImplicitly:
        ASSERT_NOT_REACHED();
        break;
    }
}

void WebExtensionContext::setPermissionState(PermissionState state, const URL& url, WallTime expirationDate)
{
    ASSERT(!url.isEmpty());
    ASSERT(!expirationDate.isNaN());

    RefPtr pattern = WebExtensionMatchPattern::getOrCreate(url);
    if (!pattern)
        return;

    setPermissionState(state, *pattern, expirationDate);
}

void WebExtensionContext::setPermissionState(PermissionState state, const WebExtensionMatchPattern& pattern, WallTime expirationDate)
{
    ASSERT(pattern.isValid());
    ASSERT(!expirationDate.isNaN());

    auto patterns = MatchPatternSet { const_cast<WebExtensionMatchPattern&>(pattern) };
    auto equalityOnly = pattern.matchesAllHosts() ? EqualityOnly::Yes : EqualityOnly::No;

    switch (state) {
    case PermissionState::DeniedExplicitly:
        denyPermissionMatchPatterns(WTF::move(patterns), expirationDate, equalityOnly);
        break;

    case PermissionState::Unknown: {
        removeGrantedPermissionMatchPatterns(patterns, equalityOnly);
        removeDeniedPermissionMatchPatterns(patterns, equalityOnly);
        break;
    }

    case PermissionState::GrantedExplicitly:
        grantPermissionMatchPatterns(WTF::move(patterns), expirationDate, equalityOnly);
        break;

    case PermissionState::DeniedImplicitly:
    case PermissionState::RequestedImplicitly:
    case PermissionState::RequestedExplicitly:
    case PermissionState::GrantedImplicitly:
        ASSERT_NOT_REACHED();
        break;
    }
}

void WebExtensionContext::clearCachedPermissionStates()
{
    m_cachedPermissionStates.clear();
    m_cachedPermissionURLs.clear();
}

bool WebExtensionContext::hasContentModificationRules()
{
    return declarativeNetRequestEnabledRulesetCount() || !m_sessionRulesIDs.isEmpty() || !m_dynamicRulesIDs.isEmpty();
}

WebExtensionContext::InjectedContentVector WebExtensionContext::injectedContents() const
{
    InjectedContentVector result = protect(extension())->staticInjectedContents();

    for (auto& entry : m_registeredScriptsMap)
        result.append(entry.value->injectedContent());

    return result;
}

bool WebExtensionContext::hasInjectedContentForURL(const URL& url)
{
    for (auto& injectedContent : injectedContents()) {
        // FIXME: <https://webkit.org/b/246492> Add support for exclude globs.
        bool isExcluded = false;
        for (auto& excludeMatchPattern : injectedContent.excludeMatchPatterns) {
            if (excludeMatchPattern->matchesURL(url)) {
                isExcluded = true;
                break;
            }
        }

        if (isExcluded)
            continue;

        // FIXME: <https://webkit.org/b/246492> Add support for include globs.
        for (auto& includeMatchPattern : injectedContent.includeMatchPatterns) {
            if (includeMatchPattern->matchesURL(url))
                return true;
        }
    }

    return false;
}

bool WebExtensionContext::hasInjectedContent()
{
    return !injectedContents().isEmpty();
}

void WebExtensionContext::addInjectedContent(const InjectedContentVector& injectedContents)
{
    if (!safeToInjectContent())
        return;

    // Only add content for one "all hosts" pattern if the extension has the permission.
    // This avoids duplicate injected content if individual hosts are granted in addition to "all hosts".
    if (hasAccessToAllHosts()) {
        addInjectedContent(injectedContents, WebExtensionMatchPattern::allHostsAndSchemesMatchPattern());
        return;
    }

    MatchPatternSet grantedMatchPatterns;
    for (auto& pattern : currentPermissionMatchPatterns())
        grantedMatchPatterns.add(pattern);

    addInjectedContent(injectedContents, grantedMatchPatterns);
}

void WebExtensionContext::addInjectedContent(const InjectedContentVector& injectedContents, const MatchPatternSet& grantedMatchPatterns)
{
    if (!safeToInjectContent())
        return;

    if (hasAccessToAllHosts()) {
        // If this is not currently granting "all hosts", then we can return early. This means
        // the "all hosts" pattern injected content was added already, and no content needs added.
        // Continuing here would add multiple copies of injected content, one for "all hosts" and
        // another for individually granted hosts.
        if (!WebExtensionMatchPattern::patternsMatchAllHosts(grantedMatchPatterns))
            return;

        // Since we are granting "all hosts" we want to remove any previously added content since
        // "all hosts" will cover any hosts previously added, and we don't want duplicate scripts.
        MatchPatternSet patternsToRemove;
        for (auto& entry : m_injectedScriptsPerPatternMap)
            patternsToRemove.add(entry.key);

        for (auto& entry : m_injectedStyleSheetsPerPatternMap)
            patternsToRemove.add(entry.key);

        for (auto& pattern : patternsToRemove)
            removeInjectedContent(pattern);
    }

    for (auto& pattern : grantedMatchPatterns)
        addInjectedContent(injectedContents, pattern);
}

static WebCore::UserScriptInjectionTime toImpl(WebExtension::InjectionTime injectionTime)
{
    switch (injectionTime) {
    case WebExtension::InjectionTime::DocumentStart:
        return WebCore::UserScriptInjectionTime::DocumentStart;
    case WebExtension::InjectionTime::DocumentIdle:
        // FIXME: <rdar://problem/57613315> Implement idle injection time. For now, the end injection time is fine.
    case WebExtension::InjectionTime::DocumentEnd:
        return WebCore::UserScriptInjectionTime::DocumentEnd;
    }

    return WebCore::UserScriptInjectionTime::DocumentEnd;
}

API::ContentWorld& WebExtensionContext::toContentWorld(WebExtensionContentWorldType contentWorldType) const
{
    ASSERT(isLoaded());

    switch (contentWorldType) {
    case WebExtensionContentWorldType::Main:
    case WebExtensionContentWorldType::WebPage:
#if ENABLE(INSPECTOR_EXTENSIONS)
    case WebExtensionContentWorldType::Inspector:
#endif
        return API::ContentWorld::pageContentWorldSingleton();
    case WebExtensionContentWorldType::ContentScript:
        return *m_contentScriptWorld;
    case WebExtensionContentWorldType::Native:
        ASSERT_NOT_REACHED();
        return API::ContentWorld::pageContentWorldSingleton();
    }

    return API::ContentWorld::pageContentWorldSingleton();
}

void WebExtensionContext::addInjectedContent(const InjectedContentVector& injectedContents, WebExtensionMatchPattern& pattern)
{
    if (!safeToInjectContent())
        return;

    auto scriptAddResult = m_injectedScriptsPerPatternMap.ensure(pattern, [&] {
        return UserScriptVector { };
    });

    auto styleSheetAddResult = m_injectedStyleSheetsPerPatternMap.ensure(pattern, [&] {
        return UserStyleSheetVector { };
    });

    auto& originInjectedScripts = scriptAddResult.iterator->value;
    auto& originInjectedStyleSheets = styleSheetAddResult.iterator->value;

    HashSet<String> baseExcludeMatchPatternsSet;

    auto& deniedMatchPatterns = deniedPermissionMatchPatterns();
    for (auto& deniedEntry : deniedMatchPatterns) {
        // Granted host patterns always win over revoked host patterns. Skip any revoked "all hosts" patterns.
        // This supports the case where "all hosts" is revoked and a handful of specific hosts are granted.
        Ref deniedMatchPattern = deniedEntry.key;
        if (deniedMatchPattern->matchesAllHosts())
            continue;

        // Only revoked patterns that match the granted pattern need to be included. This limits
        // the size of the exclude match patterns list to speed up processing.
        if (!pattern.matchesPattern(deniedMatchPattern, { WebExtensionMatchPattern::Options::IgnorePaths, WebExtensionMatchPattern::Options::MatchBidirectionally }))
            continue;

        for (const auto& deniedMatchPatternString : deniedMatchPattern->expandedStrings())
            baseExcludeMatchPatternsSet.add(deniedMatchPatternString);
    }

    auto& userContentControllers = this->userContentControllers();

    for (auto& injectedContentData : injectedContents) {
        HashSet<String> includeMatchPatternsSet;

        for (auto& includeMatchPattern : injectedContentData.includeMatchPatterns) {
            // Paths are not matched here since all we need to match at this point is scheme and host.
            // The path matching will happen in WebKit when deciding to inject content into a frame.

            // When the include pattern matches all hosts, we can generate a restricted patten here and skip
            // the more expensive calls to matchesPattern() below since we know they will match.
            if (includeMatchPattern->matchesAllHosts()) {
                auto restrictedPattern = WebExtensionMatchPattern::getOrCreate(includeMatchPattern->scheme(), pattern.host(), includeMatchPattern->path());
                if (!restrictedPattern)
                    continue;

                for (const auto& restrictedPattern : restrictedPattern->expandedStrings())
                    includeMatchPatternsSet.add(restrictedPattern);
                continue;
            }

            // When deciding if injected content patterns match, we need to check bidirectionally.
            // This allows an extension that requests *.wikipedia.org, to still inject content when
            // it is granted more specific access to *.en.wikipedia.org.
            if (!includeMatchPattern->matchesPattern(pattern, { WebExtensionMatchPattern::Options::IgnorePaths, WebExtensionMatchPattern::Options::MatchBidirectionally }))
                continue;

            // Pick the most restrictive match pattern by comparing unidirectionally to the granted origin pattern.
            // If the include pattern still matches the granted origin pattern, it is not restrictive enough.
            // In that case we need to use the include pattern scheme and path, but with the granted pattern host.
            RefPtr restrictedPattern = includeMatchPattern.ptr();
            if (includeMatchPattern->matchesPattern(pattern, { WebExtensionMatchPattern::Options::IgnoreSchemes, WebExtensionMatchPattern::Options::IgnorePaths }))
                restrictedPattern = WebExtensionMatchPattern::getOrCreate(includeMatchPattern->scheme(), pattern.host(), includeMatchPattern->path());
            if (!restrictedPattern)
                continue;

            for (const auto& restrictedPattern : restrictedPattern->expandedStrings())
                includeMatchPatternsSet.add(restrictedPattern);
        }

        if (includeMatchPatternsSet.isEmpty())
            continue;

        auto includeMatchPatterns = copyToVector(includeMatchPatternsSet);

        HashSet<String> excludeMatchPatternsSet;
        excludeMatchPatternsSet.addAll(injectedContentData.expandedExcludeMatchPatternStrings());
        excludeMatchPatternsSet.unionWith(baseExcludeMatchPatternsSet);

        auto excludeMatchPatterns = copyToVector(excludeMatchPatternsSet);

        auto injectedFrames = injectedContentData.injectsIntoAllFrames ? WebCore::UserContentInjectedFrames::InjectInAllFrames : WebCore::UserContentInjectedFrames::InjectInTopFrameOnly;
        auto injectionTime = toImpl(injectedContentData.injectionTime);
        Ref executionWorld = toContentWorld(injectedContentData.contentWorldType);
        auto styleLevel = injectedContentData.styleLevel;
        auto matchParentFrame = injectedContentData.matchParentFrame;

        auto scriptID = injectedContentData.identifier;
        bool isRegisteredScript = !scriptID.isEmpty();

        RefPtr extension = m_extension;

        for (auto& scriptPath : injectedContentData.scriptPaths) {
            auto scriptStringResult = extension->resourceStringForPath(scriptPath, WebExtension::CacheResult::Yes);
            if (!scriptStringResult) {
                recordErrorIfNeeded(scriptStringResult.error());
                continue;
            }

            auto scriptString = scriptStringResult.value();

            Ref userScript = API::UserScript::create(WebCore::UserScript { WTF::move(scriptString), URL { m_baseURL, scriptPath }, Vector { includeMatchPatterns }, Vector { excludeMatchPatterns }, injectionTime, injectedFrames, matchParentFrame }, executionWorld);
            originInjectedScripts.append(userScript);

            for (Ref userContentController : userContentControllers)
                userContentController->addUserScript(userScript, InjectUserScriptImmediately::Yes);

            if (isRegisteredScript) {
                RefPtr registeredScript = m_registeredScriptsMap.get(scriptID);
                ASSERT(registeredScript);
                if (!registeredScript)
                    continue;

                registeredScript->addUserScript(scriptID, userScript);
            }
        }

        for (auto& styleSheetPath : injectedContentData.styleSheetPaths) {
            auto styleSheetStringResult = extension->resourceStringForPath(styleSheetPath, WebExtension::CacheResult::Yes);
            if (!styleSheetStringResult) {
                recordErrorIfNeeded(styleSheetStringResult.error());
                continue;
            }

            auto styleSheetString = localizedResourceString(styleSheetStringResult.value(), "text/css"_s);

            Ref userStyleSheet = API::UserStyleSheet::create(WebCore::UserStyleSheet { WTF::move(styleSheetString), URL { m_baseURL, styleSheetPath }, Vector { includeMatchPatterns }, Vector { excludeMatchPatterns }, injectedFrames, matchParentFrame, styleLevel, std::nullopt }, executionWorld);
            originInjectedStyleSheets.append(userStyleSheet);

            for (Ref userContentController : userContentControllers)
                userContentController->addUserStyleSheet(userStyleSheet);

            if (isRegisteredScript) {
                RefPtr registeredScript = m_registeredScriptsMap.get(scriptID);
                ASSERT(registeredScript);
                if (!registeredScript)
                    continue;

                registeredScript->addUserStyleSheet(scriptID, userStyleSheet);
            }
        }
    }
}

void WebExtensionContext::addInjectedContent(WebUserContentControllerProxy& userContentController)
{
    if (!safeToInjectContent())
        return;

    for (auto& entry : m_injectedScriptsPerPatternMap) {
        for (auto& userScript : entry.value)
            userContentController.addUserScript(userScript, InjectUserScriptImmediately::Yes);
    }

    for (auto& entry : m_injectedStyleSheetsPerPatternMap) {
        for (auto& userStyleSheet : entry.value)
            userContentController.addUserStyleSheet(userStyleSheet);
    }
}

bool WebExtensionContext::hasAccessToAllURLs()
{
    for (auto& pattern : currentPermissionMatchPatterns()) {
        if (pattern->matchesAllURLs())
            return true;
    }

    return false;
}

bool WebExtensionContext::hasAccessToAllHosts()
{
    for (auto& pattern : currentPermissionMatchPatterns()) {
        if (pattern->matchesAllHosts())
            return true;
    }

    return false;
}

void WebExtensionContext::removeInjectedContent()
{
    if (!isLoaded())
        return;

    // Use all user content controllers in case the extension was briefly allowed in private browsing
    // and content was injected into any of those content controllers.
    for (Ref userContentController : extensionController()->allUserContentControllers()) {
        for (auto& entry : m_injectedScriptsPerPatternMap) {
            for (auto& userScript : entry.value)
                userContentController->removeUserScript(userScript);
        }

        for (auto& entry : m_injectedStyleSheetsPerPatternMap) {
            for (auto& userStyleSheet : entry.value)
                userContentController->removeUserStyleSheet(userStyleSheet);
        }
    }

    m_injectedScriptsPerPatternMap.clear();
    m_injectedStyleSheetsPerPatternMap.clear();
}

void WebExtensionContext::removeInjectedContent(const MatchPatternSet& removedMatchPatterns)
{
    if (!isLoaded())
        return;

    for (auto& removedPattern : removedMatchPatterns)
        removeInjectedContent(removedPattern);

    // If "all hosts" was removed, then we need to add back any individual granted hosts,
    // now that the catch all pattern has been removed.
    if (WebExtensionMatchPattern::patternsMatchAllHosts(removedMatchPatterns))
        addInjectedContent();
}

void WebExtensionContext::removeInjectedContent(WebExtensionMatchPattern& pattern)
{
    if (!isLoaded())
        return;

    auto originInjectedScripts = m_injectedScriptsPerPatternMap.take(pattern);
    auto originInjectedStyleSheets = m_injectedStyleSheetsPerPatternMap.take(pattern);

    if (originInjectedScripts.isEmpty() && originInjectedStyleSheets.isEmpty())
        return;

    // Use all user content controllers in case the extension was briefly allowed in private browsing
    // and content was injected into any of those content controllers.
    for (Ref userContentController : extensionController()->allUserContentControllers()) {
        for (auto& userScript : originInjectedScripts)
            userContentController->removeUserScript(userScript);

        for (auto& userStyleSheet : originInjectedStyleSheets)
            userContentController->removeUserStyleSheet(userStyleSheet);
    }
}

void WebExtensionContext::removeInjectedContent(WebUserContentControllerProxy& userContentController)
{
    for (auto& entry : m_injectedScriptsPerPatternMap) {
        for (auto& userScript : entry.value)
            userContentController.removeUserScript(userScript);
    }

    for (auto& entry : m_injectedStyleSheetsPerPatternMap) {
        for (auto& userStyleSheet : entry.value)
            userContentController.removeUserStyleSheet(userStyleSheet);
    }
}

#if ENABLE(DNR_ON_RULE_MATCHED_DEBUG)
void WebExtensionContext::handleContentRuleListMatchedRule(WebExtensionTab& tab, WebCore::ContentRuleListMatchedRule& matchedRule)
{
    // FIXME: <158147119> Figure out the permissions story for onRuleMatchedDebug
    if (!(hasPermission(WebExtensionPermission::declarativeNetRequestFeedback()) && hasPermission(WebExtensionPermission::declarativeNetRequest()) && hasPermission(URL { matchedRule.request.url }, &tab)))
        return;

    wakeUpBackgroundContentIfNecessaryToFireEvents({ WebExtensionEventListenerType::DeclarativeNetRequestOnRuleMatchedDebug }, [=, this, protectedThis = Ref { *this }] {
        sendToProcessesForEvent(WebExtensionEventListenerType::DeclarativeNetRequestOnRuleMatchedDebug, Messages::WebExtensionContextProxy::DispatchOnRuleMatchedDebugEvent(matchedRule));
    });
}
#endif

bool WebExtensionContext::handleContentRuleListNotificationForTab(WebExtensionTab& tab, const URL& url, WebCore::ContentRuleListResults::Result)
{
    incrementActionCountForTab(tab, 1);

    if (!hasPermission(WebExtensionPermission::declarativeNetRequestFeedback()) && !(hasPermission(WebExtensionPermission::declarativeNetRequest()) && hasPermission(url, &tab)))
        return false;

    m_matchedRules.append({
        url,
        WallTime::now(),
        tab.identifier()
    });

    return true;
}

bool WebExtensionContext::purgeMatchedRulesFromBefore(const WallTime& startTime)
{
    if (m_matchedRules.isEmpty())
        return false;

    DeclarativeNetRequestMatchedRuleVector filteredMatchedRules;
    for (auto& matchedRule : m_matchedRules) {
        if (matchedRule.timeStamp >= startTime)
            filteredMatchedRules.append(matchedRule);
    }

    m_matchedRules = WTF::move(filteredMatchedRules);

    return !m_matchedRules.isEmpty();
}

void WebExtensionContext::addDeclarativeNetRequestRules(WebUserContentControllerProxy& controllerProxy)
{
    API::ContentRuleListStore::defaultStoreSingleton().lookupContentRuleListFile(declarativeNetRequestContentRuleListFilePath(), uniqueIdentifier().isolatedCopy(), [this, protectedThis = Ref { *this }, controllerProxy = Ref { controllerProxy }](RefPtr<API::ContentRuleList> ruleList, std::error_code) {
        if (!ruleList)
            return;

        // The extension could have been unloaded before this was called.
        if (!isLoaded())
            return;

        controllerProxy->addContentRuleList(*ruleList, m_baseURL);
    });
}

void WebExtensionContext::addDeclarativeNetRequestRulesToPrivateUserContentControllers()
{
    API::ContentRuleListStore::defaultStoreSingleton().lookupContentRuleListFile(declarativeNetRequestContentRuleListFilePath(), uniqueIdentifier().isolatedCopy(), [this, protectedThis = Ref { *this }](RefPtr<API::ContentRuleList> ruleList, std::error_code) {
        if (!ruleList)
            return;

        // The extension could have been unloaded before this was called.
        if (!isLoaded())
            return;

        for (Ref controller : extensionController()->allPrivateUserContentControllers())
            controller->addContentRuleList(*ruleList, m_baseURL);
    });
}

static HashMap<WebExtensionContextIdentifier, WeakRef<WebExtensionContext>>& webExtensionContexts()
{
    static NeverDestroyed<HashMap<WebExtensionContextIdentifier, WeakRef<WebExtensionContext>>> contexts;
    return contexts;
}

WebExtensionContext* WebExtensionContext::get(WebExtensionContextIdentifier identifier)
{
    return webExtensionContexts().get(identifier);
}

WebExtensionContext::WebExtensionContext()
{
    ASSERT(!get(identifier()));
    webExtensionContexts().add(identifier(), *this);
}

WebExtensionContextIdentifier WebExtensionContext::privilegedIdentifier() const
{
    if (!m_privilegedIdentifier)
        m_privilegedIdentifier = WebExtensionContextIdentifier::generate();
    return *m_privilegedIdentifier;
}

bool WebExtensionContext::isPrivilegedMessage(IPC::Decoder& message) const
{
    if (!m_privilegedIdentifier)
        return false;
    return m_privilegedIdentifier.value().toRawValue() == message.destinationID();
}

WebExtensionContextParameters WebExtensionContext::parameters(IncludePrivilegedIdentifier includePrivilegedIdentifier) const
{
    RefPtr extension = m_extension;

    return {
        identifier(),
        includePrivilegedIdentifier == IncludePrivilegedIdentifier::Yes ? std::optional(privilegedIdentifier()) : std::nullopt,
        baseURL(),
        uniqueIdentifier(),
        unsupportedAPIs(),
        m_grantedPermissions,
        extension->serializeLocalization(),
        extension->serializeManifest(),
        extension->manifestVersion(),
        isSessionStorageAllowedInContentScripts(),
        backgroundPageIdentifier(),
#if ENABLE(INSPECTOR_EXTENSIONS)
        inspectorPageIdentifiers(),
        inspectorBackgroundPageIdentifiers(),
#endif
        popupPageIdentifiers(),
        tabPageIdentifiers()
    };
}

bool WebExtensionContext::inTestingMode() const
{
    return m_extensionController && m_extensionController->inTestingMode();
}

const WebExtensionContext::UserContentControllerProxySet& WebExtensionContext::userContentControllers() const
{
    ASSERT(isLoaded());

    if (hasAccessToPrivateData())
        return extensionController()->allUserContentControllers();
    return extensionController()->allNonPrivateUserContentControllers();
}

WebExtensionContext::WebProcessProxySet WebExtensionContext::processes(EventListenerTypeSet&& typeSet, ContentWorldTypeSet&& contentWorldTypeSet, Function<bool(WebProcessProxy&, WebPageProxy&, WebFrameProxy&)>&& predicate) const
{
    if (!isLoaded())
        return { };

#if ENABLE(INSPECTOR_EXTENSIONS)
    // Inspector content world is a special alias of Main. Include it when Main is requested (and vice versa).
    if (contentWorldTypeSet.contains(WebExtensionContentWorldType::Main))
        contentWorldTypeSet.add(WebExtensionContentWorldType::Inspector);
    else if (contentWorldTypeSet.contains(WebExtensionContentWorldType::Inspector))
        contentWorldTypeSet.add(WebExtensionContentWorldType::Main);
#endif

    WebProcessProxySet result;

    for (auto type : typeSet) {
        for (auto contentWorldType : contentWorldTypeSet) {
            auto pagesEntry = m_eventListenerFrames.find({ type, contentWorldType });
            if (pagesEntry == m_eventListenerFrames.end())
                continue;

            for (auto entry : pagesEntry->value) {
                Ref frame = entry.key;
                RefPtr page = frame->page();
                if (!page)
                    continue;

                if (!hasAccessToPrivateData() && page->sessionID().isEphemeral())
                    continue;

                Ref webProcess = frame->process();
                if (predicate && !predicate(webProcess, *page, frame))
                    continue;

                if (webProcess->canSendMessage())
                    result.add(webProcess);
            }
        }
    }

    return result;
}

String WebExtensionContext::processDisplayName()
{
    return WEB_UI_FORMAT_STRING("%s Web Extension", "Extension's process name that appears in Activity Monitor where the parameter is the name of the extension", protect(extension())->displayShortName().utf8().data());
}

Vector<String> WebExtensionContext::corsDisablingPatterns()
{
    Vector<String> patterns;

    auto grantedMatchPatterns = grantedPermissionMatchPatterns();
    for (auto& entry : grantedMatchPatterns) {
        Ref pattern = entry.key;
        patterns.appendVector(pattern->expandedStrings());
    }

    removeRepeatedElements(patterns);

    return patterns;
}

URL WebExtensionContext::backgroundContentURL()
{
    RefPtr extension = m_extension;
    if (!extension->hasBackgroundContent())
        return { };
    return { m_baseURL, extension->backgroundContentPath() };
}

void WebExtensionContext::loadBackgroundContent(CompletionHandler<void(RefPtr<API::Error>)>&& completionHandler)
{
    if (!protect(extension())->hasBackgroundContent()) {
        if (completionHandler)
            completionHandler(createError(Error::NoBackgroundContent));
        return;
    }

    wakeUpBackgroundContentIfNecessary([this, protectedThis = Ref { *this }, completionHandler = WTF::move(completionHandler)]() mutable {
        if (completionHandler)
            completionHandler(backgroundContentLoadError());
    });
}

void WebExtensionContext::loadBackgroundWebViewDuringLoad()
{
    ASSERT(isLoaded());

    RefPtr extension = m_extension;
    if (!extension->hasBackgroundContent())
        return;

    m_safeToLoadBackgroundContent = true;

    if (!extension->backgroundContentIsPersistent()) {
        loadBackgroundPageListenersFromStorage();

        bool hasEventsToFire = m_shouldFireStartupEvent || m_installReason != InstallReason::None;
        if (m_backgroundContentEventListeners.isEmpty() || hasEventsToFire)
            loadBackgroundWebView();
    } else
        loadBackgroundWebView();
}

bool WebExtensionContext::isBackgroundPage(WebCore::FrameIdentifier frameIdentifier) const
{
    RefPtr frame = WebFrameProxy::webFrame(frameIdentifier);
    if (!frame)
        return false;

    RefPtr page = frame->page();
    if (!page)
        return false;

    return isBackgroundPage(page->identifier());
}

const String& WebExtensionContext::backgroundWebViewInspectionName()
{
    if (!m_backgroundWebViewInspectionName.isEmpty())
        return m_backgroundWebViewInspectionName;

    if (protect(extension())->backgroundContentIsServiceWorker())
        m_backgroundWebViewInspectionName = WEB_UI_FORMAT_STRING("%s — Extension Service Worker", "Label for an inspectable Web Extension service worker", protect(extension())->displayShortName().utf8().data());
    else
        m_backgroundWebViewInspectionName = WEB_UI_FORMAT_STRING("%s — Extension Background Page", "Label for an inspectable Web Extension background page", protect(extension())->displayShortName().utf8().data());

    return m_backgroundWebViewInspectionName;
}

void WebExtensionContext::wakeUpBackgroundContentIfNecessary(Function<void()>&& completionHandler)
{
    if (!protect(extension())->hasBackgroundContent()) {
        completionHandler();
        return;
    }

    scheduleBackgroundContentToUnload();

    if (backgroundContentIsLoaded()) {
        completionHandler();
        return;
    }

    RELEASE_LOG_DEBUG(Extensions, "Scheduled task for after background content loads");

    m_actionsToPerformAfterBackgroundContentLoads.append(WTF::move(completionHandler));

    loadBackgroundWebViewIfNeeded();
}

void WebExtensionContext::wakeUpBackgroundContentIfNecessaryToFireEvents(EventListenerTypeSet&& types, Function<void()>&& completionHandler)
{
    RefPtr extension = m_extension;
    if (!extension->hasBackgroundContent()) {
        completionHandler();
        return;
    }

    if (!extension->backgroundContentIsPersistent()) {
        bool backgroundContentListensToAtLeastOneEvent = false;
        for (auto& type : types) {
            if (m_backgroundContentEventListeners.contains(type)) {
                backgroundContentListensToAtLeastOneEvent = true;
                break;
            }
        }

        // Don't load the background page if it isn't expecting these events.
        if (!backgroundContentListensToAtLeastOneEvent) {
            completionHandler();
            return;
        }
    }

    wakeUpBackgroundContentIfNecessary(WTF::move(completionHandler));
}

#if ENABLE(INSPECTOR_EXTENSIONS)
URL WebExtensionContext::inspectorBackgroundPageURL() const
{
    RefPtr extension = m_extension;
    if (!extension->hasInspectorBackgroundPage())
        return { };
    return { m_baseURL, extension->inspectorBackgroundPagePath() };
}

RefPtr<WebInspectorUIProxy> WebExtensionContext::inspector(const API::InspectorExtension& inspectorExtension) const
{
    ASSERT(isLoaded());
    ASSERT(protect(extension())->hasInspectorBackgroundPage());

    for (auto entry : m_inspectorContextMap) {
        if (entry.value.extension == &inspectorExtension)
            return &entry.key;
    }

    return nullptr;
}
#endif // ENABLE(INSPECTOR_EXTENSIONS)

size_t WebExtensionContext::quotaForStorageType(WebExtensionDataType storageType)
{
    switch (storageType) {
    case WebExtensionDataType::Local:
        return hasPermission(WebExtensionPermission::unlimitedStorage()) ? webExtensionUnlimitedStorageQuotaBytes : webExtensionStorageAreaLocalQuotaBytes;
    case WebExtensionDataType::Session:
        return webExtensionStorageAreaSessionQuotaBytes;
    case WebExtensionDataType::Sync:
        return webExtensionStorageAreaSyncQuotaBytes;
    }

    ASSERT_NOT_REACHED();
    return 0;
}

Ref<WebExtensionStorageSQLiteStore> WebExtensionContext::localStorageStore()
{
    if (!m_localStorageStore)
        m_localStorageStore = WebExtensionStorageSQLiteStore::create(m_uniqueIdentifier, WebExtensionDataType::Local, storageDirectory(), storageIsPersistent() ? WebExtensionStorageSQLiteStore::UsesInMemoryDatabase::No : WebExtensionStorageSQLiteStore::UsesInMemoryDatabase::Yes);
    return *m_localStorageStore;
}

Ref<WebExtensionStorageSQLiteStore> WebExtensionContext::sessionStorageStore()
{
    if (!m_sessionStorageStore)
        m_sessionStorageStore = WebExtensionStorageSQLiteStore::create(m_uniqueIdentifier, WebExtensionDataType::Session, storageDirectory(), WebExtensionStorageSQLiteStore::UsesInMemoryDatabase::Yes);
    return *m_sessionStorageStore;
}

Ref<WebExtensionStorageSQLiteStore> WebExtensionContext::syncStorageStore()
{
    if (!m_syncStorageStore)
        m_syncStorageStore = WebExtensionStorageSQLiteStore::create(m_uniqueIdentifier, WebExtensionDataType::Sync, storageDirectory(), storageIsPersistent() ? WebExtensionStorageSQLiteStore::UsesInMemoryDatabase::No : WebExtensionStorageSQLiteStore::UsesInMemoryDatabase::Yes);
    return *m_syncStorageStore;
}

Ref<WebExtensionStorageSQLiteStore> WebExtensionContext::storageForType(WebExtensionDataType storageType)
{
    switch (storageType) {
    case WebExtensionDataType::Local:
        return localStorageStore();
    case WebExtensionDataType::Session:
        return sessionStorageStore();
    case WebExtensionDataType::Sync:
        return syncStorageStore();
    }

    return sessionStorageStore();
}

Ref<WebExtensionRegisteredScriptsSQLiteStore> WebExtensionContext::registeredContentScriptsStore()
{
    if (!m_registeredContentScriptsStorage)
        m_registeredContentScriptsStorage = WebExtensionRegisteredScriptsSQLiteStore::create(m_uniqueIdentifier, storageDirectory(), !storageIsPersistent());
    return *m_registeredContentScriptsStorage;
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
