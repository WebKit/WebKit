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

#include "AXObjectCache.h"
#include "BackForwardController.h"
#include "CachedResource.h"
#include "CachedResourceLoader.h"
#include "Cookie.h"
#include "CookieJar.h"
#include "CustomHeaderFields.h"
#include "DOMWrapperWorld.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameLoadRequest.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "FrameSnapshotting.h"
#include "FrameView.h"
#include "HTMLFrameOwnerElement.h"
#include "HTMLInputElement.h"
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
#include "PageRuntimeAgent.h"
#include "RenderObject.h"
#include "RenderTheme.h"
#include "RuntimeEnabledFeatures.h"
#include "ScriptController.h"
#include "ScriptSourceCode.h"
#include "ScriptState.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "StyleScope.h"
#include "TextEncoding.h"
#include "TypingCommand.h"
#include "UserGestureIndicator.h"
#include <JavaScriptCore/ContentSearchUtilities.h>
#include <JavaScriptCore/IdentifiersFactory.h>
#include <JavaScriptCore/InjectedScriptManager.h>
#include <JavaScriptCore/RegularExpression.h>
#include <wtf/DateMath.h>
#include <wtf/ListHashSet.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Ref.h>
#include <wtf/RefPtr.h>
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

static HashMap<String, Ref<DOMWrapperWorld>>& createdUserWorlds() {
    static NeverDestroyed<HashMap<String, Ref<DOMWrapperWorld>>> nameToWorld;
    return nameToWorld;
}

static bool decodeBuffer(const uint8_t* buffer, unsigned size, const String& textEncodingName, String* result)
{
    if (buffer) {
        TextEncoding encoding(textEncodingName);
        if (!encoding.isValid())
            encoding = WindowsLatin1Encoding();
        *result = encoding.decode(buffer, size);
        return true;
    }
    return false;
}

bool InspectorPageAgent::mainResourceContent(Frame* frame, bool withBase64Encode, String* result)
{
    RefPtr<SharedBuffer> buffer = frame->loader().documentLoader()->mainResourceData();
    if (!buffer)
        return false;
    return InspectorPageAgent::dataContent(buffer->data(), buffer->size(), frame->document()->encoding(), withBase64Encode, result);
}

bool InspectorPageAgent::sharedBufferContent(RefPtr<SharedBuffer>&& buffer, const String& textEncodingName, bool withBase64Encode, String* result)
{
    return dataContent(buffer ? buffer->data() : nullptr, buffer ? buffer->size() : 0, textEncodingName, withBase64Encode, result);
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

    RefPtr<SharedBuffer> buffer;
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

CachedResource* InspectorPageAgent::cachedResource(Frame* frame, const URL& url)
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
    case EventSource:
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
        case ResourceRequest::Requester::Fetch:
            return InspectorPageAgent::FetchResource;
        case ResourceRequest::Requester::Main:
            return InspectorPageAgent::DocumentResource;
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
    for (Frame* frame = &page.mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (frame->document()->securityOrigin().toRawString() == originRawString)
            return frame;
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
    , m_injectedScriptManager(context.injectedScriptManager)
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

#if ENABLE(DARK_MODE_CSS) || HAVE(OS_DARK_MODE_SUPPORT)
    defaultAppearanceDidChange(m_inspectedPage.defaultUseDarkAppearance());
#endif

    if (!createdUserWorlds().isEmpty()) {
        Vector<DOMWrapperWorld*> worlds;
        for (const auto& world : createdUserWorlds().values())
            worlds.append(world.ptr());
        ensureUserWorldsExistInAllFrames(worlds);
    }
    return { };
}

Protocol::ErrorStringOr<void> InspectorPageAgent::disable()
{
    m_instrumentingAgents.setEnabledPageAgent(nullptr);
    m_interceptFileChooserDialog = false;
    m_bypassCSP = false;

    setShowPaintRects(false);
#if !PLATFORM(IOS_FAMILY)
    setShowRulers(false);
#endif
    overrideUserAgent(nullString());
    setEmulatedMedia(emptyString());
#if ENABLE(DARK_MODE_CSS) || HAVE(OS_DARK_MODE_SUPPORT)
    setForcedAppearance(std::nullopt);
#endif

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

Protocol::ErrorStringOr<void> InspectorPageAgent::goBack()
{
    if (!m_inspectedPage.backForward().goBack())
        return makeUnexpected("Failed to go back"_s);

    return { };
}

Protocol::ErrorStringOr<void> InspectorPageAgent::goForward()
{
    if (!m_inspectedPage.backForward().goForward())
        return makeUnexpected("Failed to go forward"_s);

    return { };
}

Protocol::ErrorStringOr<void> InspectorPageAgent::navigate(const String& url)
{
    UserGestureIndicator indicator { ProcessingUserGesture };
    Frame& frame = m_inspectedPage.mainFrame();

    ResourceRequest resourceRequest { frame.document()->completeURL(url) };
    FrameLoadRequest frameLoadRequest { *frame.document(), frame.document()->securityOrigin(), WTFMove(resourceRequest), "_self"_s, InitiatedByMainFrame::Unknown };
    frameLoadRequest.disableNavigationToInvalidURL();
    frame.loader().changeLocation(WTFMove(frameLoadRequest));

    return { };
}

Protocol::ErrorStringOr<void> InspectorPageAgent::overrideUserAgent(const String& value)
{
    m_userAgentOverride = value;

    return { };
}

Protocol::ErrorStringOr<void> InspectorPageAgent::overridePlatform(const String& value)
{
    m_platformOverride = value;

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

#if ENABLE(DEVICE_ORIENTATION)
    case Protocol::Page::Setting::DeviceOrientationEventEnabled:
        inspectedPageSettings.setDeviceOrientationEventEnabled(value.value_or(false));
        return { };
#endif

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

#if ENABLE(NOTIFICATIONS)
    case Protocol::Page::Setting::NotificationsEnabled:
        inspectedPageSettings.setNotificationsEnabled(value.value_or(false));
        return { };
#endif

#if ENABLE(FULLSCREEN_API)
    case Protocol::Page::Setting::FullScreenEnabled:
        inspectedPageSettings.setFullScreenEnabled(value.value_or(false));
        return { };
#endif

#if ENABLE(INPUT_TYPE_MONTH)
    case Protocol::Page::Setting::InputTypeMonthEnabled:
        inspectedPageSettings.setInputTypeMonthEnabled(value.value_or(false));
        return { };
#endif

#if ENABLE(INPUT_TYPE_WEEK)
    case Protocol::Page::Setting::InputTypeWeekEnabled:
        inspectedPageSettings.setInputTypeWeekEnabled(value.value_or(false));
        return { };
#endif

#if ENABLE(POINTER_LOCK)
    case Protocol::Page::Setting::PointerLockEnabled:
        inspectedPageSettings.setPointerLockEnabled(value.value_or(false));
        return { };
#endif

    case Protocol::Page::Setting::ScriptEnabled:
        inspectedPageSettings.setScriptEnabledInspectorOverride(value);
        return { };

    case Protocol::Page::Setting::ShowDebugBorders:
        inspectedPageSettings.setShowDebugBordersInspectorOverride(value);
        return { };

    case Protocol::Page::Setting::ShowRepaintCounter:
        inspectedPageSettings.setShowRepaintCounterInspectorOverride(value);
        return { };

#if ENABLE(MEDIA_STREAM)
    case Protocol::Page::Setting::SpeechRecognitionEnabled:
        inspectedPageSettings.setSpeechRecognitionEnabled(value.value_or(false));
        return { };
#endif

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

    for (Frame* frame = &m_inspectedPage.mainFrame(); frame; frame = frame->tree().traverseNext()) {
        Document* document = frame->document();
        if (!document || !document->page())
            continue;

        for (auto& url : allResourcesURLsForFrame(frame)) {
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
        errorString = "Invalid value for key name in given cookie";
        return std::nullopt;
    }

    cookie.value = cookieObject->getString(Protocol::Page::Cookie::valueKey);
    if (!cookie.value) {
        errorString = "Invalid value for key value in given cookie";
        return std::nullopt;
    }

    cookie.domain = cookieObject->getString(Protocol::Page::Cookie::domainKey);
    if (!cookie.domain) {
        errorString = "Invalid value for key domain in given cookie";
        return std::nullopt;
    }

    cookie.path = cookieObject->getString(Protocol::Page::Cookie::pathKey);
    if (!cookie.path) {
        errorString = "Invalid value for key path in given cookie";
        return std::nullopt;
    }

    auto httpOnly = cookieObject->getBoolean(Protocol::Page::Cookie::httpOnlyKey);
    if (!httpOnly) {
        errorString = "Invalid value for key httpOnly in given cookie";
        return std::nullopt;
    }

    cookie.httpOnly = *httpOnly;

    auto secure = cookieObject->getBoolean(Protocol::Page::Cookie::secureKey);
    if (!secure) {
        errorString = "Invalid value for key secure in given cookie";
        return std::nullopt;
    }

    cookie.secure = *secure;

    auto session = cookieObject->getBoolean(Protocol::Page::Cookie::sessionKey);
    cookie.expires = cookieObject->getDouble(Protocol::Page::Cookie::expiresKey);
    if (!session && !cookie.expires) {
        errorString = "Invalid value for key expires in given cookie";
        return std::nullopt;
    }

    cookie.session = *session;

    auto sameSiteString = cookieObject->getString(Protocol::Page::Cookie::sameSiteKey);
    if (!sameSiteString) {
        errorString = "Invalid value for key sameSite in given cookie";
        return std::nullopt;
    }

    auto sameSite = Protocol::Helpers::parseEnumValueFromString<Protocol::Page::CookieSameSitePolicy>(sameSiteString);
    if (!sameSite) {
        errorString = "Invalid value for key sameSite in given cookie";
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

    for (auto* frame = &m_inspectedPage.mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (auto* document = frame->document()) {
            if (auto* page = document->page())
                page->cookieJar().setRawCookie(*document, cookie.value());
        }
    }

    return { };
}

Protocol::ErrorStringOr<void> InspectorPageAgent::deleteCookie(const String& cookieName, const String& url)
{
    URL parsedURL({ }, url);
    for (Frame* frame = &m_inspectedPage.mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (auto* document = frame->document()) {
            if (auto* page = document->page())
                page->cookieJar().deleteCookie(*document, parsedURL, cookieName);
        }
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

Protocol::ErrorStringOr<void> InspectorPageAgent::setBootstrapScript(const String& source, const String& worldName)
{
    String key = worldName.isNull() ? emptyString() : worldName;
    if (source.isEmpty())
        m_worldNameToBootstrapScript.remove(key);
    else
        m_worldNameToBootstrapScript.set(key, source);

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

    for (Frame* frame = &m_inspectedPage.mainFrame(); frame; frame = frame->tree().traverseNext()) {
        for (auto* cachedResource : cachedResourcesForFrame(frame)) {
            if (auto textContent = InspectorNetworkAgent::textContentForCachedResource(*cachedResource)) {
                int matchesCount = ContentSearchUtilities::countRegularExpressionMatches(regex, *textContent);
                if (matchesCount)
                    result->addItem(buildObjectForSearchResult(frameId(frame), cachedResource->url().string(), matchesCount));
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

void InspectorPageAgent::domContentEventFired(Frame& frame)
{
    if (frame.isMainFrame())
        m_isFirstLayoutAfterOnLoad = true;
    m_frontendDispatcher->domContentEventFired(timestamp(), frameId(&frame));
}

void InspectorPageAgent::loadEventFired(Frame& frame)
{
    m_frontendDispatcher->loadEventFired(timestamp(), frameId(&frame));
}

void InspectorPageAgent::frameNavigated(Frame& frame)
{
    m_frontendDispatcher->frameNavigated(buildObjectForFrame(&frame));
}

String InspectorPageAgent::makeFrameID(ProcessIdentifier processID,  FrameIdentifier frameID)
{
    return makeString(processID.toUInt64(), ".", frameID.toUInt64());
}

static String globalIDForFrame(Frame& frame)
{
    return InspectorPageAgent::makeFrameID(Process::identifier(), *frame.loader().client().frameID());
}

void InspectorPageAgent::frameDetached(Frame& frame)
{
    String identifier = globalIDForFrame(frame);
    if (!m_identifierToFrame.take(identifier))
        return;

    m_frontendDispatcher->frameDetached(identifier);
}

Frame* InspectorPageAgent::frameForId(const Protocol::Network::FrameId& frameId)
{
    return frameId.isEmpty() ? nullptr : m_identifierToFrame.get(frameId).get();
}

String InspectorPageAgent::frameId(Frame* frame)
{
    if (!frame)
        return emptyString();

    String identifier = globalIDForFrame(*frame);
    m_identifierToFrame.set(identifier, makeWeakPtr(frame));
    return identifier;
}

String InspectorPageAgent::loaderId(DocumentLoader* loader)
{
    if (!loader)
        return emptyString();

    return String::number(loader->loaderIDForInspector());
}

Frame* InspectorPageAgent::assertFrame(Protocol::ErrorString& errorString, const Protocol::Network::FrameId& frameId)
{
    Frame* frame = frameForId(frameId);
    if (!frame)
        errorString = "Missing frame for given frameId"_s;
    return frame;
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

void InspectorPageAgent::didNavigateWithinPage(Frame& frame)
{
    String url = frame.document()->url().string();
    m_frontendDispatcher->navigatedWithinDocument(frameId(&frame), url);
}

#if ENABLE(DARK_MODE_CSS) || HAVE(OS_DARK_MODE_SUPPORT)
void InspectorPageAgent::defaultAppearanceDidChange(bool useDarkAppearance)
{
    m_frontendDispatcher->defaultAppearanceDidChange(useDarkAppearance ? Protocol::Page::Appearance::Dark : Protocol::Page::Appearance::Light);
}
#endif

void InspectorPageAgent::didClearWindowObjectInWorld(Frame& frame, DOMWrapperWorld& world)
{
    if (m_worldNameToBootstrapScript.isEmpty())
        return;

    if (world.name().isEmpty() && &world != &mainThreadNormalWorld())
       return;

    String worldName = world.name();
    // Null string cannot be used as a key.
    if (worldName.isNull())
        worldName = emptyString();

    if (!m_worldNameToBootstrapScript.contains(worldName))
        return;

    String bootstrapScript = m_worldNameToBootstrapScript.get(worldName);
    frame.script().evaluateInWorldIgnoringException(ScriptSourceCode(bootstrapScript, URL { URL(), "web-inspector://bootstrap.js"_s }), world);
}

void InspectorPageAgent::didPaint(RenderObject& renderer, const LayoutRect& rect)
{
    if (!m_showPaintRects)
        return;

    LayoutRect absoluteRect = LayoutRect(renderer.localToAbsoluteQuad(FloatRect(rect)).boundingBox());
    FrameView* view = renderer.document().view();

    LayoutRect rootRect = absoluteRect;
    if (!view->frame().isMainFrame()) {
        IntRect rootViewRect = view->contentsToRootView(snappedIntRect(absoluteRect));
        rootRect = view->frame().mainFrame().view()->rootViewToContents(rootViewRect);
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

void InspectorPageAgent::runOpenPanel(HTMLInputElement* element, bool* intercept)
{
    if (m_interceptFileChooserDialog) {
        *intercept = true;
    } else {
        return;
    }
    Document& document = element->document();
    auto* frame =  document.frame();
    if (!frame)
        return;

    auto& state = *mainWorldExecState(frame);
    auto injectedScript = m_injectedScriptManager.injectedScriptFor(&state);
    if (injectedScript.hasNoValue())
        return;

    auto object = injectedScript.wrapObject(InspectorDOMAgent::nodeAsScriptValue(state, element), WTF::String());
    if (!object)
        return;

    m_frontendDispatcher->fileChooserOpened(frameId(frame), object.releaseNonNull());
}

void InspectorPageAgent::frameAttached(Frame& frame)
{
    Frame* parent = frame.tree().parent();
    String parentFrameId = frameId(parent);
    m_frontendDispatcher->frameAttached(frameId(&frame), parentFrameId);
}

bool InspectorPageAgent::shouldBypassCSP()
{
    return m_bypassCSP;
}

void InspectorPageAgent::willCheckNewWindowPolicy(const URL& url)
{
    m_frontendDispatcher->willRequestOpenWindow(url.string());
}

void InspectorPageAgent::didCheckNewWindowPolicy(bool allowed)
{
    m_frontendDispatcher->didRequestOpenWindow(allowed);
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
        frameObject->setParentId(frameId(frame->tree().parent()));
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
    for (Frame* child = frame->tree().firstChild(); child; child = child->tree().nextSibling()) {
        if (!childrenArray) {
            childrenArray = JSON::ArrayOf<Protocol::Page::FrameResourceTree>::create();
            result->setChildFrames(*childrenArray);
        }
        childrenArray->addItem(buildObjectForFrameTree(child));
    }
    return result;
}

Protocol::ErrorStringOr<void> InspectorPageAgent::setEmulatedMedia(const String& media)
{
    if (media == m_emulatedMedia)
        return { };

    m_emulatedMedia = media;

    // FIXME: Schedule a rendering update instead of synchronously updating the layout.
    m_inspectedPage.updateStyleAfterChangeInEnvironment();

    RefPtr document = m_inspectedPage.mainFrame().document();
    if (!document)
        return { };

    document->updateLayout();
    document->evaluateMediaQueriesAndReportChanges();

    return { };
}

#if ENABLE(DARK_MODE_CSS) || HAVE(OS_DARK_MODE_SUPPORT)
Protocol::ErrorStringOr<void> InspectorPageAgent::setForcedAppearance(std::optional<Protocol::Page::Appearance>&& appearance)
{
    if (!appearance) {
        m_inspectedPage.setUseDarkAppearanceOverride(std::nullopt);
        return { };
    }

    switch (*appearance) {
    case Protocol::Page::Appearance::Light:
        m_inspectedPage.setUseDarkAppearanceOverride(false);
        return { };

    case Protocol::Page::Appearance::Dark:
        m_inspectedPage.setUseDarkAppearanceOverride(true);
        return { };
    }

    ASSERT_NOT_REACHED();
    return { };
}
#endif

void InspectorPageAgent::applyUserAgentOverride(String& userAgent)
{
    if (!m_userAgentOverride.isEmpty())
        userAgent = m_userAgentOverride;
}

void InspectorPageAgent::applyPlatformOverride(String& platform)
{
    if (!m_platformOverride.isEmpty())
        platform = m_platformOverride;
}

void InspectorPageAgent::applyEmulatedMedia(String& media)
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

Protocol::ErrorStringOr<String> InspectorPageAgent::snapshotRect(int x, int y, int width, int height, Protocol::Page::CoordinateSystem coordinateSystem, std::optional<bool>&& omitDeviceScaleFactor)
{
    SnapshotOptions options { { }, PixelFormat::BGRA8, DestinationColorSpace::SRGB() };
    if (coordinateSystem == Protocol::Page::CoordinateSystem::Viewport)
        options.flags.add(SnapshotFlags::InViewCoordinates);
    if (omitDeviceScaleFactor.has_value() && *omitDeviceScaleFactor)
        options.flags.add(SnapshotFlags::OmitDeviceScaleFactor);

    IntRect rectangle(x, y, width, height);
    auto snapshot = snapshotFrameRect(m_inspectedPage.mainFrame(), rectangle, WTFMove(options));

    if (!snapshot)
        return makeUnexpected("Could not capture snapshot"_s);

    return snapshot->toDataURL("image/png"_s, std::nullopt, PreserveResolution::Yes);
}

Protocol::ErrorStringOr<void> InspectorPageAgent::setForcedReducedMotion(std::optional<Protocol::Page::ReducedMotion>&& reducedMotion)
{
    if (!reducedMotion) {
        m_inspectedPage.setUseReducedMotionOverride(std::nullopt);
        return { };
    }

    switch (*reducedMotion) {
        case Protocol::Page::ReducedMotion::Reduce:
            m_inspectedPage.setUseReducedMotionOverride(true);
            return { };
        case Protocol::Page::ReducedMotion::NoPreference:
            m_inspectedPage.setUseReducedMotionOverride(false);
            return { };
    }

    ASSERT_NOT_REACHED();
    return { };
}

Protocol::ErrorStringOr<void> InspectorPageAgent::setTimeZone(const String& timeZone)
{
    bool success = WTF::setTimeZoneForAutomation(timeZone);
    if (!success)
        return makeUnexpected("Invalid time zone " + timeZone);

    return { };
}

Protocol::ErrorStringOr<void> InspectorPageAgent::setTouchEmulationEnabled(bool enabled)
{
#if ENABLE(TOUCH_EVENTS)
    RuntimeEnabledFeatures::sharedFeatures().setTouchEventsEnabled(enabled);
    return { };
#else
    UNUSED_PARAM(enabled);
    return makeUnexpected("Not supported"_s);
#endif
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

Protocol::ErrorStringOr<void> InspectorPageAgent::insertText(const String& text)
{
    UserGestureIndicator indicator { ProcessingUserGesture };
    Document* focusedDocument = m_inspectedPage.focusController().focusedOrMainFrame().document();
    TypingCommand::insertText(*focusedDocument, text, 0);
    return { };
}

static String roleFromObject(RefPtr<AXCoreObject> axObject)
{
    String computedRoleString = axObject->computedRoleString();
    if (!computedRoleString.isEmpty())
        return computedRoleString;
    AccessibilityRole role = axObject->roleValue();
    switch(role) {
        case AccessibilityRole::Annotation:
            return "Annotation";
        case AccessibilityRole::Application:
            return "Application";
        case AccessibilityRole::ApplicationAlert:
            return "ApplicationAlert";
        case AccessibilityRole::ApplicationAlertDialog:
            return "ApplicationAlertDialog";
        case AccessibilityRole::ApplicationDialog:
            return "ApplicationDialog";
        case AccessibilityRole::ApplicationGroup:
            return "ApplicationGroup";
        case AccessibilityRole::ApplicationLog:
            return "ApplicationLog";
        case AccessibilityRole::ApplicationMarquee:
            return "ApplicationMarquee";
        case AccessibilityRole::ApplicationStatus:
            return "ApplicationStatus";
        case AccessibilityRole::ApplicationTextGroup:
            return "ApplicationTextGroup";
        case AccessibilityRole::ApplicationTimer:
            return "ApplicationTimer";
        case AccessibilityRole::Audio:
            return "Audio";
        case AccessibilityRole::Blockquote:
            return "Blockquote";
        case AccessibilityRole::Browser:
            return "Browser";
        case AccessibilityRole::BusyIndicator:
            return "BusyIndicator";
        case AccessibilityRole::Button:
            return "Button";
        case AccessibilityRole::Canvas:
            return "Canvas";
        case AccessibilityRole::Caption:
            return "Caption";
        case AccessibilityRole::Cell:
            return "Cell";
        case AccessibilityRole::CheckBox:
            return "CheckBox";
        case AccessibilityRole::ColorWell:
            return "ColorWell";
        case AccessibilityRole::Column:
            return "Column";
        case AccessibilityRole::ColumnHeader:
            return "ColumnHeader";
        case AccessibilityRole::ComboBox:
            return "ComboBox";
        case AccessibilityRole::Definition:
            return "Definition";
        case AccessibilityRole::Deletion:
            return "Deletion";
        case AccessibilityRole::DescriptionList:
            return "DescriptionList";
        case AccessibilityRole::DescriptionListTerm:
            return "DescriptionListTerm";
        case AccessibilityRole::DescriptionListDetail:
            return "DescriptionListDetail";
        case AccessibilityRole::Details:
            return "Details";
        case AccessibilityRole::Directory:
            return "Directory";
        case AccessibilityRole::DisclosureTriangle:
            return "DisclosureTriangle";
        case AccessibilityRole::Div:
            return "Div";
        case AccessibilityRole::Document:
            return "Document";
        case AccessibilityRole::DocumentArticle:
            return "DocumentArticle";
        case AccessibilityRole::DocumentMath:
            return "DocumentMath";
        case AccessibilityRole::DocumentNote:
            return "DocumentNote";
        case AccessibilityRole::Drawer:
            return "Drawer";
        case AccessibilityRole::EditableText:
            return "EditableText";
        case AccessibilityRole::Feed:
            return "Feed";
        case AccessibilityRole::Figure:
            return "Figure";
        case AccessibilityRole::Footer:
            return "Footer";
        case AccessibilityRole::Footnote:
            return "Footnote";
        case AccessibilityRole::Form:
            return "Form";
        case AccessibilityRole::GraphicsDocument:
            return "GraphicsDocument";
        case AccessibilityRole::GraphicsObject:
            return "GraphicsObject";
        case AccessibilityRole::GraphicsSymbol:
            return "GraphicsSymbol";
        case AccessibilityRole::Grid:
            return "Grid";
        case AccessibilityRole::GridCell:
            return "GridCell";
        case AccessibilityRole::Group:
            return "Group";
        case AccessibilityRole::GrowArea:
            return "GrowArea";
        case AccessibilityRole::Heading:
            return "Heading";
        case AccessibilityRole::HelpTag:
            return "HelpTag";
        case AccessibilityRole::HorizontalRule:
            return "HorizontalRule";
        case AccessibilityRole::Ignored:
            return "Ignored";
        case AccessibilityRole::Inline:
            return "Inline";
        case AccessibilityRole::Image:
            return "Image";
        case AccessibilityRole::ImageMap:
            return "ImageMap";
        case AccessibilityRole::ImageMapLink:
            return "ImageMapLink";
        case AccessibilityRole::Incrementor:
            return "Incrementor";
        case AccessibilityRole::Insertion:
            return "Insertion";
        case AccessibilityRole::Label:
            return "Label";
        case AccessibilityRole::LandmarkBanner:
            return "LandmarkBanner";
        case AccessibilityRole::LandmarkComplementary:
            return "LandmarkComplementary";
        case AccessibilityRole::LandmarkContentInfo:
            return "LandmarkContentInfo";
        case AccessibilityRole::LandmarkDocRegion:
            return "LandmarkDocRegion";
        case AccessibilityRole::LandmarkMain:
            return "LandmarkMain";
        case AccessibilityRole::LandmarkNavigation:
            return "LandmarkNavigation";
        case AccessibilityRole::LandmarkRegion:
            return "LandmarkRegion";
        case AccessibilityRole::LandmarkSearch:
            return "LandmarkSearch";
        case AccessibilityRole::Legend:
            return "Legend";
        case AccessibilityRole::Link:
            return "Link";
        case AccessibilityRole::List:
            return "List";
        case AccessibilityRole::ListBox:
            return "ListBox";
        case AccessibilityRole::ListBoxOption:
            return "ListBoxOption";
        case AccessibilityRole::ListItem:
            return "ListItem";
        case AccessibilityRole::ListMarker:
            return "ListMarker";
        case AccessibilityRole::Mark:
            return "Mark";
        case AccessibilityRole::MathElement:
            return "MathElement";
        case AccessibilityRole::Matte:
            return "Matte";
        case AccessibilityRole::Menu:
            return "Menu";
        case AccessibilityRole::MenuBar:
            return "MenuBar";
        case AccessibilityRole::MenuButton:
            return "MenuButton";
        case AccessibilityRole::MenuItem:
            return "MenuItem";
        case AccessibilityRole::MenuItemCheckbox:
            return "MenuItemCheckbox";
        case AccessibilityRole::MenuItemRadio:
            return "MenuItemRadio";
        case AccessibilityRole::MenuListPopup:
            return "MenuListPopup";
        case AccessibilityRole::MenuListOption:
            return "MenuListOption";
        case AccessibilityRole::Meter:
            return "Meter";
        case AccessibilityRole::Outline:
            return "Outline";
        case AccessibilityRole::Paragraph:
            return "Paragraph";
        case AccessibilityRole::PopUpButton:
            return "PopUpButton";
        case AccessibilityRole::Pre:
            return "Pre";
        case AccessibilityRole::Presentational:
            return "Presentational";
        case AccessibilityRole::ProgressIndicator:
            return "ProgressIndicator";
        case AccessibilityRole::RadioButton:
            return "RadioButton";
        case AccessibilityRole::RadioGroup:
            return "RadioGroup";
        case AccessibilityRole::RowHeader:
            return "RowHeader";
        case AccessibilityRole::Row:
            return "Row";
        case AccessibilityRole::RowGroup:
            return "RowGroup";
        case AccessibilityRole::RubyBase:
            return "RubyBase";
        case AccessibilityRole::RubyBlock:
            return "RubyBlock";
        case AccessibilityRole::RubyInline:
            return "RubyInline";
        case AccessibilityRole::RubyRun:
            return "RubyRun";
        case AccessibilityRole::RubyText:
            return "RubyText";
        case AccessibilityRole::Ruler:
            return "Ruler";
        case AccessibilityRole::RulerMarker:
            return "RulerMarker";
        case AccessibilityRole::ScrollArea:
            return "ScrollArea";
        case AccessibilityRole::ScrollBar:
            return "ScrollBar";
        case AccessibilityRole::SearchField:
            return "SearchField";
        case AccessibilityRole::Sheet:
            return "Sheet";
        case AccessibilityRole::Slider:
            return "Slider";
        case AccessibilityRole::SliderThumb:
            return "SliderThumb";
        case AccessibilityRole::SpinButton:
            return "SpinButton";
        case AccessibilityRole::SpinButtonPart:
            return "SpinButtonPart";
        case AccessibilityRole::SplitGroup:
            return "SplitGroup";
        case AccessibilityRole::Splitter:
            return "Splitter";
        case AccessibilityRole::StaticText:
            return "StaticText";
        case AccessibilityRole::Subscript:
            return "Subscript";
        case AccessibilityRole::Summary:
            return "Summary";
        case AccessibilityRole::Superscript:
            return "Superscript";
        case AccessibilityRole::Switch:
            return "Switch";
        case AccessibilityRole::SystemWide:
            return "SystemWide";
        case AccessibilityRole::SVGRoot:
            return "SVGRoot";
        case AccessibilityRole::SVGText:
            return "SVGText";
        case AccessibilityRole::SVGTSpan:
            return "SVGTSpan";
        case AccessibilityRole::SVGTextPath:
            return "SVGTextPath";
        case AccessibilityRole::TabGroup:
            return "TabGroup";
        case AccessibilityRole::TabList:
            return "TabList";
        case AccessibilityRole::TabPanel:
            return "TabPanel";
        case AccessibilityRole::Tab:
            return "Tab";
        case AccessibilityRole::Table:
            return "Table";
        case AccessibilityRole::TableHeaderContainer:
            return "TableHeaderContainer";
        case AccessibilityRole::TextArea:
            return "TextArea";
        case AccessibilityRole::TextGroup:
            return "TextGroup";
        case AccessibilityRole::Term:
            return "Term";
        case AccessibilityRole::Time:
            return "Time";
        case AccessibilityRole::Tree:
            return "Tree";
        case AccessibilityRole::TreeGrid:
            return "TreeGrid";
        case AccessibilityRole::TreeItem:
            return "TreeItem";
        case AccessibilityRole::TextField:
            return "TextField";
        case AccessibilityRole::ToggleButton:
            return "ToggleButton";
        case AccessibilityRole::Toolbar:
            return "Toolbar";
        case AccessibilityRole::Unknown:
            return "Unknown";
        case AccessibilityRole::UserInterfaceTooltip:
            return "UserInterfaceTooltip";
        case AccessibilityRole::ValueIndicator:
            return "ValueIndicator";
        case AccessibilityRole::Video:
            return "Video";
        case AccessibilityRole::WebApplication:
            return "WebApplication";
        case AccessibilityRole::WebArea:
            return "WebArea";
        case AccessibilityRole::WebCoreLink:
            return "WebCoreLink";
        case AccessibilityRole::Window:
            return "Window";
    };
    return "Unknown";
}

static Ref<Inspector::Protocol::Page::AXNode> snapshotForAXObject(RefPtr<AXCoreObject> axObject, Node* nodeToFind)
{
    auto axNode = Inspector::Protocol::Page::AXNode::create()
        .setRole(roleFromObject(axObject))
        .release();

    if (!axObject->computedLabel().isEmpty())
        axNode->setName(axObject->computedLabel());
    if (!axObject->stringValue().isEmpty())
        axNode->setValue(JSON::Value::create(axObject->stringValue()));
    if (!axObject->accessibilityDescription().isEmpty())
        axNode->setDescription(axObject->accessibilityDescription());
    if (!axObject->keyShortcutsValue().isEmpty())
        axNode->setKeyshortcuts(axObject->keyShortcutsValue());
    if (!axObject->valueDescription().isEmpty())
        axNode->setValuetext(axObject->valueDescription());
    if (!axObject->roleDescription().isEmpty())
        axNode->setRoledescription(axObject->roleDescription());
    if (!axObject->isEnabled())
        axNode->setDisabled(!axObject->isEnabled());
    if (axObject->supportsExpanded())
        axNode->setExpanded(axObject->isExpanded());
    if (axObject->isFocused())
        axNode->setFocused(axObject->isFocused());
    if (axObject->isModalNode())
        axNode->setModal(axObject->isModalNode());
    bool multiline = axObject->ariaIsMultiline() || axObject->roleValue() == AccessibilityRole::TextArea;
    if (multiline)
        axNode->setMultiline(multiline);
    if (axObject->isMultiSelectable())
        axNode->setMultiselectable(axObject->isMultiSelectable());
    if (axObject->supportsReadOnly() && !axObject->canSetValueAttribute() && axObject->isEnabled())
        axNode->setReadonly(true);
    if (axObject->supportsRequiredAttribute())
        axNode->setRequired(axObject->isRequired());
    if (axObject->isSelected())
        axNode->setSelected(axObject->isSelected());
    if (axObject->supportsChecked()) {
        AccessibilityButtonState checkedState = axObject->checkboxOrRadioValue();
        switch (checkedState) {
            case AccessibilityButtonState::On:
                axNode->setChecked(Inspector::Protocol::Page::AXNode::Checked::True);
                break;
            case AccessibilityButtonState::Off:
                axNode->setChecked(Inspector::Protocol::Page::AXNode::Checked::False);
                break;
            case AccessibilityButtonState::Mixed:
                axNode->setChecked(Inspector::Protocol::Page::AXNode::Checked::Mixed);
                break;
        }
    }
    if (axObject->supportsPressed()) {
        AccessibilityButtonState checkedState = axObject->checkboxOrRadioValue();
        switch (checkedState) {
            case AccessibilityButtonState::On:
                axNode->setPressed(Inspector::Protocol::Page::AXNode::Pressed::True);
                break;
            case AccessibilityButtonState::Off:
                axNode->setPressed(Inspector::Protocol::Page::AXNode::Pressed::False);
                break;
            case AccessibilityButtonState::Mixed:
                axNode->setPressed(Inspector::Protocol::Page::AXNode::Pressed::Mixed);
                break;
        }
    }
    unsigned level = axObject->hierarchicalLevel() ? axObject->hierarchicalLevel() : axObject->headingLevel();
    if (level)
        axNode->setLevel(level);
    if (axObject->minValueForRange() != 0)
        axNode->setValuemin(axObject->minValueForRange());
    if (axObject->maxValueForRange() != 0)
        axNode->setValuemax(axObject->maxValueForRange());
    if (axObject->supportsAutoComplete())
        axNode->setAutocomplete(axObject->autoCompleteValue());
    if (axObject->hasPopup())
        axNode->setHaspopup(axObject->popupValue());

    String invalidValue = axObject->invalidStatus();
    if (invalidValue != "false") {
        if (invalidValue == "grammar")
            axNode->setInvalid(Inspector::Protocol::Page::AXNode::Invalid::Grammar);
        else if (invalidValue == "spelling")
            axNode->setInvalid(Inspector::Protocol::Page::AXNode::Invalid::Spelling);
        else // Future versions of ARIA may allow additional truthy values. Ex. format, order, or size.
            axNode->setInvalid(Inspector::Protocol::Page::AXNode::Invalid::True);
    }
    switch (axObject->orientation()) {
        case AccessibilityOrientation::Undefined:
            break;
        case AccessibilityOrientation::Vertical:
            axNode->setOrientation("vertical"_s);
            break;
        case AccessibilityOrientation::Horizontal:
            axNode->setOrientation("horizontal"_s);
            break;
    }

    if (axObject->isKeyboardFocusable())
        axNode->setFocusable(axObject->isKeyboardFocusable());

    if (nodeToFind && axObject->node() == nodeToFind)
        axNode->setFound(true);

    if (axObject->hasChildren()) {
        Ref<JSON::ArrayOf<Inspector::Protocol::Page::AXNode>> children = JSON::ArrayOf<Inspector::Protocol::Page::AXNode>::create();
        for (auto& childObject : axObject->children())
            children->addItem(snapshotForAXObject(childObject, nodeToFind));
        axNode->setChildren(WTFMove(children));
    }
    return axNode;
}


Protocol::ErrorStringOr<Ref<Protocol::Page::AXNode>> InspectorPageAgent::accessibilitySnapshot(const String& objectId)
{
    if (!WebCore::AXObjectCache::accessibilityEnabled())
        WebCore::AXObjectCache::enableAccessibility();
    RefPtr document = m_inspectedPage.mainFrame().document();
    if (!document)
        return makeUnexpected("No document for main frame"_s);

    AXObjectCache* axObjectCache = document->axObjectCache();
    if (!axObjectCache)
        return makeUnexpected("No AXObjectCache for main document"_s);

    AXCoreObject* axObject = axObjectCache->rootObject();
    if (!axObject)
        return makeUnexpected("No AXObject for main document"_s);

    Node* node = nullptr;
    if (!objectId.isEmpty()) {
        InspectorDOMAgent* domAgent = m_instrumentingAgents.persistentDOMAgent();
        ASSERT(domAgent);
        node = domAgent->nodeForObjectId(objectId);
        if (!node)
            return makeUnexpected("No Node for objectId"_s);
    }

    m_doingAccessibilitySnapshot = true;
    Ref<Inspector::Protocol::Page::AXNode> axNode = snapshotForAXObject(RefPtr { axObject }, node);
    m_doingAccessibilitySnapshot = false;
    return axNode;
}

Protocol::ErrorStringOr<void> InspectorPageAgent::setInterceptFileChooserDialog(bool enabled)
{
    m_interceptFileChooserDialog = enabled;
    return { };
}

Protocol::ErrorStringOr<void> InspectorPageAgent::setDefaultBackgroundColorOverride(RefPtr<JSON::Object>&& color)
{
    FrameView* view = m_inspectedPage.mainFrame().view();
    if (!view)
        return makeUnexpected("Internal error: No frame view to set color two"_s);

    if (!color) {
        view->updateBackgroundRecursively(std::optional<Color>());
        return { };
    }

    view->updateBackgroundRecursively(InspectorDOMAgent::parseColor(WTFMove(color)));
    return { };
}

Protocol::ErrorStringOr<void> InspectorPageAgent::createUserWorld(const String& name)
{
    if (createdUserWorlds().contains(name))
        return makeUnexpected("World with the given name already exists"_s);

    Ref<DOMWrapperWorld> world = ScriptController::createWorld(name, ScriptController::WorldType::User);
    ensureUserWorldsExistInAllFrames({world.ptr()});
    createdUserWorlds().set(name, WTFMove(world));
    return { };
}

void InspectorPageAgent::ensureUserWorldsExistInAllFrames(const Vector<DOMWrapperWorld*>& worlds)
{
    for (Frame* frame = &m_inspectedPage.mainFrame(); frame; frame = frame->tree().traverseNext()) {
        for (auto* world : worlds)
            frame->windowProxy().jsWindowProxy(*world)->window();
    }
}

Protocol::ErrorStringOr<void> InspectorPageAgent::setBypassCSP(bool enabled)
{
    m_bypassCSP = enabled;
    return { };
}

Protocol::ErrorStringOr<void> InspectorPageAgent::crash()
{
    CRASH();
    return { };
}

Protocol::ErrorStringOr<void> InspectorPageAgent::setOrientationOverride(std::optional<int>&& angle)
{
#if ENABLE(ORIENTATION_EVENTS)
    m_inspectedPage.setOverrideOrientation(WTFMove(angle));
    return { };
#else
    UNUSED_PARAM(angle);
    return makeUnexpected("Orientation events are disabled in this build");
#endif
}

static std::optional<FloatBoxExtent> parseInsets(RefPtr<JSON::Object>&& insets)
{
    std::optional<double> top = insets->getDouble("top");
    std::optional<double> right = insets->getDouble("right");
    std::optional<double> bottom = insets->getDouble("bottom");
    std::optional<double> left = insets->getDouble("left");
    if (top && right && bottom && left)
        return FloatBoxExtent(static_cast<float>(*top), static_cast<float>(*right), static_cast<float>(*bottom), static_cast<float>(*left));
    return std::optional<FloatBoxExtent>();
}

static std::optional<FloatRect> parseRect(RefPtr<JSON::Object>&& insets)
{
    std::optional<double> x = insets->getDouble("x");
    std::optional<double> y = insets->getDouble("y");
    std::optional<double> width = insets->getDouble("width");
    std::optional<double> height = insets->getDouble("height");
    if (x && y && width && height)
        return FloatRect(static_cast<float>(*x), static_cast<float>(*y), static_cast<float>(*width), static_cast<float>(*height));
    return std::optional<FloatRect>();
}

Protocol::ErrorStringOr<void> InspectorPageAgent::setVisibleContentRects(RefPtr<JSON::Object>&& unobscuredContentRect, RefPtr<JSON::Object>&& contentInsets, RefPtr<JSON::Object>&& obscuredInsets, RefPtr<JSON::Object>&& unobscuredInsets)
{
    FrameView* view = m_inspectedPage.mainFrame().view();
    if (!view)
        return makeUnexpected("Internal error: No frame view to set content rects for"_s);

    if (unobscuredContentRect) {
        std::optional<FloatRect> ucr = parseRect(WTFMove(unobscuredContentRect));
        if (!ucr)
            return makeUnexpected("Invalid unobscured content rect");

        view->setUnobscuredContentSize(FloatSize(ucr->width(), ucr->height()));
    }

    if (contentInsets) {
        std::optional<FloatBoxExtent> ci = parseInsets(WTFMove(contentInsets));
        if (!ci)
            return makeUnexpected("Invalid content insets");

        m_inspectedPage.setContentInsets(*ci);
    }

    if (obscuredInsets) {
        std::optional<FloatBoxExtent> oi = parseInsets(WTFMove(obscuredInsets));
        if (!oi)
            return makeUnexpected("Invalid obscured insets");

        m_inspectedPage.setObscuredInsets(*oi);
    }

    if (unobscuredInsets) {
        std::optional<FloatBoxExtent> ui = parseInsets(WTFMove(unobscuredInsets));
        if (!ui)
            return makeUnexpected("Invalid unobscured insets");

        m_inspectedPage.setUnobscuredSafeAreaInsets(*ui);
    }
    return {};
}

Protocol::ErrorStringOr<void> InspectorPageAgent::updateScrollingState()
{
    auto* scrollingCoordinator = m_inspectedPage.scrollingCoordinator();
    if (!scrollingCoordinator)
        return {};
    scrollingCoordinator->commitTreeStateIfNeeded();
    return {};
}

} // namespace WebCore
