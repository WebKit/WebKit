/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2015-2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "InspectorPageAgent.h"

#include "CachedResource.h"
#include "Cookie.h"
#include "CookieJar.h"
#include "DOMWrapperWorld.h"
#include "DocumentLoader.h"
#include "DocumentResourceLoader.h"
#include "DocumentView.h"
#include "ElementInlines.h"
#include "EventTargetInlines.h"
#include "ForcedAccessibilityValue.h"
#include "FrameInlines.h"
#include "FrameLoadRequest.h"
#include "FrameLoader.h"
#include "FrameSnapshotting.h"
#include "HTMLFrameOwnerElement.h"
#include "HTMLNames.h"
#include "ImageBuffer.h"
#include "InspectorBackendClient.h"
#include "InspectorDOMAgent.h"
#include "InspectorNetworkAgent.h"
#include "InspectorOverlay.h"
#include "InspectorResourceUtilities.h"
#include "InstrumentingAgents.h"
#include "LocalFrame.h"
#include "LocalFrameView.h"
#include "MIMETypeRegistry.h"
#include "MemoryCache.h"
#include "Page.h"
#include "RenderObjectInlines.h"
#include "RenderTheme.h"
#include "ScriptController.h"
#include "ScriptSourceCode.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "ShouldPartitionCookie.h"
#include "StyleScope.h"
#include "Theme.h"
#include <pal/text/TextEncoding.h>
#include "UserGestureIndicator.h"
#include <JavaScriptCore/ContentSearchUtilities.h>
#include <JavaScriptCore/IdentifiersFactory.h>
#include <JavaScriptCore/RegularExpression.h>
#include <wtf/ListHashSet.h>
#include <wtf/Stopwatch.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/Base64.h>
#include <wtf/text/StringBuilder.h>

#if ENABLE(APPLICATION_MANIFEST)
#include "CachedApplicationManifest.h"
#endif

#if ENABLE(WEB_ARCHIVE) && USE(CF)
#include "LegacyWebArchive.h"
#endif

#if USE(CF)
#include <wtf/cf/VectorCF.h>
#endif


namespace WebCore {

using namespace Inspector;

WTF_MAKE_TZONE_ALLOCATED_IMPL(InspectorPageAgent);

Ref<InspectorOverlay> InspectorPageAgent::protectedOverlay() const
{
    return m_overlay.get();
}

InspectorPageAgent::InspectorPageAgent(PageAgentContext& context, InspectorBackendClient* client, InspectorOverlay& overlay)
    : InspectorAgentBase("Page"_s, context)
    , m_frontendDispatcher(makeUniqueRef<Inspector::PageFrontendDispatcher>(context.frontendRouter))
    , m_backendDispatcher(Inspector::PageBackendDispatcher::create(context.backendDispatcher, this))
    , m_inspectedPage(context.inspectedPage)
    , m_client(client)
    , m_overlay(overlay)
{
}

InspectorPageAgent::~InspectorPageAgent() = default;

void InspectorPageAgent::didCreateFrontendAndBackend()
{
}

void InspectorPageAgent::willDestroyFrontendAndBackend(Inspector::DisconnectReason)
{
    disable();
}

Inspector::Protocol::ErrorStringOr<void> InspectorPageAgent::enable()
{
    Ref agents = m_instrumentingAgents.get();
    if (agents->enabledPageAgent() == this)
        return makeUnexpected("Page domain already enabled"_s);

    agents->setEnabledPageAgent(this);

    auto& stopwatch = m_environment.executionStopwatch();
    stopwatch.reset();
    stopwatch.start();

    defaultUserPreferencesDidChange();

    return { };
}

Inspector::Protocol::ErrorStringOr<void> InspectorPageAgent::disable()
{
    Ref { m_instrumentingAgents.get() }->setEnabledPageAgent(nullptr);

    setShowPaintRects(false);
#if !PLATFORM(IOS_FAMILY)
    setShowRulers(false);
#endif
    overrideUserAgent(nullString());
    setEmulatedMedia(emptyString());
    overridePrefersColorScheme(std::nullopt);

    auto& inspectedPageSettings = m_inspectedPage->settings();
    inspectedPageSettings.setAuthorAndUserStylesEnabledInspectorOverride(std::nullopt);
    inspectedPageSettings.setICECandidateFilteringEnabledInspectorOverride(std::nullopt);
    inspectedPageSettings.setImagesEnabledInspectorOverride(std::nullopt);
    inspectedPageSettings.setMediaCaptureRequiresSecureConnectionInspectorOverride(std::nullopt);
    inspectedPageSettings.setMockCaptureDevicesEnabledInspectorOverride(std::nullopt);
    inspectedPageSettings.setNeedsSiteSpecificQuirksInspectorOverride(std::nullopt);
    inspectedPageSettings.setScriptEnabledInspectorOverride(std::nullopt);
    inspectedPageSettings.setShowDebugBordersInspectorOverride(std::nullopt);
    inspectedPageSettings.setShowRepaintCounterInspectorOverride(std::nullopt);
    inspectedPageSettings.setWebSecurityEnabledInspectorOverride(std::nullopt);
    inspectedPageSettings.setForcedPrefersReducedMotionAccessibilityValue(ForcedAccessibilityValue::System);
    inspectedPageSettings.setForcedPrefersContrastAccessibilityValue(ForcedAccessibilityValue::System);

    m_client->setDeveloperPreferenceOverride(InspectorBackendClient::DeveloperPreference::PrivateClickMeasurementDebugModeEnabled, std::nullopt);
    m_client->setDeveloperPreferenceOverride(InspectorBackendClient::DeveloperPreference::ITPDebugModeEnabled, std::nullopt);
    m_client->setDeveloperPreferenceOverride(InspectorBackendClient::DeveloperPreference::MockCaptureDevicesEnabled, std::nullopt);
    m_client->setDeveloperPreferenceOverride(InspectorBackendClient::DeveloperPreference::NeedsSiteSpecificQuirks, std::nullopt);

    return { };
}

double InspectorPageAgent::timestamp()
{
    return m_environment.executionStopwatch().elapsedTime().seconds();
}

Inspector::Protocol::ErrorStringOr<void> InspectorPageAgent::reload(std::optional<bool>&& ignoreCache, std::optional<bool>&& revalidateAllResources)
{
    OptionSet<ReloadOption> reloadOptions;
    if (ignoreCache && *ignoreCache)
        reloadOptions.add(ReloadOption::FromOrigin);
    if (!revalidateAllResources || !*revalidateAllResources)
        reloadOptions.add(ReloadOption::ExpiredOnly);

    RefPtr localMainFrame = m_inspectedPage->localMainFrame();
    if (!localMainFrame)
        return makeUnexpected("main frame is not local"_s);
    localMainFrame->loader().reload(reloadOptions);

    return { };
}

Inspector::Protocol::ErrorStringOr<void> InspectorPageAgent::overrideUserAgent(const String& value)
{
    m_userAgentOverride = value;

    return { };
}

Inspector::Protocol::ErrorStringOr<void> InspectorPageAgent::overrideSetting(Inspector::Protocol::Page::Setting setting, std::optional<bool>&& value)
{
    auto& inspectedPageSettings = m_inspectedPage->settings();

    switch (setting) {
    case Inspector::Protocol::Page::Setting::PrivateClickMeasurementDebugModeEnabled:
        m_client->setDeveloperPreferenceOverride(InspectorBackendClient::DeveloperPreference::PrivateClickMeasurementDebugModeEnabled, value);
        return { };

    case Inspector::Protocol::Page::Setting::AuthorAndUserStylesEnabled:
        inspectedPageSettings.setAuthorAndUserStylesEnabledInspectorOverride(value);
        return { };

    case Inspector::Protocol::Page::Setting::ICECandidateFilteringEnabled:
        inspectedPageSettings.setICECandidateFilteringEnabledInspectorOverride(value);
        return { };

    case Inspector::Protocol::Page::Setting::ITPDebugModeEnabled:
        m_client->setDeveloperPreferenceOverride(InspectorBackendClient::DeveloperPreference::ITPDebugModeEnabled, value);
        return { };

    case Inspector::Protocol::Page::Setting::ImagesEnabled:
        inspectedPageSettings.setImagesEnabledInspectorOverride(value);
        return { };

    case Inspector::Protocol::Page::Setting::MediaCaptureRequiresSecureConnection:
        inspectedPageSettings.setMediaCaptureRequiresSecureConnectionInspectorOverride(value);
        return { };

    case Inspector::Protocol::Page::Setting::MockCaptureDevicesEnabled:
        inspectedPageSettings.setMockCaptureDevicesEnabledInspectorOverride(value);
        m_client->setDeveloperPreferenceOverride(InspectorBackendClient::DeveloperPreference::MockCaptureDevicesEnabled, value);
        return { };

    case Inspector::Protocol::Page::Setting::NeedsSiteSpecificQuirks:
        inspectedPageSettings.setNeedsSiteSpecificQuirksInspectorOverride(value);
        m_client->setDeveloperPreferenceOverride(InspectorBackendClient::DeveloperPreference::NeedsSiteSpecificQuirks, value);
        return { };

    case Inspector::Protocol::Page::Setting::ScriptEnabled:
        inspectedPageSettings.setScriptEnabledInspectorOverride(value);
        return { };

    case Inspector::Protocol::Page::Setting::ShowDebugBorders:
        inspectedPageSettings.setShowDebugBordersInspectorOverride(value);
        return { };

    case Inspector::Protocol::Page::Setting::ShowRepaintCounter:
        inspectedPageSettings.setShowRepaintCounterInspectorOverride(value);
        return { };

    case Inspector::Protocol::Page::Setting::WebSecurityEnabled:
        inspectedPageSettings.setWebSecurityEnabledInspectorOverride(value);
        return { };
    }

    ASSERT_NOT_REACHED();
    return { };
}

Inspector::Protocol::ErrorStringOr<void> InspectorPageAgent::overrideUserPreference(Inspector::Protocol::Page::UserPreferenceName preference, std::optional<Inspector::Protocol::Page::UserPreferenceValue>&& value)
{
    switch (preference) {
    case Inspector::Protocol::Page::UserPreferenceName::PrefersReducedMotion:
        overridePrefersReducedMotion(WTFMove(value));
        return { };

    case Inspector::Protocol::Page::UserPreferenceName::PrefersContrast:
        overridePrefersContrast(WTFMove(value));
        return { };

    case Inspector::Protocol::Page::UserPreferenceName::PrefersColorScheme:
        overridePrefersColorScheme(WTFMove(value));
        return { };
    }

    ASSERT_NOT_REACHED();
    return { };
}

void InspectorPageAgent::overridePrefersReducedMotion(std::optional<Inspector::Protocol::Page::UserPreferenceValue>&& value)
{
    ForcedAccessibilityValue forcedValue = ForcedAccessibilityValue::System;

    if (value == Inspector::Protocol::Page::UserPreferenceValue::Reduce)
        forcedValue = ForcedAccessibilityValue::On;
    else if (value == Inspector::Protocol::Page::UserPreferenceValue::NoPreference)
        forcedValue = ForcedAccessibilityValue::Off;

    m_inspectedPage->settings().setForcedPrefersReducedMotionAccessibilityValue(forcedValue);
    m_inspectedPage->accessibilitySettingsDidChange();
}

void InspectorPageAgent::overridePrefersContrast(std::optional<Inspector::Protocol::Page::UserPreferenceValue>&& value)
{
    ForcedAccessibilityValue forcedValue = ForcedAccessibilityValue::System;

    if (value == Inspector::Protocol::Page::UserPreferenceValue::More)
        forcedValue = ForcedAccessibilityValue::On;
    else if (value == Inspector::Protocol::Page::UserPreferenceValue::NoPreference)
        forcedValue = ForcedAccessibilityValue::Off;

    m_inspectedPage->settings().setForcedPrefersContrastAccessibilityValue(forcedValue);
    m_inspectedPage->accessibilitySettingsDidChange();
}

void InspectorPageAgent::overridePrefersColorScheme(std::optional<Inspector::Protocol::Page::UserPreferenceValue>&& value)
{
#if ENABLE(DARK_MODE_CSS)
    if (!value)
        m_inspectedPage->setUseDarkAppearanceOverride(std::nullopt);
    else if (value == Inspector::Protocol::Page::UserPreferenceValue::Light)
        m_inspectedPage->setUseDarkAppearanceOverride(false);
    else if (value == Inspector::Protocol::Page::UserPreferenceValue::Dark)
        m_inspectedPage->setUseDarkAppearanceOverride(true);
#else
    UNUSED_PARAM(value);
#endif
}

static Inspector::Protocol::Page::CookieSameSitePolicy cookieSameSitePolicyJSON(Cookie::SameSitePolicy policy)
{
    switch (policy) {
    case Cookie::SameSitePolicy::None:
        return Inspector::Protocol::Page::CookieSameSitePolicy::None;
    case Cookie::SameSitePolicy::Lax:
        return Inspector::Protocol::Page::CookieSameSitePolicy::Lax;
    case Cookie::SameSitePolicy::Strict:
        return Inspector::Protocol::Page::CookieSameSitePolicy::Strict;
    }
    ASSERT_NOT_REACHED();
    return Inspector::Protocol::Page::CookieSameSitePolicy::None;
}

static Ref<Inspector::Protocol::Page::Cookie> buildObjectForCookie(const Cookie& cookie)
{
    Ref protocolCookie = Inspector::Protocol::Page::Cookie::create()
        .setName(cookie.name)
        .setValue(cookie.value)
        .setDomain(cookie.domain)
        .setPath(cookie.path)
        .setExpires(cookie.expires.value_or(0))
        .setSession(cookie.session)
        .setHttpOnly(cookie.httpOnly)
        .setSecure(cookie.secure)
        .setSameSite(cookieSameSitePolicyJSON(cookie.sameSite))
        .release();
    if (!cookie.partitionKey.isEmpty())
        protocolCookie->setPartitionKey(cookie.partitionKey);

    return protocolCookie;
}

static Ref<JSON::ArrayOf<Inspector::Protocol::Page::Cookie>> buildArrayForCookies(ListHashSet<Cookie>& cookiesList)
{
    auto cookies = JSON::ArrayOf<Inspector::Protocol::Page::Cookie>::create();

    for (const auto& cookie : cookiesList)
        cookies->addItem(buildObjectForCookie(cookie));

    return cookies;
}

static Vector<URL> allResourcesURLsForFrame(LocalFrame* frame)
{
    Vector<URL> result;

    result.append(frame->loader().documentLoader()->url());

    for (auto* cachedResource : ResourceUtilities::cachedResourcesForFrame(frame))
        result.append(cachedResource->url());

    return result;
}

Inspector::Protocol::ErrorStringOr<Ref<JSON::ArrayOf<Inspector::Protocol::Page::Cookie>>> InspectorPageAgent::getCookies()
{
    ListHashSet<Cookie> allRawCookies;

    for (Frame* frame = &m_inspectedPage->mainFrame(); frame; frame = frame->tree().traverseNext()) {
        auto* localFrame = dynamicDowncast<LocalFrame>(frame);
        if (!localFrame)
            continue;
        auto* document = localFrame->document();
        if (!document || !document->page())
            continue;

        for (auto& url : allResourcesURLsForFrame(localFrame)) {
            Vector<Cookie> rawCookiesForURLInDocument;
            if (!document->page()->cookieJar().getRawCookies(*document, url, rawCookiesForURLInDocument))
                continue;

            for (auto& rawCookieForURLInDocument : rawCookiesForURLInDocument)
                allRawCookies.add(rawCookieForURLInDocument);
        }
    }

    return buildArrayForCookies(allRawCookies);
}

static std::optional<Cookie> parseCookieObject(Inspector::Protocol::ErrorString& errorString, Ref<JSON::Object>&& cookieObject)
{
    Cookie cookie;

    cookie.name = cookieObject->getString("name"_s);
    if (!cookie.name) {
        errorString = "Invalid value for key name in given cookie"_s;
        return std::nullopt;
    }

    cookie.value = cookieObject->getString("value"_s);
    if (!cookie.value) {
        errorString = "Invalid value for key value in given cookie"_s;
        return std::nullopt;
    }

    cookie.domain = cookieObject->getString("domain"_s);
    if (!cookie.domain) {
        errorString = "Invalid value for key domain in given cookie"_s;
        return std::nullopt;
    }

    cookie.path = cookieObject->getString("path"_s);
    if (!cookie.path) {
        errorString = "Invalid value for key path in given cookie"_s;
        return std::nullopt;
    }

    auto httpOnly = cookieObject->getBoolean("httpOnly"_s);
    if (!httpOnly) {
        errorString = "Invalid value for key httpOnly in given cookie"_s;
        return std::nullopt;
    }

    cookie.httpOnly = *httpOnly;

    auto secure = cookieObject->getBoolean("secure"_s);
    if (!secure) {
        errorString = "Invalid value for key secure in given cookie"_s;
        return std::nullopt;
    }

    cookie.secure = *secure;

    auto session = cookieObject->getBoolean("session"_s);
    cookie.expires = cookieObject->getDouble("expires"_s);
    if (!session && !cookie.expires) {
        errorString = "Invalid value for key expires in given cookie"_s;
        return std::nullopt;
    }

    cookie.session = *session;

    auto sameSiteString = cookieObject->getString("sameSite"_s);
    if (!sameSiteString) {
        errorString = "Invalid value for key sameSite in given cookie"_s;
        return std::nullopt;
    }

    auto sameSite = Inspector::Protocol::Helpers::parseEnumValueFromString<Inspector::Protocol::Page::CookieSameSitePolicy>(sameSiteString);
    if (!sameSite) {
        errorString = "Invalid value for key sameSite in given cookie"_s;
        return std::nullopt;
    }

    switch (sameSite.value()) {
    case Inspector::Protocol::Page::CookieSameSitePolicy::None:
        cookie.sameSite = Cookie::SameSitePolicy::None;
        break;

    case Inspector::Protocol::Page::CookieSameSitePolicy::Lax:
        cookie.sameSite = Cookie::SameSitePolicy::Lax;
        break;

    case Inspector::Protocol::Page::CookieSameSitePolicy::Strict:
        cookie.sameSite = Cookie::SameSitePolicy::Strict;
        break;
    }

    cookie.partitionKey = cookieObject->getString("partitionKey"_s);

    return cookie;
}

Inspector::Protocol::ErrorStringOr<void> InspectorPageAgent::setCookie(Ref<JSON::Object>&& cookieObject, std::optional<bool>&& shouldPartition)
{
    Inspector::Protocol::ErrorString errorString;

    auto cookie = parseCookieObject(errorString, WTFMove(cookieObject));
    if (!cookie)
        return makeUnexpected(errorString);

    auto shouldPartitionCookie = shouldPartition.value_or(false) ? ShouldPartitionCookie::Yes : ShouldPartitionCookie::No;
    for (Frame* frame = &m_inspectedPage->mainFrame(); frame; frame = frame->tree().traverseNext()) {
        auto* localFrame = dynamicDowncast<LocalFrame>(frame);
        if (!localFrame)
            continue;
        auto* document = localFrame->document();
        if (!document)
            continue;
        auto* page = document->page();
        if (!page)
            continue;
        page->cookieJar().setRawCookie(*document, cookie.value(), shouldPartitionCookie);
    }

    return { };
}

Inspector::Protocol::ErrorStringOr<void> InspectorPageAgent::deleteCookie(const String& cookieName, const String& url)
{
    URL parsedURL({ }, url);
    for (Frame* frame = &m_inspectedPage->mainFrame(); frame; frame = frame->tree().traverseNext()) {
        auto* localFrame = dynamicDowncast<LocalFrame>(frame);
        if (!localFrame)
            continue;
        auto* document = localFrame->document();
        if (!document)
            continue;
        auto* page = document->page();
        if (!page)
            continue;
        page->cookieJar().deleteCookie(*document, parsedURL, cookieName, [] { });
    }

    return { };
}

Inspector::Protocol::ErrorStringOr<Ref<Inspector::Protocol::Page::FrameResourceTree>> InspectorPageAgent::getResourceTree()
{
    RefPtr localMainFrame = m_inspectedPage->localMainFrame();
    return buildObjectForFrameTree(localMainFrame.get());
}

Inspector::Protocol::ErrorStringOr<std::tuple<String, bool /* base64Encoded */>> InspectorPageAgent::getResourceContent(const Inspector::Protocol::Network::FrameId& frameId, const String& url)
{
    Inspector::Protocol::ErrorString errorString;

    auto* frame = assertFrame(errorString, frameId);
    if (!frame)
        return makeUnexpected(errorString);

    String content;
    bool base64Encoded;

    ResourceUtilities::resourceContent(errorString, frame, URL({ }, url), &content, &base64Encoded);

    return { { content, base64Encoded } };
}

Inspector::Protocol::ErrorStringOr<void> InspectorPageAgent::setBootstrapScript(const String& source)
{
    m_bootstrapScript = source;

    return { };
}

Inspector::Protocol::ErrorStringOr<Ref<JSON::ArrayOf<Inspector::Protocol::GenericTypes::SearchMatch>>> InspectorPageAgent::searchInResource(const Inspector::Protocol::Network::FrameId& frameId, const String& url, const String& query, std::optional<bool>&& caseSensitive, std::optional<bool>&& isRegex, const Inspector::Protocol::Network::RequestId& requestId)
{
    Inspector::Protocol::ErrorString errorString;

    if (!!requestId) {
        if (auto* networkAgent = Ref { m_instrumentingAgents.get() }->enabledNetworkAgent()) {
            RefPtr<JSON::ArrayOf<Inspector::Protocol::GenericTypes::SearchMatch>> result;
            networkAgent->searchInRequest(errorString, requestId, query, caseSensitive && *caseSensitive, isRegex && *isRegex, result);
            if (!result)
                return makeUnexpected(errorString);
            return result.releaseNonNull();
        }
    }

    auto* frame = assertFrame(errorString, frameId);
    if (!frame)
        return makeUnexpected(errorString);

    DocumentLoader* loader = ResourceUtilities::assertDocumentLoader(errorString, frame);
    if (!loader)
        return makeUnexpected(errorString);

    URL kurl({ }, url);

    String content;
    bool success = false;
    if (equalIgnoringFragmentIdentifier(kurl, loader->url()))
        success = ResourceUtilities::mainResourceContent(frame, false, &content);

    if (!success) {
        if (auto* resource = ResourceUtilities::cachedResource(frame, kurl)) {
            if (auto textContent = ResourceUtilities::textContentForCachedResource(*resource)) {
                content = *textContent;
                success = true;
            }
        }
    }

    if (!success)
        return JSON::ArrayOf<Inspector::Protocol::GenericTypes::SearchMatch>::create();

    return ContentSearchUtilities::searchInTextByLines(content, query, caseSensitive && *caseSensitive, isRegex && *isRegex);
}

static Ref<Inspector::Protocol::Page::SearchResult> buildObjectForSearchResult(const Inspector::Protocol::Network::FrameId& frameId, const String& url, int matchesCount)
{
    return Inspector::Protocol::Page::SearchResult::create()
        .setUrl(url)
        .setFrameId(frameId)
        .setMatchesCount(matchesCount)
        .release();
}

Inspector::Protocol::ErrorStringOr<Ref<JSON::ArrayOf<Inspector::Protocol::Page::SearchResult>>> InspectorPageAgent::searchInResources(const String& text, std::optional<bool>&& caseSensitive, std::optional<bool>&& isRegex)
{
    auto result = JSON::ArrayOf<Inspector::Protocol::Page::SearchResult>::create();

    auto searchType = (isRegex && *isRegex) ? ContentSearchUtilities::SearchType::Regex : ContentSearchUtilities::SearchType::ContainsString;
    auto searchCaseSensitive = (caseSensitive && *caseSensitive) ? ContentSearchUtilities::SearchCaseSensitive::Yes : ContentSearchUtilities::SearchCaseSensitive::No;
    auto regex = ContentSearchUtilities::createRegularExpressionForString(text, searchType, searchCaseSensitive);

    // FIXME: rework this frame tree traversal as it won't work with Site Isolation enabled.
    for (Frame* frame = &m_inspectedPage->mainFrame(); frame; frame = frame->tree().traverseNext()) {
        auto* localFrame = dynamicDowncast<LocalFrame>(frame);
        if (!localFrame)
            continue;
        for (auto* cachedResource : ResourceUtilities::cachedResourcesForFrame(localFrame)) {
            if (auto textContent = ResourceUtilities::textContentForCachedResource(*cachedResource)) {
                int matchesCount = ContentSearchUtilities::countRegularExpressionMatches(regex, *textContent);
                if (matchesCount)
                    result->addItem(buildObjectForSearchResult(frameId(localFrame), cachedResource->url().string(), matchesCount));
            }
        }
    }

    if (auto* networkAgent = Ref { m_instrumentingAgents.get() }->enabledNetworkAgent())
        networkAgent->searchOtherRequests(regex, result);

    return result;
}

#if !PLATFORM(IOS_FAMILY)
Inspector::Protocol::ErrorStringOr<void> InspectorPageAgent::setShowRulers(bool showRulers)
{
    protectedOverlay()->setShowRulers(showRulers);

    return { };
}
#endif

Inspector::Protocol::ErrorStringOr<void> InspectorPageAgent::setShowPaintRects(bool show)
{
    m_showPaintRects = show;
    m_client->setShowPaintRects(show);

    if (m_client->overridesShowPaintRects())
        return { };

    protectedOverlay()->setShowPaintRects(show);

    return { };
}

void InspectorPageAgent::domContentEventFired()
{
    m_isFirstLayoutAfterOnLoad = true;
    m_frontendDispatcher->domContentEventFired(timestamp());
}

void InspectorPageAgent::loadEventFired()
{
    m_frontendDispatcher->loadEventFired(timestamp());
}

void InspectorPageAgent::frameNavigated(LocalFrame& frame)
{
    m_frontendDispatcher->frameNavigated(buildObjectForFrame(&frame));
}

void InspectorPageAgent::frameDetached(LocalFrame& frame)
{
    auto identifier = m_frameToIdentifier.take(frame);
    if (identifier.isNull())
        return;
    m_frontendDispatcher->frameDetached(identifier);
    m_identifierToFrame.remove(identifier);
}

Frame* InspectorPageAgent::frameForId(const Inspector::Protocol::Network::FrameId& frameId)
{
    return frameId.isEmpty() ? nullptr : m_identifierToFrame.get(frameId);
}

String InspectorPageAgent::frameId(Frame* frame)
{
    if (!frame)
        return emptyString();
    return m_frameToIdentifier.ensure(*frame, [this, frame] {
        auto identifier = IdentifiersFactory::createIdentifier();
        m_identifierToFrame.set(identifier, frame);
        return identifier;
    }).iterator->value;
}

String InspectorPageAgent::loaderId(DocumentLoader* loader)
{
    if (!loader)
        return emptyString();
    return m_loaderToIdentifier.ensure(loader, [] {
        return IdentifiersFactory::createIdentifier();
    }).iterator->value;
}

LocalFrame* InspectorPageAgent::assertFrame(Inspector::Protocol::ErrorString& errorString, const Inspector::Protocol::Network::FrameId& frameId)
{
    auto* frame = dynamicDowncast<LocalFrame>(frameForId(frameId));
    if (!frame)
        errorString = "Missing frame for given frameId"_s;
    return frame;
}

void InspectorPageAgent::loaderDetachedFromFrame(DocumentLoader& loader)
{
    m_loaderToIdentifier.remove(&loader);
}

void InspectorPageAgent::accessibilitySettingsDidChange()
{
    defaultUserPreferencesDidChange();
}

void InspectorPageAgent::defaultUserPreferencesDidChange()
{
    auto defaultUserPreferences = JSON::ArrayOf<Inspector::Protocol::Page::UserPreference>::create();

    bool prefersReducedMotion = Theme::singleton().userPrefersReducedMotion();

    auto prefersReducedMotionUserPreference = Inspector::Protocol::Page::UserPreference::create()
        .setName(Inspector::Protocol::Page::UserPreferenceName::PrefersReducedMotion)
        .setValue(prefersReducedMotion ? Inspector::Protocol::Page::UserPreferenceValue::Reduce : Inspector::Protocol::Page::UserPreferenceValue::NoPreference)
        .release();

    defaultUserPreferences->addItem(WTFMove(prefersReducedMotionUserPreference));

    bool prefersContrast = Theme::singleton().userPrefersContrast();

    auto prefersContrastUserPreference = Inspector::Protocol::Page::UserPreference::create()
        .setName(Inspector::Protocol::Page::UserPreferenceName::PrefersContrast)
        .setValue(prefersContrast ? Inspector::Protocol::Page::UserPreferenceValue::More : Inspector::Protocol::Page::UserPreferenceValue::NoPreference)
        .release();

    defaultUserPreferences->addItem(WTFMove(prefersContrastUserPreference));

#if ENABLE(DARK_MODE_CSS)
    auto prefersColorSchemeUserPreference = Inspector::Protocol::Page::UserPreference::create()
        .setName(Inspector::Protocol::Page::UserPreferenceName::PrefersColorScheme)
        .setValue(m_inspectedPage->defaultUseDarkAppearance() ? Inspector::Protocol::Page::UserPreferenceValue::Dark : Inspector::Protocol::Page::UserPreferenceValue::Light)
        .release();

    defaultUserPreferences->addItem(WTFMove(prefersColorSchemeUserPreference));
#endif

    m_frontendDispatcher->defaultUserPreferencesDidChange(WTFMove(defaultUserPreferences));
}

#if ENABLE(DARK_MODE_CSS)
void InspectorPageAgent::defaultAppearanceDidChange()
{
    defaultUserPreferencesDidChange();
}
#endif

void InspectorPageAgent::didClearWindowObjectInWorld(LocalFrame& frame, DOMWrapperWorld& world)
{
    if (&world != &mainThreadNormalWorldSingleton())
        return;

    if (m_bootstrapScript.isEmpty())
        return;

    frame.script().evaluateIgnoringException(ScriptSourceCode(m_bootstrapScript, JSC::SourceTaintedOrigin::Untainted, URL { "web-inspector://bootstrap.js"_str }));
}

void InspectorPageAgent::didPaint(RenderObject& renderer, const LayoutRect& rect)
{
    if (!m_showPaintRects)
        return;

    LayoutRect absoluteRect = LayoutRect(renderer.localToAbsoluteQuad(FloatRect(rect)).boundingBox());
    auto* view = renderer.document().view();

    LayoutRect rootRect = absoluteRect;
    Ref localFrame = view->frame();
    if (!localFrame->isMainFrame()) {
        IntRect rootViewRect = view->contentsToRootView(snappedIntRect(absoluteRect));
        rootRect = localFrame->mainFrame().virtualView()->rootViewToContents(rootViewRect);
    }

    if (m_client->overridesShowPaintRects()) {
        m_client->showPaintRect(rootRect);
        return;
    }

    protectedOverlay()->showPaintRect(rootRect);
}

void InspectorPageAgent::didLayout()
{
    bool isFirstLayout = m_isFirstLayoutAfterOnLoad;
    if (isFirstLayout)
        m_isFirstLayoutAfterOnLoad = false;

    protectedOverlay()->update();
}

void InspectorPageAgent::didScroll()
{
    m_overlay->update();
}

void InspectorPageAgent::didRecalculateStyle()
{
    protectedOverlay()->update();
}

Ref<Inspector::Protocol::Page::Frame> InspectorPageAgent::buildObjectForFrame(LocalFrame* frame)
{
    ASSERT_ARG(frame, frame);

    auto frameObject = Inspector::Protocol::Page::Frame::create()
        .setId(frameId(frame))
        .setLoaderId(loaderId(frame->loader().documentLoader()))
        .setUrl(frame->document()->url().string())
        .setMimeType(frame->loader().documentLoader()->responseMIMEType())
        .setSecurityOrigin(frame->document()->securityOrigin().toRawString())
        .release();
    if (frame->tree().parent())
        frameObject->setParentId(frameId(dynamicDowncast<LocalFrame>(frame->tree().parent())));
    if (frame->ownerElement()) {
        String name = frame->ownerElement()->getNameAttribute();
        if (name.isEmpty())
            name = frame->ownerElement()->attributeWithoutSynchronization(HTMLNames::idAttr);
        frameObject->setName(name);
    }

    return frameObject;
}

Ref<Inspector::Protocol::Page::FrameResourceTree> InspectorPageAgent::buildObjectForFrameTree(LocalFrame* frame)
{
    ASSERT_ARG(frame, frame);

    auto frameObject = buildObjectForFrame(frame);
    auto subresources = JSON::ArrayOf<Inspector::Protocol::Page::FrameResource>::create();
    auto result = Inspector::Protocol::Page::FrameResourceTree::create()
        .setFrame(WTFMove(frameObject))
        .setResources(subresources.copyRef())
        .release();

    for (auto* cachedResource : ResourceUtilities::cachedResourcesForFrame(frame)) {
        auto resourceObject = Inspector::Protocol::Page::FrameResource::create()
            .setUrl(cachedResource->url().string())
            .setType(ResourceUtilities::cachedResourceTypeToProtocol(*cachedResource))
            .setMimeType(cachedResource->response().mimeType())
            .release();
        if (cachedResource->wasCanceled())
            resourceObject->setCanceled(true);
        else if (cachedResource->status() == CachedResource::LoadError || cachedResource->status() == CachedResource::DecodeError)
            resourceObject->setFailed(true);
        String sourceMappingURL = ResourceUtilities::sourceMapURLForResource(cachedResource);
        if (!sourceMappingURL.isEmpty())
            resourceObject->setSourceMapURL(sourceMappingURL);
        String targetId = cachedResource->resourceRequest().initiatorIdentifier();
        if (!targetId.isEmpty())
            resourceObject->setTargetId(targetId);
        subresources->addItem(WTFMove(resourceObject));
    }

    RefPtr<JSON::ArrayOf<Inspector::Protocol::Page::FrameResourceTree>> childrenArray;
    for (Frame* child = frame->tree().firstChild(); child; child = child->tree().nextSibling()) {
        if (!childrenArray) {
            childrenArray = JSON::ArrayOf<Inspector::Protocol::Page::FrameResourceTree>::create();
            result->setChildFrames(*childrenArray);
        }
        auto* localChild = dynamicDowncast<LocalFrame>(child);
        if (!localChild)
            continue;
        childrenArray->addItem(buildObjectForFrameTree(localChild));
    }
    return result;
}

Inspector::Protocol::ErrorStringOr<void> InspectorPageAgent::setEmulatedMedia(const String& media)
{
    if (media == m_emulatedMedia)
        return { };

    m_emulatedMedia = AtomString(media);

    // FIXME: Schedule a rendering update instead of synchronously updating the layout.
    m_inspectedPage->updateStyleAfterChangeInEnvironment();

    RefPtr document = m_inspectedPage->localTopDocument();
    if (!document)
        return { };

    document->updateLayout();
    document->evaluateMediaQueriesAndReportChanges();

    return { };
}

void InspectorPageAgent::applyUserAgentOverride(String& userAgent)
{
    if (!m_userAgentOverride.isEmpty())
        userAgent = m_userAgentOverride;
}

void InspectorPageAgent::applyEmulatedMedia(AtomString& media)
{
    if (!m_emulatedMedia.isEmpty())
        media = m_emulatedMedia;
}

Inspector::Protocol::ErrorStringOr<String> InspectorPageAgent::snapshotNode(Inspector::Protocol::DOM::NodeId nodeId)
{
    Inspector::Protocol::ErrorString errorString;

    InspectorDOMAgent* domAgent = Ref { m_instrumentingAgents.get() }->persistentDOMAgent();
    ASSERT(domAgent);
    Node* node = domAgent->assertNode(errorString, nodeId);
    if (!node)
        return makeUnexpected(errorString);
    
    RefPtr localMainFrame = m_inspectedPage->localMainFrame();
    if (!localMainFrame)
        return makeUnexpected("Main frame isn't local"_s);

    auto snapshot = WebCore::snapshotNode(*localMainFrame, *node, { { }, PixelFormat::BGRA8, DestinationColorSpace::SRGB() });
    if (!snapshot)
        return makeUnexpected("Could not capture snapshot"_s);

    return snapshot->toDataURL("image/png"_s, std::nullopt, PreserveResolution::Yes);
}

Inspector::Protocol::ErrorStringOr<String> InspectorPageAgent::snapshotRect(int x, int y, int width, int height, Inspector::Protocol::Page::CoordinateSystem coordinateSystem)
{
    SnapshotOptions options { { }, PixelFormat::BGRA8, DestinationColorSpace::SRGB() };
    if (coordinateSystem == Inspector::Protocol::Page::CoordinateSystem::Viewport)
        options.flags.add(SnapshotFlags::InViewCoordinates);

    IntRect rectangle(x, y, width, height);
    RefPtr localMainFrame = m_inspectedPage->localMainFrame();
    if (!localMainFrame)
        return makeUnexpected("Main frame isn't local"_s);
    auto snapshot = snapshotFrameRect(*localMainFrame, rectangle, WTFMove(options));

    if (!snapshot)
        return makeUnexpected("Could not capture snapshot"_s);

    return snapshot->toDataURL("image/png"_s, std::nullopt, PreserveResolution::Yes);
}

#if ENABLE(WEB_ARCHIVE) && USE(CF)
Inspector::Protocol::ErrorStringOr<String> InspectorPageAgent::archive()
{
    RefPtr localMainFrame = m_inspectedPage->localMainFrame();
    if (!localMainFrame)
        return makeUnexpected("Main frame isn't local"_s);

    auto archive = LegacyWebArchive::create(*localMainFrame);
    if (!archive)
        return makeUnexpected("Could not create web archive for main frame"_s);

    RetainPtr<CFDataRef> buffer = archive->rawDataRepresentation();
    return base64EncodeToString(span(buffer.get()));
}
#endif

#if !PLATFORM(COCOA)
Inspector::Protocol::ErrorStringOr<void> InspectorPageAgent::setScreenSizeOverride(std::optional<int>&& width, std::optional<int>&& height)
{
    if (width.has_value() != height.has_value())
        return makeUnexpected("Screen width and height override should be both specified or omitted"_s);

    if (width && *width <= 0)
        return makeUnexpected("Screen width override should be a positive integer"_s);

    if (height && *height <= 0)
        return makeUnexpected("Screen height override should be a positive integer"_s);

    RefPtr localMainFrame = m_inspectedPage->localMainFrame();
    if (!localMainFrame)
        return makeUnexpected("Main frame isn't local"_s);
    localMainFrame->setOverrideScreenSize(FloatSize(width.value_or(0), height.value_or(0)));
    return { };
}
#endif

} // namespace WebCore
