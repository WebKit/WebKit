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
#include "CustomHeaderFields.h"
#include "DOMWrapperWorld.h"
#include "Document.h"
#include "DocumentLoader.h"
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
#include "TextEncoding.h"
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

static bool decodeBuffer(const char* buffer, unsigned size, const String& textEncodingName, String* result)
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

bool InspectorPageAgent::dataContent(const char* data, unsigned size, const String& textEncodingName, bool withBase64Encode, String* result)
{
    if (withBase64Encode) {
        *result = base64Encode(data, size);
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
#if ENABLE(SVG_FONTS)
        case CachedResource::Type::SVGFontResource:
#endif
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

void InspectorPageAgent::resourceContent(ErrorString& errorString, Frame* frame, const URL& url, String* result, bool* base64Encoded)
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

Inspector::Protocol::Page::ResourceType InspectorPageAgent::resourceTypeJSON(InspectorPageAgent::ResourceType resourceType)
{
    switch (resourceType) {
    case DocumentResource:
        return Inspector::Protocol::Page::ResourceType::Document;
    case ImageResource:
        return Inspector::Protocol::Page::ResourceType::Image;
    case FontResource:
        return Inspector::Protocol::Page::ResourceType::Font;
    case StyleSheetResource:
        return Inspector::Protocol::Page::ResourceType::StyleSheet;
    case ScriptResource:
        return Inspector::Protocol::Page::ResourceType::Script;
    case XHRResource:
        return Inspector::Protocol::Page::ResourceType::XHR;
    case FetchResource:
        return Inspector::Protocol::Page::ResourceType::Fetch;
    case PingResource:
        return Inspector::Protocol::Page::ResourceType::Ping;
    case BeaconResource:
        return Inspector::Protocol::Page::ResourceType::Beacon;
    case WebSocketResource:
        return Inspector::Protocol::Page::ResourceType::WebSocket;
    case OtherResource:
        return Inspector::Protocol::Page::ResourceType::Other;
#if ENABLE(APPLICATION_MANIFEST)
    case ApplicationManifestResource:
        break;
#endif
    }
    return Inspector::Protocol::Page::ResourceType::Other;
}

InspectorPageAgent::ResourceType InspectorPageAgent::inspectorResourceType(CachedResource::Type type)
{
    switch (type) {
    case CachedResource::Type::ImageResource:
        return InspectorPageAgent::ImageResource;
#if ENABLE(SVG_FONTS)
    case CachedResource::Type::SVGFontResource:
#endif
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

Inspector::Protocol::Page::ResourceType InspectorPageAgent::cachedResourceTypeJSON(const CachedResource& cachedResource)
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

DocumentLoader* InspectorPageAgent::assertDocumentLoader(ErrorString& errorString, Frame* frame)
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
    ErrorString unused;
    disable(unused);
}

void InspectorPageAgent::enable(ErrorString& errorString)
{
    if (m_instrumentingAgents.enabledPageAgent() == this) {
        errorString = "Page domain already enabled"_s;
        return;
    }

    m_instrumentingAgents.setEnabledPageAgent(this);

    auto& stopwatch = m_environment.executionStopwatch();
    stopwatch.reset();
    stopwatch.start();

#if ENABLE(DARK_MODE_CSS) || HAVE(OS_DARK_MODE_SUPPORT)
    defaultAppearanceDidChange(m_inspectedPage.defaultUseDarkAppearance());
#endif
}

void InspectorPageAgent::disable(ErrorString&)
{
    m_instrumentingAgents.setEnabledPageAgent(nullptr);

    ErrorString unused;
    setShowPaintRects(unused, false);
#if !PLATFORM(IOS_FAMILY)
    setShowRulers(unused, false);
#endif
    overrideUserAgent(unused, nullptr);
    setEmulatedMedia(unused, emptyString());
#if ENABLE(DARK_MODE_CSS) || HAVE(OS_DARK_MODE_SUPPORT)
    setForcedAppearance(unused, emptyString());
#endif

    auto& inspectedPageSettings = m_inspectedPage.settings();
    inspectedPageSettings.setAuthorAndUserStylesEnabledInspectorOverride(WTF::nullopt);
    inspectedPageSettings.setICECandidateFilteringEnabledInspectorOverride(WTF::nullopt);
    inspectedPageSettings.setImagesEnabledInspectorOverride(WTF::nullopt);
    inspectedPageSettings.setMediaCaptureRequiresSecureConnectionInspectorOverride(WTF::nullopt);
    inspectedPageSettings.setMockCaptureDevicesEnabledInspectorOverride(WTF::nullopt);
    inspectedPageSettings.setNeedsSiteSpecificQuirksInspectorOverride(WTF::nullopt);
    inspectedPageSettings.setScriptEnabledInspectorOverride(WTF::nullopt);
    inspectedPageSettings.setShowDebugBordersInspectorOverride(WTF::nullopt);
    inspectedPageSettings.setShowRepaintCounterInspectorOverride(WTF::nullopt);
    inspectedPageSettings.setWebRTCEncryptionEnabledInspectorOverride(WTF::nullopt);
    inspectedPageSettings.setWebSecurityEnabledInspectorOverride(WTF::nullopt);

    m_client->setDeveloperPreferenceOverride(InspectorClient::DeveloperPreference::AdClickAttributionDebugModeEnabled, WTF::nullopt);
    m_client->setDeveloperPreferenceOverride(InspectorClient::DeveloperPreference::ITPDebugModeEnabled, WTF::nullopt);
    m_client->setDeveloperPreferenceOverride(InspectorClient::DeveloperPreference::MockCaptureDevicesEnabled, WTF::nullopt);
}

double InspectorPageAgent::timestamp()
{
    return m_environment.executionStopwatch().elapsedTime().seconds();
}

void InspectorPageAgent::reload(ErrorString&, const bool* optionalReloadFromOrigin, const bool* optionalRevalidateAllResources)
{
    bool reloadFromOrigin = optionalReloadFromOrigin && *optionalReloadFromOrigin;
    bool revalidateAllResources = optionalRevalidateAllResources && *optionalRevalidateAllResources;

    OptionSet<ReloadOption> reloadOptions;
    if (reloadFromOrigin)
        reloadOptions.add(ReloadOption::FromOrigin);
    if (!revalidateAllResources)
        reloadOptions.add(ReloadOption::ExpiredOnly);

    m_inspectedPage.mainFrame().loader().reload(reloadOptions);
}

void InspectorPageAgent::navigate(ErrorString&, const String& url)
{
    UserGestureIndicator indicator { ProcessingUserGesture };
    Frame& frame = m_inspectedPage.mainFrame();

    ResourceRequest resourceRequest { frame.document()->completeURL(url) };
    FrameLoadRequest frameLoadRequest { *frame.document(), frame.document()->securityOrigin(), WTFMove(resourceRequest), "_self"_s, InitiatedByMainFrame::Unknown };
    frameLoadRequest.disableNavigationToInvalidURL();
    frame.loader().changeLocation(WTFMove(frameLoadRequest));
}

void InspectorPageAgent::overrideUserAgent(ErrorString&, const String* value)
{
    m_userAgentOverride = value ? *value : String();
}

static inline Optional<bool> asOptionalBool(const bool* value)
{
    if (!value)
        return WTF::nullopt;
    return *value;
}

void InspectorPageAgent::overrideSetting(ErrorString& errorString, const String& settingString, const bool* value)
{
    if (settingString.isEmpty()) {
        errorString = "settingString is empty"_s;
        return;
    }

    auto setting = Inspector::Protocol::InspectorHelpers::parseEnumValueFromString<Inspector::Protocol::Page::Setting>(settingString);
    if (!setting) {
        errorString = makeString("Unknown settingString: "_s, settingString);
        return;
    }

    auto& inspectedPageSettings = m_inspectedPage.settings();

    auto overrideValue = asOptionalBool(value);
    switch (setting.value()) {
    case Inspector::Protocol::Page::Setting::AdClickAttributionDebugModeEnabled:
        m_client->setDeveloperPreferenceOverride(InspectorClient::DeveloperPreference::AdClickAttributionDebugModeEnabled, overrideValue);
        return;

    case Inspector::Protocol::Page::Setting::AuthorAndUserStylesEnabled:
        inspectedPageSettings.setAuthorAndUserStylesEnabledInspectorOverride(overrideValue);
        return;

    case Inspector::Protocol::Page::Setting::ICECandidateFilteringEnabled:
        inspectedPageSettings.setICECandidateFilteringEnabledInspectorOverride(overrideValue);
        return;

    case Inspector::Protocol::Page::Setting::ITPDebugModeEnabled:
        m_client->setDeveloperPreferenceOverride(InspectorClient::DeveloperPreference::ITPDebugModeEnabled, overrideValue);
        return;

    case Inspector::Protocol::Page::Setting::ImagesEnabled:
        inspectedPageSettings.setImagesEnabledInspectorOverride(overrideValue);
        return;

    case Inspector::Protocol::Page::Setting::MediaCaptureRequiresSecureConnection:
        inspectedPageSettings.setMediaCaptureRequiresSecureConnectionInspectorOverride(overrideValue);
        return;

    case Inspector::Protocol::Page::Setting::MockCaptureDevicesEnabled:
        inspectedPageSettings.setMockCaptureDevicesEnabledInspectorOverride(overrideValue);
        m_client->setDeveloperPreferenceOverride(InspectorClient::DeveloperPreference::MockCaptureDevicesEnabled, overrideValue);
        return;

    case Inspector::Protocol::Page::Setting::NeedsSiteSpecificQuirks:
        inspectedPageSettings.setNeedsSiteSpecificQuirksInspectorOverride(overrideValue);
        return;

    case Inspector::Protocol::Page::Setting::ScriptEnabled:
        inspectedPageSettings.setScriptEnabledInspectorOverride(overrideValue);
        return;

    case Inspector::Protocol::Page::Setting::ShowDebugBorders:
        inspectedPageSettings.setShowDebugBordersInspectorOverride(overrideValue);
        return;

    case Inspector::Protocol::Page::Setting::ShowRepaintCounter:
        inspectedPageSettings.setShowRepaintCounterInspectorOverride(overrideValue);
        return;

    case Inspector::Protocol::Page::Setting::WebRTCEncryptionEnabled:
        inspectedPageSettings.setWebRTCEncryptionEnabledInspectorOverride(overrideValue);
        return;

    case Inspector::Protocol::Page::Setting::WebSecurityEnabled:
        inspectedPageSettings.setWebSecurityEnabledInspectorOverride(overrideValue);
        return;
    }

    ASSERT_NOT_REACHED();
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
    return Inspector::Protocol::Page::Cookie::create()
        .setName(cookie.name)
        .setValue(cookie.value)
        .setDomain(cookie.domain)
        .setPath(cookie.path)
        .setExpires(cookie.expires.valueOr(0))
        .setSession(cookie.session)
        .setHttpOnly(cookie.httpOnly)
        .setSecure(cookie.secure)
        .setSameSite(cookieSameSitePolicyJSON(cookie.sameSite))
        .release();
}

static Ref<JSON::ArrayOf<Inspector::Protocol::Page::Cookie>> buildArrayForCookies(ListHashSet<Cookie>& cookiesList)
{
    auto cookies = JSON::ArrayOf<Inspector::Protocol::Page::Cookie>::create();

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

void InspectorPageAgent::getCookies(ErrorString&, RefPtr<JSON::ArrayOf<Inspector::Protocol::Page::Cookie>>& cookies)
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

    cookies = buildArrayForCookies(allRawCookies);
}

static Optional<Cookie> parseCookieObject(ErrorString& errorString, const JSON::Object& cookieObject)
{
    Cookie cookie;

    if (!cookieObject.getString("name"_s, cookie.name)) {
        errorString = "Invalid value for key name in given cookie";
        return WTF::nullopt;
    }

    if (!cookieObject.getString("value"_s, cookie.value)) {
        errorString = "Invalid value for key value in given cookie";
        return WTF::nullopt;
    }

    if (!cookieObject.getString("domain"_s, cookie.domain)) {
        errorString = "Invalid value for key domain in given cookie";
        return WTF::nullopt;
    }

    if (!cookieObject.getString("path"_s, cookie.path)) {
        errorString = "Invalid value for key path in given cookie";
        return WTF::nullopt;
    }

    if (!cookieObject.getBoolean("httpOnly"_s, cookie.httpOnly)) {
        errorString = "Invalid value for key httpOnly in given cookie";
        return WTF::nullopt;
    }

    if (!cookieObject.getBoolean("secure"_s, cookie.secure)) {
        errorString = "Invalid value for key secure in given cookie";
        return WTF::nullopt;
    }

    bool validSession = cookieObject.getBoolean("session"_s, cookie.session);
    cookie.expires = cookieObject.getNumber<double>("expires"_s);
    if (!validSession && !cookie.expires) {
        errorString = "Invalid value for key expires in given cookie";
        return WTF::nullopt;
    }

    String sameSiteString;
    if (!cookieObject.getString("sameSite"_s, sameSiteString)) {
        errorString = "Invalid value for key sameSite in given cookie";
        return WTF::nullopt;
    }

    auto sameSite = Inspector::Protocol::InspectorHelpers::parseEnumValueFromString<Inspector::Protocol::Page::CookieSameSitePolicy>(sameSiteString);
    if (!sameSite) {
        errorString = "Invalid value for key sameSite in given cookie";
        return WTF::nullopt;
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

    return cookie;
}

void InspectorPageAgent::setCookie(ErrorString& errorString, const JSON::Object& cookieObject)
{
    auto cookie = parseCookieObject(errorString, cookieObject);
    if (!cookie)
        return;

    for (auto* frame = &m_inspectedPage.mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (auto* document = frame->document()) {
            if (auto* page = document->page())
                page->cookieJar().setRawCookie(*document, cookie.value());
        }
    }
}

void InspectorPageAgent::deleteCookie(ErrorString&, const String& cookieName, const String& url)
{
    URL parsedURL({ }, url);
    for (Frame* frame = &m_inspectedPage.mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (auto* document = frame->document()) {
            if (auto* page = document->page())
                page->cookieJar().deleteCookie(*document, parsedURL, cookieName);
        }
    }
}

void InspectorPageAgent::getResourceTree(ErrorString&, RefPtr<Inspector::Protocol::Page::FrameResourceTree>& object)
{
    object = buildObjectForFrameTree(&m_inspectedPage.mainFrame());
}

void InspectorPageAgent::getResourceContent(ErrorString& errorString, const String& frameId, const String& url, String* content, bool* base64Encoded)
{
    Frame* frame = assertFrame(errorString, frameId);
    if (!frame)
        return;

    resourceContent(errorString, frame, URL({ }, url), content, base64Encoded);
}

void InspectorPageAgent::setBootstrapScript(ErrorString&, const String* optionalSource)
{
    m_bootstrapScript = optionalSource ? *optionalSource : nullString();
}

void InspectorPageAgent::searchInResource(ErrorString& errorString, const String& frameId, const String& url, const String& query, const bool* optionalCaseSensitive, const bool* optionalIsRegex, const String* optionalRequestId, RefPtr<JSON::ArrayOf<Inspector::Protocol::GenericTypes::SearchMatch>>& results)
{
    results = JSON::ArrayOf<Inspector::Protocol::GenericTypes::SearchMatch>::create();

    bool isRegex = optionalIsRegex ? *optionalIsRegex : false;
    bool caseSensitive = optionalCaseSensitive ? *optionalCaseSensitive : false;

    if (optionalRequestId) {
        if (auto* networkAgent = m_instrumentingAgents.enabledNetworkAgent()) {
            networkAgent->searchInRequest(errorString, *optionalRequestId, query, caseSensitive, isRegex, results);
            return;
        }
    }

    Frame* frame = assertFrame(errorString, frameId);
    if (!frame)
        return;

    DocumentLoader* loader = assertDocumentLoader(errorString, frame);
    if (!loader)
        return;

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
        return;

    results = ContentSearchUtilities::searchInTextByLines(content, query, caseSensitive, isRegex);
}

static Ref<Inspector::Protocol::Page::SearchResult> buildObjectForSearchResult(const String& frameId, const String& url, int matchesCount)
{
    return Inspector::Protocol::Page::SearchResult::create()
        .setUrl(url)
        .setFrameId(frameId)
        .setMatchesCount(matchesCount)
        .release();
}

void InspectorPageAgent::searchInResources(ErrorString&, const String& text, const bool* optionalCaseSensitive, const bool* optionalIsRegex, RefPtr<JSON::ArrayOf<Inspector::Protocol::Page::SearchResult>>& result)
{
    result = JSON::ArrayOf<Inspector::Protocol::Page::SearchResult>::create();

    bool caseSensitive = optionalCaseSensitive ? *optionalCaseSensitive : false;
    bool isRegex = optionalIsRegex ? *optionalIsRegex : false;

    auto searchStringType = isRegex ? ContentSearchUtilities::SearchStringType::Regex : ContentSearchUtilities::SearchStringType::ContainsString;
    auto regex = ContentSearchUtilities::createRegularExpressionForSearchString(text, caseSensitive, searchStringType);

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
}

#if !PLATFORM(IOS_FAMILY)
void InspectorPageAgent::setShowRulers(ErrorString&, bool showRulers)
{
    m_overlay->setShowRulers(showRulers);
}
#endif

void InspectorPageAgent::setShowPaintRects(ErrorString&, bool show)
{
    m_showPaintRects = show;
    m_client->setShowPaintRects(show);

    if (m_client->overridesShowPaintRects())
        return;

    m_overlay->setShowPaintRects(show);
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

Frame* InspectorPageAgent::frameForId(const String& frameId)
{
    return frameId.isEmpty() ? nullptr : m_identifierToFrame.get(frameId);
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

Frame* InspectorPageAgent::assertFrame(ErrorString& errorString, const String& frameId)
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

#if ENABLE(DARK_MODE_CSS) || HAVE(OS_DARK_MODE_SUPPORT)
void InspectorPageAgent::defaultAppearanceDidChange(bool useDarkAppearance)
{
    m_frontendDispatcher->defaultAppearanceDidChange(useDarkAppearance ? Inspector::Protocol::Page::Appearance::Dark : Inspector::Protocol::Page::Appearance::Light);
}
#endif

void InspectorPageAgent::didClearWindowObjectInWorld(Frame& frame, DOMWrapperWorld& world)
{
    if (&world != &mainThreadNormalWorld())
        return;

    if (m_bootstrapScript.isEmpty())
        return;

    frame.script().evaluateIgnoringException(ScriptSourceCode(m_bootstrapScript, URL { URL(), "web-inspector://bootstrap.js"_s }));
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

Ref<Inspector::Protocol::Page::Frame> InspectorPageAgent::buildObjectForFrame(Frame* frame)
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
        frameObject->setParentId(frameId(frame->tree().parent()));
    if (frame->ownerElement()) {
        String name = frame->ownerElement()->getNameAttribute();
        if (name.isEmpty())
            name = frame->ownerElement()->attributeWithoutSynchronization(HTMLNames::idAttr);
        frameObject->setName(name);
    }

    return frameObject;
}

Ref<Inspector::Protocol::Page::FrameResourceTree> InspectorPageAgent::buildObjectForFrameTree(Frame* frame)
{
    ASSERT_ARG(frame, frame);

    Ref<Inspector::Protocol::Page::Frame> frameObject = buildObjectForFrame(frame);
    auto subresources = JSON::ArrayOf<Inspector::Protocol::Page::FrameResource>::create();
    auto result = Inspector::Protocol::Page::FrameResourceTree::create()
        .setFrame(WTFMove(frameObject))
        .setResources(subresources.copyRef())
        .release();

    for (auto* cachedResource : cachedResourcesForFrame(frame)) {
        auto resourceObject = Inspector::Protocol::Page::FrameResource::create()
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

    RefPtr<JSON::ArrayOf<Inspector::Protocol::Page::FrameResourceTree>> childrenArray;
    for (Frame* child = frame->tree().firstChild(); child; child = child->tree().nextSibling()) {
        if (!childrenArray) {
            childrenArray = JSON::ArrayOf<Inspector::Protocol::Page::FrameResourceTree>::create();
            result->setChildFrames(childrenArray);
        }
        childrenArray->addItem(buildObjectForFrameTree(child));
    }
    return result;
}

void InspectorPageAgent::setEmulatedMedia(ErrorString&, const String& media)
{
    if (media == m_emulatedMedia)
        return;

    m_emulatedMedia = media;

    // FIXME: Schedule a rendering update instead of synchronously updating the layout.
    m_inspectedPage.updateStyleAfterChangeInEnvironment();

    auto document = makeRefPtr(m_inspectedPage.mainFrame().document());
    if (!document)
        return;

    document->updateLayout();
    document->evaluateMediaQueriesAndReportChanges();
}

#if ENABLE(DARK_MODE_CSS) || HAVE(OS_DARK_MODE_SUPPORT)
void InspectorPageAgent::setForcedAppearance(ErrorString&, const String& appearance)
{
    if (appearance == "Light"_s)
        m_inspectedPage.setUseDarkAppearanceOverride(false);
    else if (appearance == "Dark"_s)
        m_inspectedPage.setUseDarkAppearanceOverride(true);
    else
        m_inspectedPage.setUseDarkAppearanceOverride(WTF::nullopt);
}
#endif

void InspectorPageAgent::applyUserAgentOverride(String& userAgent)
{
    if (!m_userAgentOverride.isEmpty())
        userAgent = m_userAgentOverride;
}

void InspectorPageAgent::applyEmulatedMedia(String& media)
{
    if (!m_emulatedMedia.isEmpty())
        media = m_emulatedMedia;
}

void InspectorPageAgent::snapshotNode(ErrorString& errorString, int nodeId, String* outDataURL)
{
    InspectorDOMAgent* domAgent = m_instrumentingAgents.persistentDOMAgent();
    ASSERT(domAgent);
    Node* node = domAgent->assertNode(errorString, nodeId);
    if (!node)
        return;

    std::unique_ptr<ImageBuffer> snapshot = WebCore::snapshotNode(m_inspectedPage.mainFrame(), *node);
    if (!snapshot) {
        errorString = "Could not capture snapshot"_s;
        return;
    }

    *outDataURL = snapshot->toDataURL("image/png"_s, WTF::nullopt, PreserveResolution::Yes);
}

void InspectorPageAgent::snapshotRect(ErrorString& errorString, int x, int y, int width, int height, const String& coordinateSystem, String* outDataURL)
{
    SnapshotOptions options = SnapshotOptionsNone;
    if (coordinateSystem == "Viewport")
        options |= SnapshotOptionsInViewCoordinates;

    IntRect rectangle(x, y, width, height);
    std::unique_ptr<ImageBuffer> snapshot = snapshotFrameRect(m_inspectedPage.mainFrame(), rectangle, options);

    if (!snapshot) {
        errorString = "Could not capture snapshot"_s;
        return;
    }

    *outDataURL = snapshot->toDataURL("image/png"_s, WTF::nullopt, PreserveResolution::Yes);
}

#if ENABLE(WEB_ARCHIVE) && USE(CF)
void InspectorPageAgent::archive(ErrorString& errorString, String* data)
{
    auto archive = LegacyWebArchive::create(m_inspectedPage.mainFrame());
    if (!archive) {
        errorString = "Could not create web archive for main frame"_s;
        return;
    }

    RetainPtr<CFDataRef> buffer = archive->rawDataRepresentation();
    *data = base64Encode(CFDataGetBytePtr(buffer.get()), CFDataGetLength(buffer.get()));
}
#endif

} // namespace WebCore
