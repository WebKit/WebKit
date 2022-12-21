/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
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
#include "CachedResourceLoader.h"
#include "Cookie.h"
#include "CookieJar.h"
#include "DOMWrapperWorld.h"
#include "DocumentInlines.h"
#include "DocumentLoader.h"
#include "ElementInlines.h"
#include "ForcedAccessibilityValue.h"
#include "Frame.h"
#include "FrameLoadRequest.h"
#include "FrameLoader.h"
#include "FrameSnapshotting.h"
#include "FrameView.h"
#include "HTMLFrameOwnerElement.h"
#include "HTMLNames.h"
#include "ImageBuffer.h"
#include "InspectorClient.h"
#include "InspectorDOMAgent.h"
#include "InspectorNetworkAgent.h"
#include "InspectorOverlay.h"
#include "InstrumentingAgents.h"
#include "MIMETypeRegistry.h"
#include "MemoryCache.h"
#include "Page.h"
#include "RenderObject.h"
#include "RenderTheme.h"
#include "ScriptController.h"
#include "ScriptSourceCode.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "StyleScope.h"
#include "Theme.h"
#include <pal/text/TextEncoding.h>
#include "UserGestureIndicator.h"
#include <JavaScriptCore/ContentSearchUtilities.h>
#include <JavaScriptCore/IdentifiersFactory.h>
#include <JavaScriptCore/RegularExpression.h>
#include <wtf/ListHashSet.h>
#include <wtf/Stopwatch.h>
#include <wtf/text/Base64.h>
#include <wtf/text/StringBuilder.h>

#if ENABLE(APPLICATION_MANIFEST)
#include "CachedApplicationManifest.h"
#endif

#if ENABLE(WEB_ARCHIVE) && USE(CF)
#include "LegacyWebArchive.h"
#endif


namespace WebCore {

using namespace Inspector;

static bool decodeBuffer(const uint8_t* buffer, unsigned size, const String& textEncodingName, String* result)
{
    if (buffer) {
        PAL::TextEncoding encoding(textEncodingName);
        if (!encoding.isValid())
            encoding = PAL::WindowsLatin1Encoding();
        *result = encoding.decode(buffer, size);
        return true;
    }
    return false;
}

bool InspectorPageAgent::mainResourceContent(Frame* frame, bool withBase64Encode, String* result)
{
    RefPtr<FragmentedSharedBuffer> buffer = frame->loader().documentLoader()->mainResourceData();
    if (!buffer)
        return false;
    return InspectorPageAgent::dataContent(buffer->makeContiguous()->data(), buffer->size(), frame->document()->encoding(), withBase64Encode, result);
}

bool InspectorPageAgent::sharedBufferContent(RefPtr<FragmentedSharedBuffer>&& buffer, const String& textEncodingName, bool withBase64Encode, String* result)
{
    return dataContent(buffer ? buffer->makeContiguous()->data() : nullptr, buffer ? buffer->size() : 0, textEncodingName, withBase64Encode, result);
}

bool InspectorPageAgent::dataContent(const uint8_t* data, unsigned size, const String& textEncodingName, bool withBase64Encode, String* result)
{
    if (withBase64Encode) {
        *result = base64EncodeToString(data, size);
        return true;
    }

    return decodeBuffer(data, size, textEncodingName, result);
}

Vector<CachedResource*> InspectorPageAgent::cachedResourcesForFrame(Frame* frame)
{
    Vector<CachedResource*> result;

    for (auto& cachedResourceHandle : frame->document()->cachedResourceLoader().allCachedResources().values()) {
        auto* cachedResource = cachedResourceHandle.get();
        if (cachedResource->resourceRequest().hiddenFromInspector())
            continue;

        switch (cachedResource->type()) {
        case CachedResource::Type::ImageResource:
            // Skip images that were not auto loaded (images disabled in the user agent).
        case CachedResource::Type::SVGFontResource:
        case CachedResource::Type::FontResource:
            // Skip fonts that were referenced in CSS but never used/downloaded.
            if (cachedResource->stillNeedsLoad())
                continue;
            break;
        default:
            // All other CachedResource types download immediately.
            break;
        }

        result.append(cachedResource);
    }

    return result;
}

void InspectorPageAgent::resourceContent(Protocol::ErrorString& errorString, Frame* frame, const URL& url, String* result, bool* base64Encoded)
{
    DocumentLoader* loader = assertDocumentLoader(errorString, frame);
    if (!loader)
        return;

    RefPtr<FragmentedSharedBuffer> buffer;
    bool success = false;
    if (equalIgnoringFragmentIdentifier(url, loader->url())) {
        *base64Encoded = false;
        success = mainResourceContent(frame, *base64Encoded, result);
    }

    if (!success) {
        if (auto* resource = cachedResource(frame, url))
            success = InspectorNetworkAgent::cachedResourceContent(*resource, result, base64Encoded);
    }

    if (!success)
        errorString = "Missing resource for given url"_s;
}

String InspectorPageAgent::sourceMapURLForResource(CachedResource* cachedResource)
{
    if (!cachedResource)
        return String();

    // Scripts are handled in a separate path.
    if (cachedResource->type() != CachedResource::Type::CSSStyleSheet)
        return String();

    String sourceMapHeader = cachedResource->response().httpHeaderField(HTTPHeaderName::SourceMap);
    if (!sourceMapHeader.isEmpty())
        return sourceMapHeader;

    sourceMapHeader = cachedResource->response().httpHeaderField(HTTPHeaderName::XSourceMap);
    if (!sourceMapHeader.isEmpty())
        return sourceMapHeader;

    String content;
    bool base64Encoded;
    if (InspectorNetworkAgent::cachedResourceContent(*cachedResource, &content, &base64Encoded) && !base64Encoded)
        return ContentSearchUtilities::findStylesheetSourceMapURL(content);

    return String();
}

CachedResource* InspectorPageAgent::cachedResource(const Frame* frame, const URL& url)
{
    if (url.isNull())
        return nullptr;

    CachedResource* cachedResource = frame->document()->cachedResourceLoader().cachedResource(MemoryCache::removeFragmentIdentifierIfNeeded(url));
    if (!cachedResource) {
        ResourceRequest request(url);
        request.setDomainForCachePartition(frame->document()->domainForCachePartition());
        cachedResource = MemoryCache::singleton().resourceForRequest(request, frame->page()->sessionID());
    }

    return cachedResource;
}

Protocol::Page::ResourceType InspectorPageAgent::resourceTypeJSON(InspectorPageAgent::ResourceType resourceType)
{
    switch (resourceType) {
    case DocumentResource:
        return Protocol::Page::ResourceType::Document;
    case ImageResource:
        return Protocol::Page::ResourceType::Image;
    case FontResource:
        return Protocol::Page::ResourceType::Font;
    case StyleSheetResource:
        return Protocol::Page::ResourceType::StyleSheet;
    case ScriptResource:
        return Protocol::Page::ResourceType::Script;
    case XHRResource:
        return Protocol::Page::ResourceType::XHR;
    case FetchResource:
        return Protocol::Page::ResourceType::Fetch;
    case PingResource:
        return Protocol::Page::ResourceType::Ping;
    case BeaconResource:
        return Protocol::Page::ResourceType::Beacon;
    case WebSocketResource:
        return Protocol::Page::ResourceType::WebSocket;
    case EventSourceResource:
        return Protocol::Page::ResourceType::EventSource;
    case OtherResource:
        return Protocol::Page::ResourceType::Other;
#if ENABLE(APPLICATION_MANIFEST)
    case ApplicationManifestResource:
        break;
#endif
    }
    return Protocol::Page::ResourceType::Other;
}

InspectorPageAgent::ResourceType InspectorPageAgent::inspectorResourceType(CachedResource::Type type)
{
    switch (type) {
    case CachedResource::Type::ImageResource:
        return InspectorPageAgent::ImageResource;
    case CachedResource::Type::SVGFontResource:
    case CachedResource::Type::FontResource:
        return InspectorPageAgent::FontResource;
#if ENABLE(XSLT)
    case CachedResource::Type::XSLStyleSheet:
#endif
    case CachedResource::Type::CSSStyleSheet:
        return InspectorPageAgent::StyleSheetResource;
    case CachedResource::Type::Script:
        return InspectorPageAgent::ScriptResource;
    case CachedResource::Type::MainResource:
        return InspectorPageAgent::DocumentResource;
    case CachedResource::Type::Beacon:
        return InspectorPageAgent::BeaconResource;
#if ENABLE(APPLICATION_MANIFEST)
    case CachedResource::Type::ApplicationManifest:
        return InspectorPageAgent::ApplicationManifestResource;
#endif
    case CachedResource::Type::Ping:
        return InspectorPageAgent::PingResource;
    case CachedResource::Type::MediaResource:
    case CachedResource::Type::Icon:
    case CachedResource::Type::RawResource:
    default:
        return InspectorPageAgent::OtherResource;
    }
}

InspectorPageAgent::ResourceType InspectorPageAgent::inspectorResourceType(const CachedResource& cachedResource)
{
    if (cachedResource.type() == CachedResource::Type::RawResource) {
        switch (cachedResource.resourceRequest().requester()) {
        case ResourceRequestRequester::Fetch:
            return InspectorPageAgent::FetchResource;
        case ResourceRequestRequester::Main:
            return InspectorPageAgent::DocumentResource;
        case ResourceRequestRequester::EventSource:
            return InspectorPageAgent::EventSourceResource;
        default:
            return InspectorPageAgent::XHRResource;
        }
    }

    return inspectorResourceType(cachedResource.type());
}

Protocol::Page::ResourceType InspectorPageAgent::cachedResourceTypeJSON(const CachedResource& cachedResource)
{
    return resourceTypeJSON(inspectorResourceType(cachedResource));
}

Frame* InspectorPageAgent::findFrameWithSecurityOrigin(Page& page, const String& originRawString)
{
    for (AbstractFrame* frame = &page.mainFrame(); frame; frame = frame->tree().traverseNext()) {
        auto* localFrame = dynamicDowncast<LocalFrame>(frame);
        if (!localFrame)
            continue;
        if (localFrame->document()->securityOrigin().toRawString() == originRawString)
            return localFrame;
    }
    return nullptr;
}

DocumentLoader* InspectorPageAgent::assertDocumentLoader(Protocol::ErrorString& errorString, Frame* frame)
{
    FrameLoader& frameLoader = frame->loader();
    DocumentLoader* documentLoader = frameLoader.documentLoader();
    if (!documentLoader)
        errorString = "Missing document loader for given frame"_s;
    return documentLoader;
}

InspectorPageAgent::InspectorPageAgent(PageAgentContext& context, InspectorClient* client, InspectorOverlay* overlay)
    : InspectorAgentBase("Page"_s, context)
    , m_frontendDispatcher(makeUnique<Inspector::PageFrontendDispatcher>(context.frontendRouter))
    , m_backendDispatcher(Inspector::PageBackendDispatcher::create(context.backendDispatcher, this))
    , m_inspectedPage(context.inspectedPage)
    , m_client(client)
    , m_overlay(overlay)
{
}

InspectorPageAgent::~InspectorPageAgent() = default;

void InspectorPageAgent::didCreateFrontendAndBackend(Inspector::FrontendRouter*, Inspector::BackendDispatcher*)
{
}

void InspectorPageAgent::willDestroyFrontendAndBackend(Inspector::DisconnectReason)
{
    disable();
}

Protocol::ErrorStringOr<void> InspectorPageAgent::enable()
{
    if (m_instrumentingAgents.enabledPageAgent() == this)
        return makeUnexpected("Page domain already enabled"_s);

    m_instrumentingAgents.setEnabledPageAgent(this);

    auto& stopwatch = m_environment.executionStopwatch();
    stopwatch.reset();
    stopwatch.start();

    defaultUserPreferencesDidChange();

    return { };
}

Protocol::ErrorStringOr<void> InspectorPageAgent::disable()
{
    m_instrumentingAgents.setEnabledPageAgent(nullptr);

    setShowPaintRects(false);
#if !PLATFORM(IOS_FAMILY)
    setShowRulers(false);
#endif
    overrideUserAgent(nullString());
    setEmulatedMedia(emptyString());
    overridePrefersColorScheme(std::nullopt);

    auto& inspectedPageSettings = m_inspectedPage.settings();
    inspectedPageSettings.setAuthorAndUserStylesEnabledInspectorOverride(std::nullopt);
    inspectedPageSettings.setICECandidateFilteringEnabledInspectorOverride(std::nullopt);
    inspectedPageSettings.setImagesEnabledInspectorOverride(std::nullopt);
    inspectedPageSettings.setMediaCaptureRequiresSecureConnectionInspectorOverride(std::nullopt);
    inspectedPageSettings.setMockCaptureDevicesEnabledInspectorOverride(std::nullopt);
    inspectedPageSettings.setNeedsSiteSpecificQuirksInspectorOverride(std::nullopt);
    inspectedPageSettings.setScriptEnabledInspectorOverride(std::nullopt);
    inspectedPageSettings.setShowDebugBordersInspectorOverride(std::nullopt);
    inspectedPageSettings.setShowRepaintCounterInspectorOverride(std::nullopt);
    inspectedPageSettings.setWebRTCEncryptionEnabledInspectorOverride(std::nullopt);
    inspectedPageSettings.setWebSecurityEnabledInspectorOverride(std::nullopt);
    inspectedPageSettings.setForcedPrefersReducedMotionAccessibilityValue(ForcedAccessibilityValue::System);
    inspectedPageSettings.setForcedPrefersContrastAccessibilityValue(ForcedAccessibilityValue::System);

    m_client->setDeveloperPreferenceOverride(InspectorClient::DeveloperPreference::PrivateClickMeasurementDebugModeEnabled, std::nullopt);
    m_client->setDeveloperPreferenceOverride(InspectorClient::DeveloperPreference::ITPDebugModeEnabled, std::nullopt);
    m_client->setDeveloperPreferenceOverride(InspectorClient::DeveloperPreference::MockCaptureDevicesEnabled, std::nullopt);

    return { };
}

double InspectorPageAgent::timestamp()
{
    return m_environment.executionStopwatch().elapsedTime().seconds();
}

Protocol::ErrorStringOr<void> InspectorPageAgent::reload(std::optional<bool>&& ignoreCache, std::optional<bool>&& revalidateAllResources)
{
    OptionSet<ReloadOption> reloadOptions;
    if (ignoreCache && *ignoreCache)
        reloadOptions.add(ReloadOption::FromOrigin);
    if (!revalidateAllResources || !*revalidateAllResources)
        reloadOptions.add(ReloadOption::ExpiredOnly);
    m_inspectedPage.mainFrame().loader().reload(reloadOptions);

    return { };
}

Protocol::ErrorStringOr<void> InspectorPageAgent::navigate(const String& url)
{
    Frame& frame = m_inspectedPage.mainFrame();

    UserGestureIndicator indicator { ProcessingUserGesture, frame.document() };

    ResourceRequest resourceRequest { frame.document()->completeURL(url) };
    FrameLoadRequest frameLoadRequest { *frame.document(), frame.document()->securityOrigin(), WTFMove(resourceRequest), selfTargetFrameName(), InitiatedByMainFrame::Unknown };
    frameLoadRequest.disableNavigationToInvalidURL();
    frame.loader().changeLocation(WTFMove(frameLoadRequest));

    return { };
}

Protocol::ErrorStringOr<void> InspectorPageAgent::overrideUserAgent(const String& value)
{
    m_userAgentOverride = value;

    return { };
}

Protocol::ErrorStringOr<void> InspectorPageAgent::overrideSetting(Protocol::Page::Setting setting, std::optional<bool>&& value)
{
    auto& inspectedPageSettings = m_inspectedPage.settings();

    switch (setting) {
    case Protocol::Page::Setting::PrivateClickMeasurementDebugModeEnabled:
        m_client->setDeveloperPreferenceOverride(InspectorClient::DeveloperPreference::PrivateClickMeasurementDebugModeEnabled, value);
        return { };

    case Protocol::Page::Setting::AuthorAndUserStylesEnabled:
        inspectedPageSettings.setAuthorAndUserStylesEnabledInspectorOverride(value);
        return { };

    case Protocol::Page::Setting::ICECandidateFilteringEnabled:
        inspectedPageSettings.setICECandidateFilteringEnabledInspectorOverride(value);
        return { };

    case Protocol::Page::Setting::ITPDebugModeEnabled:
        m_client->setDeveloperPreferenceOverride(InspectorClient::DeveloperPreference::ITPDebugModeEnabled, value);
        return { };

    case Protocol::Page::Setting::ImagesEnabled:
        inspectedPageSettings.setImagesEnabledInspectorOverride(value);
        return { };

    case Protocol::Page::Setting::MediaCaptureRequiresSecureConnection:
        inspectedPageSettings.setMediaCaptureRequiresSecureConnectionInspectorOverride(value);
        return { };

    case Protocol::Page::Setting::MockCaptureDevicesEnabled:
        inspectedPageSettings.setMockCaptureDevicesEnabledInspectorOverride(value);
        m_client->setDeveloperPreferenceOverride(InspectorClient::DeveloperPreference::MockCaptureDevicesEnabled, value);
        return { };

    case Protocol::Page::Setting::NeedsSiteSpecificQuirks:
        inspectedPageSettings.setNeedsSiteSpecificQuirksInspectorOverride(value);
        return { };

    case Protocol::Page::Setting::ScriptEnabled:
        inspectedPageSettings.setScriptEnabledInspectorOverride(value);
        return { };

    case Protocol::Page::Setting::ShowDebugBorders:
        inspectedPageSettings.setShowDebugBordersInspectorOverride(value);
        return { };

    case Protocol::Page::Setting::ShowRepaintCounter:
        inspectedPageSettings.setShowRepaintCounterInspectorOverride(value);
        return { };

    case Protocol::Page::Setting::WebRTCEncryptionEnabled:
        inspectedPageSettings.setWebRTCEncryptionEnabledInspectorOverride(value);
        return { };

    case Protocol::Page::Setting::WebSecurityEnabled:
        inspectedPageSettings.setWebSecurityEnabledInspectorOverride(value);
        return { };
    }

    ASSERT_NOT_REACHED();
    return { };
}

Protocol::ErrorStringOr<void> InspectorPageAgent::overrideUserPreference(Protocol::Page::UserPreferenceName preference, std::optional<Protocol::Page::UserPreferenceValue>&& value)
{
    switch (preference) {
    case Protocol::Page::UserPreferenceName::PrefersReducedMotion:
        overridePrefersReducedMotion(WTFMove(value));
        return { };

    case Protocol::Page::UserPreferenceName::PrefersContrast:
        overridePrefersContrast(WTFMove(value));
        return { };

    case Protocol::Page::UserPreferenceName::PrefersColorScheme:
        overridePrefersColorScheme(WTFMove(value));
        return { };
    }

    ASSERT_NOT_REACHED();
    return { };
}

void InspectorPageAgent::overridePrefersReducedMotion(std::optional<Protocol::Page::UserPreferenceValue>&& value)
{
    ForcedAccessibilityValue forcedValue = ForcedAccessibilityValue::System;

    if (value == Protocol::Page::UserPreferenceValue::Reduce)
        forcedValue = ForcedAccessibilityValue::On;
    else if (value == Protocol::Page::UserPreferenceValue::NoPreference)
        forcedValue = ForcedAccessibilityValue::Off;

    m_inspectedPage.settings().setForcedPrefersReducedMotionAccessibilityValue(forcedValue);
    m_inspectedPage.accessibilitySettingsDidChange();
}

void InspectorPageAgent::overridePrefersContrast(std::optional<Protocol::Page::UserPreferenceValue>&& value)
{
    ForcedAccessibilityValue forcedValue = ForcedAccessibilityValue::System;

    if (value == Protocol::Page::UserPreferenceValue::More)
        forcedValue = ForcedAccessibilityValue::On;
    else if (value == Protocol::Page::UserPreferenceValue::NoPreference)
        forcedValue = ForcedAccessibilityValue::Off;

    m_inspectedPage.settings().setForcedPrefersContrastAccessibilityValue(forcedValue);
    m_inspectedPage.accessibilitySettingsDidChange();
}

void InspectorPageAgent::overridePrefersColorScheme(std::optional<Protocol::Page::UserPreferenceValue>&& value)
{
#if ENABLE(DARK_MODE_CSS) || HAVE(OS_DARK_MODE_SUPPORT)
    if (!value)
        m_inspectedPage.setUseDarkAppearanceOverride(std::nullopt);
    else if (value == Protocol::Page::UserPreferenceValue::Light)
        m_inspectedPage.setUseDarkAppearanceOverride(false);
    else if (value == Protocol::Page::UserPreferenceValue::Dark)
        m_inspectedPage.setUseDarkAppearanceOverride(true);
#else
    UNUSED_PARAM(value);
#endif
}

static Protocol::Page::CookieSameSitePolicy cookieSameSitePolicyJSON(Cookie::SameSitePolicy policy)
{
    switch (policy) {
    case Cookie::SameSitePolicy::None:
        return Protocol::Page::CookieSameSitePolicy::None;
    case Cookie::SameSitePolicy::Lax:
        return Protocol::Page::CookieSameSitePolicy::Lax;
    case Cookie::SameSitePolicy::Strict:
        return Protocol::Page::CookieSameSitePolicy::Strict;
    }
    ASSERT_NOT_REACHED();
    return Protocol::Page::CookieSameSitePolicy::None;
}

static Ref<Protocol::Page::Cookie> buildObjectForCookie(const Cookie& cookie)
{
    return Protocol::Page::Cookie::create()
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
}

static Ref<JSON::ArrayOf<Protocol::Page::Cookie>> buildArrayForCookies(ListHashSet<Cookie>& cookiesList)
{
    auto cookies = JSON::ArrayOf<Protocol::Page::Cookie>::create();

    for (const auto& cookie : cookiesList)
        cookies->addItem(buildObjectForCookie(cookie));

    return cookies;
}

static Vector<URL> allResourcesURLsForFrame(Frame* frame)
{
    Vector<URL> result;

    result.append(frame->loader().documentLoader()->url());

    for (auto* cachedResource : InspectorPageAgent::cachedResourcesForFrame(frame))
        result.append(cachedResource->url());

    return result;
}

Protocol::ErrorStringOr<Ref<JSON::ArrayOf<Protocol::Page::Cookie>>> InspectorPageAgent::getCookies()
{
    ListHashSet<Cookie> allRawCookies;

    for (AbstractFrame* frame = &m_inspectedPage.mainFrame(); frame; frame = frame->tree().traverseNext()) {
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

static std::optional<Cookie> parseCookieObject(Protocol::ErrorString& errorString, Ref<JSON::Object>&& cookieObject)
{
    Cookie cookie;

    cookie.name = cookieObject->getString(Protocol::Page::Cookie::nameKey);
    if (!cookie.name) {
        errorString = "Invalid value for key name in given cookie"_s;
        return std::nullopt;
    }

    cookie.value = cookieObject->getString(Protocol::Page::Cookie::valueKey);
    if (!cookie.value) {
        errorString = "Invalid value for key value in given cookie"_s;
        return std::nullopt;
    }

    cookie.domain = cookieObject->getString(Protocol::Page::Cookie::domainKey);
    if (!cookie.domain) {
        errorString = "Invalid value for key domain in given cookie"_s;
        return std::nullopt;
    }

    cookie.path = cookieObject->getString(Protocol::Page::Cookie::pathKey);
    if (!cookie.path) {
        errorString = "Invalid value for key path in given cookie"_s;
        return std::nullopt;
    }

    auto httpOnly = cookieObject->getBoolean(Protocol::Page::Cookie::httpOnlyKey);
    if (!httpOnly) {
        errorString = "Invalid value for key httpOnly in given cookie"_s;
        return std::nullopt;
    }

    cookie.httpOnly = *httpOnly;

    auto secure = cookieObject->getBoolean(Protocol::Page::Cookie::secureKey);
    if (!secure) {
        errorString = "Invalid value for key secure in given cookie"_s;
        return std::nullopt;
    }

    cookie.secure = *secure;

    auto session = cookieObject->getBoolean(Protocol::Page::Cookie::sessionKey);
    cookie.expires = cookieObject->getDouble(Protocol::Page::Cookie::expiresKey);
    if (!session && !cookie.expires) {
        errorString = "Invalid value for key expires in given cookie"_s;
        return std::nullopt;
    }

    cookie.session = *session;

    auto sameSiteString = cookieObject->getString(Protocol::Page::Cookie::sameSiteKey);
    if (!sameSiteString) {
        errorString = "Invalid value for key sameSite in given cookie"_s;
        return std::nullopt;
    }

    auto sameSite = Protocol::Helpers::parseEnumValueFromString<Protocol::Page::CookieSameSitePolicy>(sameSiteString);
    if (!sameSite) {
        errorString = "Invalid value for key sameSite in given cookie"_s;
        return std::nullopt;
    }

    switch (sameSite.value()) {
    case Protocol::Page::CookieSameSitePolicy::None:
        cookie.sameSite = Cookie::SameSitePolicy::None;
        break;

    case Protocol::Page::CookieSameSitePolicy::Lax:
        cookie.sameSite = Cookie::SameSitePolicy::Lax;
        break;

    case Protocol::Page::CookieSameSitePolicy::Strict:
        cookie.sameSite = Cookie::SameSitePolicy::Strict;
        break;
    }

    return cookie;
}

Protocol::ErrorStringOr<void> InspectorPageAgent::setCookie(Ref<JSON::Object>&& cookieObject)
{
    Protocol::ErrorString errorString;

    auto cookie = parseCookieObject(errorString, WTFMove(cookieObject));
    if (!cookie)
        return makeUnexpected(errorString);

    for (AbstractFrame* frame = &m_inspectedPage.mainFrame(); frame; frame = frame->tree().traverseNext()) {
        auto* localFrame = dynamicDowncast<LocalFrame>(frame);
        if (!localFrame)
            continue;
        auto* document = localFrame->document();
        if (!document)
            continue;
        auto* page = document->page();
        if (!page)
            continue;
        page->cookieJar().setRawCookie(*document, cookie.value());
    }

    return { };
}

Protocol::ErrorStringOr<void> InspectorPageAgent::deleteCookie(const String& cookieName, const String& url)
{
    URL parsedURL({ }, url);
    for (AbstractFrame* frame = &m_inspectedPage.mainFrame(); frame; frame = frame->tree().traverseNext()) {
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

Protocol::ErrorStringOr<Ref<Protocol::Page::FrameResourceTree>> InspectorPageAgent::getResourceTree()
{
    return buildObjectForFrameTree(&m_inspectedPage.mainFrame());
}

Protocol::ErrorStringOr<std::tuple<String, bool /* base64Encoded */>> InspectorPageAgent::getResourceContent(const Protocol::Network::FrameId& frameId, const String& url)
{
    Protocol::ErrorString errorString;

    Frame* frame = assertFrame(errorString, frameId);
    if (!frame)
        return makeUnexpected(errorString);

    String content;
    bool base64Encoded;

    resourceContent(errorString, frame, URL({ }, url), &content, &base64Encoded);

    return { { content, base64Encoded } };
}

Protocol::ErrorStringOr<void> InspectorPageAgent::setBootstrapScript(const String& source)
{
    m_bootstrapScript = source;

    return { };
}

Protocol::ErrorStringOr<Ref<JSON::ArrayOf<Protocol::GenericTypes::SearchMatch>>> InspectorPageAgent::searchInResource(const Protocol::Network::FrameId& frameId, const String& url, const String& query, std::optional<bool>&& caseSensitive, std::optional<bool>&& isRegex, const Protocol::Network::RequestId& requestId)
{
    Protocol::ErrorString errorString;

    if (!!requestId) {
        if (auto* networkAgent = m_instrumentingAgents.enabledNetworkAgent()) {
            RefPtr<JSON::ArrayOf<Protocol::GenericTypes::SearchMatch>> result;
            networkAgent->searchInRequest(errorString, requestId, query, caseSensitive && *caseSensitive, isRegex && *isRegex, result);
            if (!result)
                return makeUnexpected(errorString);
            return result.releaseNonNull();
        }
    }

    Frame* frame = assertFrame(errorString, frameId);
    if (!frame)
        return makeUnexpected(errorString);

    DocumentLoader* loader = assertDocumentLoader(errorString, frame);
    if (!loader)
        return makeUnexpected(errorString);

    URL kurl({ }, url);

    String content;
    bool success = false;
    if (equalIgnoringFragmentIdentifier(kurl, loader->url()))
        success = mainResourceContent(frame, false, &content);

    if (!success) {
        if (auto* resource = cachedResource(frame, kurl)) {
            if (auto textContent = InspectorNetworkAgent::textContentForCachedResource(*resource)) {
                content = *textContent;
                success = true;
            }
        }
    }

    if (!success)
        return JSON::ArrayOf<Protocol::GenericTypes::SearchMatch>::create();

    return ContentSearchUtilities::searchInTextByLines(content, query, caseSensitive && *caseSensitive, isRegex && *isRegex);
}

static Ref<Protocol::Page::SearchResult> buildObjectForSearchResult(const Protocol::Network::FrameId& frameId, const String& url, int matchesCount)
{
    return Protocol::Page::SearchResult::create()
        .setUrl(url)
        .setFrameId(frameId)
        .setMatchesCount(matchesCount)
        .release();
}

Protocol::ErrorStringOr<Ref<JSON::ArrayOf<Protocol::Page::SearchResult>>> InspectorPageAgent::searchInResources(const String& text, std::optional<bool>&& caseSensitive, std::optional<bool>&& isRegex)
{
    auto result = JSON::ArrayOf<Protocol::Page::SearchResult>::create();

    auto searchStringType = (isRegex && *isRegex) ? ContentSearchUtilities::SearchStringType::Regex : ContentSearchUtilities::SearchStringType::ContainsString;
    auto regex = ContentSearchUtilities::createRegularExpressionForSearchString(text, caseSensitive && *caseSensitive, searchStringType);

    for (AbstractFrame* frame = &m_inspectedPage.mainFrame(); frame; frame = frame->tree().traverseNext()) {
        auto* localFrame = dynamicDowncast<LocalFrame>(frame);
        if (!localFrame)
            continue;
        for (auto* cachedResource : cachedResourcesForFrame(localFrame)) {
            if (auto textContent = InspectorNetworkAgent::textContentForCachedResource(*cachedResource)) {
                int matchesCount = ContentSearchUtilities::countRegularExpressionMatches(regex, *textContent);
                if (matchesCount)
                    result->addItem(buildObjectForSearchResult(frameId(localFrame), cachedResource->url().string(), matchesCount));
            }
        }
    }

    if (auto* networkAgent = m_instrumentingAgents.enabledNetworkAgent())
        networkAgent->searchOtherRequests(regex, result);

    return result;
}

#if !PLATFORM(IOS_FAMILY)
Protocol::ErrorStringOr<void> InspectorPageAgent::setShowRulers(bool showRulers)
{
    m_overlay->setShowRulers(showRulers);

    return { };
}
#endif

Protocol::ErrorStringOr<void> InspectorPageAgent::setShowPaintRects(bool show)
{
    m_showPaintRects = show;
    m_client->setShowPaintRects(show);

    if (m_client->overridesShowPaintRects())
        return { };

    m_overlay->setShowPaintRects(show);

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

void InspectorPageAgent::frameNavigated(Frame& frame)
{
    m_frontendDispatcher->frameNavigated(buildObjectForFrame(&frame));
}

void InspectorPageAgent::frameDetached(Frame& frame)
{
    auto identifier = m_frameToIdentifier.take(&frame);
    if (identifier.isNull())
        return;
    m_frontendDispatcher->frameDetached(identifier);
    m_identifierToFrame.remove(identifier);
}

Frame* InspectorPageAgent::frameForId(const Protocol::Network::FrameId& frameId)
{
    return frameId.isEmpty() ? nullptr : m_identifierToFrame.get(frameId).get();
}

String InspectorPageAgent::frameId(Frame* frame)
{
    if (!frame)
        return emptyString();
    return m_frameToIdentifier.ensure(frame, [this, frame] {
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

Frame* InspectorPageAgent::assertFrame(Protocol::ErrorString& errorString, const Protocol::Network::FrameId& frameId)
{
    Frame* frame = frameForId(frameId);
    if (!frame)
        errorString = "Missing frame for given frameId"_s;
    return frame;
}

void InspectorPageAgent::loaderDetachedFromFrame(DocumentLoader& loader)
{
    m_loaderToIdentifier.remove(&loader);
}

void InspectorPageAgent::frameStartedLoading(Frame& frame)
{
    m_frontendDispatcher->frameStartedLoading(frameId(&frame));
}

void InspectorPageAgent::frameStoppedLoading(Frame& frame)
{
    m_frontendDispatcher->frameStoppedLoading(frameId(&frame));
}

void InspectorPageAgent::frameScheduledNavigation(Frame& frame, Seconds delay)
{
    m_frontendDispatcher->frameScheduledNavigation(frameId(&frame), delay.value());
}

void InspectorPageAgent::frameClearedScheduledNavigation(Frame& frame)
{
    m_frontendDispatcher->frameClearedScheduledNavigation(frameId(&frame));
}

void InspectorPageAgent::accessibilitySettingsDidChange()
{
    defaultUserPreferencesDidChange();
}

void InspectorPageAgent::defaultUserPreferencesDidChange()
{
    auto defaultUserPreferences = JSON::ArrayOf<Protocol::Page::UserPreference>::create();

#if USE(NEW_THEME)
    bool prefersReducedMotion = Theme::singleton().userPrefersReducedMotion();
#else
    bool prefersReducedMotion = false;
#endif

    auto prefersReducedMotionUserPreference = Protocol::Page::UserPreference::create()
        .setName(Protocol::Page::UserPreferenceName::PrefersReducedMotion)
        .setValue(prefersReducedMotion ? Protocol::Page::UserPreferenceValue::Reduce : Protocol::Page::UserPreferenceValue::NoPreference)
        .release();

    defaultUserPreferences->addItem(WTFMove(prefersReducedMotionUserPreference));

#if USE(NEW_THEME)
    bool prefersContrast = Theme::singleton().userPrefersContrast();
#else
    bool prefersContrast = false;
#endif

    auto prefersContrastUserPreference = Protocol::Page::UserPreference::create()
        .setName(Protocol::Page::UserPreferenceName::PrefersContrast)
        .setValue(prefersContrast ? Protocol::Page::UserPreferenceValue::More : Protocol::Page::UserPreferenceValue::NoPreference)
        .release();

    defaultUserPreferences->addItem(WTFMove(prefersContrastUserPreference));

#if ENABLE(DARK_MODE_CSS) || HAVE(OS_DARK_MODE_SUPPORT)
    auto prefersColorSchemeUserPreference = Protocol::Page::UserPreference::create()
        .setName(Protocol::Page::UserPreferenceName::PrefersColorScheme)
        .setValue(m_inspectedPage.defaultUseDarkAppearance() ? Protocol::Page::UserPreferenceValue::Dark : Protocol::Page::UserPreferenceValue::Light)
        .release();

    defaultUserPreferences->addItem(WTFMove(prefersColorSchemeUserPreference));
#endif

    m_frontendDispatcher->defaultUserPreferencesDidChange(WTFMove(defaultUserPreferences));
}

#if ENABLE(DARK_MODE_CSS) || HAVE(OS_DARK_MODE_SUPPORT)
void InspectorPageAgent::defaultAppearanceDidChange()
{
    defaultUserPreferencesDidChange();
}
#endif

void InspectorPageAgent::didClearWindowObjectInWorld(Frame& frame, DOMWrapperWorld& world)
{
    if (&world != &mainThreadNormalWorld())
        return;

    if (m_bootstrapScript.isEmpty())
        return;

    frame.script().evaluateIgnoringException(ScriptSourceCode(m_bootstrapScript, URL { "web-inspector://bootstrap.js"_str }));
}

void InspectorPageAgent::didPaint(RenderObject& renderer, const LayoutRect& rect)
{
    if (!m_showPaintRects)
        return;

    LayoutRect absoluteRect = LayoutRect(renderer.localToAbsoluteQuad(FloatRect(rect)).boundingBox());
    FrameView* view = renderer.document().view();

    LayoutRect rootRect = absoluteRect;
    auto* localFrame = dynamicDowncast<LocalFrame>(view->frame());
    if (localFrame && !localFrame->isMainFrame()) {
        IntRect rootViewRect = view->contentsToRootView(snappedIntRect(absoluteRect));
        rootRect = localFrame->mainFrame().view()->rootViewToContents(rootViewRect);
    }

    if (m_client->overridesShowPaintRects()) {
        m_client->showPaintRect(rootRect);
        return;
    }

    m_overlay->showPaintRect(rootRect);
}

void InspectorPageAgent::didLayout()
{
    bool isFirstLayout = m_isFirstLayoutAfterOnLoad;
    if (isFirstLayout)
        m_isFirstLayoutAfterOnLoad = false;

    m_overlay->update();
}

void InspectorPageAgent::didScroll()
{
    m_overlay->update();
}

void InspectorPageAgent::didRecalculateStyle()
{
    m_overlay->update();
}

Ref<Protocol::Page::Frame> InspectorPageAgent::buildObjectForFrame(Frame* frame)
{
    ASSERT_ARG(frame, frame);

    auto frameObject = Protocol::Page::Frame::create()
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

Ref<Protocol::Page::FrameResourceTree> InspectorPageAgent::buildObjectForFrameTree(Frame* frame)
{
    ASSERT_ARG(frame, frame);

    auto frameObject = buildObjectForFrame(frame);
    auto subresources = JSON::ArrayOf<Protocol::Page::FrameResource>::create();
    auto result = Protocol::Page::FrameResourceTree::create()
        .setFrame(WTFMove(frameObject))
        .setResources(subresources.copyRef())
        .release();

    for (auto* cachedResource : cachedResourcesForFrame(frame)) {
        auto resourceObject = Protocol::Page::FrameResource::create()
            .setUrl(cachedResource->url().string())
            .setType(cachedResourceTypeJSON(*cachedResource))
            .setMimeType(cachedResource->response().mimeType())
            .release();
        if (cachedResource->wasCanceled())
            resourceObject->setCanceled(true);
        else if (cachedResource->status() == CachedResource::LoadError || cachedResource->status() == CachedResource::DecodeError)
            resourceObject->setFailed(true);
        String sourceMappingURL = InspectorPageAgent::sourceMapURLForResource(cachedResource);
        if (!sourceMappingURL.isEmpty())
            resourceObject->setSourceMapURL(sourceMappingURL);
        String targetId = cachedResource->resourceRequest().initiatorIdentifier();
        if (!targetId.isEmpty())
            resourceObject->setTargetId(targetId);
        subresources->addItem(WTFMove(resourceObject));
    }

    RefPtr<JSON::ArrayOf<Protocol::Page::FrameResourceTree>> childrenArray;
    for (AbstractFrame* child = frame->tree().firstChild(); child; child = child->tree().nextSibling()) {
        if (!childrenArray) {
            childrenArray = JSON::ArrayOf<Protocol::Page::FrameResourceTree>::create();
            result->setChildFrames(*childrenArray);
        }
        auto* localChild = dynamicDowncast<LocalFrame>(child);
        if (!localChild)
            continue;
        childrenArray->addItem(buildObjectForFrameTree(localChild));
    }
    return result;
}

Protocol::ErrorStringOr<void> InspectorPageAgent::setEmulatedMedia(const String& media)
{
    if (media == m_emulatedMedia)
        return { };

    m_emulatedMedia = AtomString(media);

    // FIXME: Schedule a rendering update instead of synchronously updating the layout.
    m_inspectedPage.updateStyleAfterChangeInEnvironment();

    RefPtr document = m_inspectedPage.mainFrame().document();
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

Protocol::ErrorStringOr<String> InspectorPageAgent::snapshotNode(Protocol::DOM::NodeId nodeId)
{
    Protocol::ErrorString errorString;

    InspectorDOMAgent* domAgent = m_instrumentingAgents.persistentDOMAgent();
    ASSERT(domAgent);
    Node* node = domAgent->assertNode(errorString, nodeId);
    if (!node)
        return makeUnexpected(errorString);

    auto snapshot = WebCore::snapshotNode(m_inspectedPage.mainFrame(), *node, { { }, PixelFormat::BGRA8, DestinationColorSpace::SRGB() });
    if (!snapshot)
        return makeUnexpected("Could not capture snapshot"_s);

    return snapshot->toDataURL("image/png"_s, std::nullopt, PreserveResolution::Yes);
}

Protocol::ErrorStringOr<String> InspectorPageAgent::snapshotRect(int x, int y, int width, int height, Protocol::Page::CoordinateSystem coordinateSystem)
{
    SnapshotOptions options { { }, PixelFormat::BGRA8, DestinationColorSpace::SRGB() };
    if (coordinateSystem == Protocol::Page::CoordinateSystem::Viewport)
        options.flags.add(SnapshotFlags::InViewCoordinates);

    IntRect rectangle(x, y, width, height);
    auto snapshot = snapshotFrameRect(m_inspectedPage.mainFrame(), rectangle, WTFMove(options));

    if (!snapshot)
        return makeUnexpected("Could not capture snapshot"_s);

    return snapshot->toDataURL("image/png"_s, std::nullopt, PreserveResolution::Yes);
}

#if ENABLE(WEB_ARCHIVE) && USE(CF)
Protocol::ErrorStringOr<String> InspectorPageAgent::archive()
{
    auto archive = LegacyWebArchive::create(m_inspectedPage.mainFrame());
    if (!archive)
        return makeUnexpected("Could not create web archive for main frame"_s);

    RetainPtr<CFDataRef> buffer = archive->rawDataRepresentation();
    return base64EncodeToString(CFDataGetBytePtr(buffer.get()), CFDataGetLength(buffer.get()));
}
#endif

#if !PLATFORM(COCOA)
Protocol::ErrorStringOr<void> InspectorPageAgent::setScreenSizeOverride(std::optional<int>&& width, std::optional<int>&& height)
{
    if (width.has_value() != height.has_value())
        return makeUnexpected("Screen width and height override should be both specified or omitted"_s);

    if (width && *width <= 0)
        return makeUnexpected("Screen width override should be a positive integer"_s);

    if (height && *height <= 0)
        return makeUnexpected("Screen height override should be a positive integer"_s);

    m_inspectedPage.mainFrame().setOverrideScreenSize(FloatSize(width.value_or(0), height.value_or(0)));
    return { };
}
#endif

} // namespace WebCore
