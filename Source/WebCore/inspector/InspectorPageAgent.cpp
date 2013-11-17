/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#if ENABLE(INSPECTOR)

#include "InspectorPageAgent.h"

#include "CachedCSSStyleSheet.h"
#include "CachedFont.h"
#include "CachedImage.h"
#include "CachedResource.h"
#include "CachedResourceLoader.h"
#include "CachedScript.h"
#include "ContentSearchUtils.h"
#include "Cookie.h"
#include "CookieJar.h"
#include "DOMImplementation.h"
#include "DOMPatchSupport.h"
#include "DOMWrapperWorld.h"
#include "DeviceOrientationController.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameView.h"
#include "GeolocationController.h"
#include "GeolocationError.h"
#include "HTMLFrameOwnerElement.h"
#include "HTMLNames.h"
#include "IdentifiersFactory.h"
#include "InjectedScriptManager.h"
#include "InspectorAgent.h"
#include "InspectorClient.h"
#include "InspectorFrontend.h"
#include "InspectorInstrumentation.h"
#include "InspectorOverlay.h"
#include "InspectorValues.h"
#include "InstrumentingAgents.h"
#include "MainFrame.h"
#include "MemoryCache.h"
#include "Page.h"
#include "RegularExpression.h"
#include "ResourceBuffer.h"
#include "ScriptController.h"
#include "ScriptObject.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "TextEncoding.h"
#include "TextResourceDecoder.h"
#include "UserGestureIndicator.h"
#include <wtf/CurrentTime.h>
#include <wtf/ListHashSet.h>
#include <wtf/Vector.h>
#include <wtf/text/Base64.h>
#include <wtf/text/StringBuilder.h>

#if ENABLE(WEB_ARCHIVE) && USE(CF)
#include "LegacyWebArchive.h"
#endif

namespace WebCore {

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

static bool prepareCachedResourceBuffer(CachedResource* cachedResource, bool* hasZeroSize)
{
    *hasZeroSize = false;
    if (!cachedResource)
        return false;

    // Zero-sized resources don't have data at all -- so fake the empty buffer, instead of indicating error by returning 0.
    if (!cachedResource->encodedSize()) {
        *hasZeroSize = true;
        return true;
    }

    if (cachedResource->isPurgeable()) {
        // If the resource is purgeable then make it unpurgeable to get
        // get its data. This might fail, in which case we return an
        // empty String.
        // FIXME: should we do something else in the case of a purged
        // resource that informs the user why there is no data in the
        // inspector?
        if (!cachedResource->makePurgeable(false))
            return false;
    }

    return true;
}

static bool hasTextContent(CachedResource* cachedResource)
{
    InspectorPageAgent::ResourceType type = InspectorPageAgent::cachedResourceType(*cachedResource);
    return type == InspectorPageAgent::DocumentResource || type == InspectorPageAgent::StylesheetResource || type == InspectorPageAgent::ScriptResource || type == InspectorPageAgent::XHRResource;
}

static PassRefPtr<TextResourceDecoder> createXHRTextDecoder(const String& mimeType, const String& textEncodingName)
{
    RefPtr<TextResourceDecoder> decoder;
    if (!textEncodingName.isEmpty())
        decoder = TextResourceDecoder::create("text/plain", textEncodingName);
    else if (DOMImplementation::isXMLMIMEType(mimeType.lower())) {
        decoder = TextResourceDecoder::create("application/xml");
        decoder->useLenientXMLDecoding();
    } else if (equalIgnoringCase(mimeType, "text/html"))
        decoder = TextResourceDecoder::create("text/html", "UTF-8");
    else
        decoder = TextResourceDecoder::create("text/plain", "UTF-8");
    return decoder;
}

bool InspectorPageAgent::cachedResourceContent(CachedResource* cachedResource, String* result, bool* base64Encoded)
{
    bool hasZeroSize;
    bool prepared = prepareCachedResourceBuffer(cachedResource, &hasZeroSize);
    if (!prepared)
        return false;

    *base64Encoded = !hasTextContent(cachedResource);
    if (*base64Encoded) {
        RefPtr<SharedBuffer> buffer = hasZeroSize ? SharedBuffer::create() : cachedResource->resourceBuffer()->sharedBuffer();

        if (!buffer)
            return false;

        *result = base64Encode(buffer->data(), buffer->size());
        return true;
    }

    if (hasZeroSize) {
        *result = "";
        return true;
    }

    if (cachedResource) {
        switch (cachedResource->type()) {
        case CachedResource::CSSStyleSheet:
            *result = static_cast<CachedCSSStyleSheet*>(cachedResource)->sheetText(false);
            return true;
        case CachedResource::Script:
            *result = static_cast<CachedScript*>(cachedResource)->script();
            return true;
        case CachedResource::RawResource: {
            ResourceBuffer* buffer = cachedResource->resourceBuffer();
            if (!buffer)
                return false;
            RefPtr<TextResourceDecoder> decoder = createXHRTextDecoder(cachedResource->response().mimeType(), cachedResource->response().textEncodingName());
            // We show content for raw resources only for certain mime types (text, html and xml). Otherwise decoder will be null.
            if (!decoder)
                return false;
            String content = decoder->decode(buffer->data(), buffer->size());
            *result = content + decoder->flush();
            return true;
        }
        default:
            ResourceBuffer* buffer = cachedResource->resourceBuffer();
            return decodeBuffer(buffer ? buffer->data() : 0, buffer ? buffer->size() : 0, cachedResource->encoding(), result);
        }
    }
    return false;
}

bool InspectorPageAgent::mainResourceContent(Frame* frame, bool withBase64Encode, String* result)
{
    RefPtr<ResourceBuffer> buffer = frame->loader().documentLoader()->mainResourceData();
    if (!buffer)
        return false;
    String textEncodingName = frame->document()->inputEncoding();

    return InspectorPageAgent::dataContent(buffer->data(), buffer->size(), textEncodingName, withBase64Encode, result);
}

// static
bool InspectorPageAgent::sharedBufferContent(PassRefPtr<SharedBuffer> buffer, const String& textEncodingName, bool withBase64Encode, String* result)
{
    return dataContent(buffer ? buffer->data() : 0, buffer ? buffer->size() : 0, textEncodingName, withBase64Encode, result);
}

bool InspectorPageAgent::dataContent(const char* data, unsigned size, const String& textEncodingName, bool withBase64Encode, String* result)
{
    if (withBase64Encode) {
        *result = base64Encode(data, size);
        return true;
    }

    return decodeBuffer(data, size, textEncodingName, result);
}

PassOwnPtr<InspectorPageAgent> InspectorPageAgent::create(InstrumentingAgents* instrumentingAgents, Page* page, InspectorAgent* inspectorAgent, InjectedScriptManager* injectedScriptManager, InspectorClient* client, InspectorOverlay* overlay)
{
    return adoptPtr(new InspectorPageAgent(instrumentingAgents, page, inspectorAgent, injectedScriptManager, client, overlay));
}

// static
void InspectorPageAgent::resourceContent(ErrorString* errorString, Frame* frame, const URL& url, String* result, bool* base64Encoded)
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

    if (!success)
        success = cachedResourceContent(cachedResource(frame, url), result, base64Encoded);

    if (!success)
        *errorString = "No resource with given URL found";
}

//static
String InspectorPageAgent::sourceMapURLForResource(CachedResource* cachedResource)
{
    DEFINE_STATIC_LOCAL(String, sourceMapHTTPHeader, (ASCIILiteral("SourceMap")));
    DEFINE_STATIC_LOCAL(String, sourceMapHTTPHeaderDeprecated, (ASCIILiteral("X-SourceMap")));

    if (!cachedResource)
        return String();

    // Scripts are handled in a separate path.
    if (cachedResource->type() != CachedResource::CSSStyleSheet)
        return String();

    String sourceMapHeader = cachedResource->response().httpHeaderField(sourceMapHTTPHeader);
    if (!sourceMapHeader.isEmpty())
        return sourceMapHeader;

    sourceMapHeader = cachedResource->response().httpHeaderField(sourceMapHTTPHeaderDeprecated);
    if (!sourceMapHeader.isEmpty())
        return sourceMapHeader;

    String content;
    bool base64Encoded;
    if (InspectorPageAgent::cachedResourceContent(cachedResource, &content, &base64Encoded) && !base64Encoded)
        return ContentSearchUtils::findStylesheetSourceMapURL(content);

    return String();
}

CachedResource* InspectorPageAgent::cachedResource(Frame* frame, const URL& url)
{
    CachedResource* cachedResource = frame->document()->cachedResourceLoader()->cachedResource(url);
    if (!cachedResource) {
        ResourceRequest request(url);
#if ENABLE(CACHE_PARTITIONING)
        request.setCachePartition(frame->document()->topOrigin()->cachePartition());
#endif
        cachedResource = memoryCache()->resourceForRequest(request);
    }

    return cachedResource;
}

TypeBuilder::Page::ResourceType::Enum InspectorPageAgent::resourceTypeJson(InspectorPageAgent::ResourceType resourceType)
{
    switch (resourceType) {
    case DocumentResource:
        return TypeBuilder::Page::ResourceType::Document;
    case ImageResource:
        return TypeBuilder::Page::ResourceType::Image;
    case FontResource:
        return TypeBuilder::Page::ResourceType::Font;
    case StylesheetResource:
        return TypeBuilder::Page::ResourceType::Stylesheet;
    case ScriptResource:
        return TypeBuilder::Page::ResourceType::Script;
    case XHRResource:
        return TypeBuilder::Page::ResourceType::XHR;
    case WebSocketResource:
        return TypeBuilder::Page::ResourceType::WebSocket;
    case OtherResource:
        return TypeBuilder::Page::ResourceType::Other;
    }
    return TypeBuilder::Page::ResourceType::Other;
}

InspectorPageAgent::ResourceType InspectorPageAgent::cachedResourceType(const CachedResource& cachedResource)
{
    switch (cachedResource.type()) {
    case CachedResource::ImageResource:
        return InspectorPageAgent::ImageResource;
    case CachedResource::FontResource:
        return InspectorPageAgent::FontResource;
    case CachedResource::CSSStyleSheet:
        // Fall through.
#if ENABLE(XSLT)
    case CachedResource::XSLStyleSheet:
#endif
        return InspectorPageAgent::StylesheetResource;
    case CachedResource::Script:
        return InspectorPageAgent::ScriptResource;
    case CachedResource::RawResource:
        return InspectorPageAgent::XHRResource;
    case CachedResource::MainResource:
        return InspectorPageAgent::DocumentResource;
    default:
        break;
    }
    return InspectorPageAgent::OtherResource;
}

TypeBuilder::Page::ResourceType::Enum InspectorPageAgent::cachedResourceTypeJson(const CachedResource& cachedResource)
{
    return resourceTypeJson(cachedResourceType(cachedResource));
}

InspectorPageAgent::InspectorPageAgent(InstrumentingAgents* instrumentingAgents, Page* page, InspectorAgent* inspectorAgent, InjectedScriptManager* injectedScriptManager, InspectorClient* client, InspectorOverlay* overlay)
    : InspectorBaseAgent(ASCIILiteral("Page"), instrumentingAgents)
    , m_page(page)
    , m_inspectorAgent(inspectorAgent)
    , m_injectedScriptManager(injectedScriptManager)
    , m_client(client)
    , m_overlay(overlay)
    , m_lastScriptIdentifier(0)
    , m_screenWidthOverride(0)
    , m_screenHeightOverride(0)
    , m_fontScaleFactorOverride(1)
    , m_fitWindowOverride(false)
    , m_enabled(false)
    , m_isFirstLayoutAfterOnLoad(false)
    , m_originalScriptExecutionDisabled(false)
    , m_geolocationOverridden(false)
    , m_ignoreScriptsEnabledNotification(false)
    , m_showPaintRects(false)
{
}

void InspectorPageAgent::didCreateFrontendAndBackend(InspectorFrontendChannel* frontendChannel, InspectorBackendDispatcher* backendDispatcher)
{
    m_frontendDispatcher = std::make_unique<InspectorPageFrontendDispatcher>(frontendChannel);
    m_backendDispatcher = InspectorPageBackendDispatcher::create(backendDispatcher, this);
}

void InspectorPageAgent::willDestroyFrontendAndBackend()
{
    m_frontendDispatcher = nullptr;
    m_backendDispatcher.clear();

    ErrorString error;
    disable(&error);
#if ENABLE(TOUCH_EVENTS)
    updateTouchEventEmulationInPage(false);
#endif
}

void InspectorPageAgent::webViewResized(const IntSize& size)
{
    m_overlay->resize(m_screenWidthOverride ? size : IntSize());
}

void InspectorPageAgent::enable(ErrorString*)
{
    m_enabled = true;
    m_instrumentingAgents->setInspectorPageAgent(this);

    if (Frame* frame = mainFrame())
        m_originalScriptExecutionDisabled = !frame->settings().isScriptEnabled();
}

void InspectorPageAgent::disable(ErrorString*)
{
    m_enabled = false;
    m_scriptsToEvaluateOnLoad.clear();
    m_instrumentingAgents->setInspectorPageAgent(0);

    setScriptExecutionDisabled(0, m_originalScriptExecutionDisabled);
    setShowPaintRects(0, false);
    setShowDebugBorders(0, false);
    setShowFPSCounter(0, false);
    setEmulatedMedia(0, "");
    setContinuousPaintingEnabled(0, false);

    if (!deviceMetricsChanged(0, 0, 1, false))
        return;

    // When disabling the agent, reset the override values if necessary.
    updateViewMetrics(0, 0, 1, false);
    m_screenWidthOverride = 0;
    m_screenHeightOverride = 0;
    m_fontScaleFactorOverride = 1;
    m_fitWindowOverride = false;
}

void InspectorPageAgent::addScriptToEvaluateOnLoad(ErrorString*, const String& source, String* identifier)
{
    if (!m_scriptsToEvaluateOnLoad)
        m_scriptsToEvaluateOnLoad = InspectorObject::create();

    // Assure we don't override existing ids -- m_lastScriptIdentifier could get out of sync WRT actual
    // scripts once we restored the scripts from the cookie during navigation.
    do {
        *identifier = String::number(++m_lastScriptIdentifier);
    } while (m_scriptsToEvaluateOnLoad->find(*identifier) != m_scriptsToEvaluateOnLoad->end());

    m_scriptsToEvaluateOnLoad->setString(*identifier, source);
}

void InspectorPageAgent::removeScriptToEvaluateOnLoad(ErrorString* error, const String& identifier)
{
    if (!m_scriptsToEvaluateOnLoad || m_scriptsToEvaluateOnLoad->find(identifier) == m_scriptsToEvaluateOnLoad->end()) {
        *error = "Script not found";
        return;
    }

    m_scriptsToEvaluateOnLoad->remove(identifier);
}

void InspectorPageAgent::reload(ErrorString*, const bool* const optionalIgnoreCache, const String* optionalScriptToEvaluateOnLoad, const String* optionalScriptPreprocessor)
{
    m_pendingScriptToEvaluateOnLoadOnce = optionalScriptToEvaluateOnLoad ? *optionalScriptToEvaluateOnLoad : "";
    m_pendingScriptPreprocessor = optionalScriptPreprocessor ? *optionalScriptPreprocessor : "";
    m_page->mainFrame().loader().reload(optionalIgnoreCache ? *optionalIgnoreCache : false);
}

void InspectorPageAgent::navigate(ErrorString*, const String& url)
{
    UserGestureIndicator indicator(DefinitelyProcessingUserGesture);
    Frame& frame = m_page->mainFrame();
    frame.loader().changeLocation(frame.document()->securityOrigin(), frame.document()->completeURL(url), "", false, false);
}

static PassRefPtr<TypeBuilder::Page::Cookie> buildObjectForCookie(const Cookie& cookie)
{
    return TypeBuilder::Page::Cookie::create()
        .setName(cookie.name)
        .setValue(cookie.value)
        .setDomain(cookie.domain)
        .setPath(cookie.path)
        .setExpires(cookie.expires)
        .setSize((cookie.name.length() + cookie.value.length()))
        .setHttpOnly(cookie.httpOnly)
        .setSecure(cookie.secure)
        .setSession(cookie.session)
        .release();
}

static PassRefPtr<TypeBuilder::Array<TypeBuilder::Page::Cookie>> buildArrayForCookies(ListHashSet<Cookie>& cookiesList)
{
    RefPtr<TypeBuilder::Array<TypeBuilder::Page::Cookie>> cookies = TypeBuilder::Array<TypeBuilder::Page::Cookie>::create();

    ListHashSet<Cookie>::iterator end = cookiesList.end();
    ListHashSet<Cookie>::iterator it = cookiesList.begin();
    for (int i = 0; it != end; ++it, i++)
        cookies->addItem(buildObjectForCookie(*it));

    return cookies;
}

static Vector<CachedResource*> cachedResourcesForFrame(Frame* frame)
{
    Vector<CachedResource*> result;

    const CachedResourceLoader::DocumentResourceMap& allResources = frame->document()->cachedResourceLoader()->allCachedResources();
    CachedResourceLoader::DocumentResourceMap::const_iterator end = allResources.end();
    for (CachedResourceLoader::DocumentResourceMap::const_iterator it = allResources.begin(); it != end; ++it) {
        CachedResource* cachedResource = it->value.get();

        switch (cachedResource->type()) {
        case CachedResource::ImageResource:
            // Skip images that were not auto loaded (images disabled in the user agent).
        case CachedResource::FontResource:
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

static Vector<URL> allResourcesURLsForFrame(Frame* frame)
{
    Vector<URL> result;

    result.append(frame->loader().documentLoader()->url());

    Vector<CachedResource*> allResources = cachedResourcesForFrame(frame);
    for (Vector<CachedResource*>::const_iterator it = allResources.begin(); it != allResources.end(); ++it)
        result.append((*it)->url());

    return result;
}

void InspectorPageAgent::getCookies(ErrorString*, RefPtr<TypeBuilder::Array<TypeBuilder::Page::Cookie>>& cookies, WTF::String* cookiesString)
{
    // If we can get raw cookies.
    ListHashSet<Cookie> rawCookiesList;

    // If we can't get raw cookies - fall back to String representation
    StringBuilder stringCookiesList;

    // Return value to getRawCookies should be the same for every call because
    // the return value is platform/network backend specific, and the call will
    // always return the same true/false value.
    bool rawCookiesImplemented = false;

    for (Frame* frame = mainFrame(); frame; frame = frame->tree().traverseNext(mainFrame())) {
        Document* document = frame->document();
        Vector<URL> allURLs = allResourcesURLsForFrame(frame);
        for (Vector<URL>::const_iterator it = allURLs.begin(); it != allURLs.end(); ++it) {
            Vector<Cookie> docCookiesList;
            rawCookiesImplemented = getRawCookies(document, URL(ParsedURLString, *it), docCookiesList);
            if (!rawCookiesImplemented) {
                // FIXME: We need duplication checking for the String representation of cookies.
                //
                // Exceptions are thrown by cookie() in sandboxed frames. That won't happen here
                // because "document" is the document of the main frame of the page.
                stringCookiesList.append(document->cookie(ASSERT_NO_EXCEPTION));
            } else {
                int cookiesSize = docCookiesList.size();
                for (int i = 0; i < cookiesSize; i++) {
                    if (!rawCookiesList.contains(docCookiesList[i]))
                        rawCookiesList.add(docCookiesList[i]);
                }
            }
        }
    }

    // FIXME: Do not return empty string/empty array. Make returns optional instead. https://bugs.webkit.org/show_bug.cgi?id=80855
    if (rawCookiesImplemented) {
        cookies = buildArrayForCookies(rawCookiesList);
        *cookiesString = "";
    } else {
        cookies = TypeBuilder::Array<TypeBuilder::Page::Cookie>::create();
        *cookiesString = stringCookiesList.toString();
    }
}

void InspectorPageAgent::deleteCookie(ErrorString*, const String& cookieName, const String& url)
{
    URL parsedURL(ParsedURLString, url);
    for (Frame* frame = &m_page->mainFrame(); frame; frame = frame->tree().traverseNext(&m_page->mainFrame()))
        WebCore::deleteCookie(frame->document(), parsedURL, cookieName);
}

void InspectorPageAgent::getResourceTree(ErrorString*, RefPtr<TypeBuilder::Page::FrameResourceTree>& object)
{
    object = buildObjectForFrameTree(&m_page->mainFrame());
}

void InspectorPageAgent::getResourceContent(ErrorString* errorString, const String& frameId, const String& url, String* content, bool* base64Encoded)
{
    Frame* frame = assertFrame(errorString, frameId);
    if (!frame)
        return;

    resourceContent(errorString, frame, URL(ParsedURLString, url), content, base64Encoded);
}

static bool textContentForCachedResource(CachedResource* cachedResource, String* result)
{
    if (hasTextContent(cachedResource)) {
        String content;
        bool base64Encoded;
        if (InspectorPageAgent::cachedResourceContent(cachedResource, result, &base64Encoded)) {
            ASSERT(!base64Encoded);
            return true;
        }
    }
    return false;
}

void InspectorPageAgent::searchInResource(ErrorString*, const String& frameId, const String& url, const String& query, const bool* const optionalCaseSensitive, const bool* const optionalIsRegex, RefPtr<TypeBuilder::Array<TypeBuilder::Page::SearchMatch>>& results)
{
    results = TypeBuilder::Array<TypeBuilder::Page::SearchMatch>::create();

    bool isRegex = optionalIsRegex ? *optionalIsRegex : false;
    bool caseSensitive = optionalCaseSensitive ? *optionalCaseSensitive : false;

    Frame* frame = frameForId(frameId);
    if (!frame)
        return;

    DocumentLoader* loader = frame->loader().documentLoader();
    if (!loader)
        return;

    URL kurl(ParsedURLString, url);

    String content;
    bool success = false;
    if (equalIgnoringFragmentIdentifier(kurl, loader->url()))
        success = mainResourceContent(frame, false, &content);

    if (!success) {
        CachedResource* resource = cachedResource(frame, kurl);
        if (resource)
            success = textContentForCachedResource(resource, &content);
    }

    if (!success)
        return;

    results = ContentSearchUtils::searchInTextByLines(content, query, caseSensitive, isRegex);
}

static PassRefPtr<TypeBuilder::Page::SearchResult> buildObjectForSearchResult(const String& frameId, const String& url, int matchesCount)
{
    return TypeBuilder::Page::SearchResult::create()
        .setUrl(url)
        .setFrameId(frameId)
        .setMatchesCount(matchesCount)
        .release();
}

void InspectorPageAgent::searchInResources(ErrorString*, const String& text, const bool* const optionalCaseSensitive, const bool* const optionalIsRegex, RefPtr<TypeBuilder::Array<TypeBuilder::Page::SearchResult>>& results)
{
    RefPtr<TypeBuilder::Array<TypeBuilder::Page::SearchResult>> searchResults = TypeBuilder::Array<TypeBuilder::Page::SearchResult>::create();

    bool isRegex = optionalIsRegex ? *optionalIsRegex : false;
    bool caseSensitive = optionalCaseSensitive ? *optionalCaseSensitive : false;
    RegularExpression regex = ContentSearchUtils::createSearchRegex(text, caseSensitive, isRegex);

    for (Frame* frame = &m_page->mainFrame(); frame; frame = frame->tree().traverseNext(&m_page->mainFrame())) {
        String content;
        Vector<CachedResource*> allResources = cachedResourcesForFrame(frame);
        for (Vector<CachedResource*>::const_iterator it = allResources.begin(); it != allResources.end(); ++it) {
            CachedResource* cachedResource = *it;
            if (textContentForCachedResource(cachedResource, &content)) {
                int matchesCount = ContentSearchUtils::countRegularExpressionMatches(regex, content);
                if (matchesCount)
                    searchResults->addItem(buildObjectForSearchResult(frameId(frame), cachedResource->url(), matchesCount));
            }
        }
        if (mainResourceContent(frame, false, &content)) {
            int matchesCount = ContentSearchUtils::countRegularExpressionMatches(regex, content);
            if (matchesCount)
                searchResults->addItem(buildObjectForSearchResult(frameId(frame), frame->document()->url(), matchesCount));
        }
    }

    results = searchResults;
}

void InspectorPageAgent::setDocumentContent(ErrorString* errorString, const String& frameId, const String& html)
{
    Frame* frame = assertFrame(errorString, frameId);
    if (!frame)
        return;

    Document* document = frame->document();
    if (!document) {
        *errorString = "No Document instance to set HTML for";
        return;
    }
    DOMPatchSupport::patchDocument(document, html);
}

void InspectorPageAgent::canOverrideDeviceMetrics(ErrorString*, bool* result)
{
    *result = m_client->canOverrideDeviceMetrics();
}

void InspectorPageAgent::setDeviceMetricsOverride(ErrorString* errorString, int width, int height, double fontScaleFactor, bool fitWindow)
{
    const static long maxDimension = 10000000;

    if (width < 0 || height < 0 || width > maxDimension || height > maxDimension) {
        *errorString = makeString("Width and height values must be positive, not greater than ", String::number(maxDimension));
        return;
    }

    if (!width ^ !height) {
        *errorString = "Both width and height must be either zero or non-zero at once";
        return;
    }

    if (fontScaleFactor <= 0) {
        *errorString = "fontScaleFactor must be positive";
        return;
    }

    if (!deviceMetricsChanged(width, height, fontScaleFactor, fitWindow))
        return;

    m_screenWidthOverride = width;
    m_screenHeightOverride = height;
    m_fontScaleFactorOverride = fontScaleFactor;
    m_fitWindowOverride = fitWindow;

    updateViewMetrics(width, height, fontScaleFactor, fitWindow);
}

bool InspectorPageAgent::deviceMetricsChanged(int width, int height, double fontScaleFactor, bool fitWindow)
{
    return width != m_screenWidthOverride || height != m_screenHeightOverride || fontScaleFactor != m_fontScaleFactorOverride || fitWindow != m_fitWindowOverride;
}

void InspectorPageAgent::setShowPaintRects(ErrorString*, bool show)
{
    m_showPaintRects = show;
    m_client->setShowPaintRects(show);

    if (!show && mainFrame() && mainFrame()->view())
        mainFrame()->view()->invalidate();
}

void InspectorPageAgent::canShowDebugBorders(ErrorString*, bool* outParam)
{
    *outParam = m_client->canShowDebugBorders();
}

void InspectorPageAgent::setShowDebugBorders(ErrorString*, bool show)
{
    m_client->setShowDebugBorders(show);
    if (mainFrame() && mainFrame()->view())
        mainFrame()->view()->invalidate();
}

void InspectorPageAgent::canShowFPSCounter(ErrorString*, bool* outParam)
{
    *outParam = m_client->canShowFPSCounter();
}

void InspectorPageAgent::setShowFPSCounter(ErrorString*, bool show)
{
    m_client->setShowFPSCounter(show);

    if (mainFrame() && mainFrame()->view())
        mainFrame()->view()->invalidate();
}

void InspectorPageAgent::canContinuouslyPaint(ErrorString*, bool* outParam)
{
    *outParam = m_client->canContinuouslyPaint();
}

void InspectorPageAgent::setContinuousPaintingEnabled(ErrorString*, bool enabled)
{
    m_client->setContinuousPaintingEnabled(enabled);

    if (!enabled && mainFrame() && mainFrame()->view())
        mainFrame()->view()->invalidate();
}

void InspectorPageAgent::getScriptExecutionStatus(ErrorString*, InspectorPageBackendDispatcherHandler::Result::Enum* status)
{
    bool disabledByScriptController = false;
    bool disabledInSettings = false;
    Frame* frame = mainFrame();
    if (frame) {
        disabledByScriptController = !frame->script().canExecuteScripts(NotAboutToExecuteScript);
        disabledInSettings = !frame->settings().isScriptEnabled();
    }

    if (!disabledByScriptController) {
        *status = InspectorPageBackendDispatcherHandler::Result::Allowed;
        return;
    }

    if (disabledInSettings)
        *status = InspectorPageBackendDispatcherHandler::Result::Disabled;
    else
        *status = InspectorPageBackendDispatcherHandler::Result::Forbidden;
}

void InspectorPageAgent::setScriptExecutionDisabled(ErrorString*, bool value)
{
    if (!mainFrame())
        return;

    m_ignoreScriptsEnabledNotification = true;
    mainFrame()->settings().setScriptEnabled(!value);
    m_ignoreScriptsEnabledNotification = false;
}

void InspectorPageAgent::didClearWindowObjectInWorld(Frame* frame, DOMWrapperWorld& world)
{
    if (&world != &mainThreadNormalWorld())
        return;

    if (frame->isMainFrame())
        m_injectedScriptManager->discardInjectedScripts();

    if (!m_frontendDispatcher)
        return;

    if (m_scriptsToEvaluateOnLoad) {
        InspectorObject::const_iterator end = m_scriptsToEvaluateOnLoad->end();
        for (InspectorObject::const_iterator it = m_scriptsToEvaluateOnLoad->begin(); it != end; ++it) {
            String scriptText;
            if (it->value->asString(&scriptText))
                frame->script().executeScript(scriptText);
        }
    }

    if (!m_scriptToEvaluateOnLoadOnce.isEmpty())
        frame->script().executeScript(m_scriptToEvaluateOnLoadOnce);
}

void InspectorPageAgent::domContentEventFired()
{
    m_isFirstLayoutAfterOnLoad = true;
    m_frontendDispatcher->domContentEventFired(currentTime());
}

void InspectorPageAgent::loadEventFired()
{
    m_frontendDispatcher->loadEventFired(currentTime());
}

void InspectorPageAgent::frameNavigated(DocumentLoader* loader)
{
    if (loader->frame()->isMainFrame()) {
        m_scriptToEvaluateOnLoadOnce = m_pendingScriptToEvaluateOnLoadOnce;
        m_scriptPreprocessor = m_pendingScriptPreprocessor;
        m_pendingScriptToEvaluateOnLoadOnce = String();
        m_pendingScriptPreprocessor = String();
    }
    m_frontendDispatcher->frameNavigated(buildObjectForFrame(loader->frame()));
}

void InspectorPageAgent::frameDetached(Frame* frame)
{
    HashMap<Frame*, String>::iterator iterator = m_frameToIdentifier.find(frame);
    if (iterator != m_frameToIdentifier.end()) {
        m_frontendDispatcher->frameDetached(iterator->value);
        m_identifierToFrame.remove(iterator->value);
        m_frameToIdentifier.remove(iterator);
    }
}

Frame* InspectorPageAgent::mainFrame()
{
    // FIXME: This should return a Frame&
    return &m_page->mainFrame();
}

Frame* InspectorPageAgent::frameForId(const String& frameId)
{
    return frameId.isEmpty() ? 0 : m_identifierToFrame.get(frameId);
}

String InspectorPageAgent::frameId(Frame* frame)
{
    if (!frame)
        return "";
    String identifier = m_frameToIdentifier.get(frame);
    if (identifier.isNull()) {
        identifier = IdentifiersFactory::createIdentifier();
        m_frameToIdentifier.set(frame, identifier);
        m_identifierToFrame.set(identifier, frame);
    }
    return identifier;
}

bool InspectorPageAgent::hasIdForFrame(Frame* frame) const
{
    return frame && m_frameToIdentifier.contains(frame);
}

String InspectorPageAgent::loaderId(DocumentLoader* loader)
{
    if (!loader)
        return "";
    String identifier = m_loaderToIdentifier.get(loader);
    if (identifier.isNull()) {
        identifier = IdentifiersFactory::createIdentifier();
        m_loaderToIdentifier.set(loader, identifier);
    }
    return identifier;
}

Frame* InspectorPageAgent::findFrameWithSecurityOrigin(const String& originRawString)
{
    for (Frame* frame = &m_page->mainFrame(); frame; frame = frame->tree().traverseNext()) {
        RefPtr<SecurityOrigin> documentOrigin = frame->document()->securityOrigin();
        if (documentOrigin->toRawString() == originRawString)
            return frame;
    }
    return 0;
}

Frame* InspectorPageAgent::assertFrame(ErrorString* errorString, const String& frameId)
{
    Frame* frame = frameForId(frameId);
    if (!frame)
        *errorString = "No frame for given id found";
    return frame;
}

// static
DocumentLoader* InspectorPageAgent::assertDocumentLoader(ErrorString* errorString, Frame* frame)
{
    FrameLoader& frameLoader = frame->loader();
    DocumentLoader* documentLoader = frameLoader.documentLoader();
    if (!documentLoader)
        *errorString = "No documentLoader for given frame found";
    return documentLoader;
}

void InspectorPageAgent::loaderDetachedFromFrame(DocumentLoader* loader)
{
    HashMap<DocumentLoader*, String>::iterator iterator = m_loaderToIdentifier.find(loader);
    if (iterator != m_loaderToIdentifier.end())
        m_loaderToIdentifier.remove(iterator);
}

void InspectorPageAgent::frameStartedLoading(Frame* frame)
{
    m_frontendDispatcher->frameStartedLoading(frameId(frame));
}

void InspectorPageAgent::frameStoppedLoading(Frame* frame)
{
    m_frontendDispatcher->frameStoppedLoading(frameId(frame));
}

void InspectorPageAgent::frameScheduledNavigation(Frame* frame, double delay)
{
    m_frontendDispatcher->frameScheduledNavigation(frameId(frame), delay);
}

void InspectorPageAgent::frameClearedScheduledNavigation(Frame* frame)
{
    m_frontendDispatcher->frameClearedScheduledNavigation(frameId(frame));
}

void InspectorPageAgent::willRunJavaScriptDialog(const String& message)
{
    m_frontendDispatcher->javascriptDialogOpening(message);
}

void InspectorPageAgent::didRunJavaScriptDialog()
{
    m_frontendDispatcher->javascriptDialogClosed();
}

void InspectorPageAgent::applyScreenWidthOverride(long* width)
{
    if (m_screenWidthOverride)
        *width = m_screenWidthOverride;
}

void InspectorPageAgent::applyScreenHeightOverride(long* height)
{
    if (m_screenHeightOverride)
        *height = m_screenHeightOverride;
}

void InspectorPageAgent::didPaint(GraphicsContext* context, const LayoutRect& rect)
{
    if (!m_enabled || m_client->overridesShowPaintRects() || !m_showPaintRects)
        return;

    static int colorSelector = 0;
    const Color colors[] = {
        Color(0xFF, 0, 0, 0x3F),
        Color(0xFF, 0, 0xFF, 0x3F),
        Color(0, 0, 0xFF, 0x3F),
    };

    LayoutRect inflatedRect(rect);
    inflatedRect.inflate(-1);
    m_overlay->drawOutline(context, inflatedRect, colors[colorSelector++ % WTF_ARRAY_LENGTH(colors)]);
}

void InspectorPageAgent::didLayout()
{
    bool isFirstLayout = m_isFirstLayoutAfterOnLoad;
    if (isFirstLayout)
        m_isFirstLayoutAfterOnLoad = false;

    if (!m_enabled)
        return;

    if (isFirstLayout) {
        if (m_screenWidthOverride && m_screenHeightOverride)
            m_client->autoZoomPageToFitWidth();
    }
    m_overlay->update();
}

void InspectorPageAgent::didScroll()
{
    if (m_enabled)
        m_overlay->update();
}

void InspectorPageAgent::didRecalculateStyle()
{
    if (m_enabled)
        m_overlay->update();
}

void InspectorPageAgent::scriptsEnabled(bool isEnabled)
{
    if (m_ignoreScriptsEnabledNotification)
        return;

    m_frontendDispatcher->scriptsEnabled(isEnabled);
}

PassRefPtr<TypeBuilder::Page::Frame> InspectorPageAgent::buildObjectForFrame(Frame* frame)
{
    RefPtr<TypeBuilder::Page::Frame> frameObject = TypeBuilder::Page::Frame::create()
        .setId(frameId(frame))
        .setLoaderId(loaderId(frame->loader().documentLoader()))
        .setUrl(frame->document()->url().string())
        .setMimeType(frame->loader().documentLoader()->responseMIMEType())
        .setSecurityOrigin(frame->document()->securityOrigin()->toRawString());
    if (frame->tree().parent())
        frameObject->setParentId(frameId(frame->tree().parent()));
    if (frame->ownerElement()) {
        String name = frame->ownerElement()->getNameAttribute();
        if (name.isEmpty())
            name = frame->ownerElement()->getAttribute(HTMLNames::idAttr);
        frameObject->setName(name);
    }

    return frameObject;
}

PassRefPtr<TypeBuilder::Page::FrameResourceTree> InspectorPageAgent::buildObjectForFrameTree(Frame* frame)
{
    RefPtr<TypeBuilder::Page::Frame> frameObject = buildObjectForFrame(frame);
    RefPtr<TypeBuilder::Array<TypeBuilder::Page::FrameResourceTree::Resources>> subresources = TypeBuilder::Array<TypeBuilder::Page::FrameResourceTree::Resources>::create();
    RefPtr<TypeBuilder::Page::FrameResourceTree> result = TypeBuilder::Page::FrameResourceTree::create()
         .setFrame(frameObject)
         .setResources(subresources);

    Vector<CachedResource*> allResources = cachedResourcesForFrame(frame);
    for (Vector<CachedResource*>::const_iterator it = allResources.begin(); it != allResources.end(); ++it) {
        CachedResource* cachedResource = *it;

        RefPtr<TypeBuilder::Page::FrameResourceTree::Resources> resourceObject = TypeBuilder::Page::FrameResourceTree::Resources::create()
            .setUrl(cachedResource->url())
            .setType(cachedResourceTypeJson(*cachedResource))
            .setMimeType(cachedResource->response().mimeType());
        if (cachedResource->wasCanceled())
            resourceObject->setCanceled(true);
        else if (cachedResource->status() == CachedResource::LoadError)
            resourceObject->setFailed(true);
        String sourceMappingURL = InspectorPageAgent::sourceMapURLForResource(cachedResource);
        if (!sourceMappingURL.isEmpty())
            resourceObject->setSourceMapURL(sourceMappingURL);
        subresources->addItem(resourceObject);
    }

    RefPtr<TypeBuilder::Array<TypeBuilder::Page::FrameResourceTree>> childrenArray;
    for (Frame* child = frame->tree().firstChild(); child; child = child->tree().nextSibling()) {
        if (!childrenArray) {
            childrenArray = TypeBuilder::Array<TypeBuilder::Page::FrameResourceTree>::create();
            result->setChildFrames(childrenArray);
        }
        childrenArray->addItem(buildObjectForFrameTree(child));
    }
    return result;
}

void InspectorPageAgent::updateViewMetrics(int width, int height, double fontScaleFactor, bool fitWindow)
{
    m_client->overrideDeviceMetrics(width, height, static_cast<float>(fontScaleFactor), fitWindow);

    Document* document = mainFrame()->document();
    if (document)
        document->styleResolverChanged(RecalcStyleImmediately);
    InspectorInstrumentation::mediaQueryResultChanged(document);
}

#if ENABLE(TOUCH_EVENTS)
void InspectorPageAgent::updateTouchEventEmulationInPage(bool enabled)
{
    if (mainFrame())
        mainFrame()->settings().setTouchEventEmulationEnabled(enabled);
}
#endif

void InspectorPageAgent::setGeolocationOverride(ErrorString* error, const double* latitude, const double* longitude, const double* accuracy)
{
#if ENABLE (GEOLOCATION)
    GeolocationController* controller = GeolocationController::from(m_page);
    GeolocationPosition* position = 0;
    if (!controller) {
        *error = "Internal error: unable to override geolocation";
        return;
    }
    position = controller->lastPosition();
    if (!m_geolocationOverridden && position)
        m_platformGeolocationPosition = position;

    m_geolocationOverridden = true;
    if (latitude && longitude && accuracy)
        m_geolocationPosition = GeolocationPosition::create(currentTimeMS(), *latitude, *longitude, *accuracy);
    else
        m_geolocationPosition.clear();

    controller->positionChanged(0); // Kick location update.
#else
    *error = "Geolocation is not available";
    UNUSED_PARAM(latitude);
    UNUSED_PARAM(longitude);
    UNUSED_PARAM(accuracy);
#endif
}

void InspectorPageAgent::clearGeolocationOverride(ErrorString* error)
{
    if (!m_geolocationOverridden)
        return;
#if ENABLE(GEOLOCATION)
    UNUSED_PARAM(error);
    m_geolocationOverridden = false;
    m_geolocationPosition.clear();

    GeolocationController* controller = GeolocationController::from(m_page);
    if (controller && m_platformGeolocationPosition.get())
        controller->positionChanged(m_platformGeolocationPosition.get());
#else
    *error = "Geolocation is not available";
#endif
}

void InspectorPageAgent::canOverrideGeolocation(ErrorString*, bool* out_param)
{
#if ENABLE(GEOLOCATION)
    *out_param = true;
#else
    *out_param = false;
#endif
}

GeolocationPosition* InspectorPageAgent::overrideGeolocationPosition(GeolocationPosition* position)
{
    if (m_geolocationOverridden) {
        if (position)
            m_platformGeolocationPosition = position;
        return m_geolocationPosition.get();
    }
    return position;
}

void InspectorPageAgent::setDeviceOrientationOverride(ErrorString* error, double alpha, double beta, double gamma)
{
    DeviceOrientationController* controller = DeviceOrientationController::from(m_page);
    if (!controller) {
        *error = "Internal error: unable to override device orientation";
        return;
    }

    ErrorString clearError;
    clearDeviceOrientationOverride(&clearError);

    m_deviceOrientation = DeviceOrientationData::create(true, alpha, true, beta, true, gamma);
    controller->didChangeDeviceOrientation(m_deviceOrientation.get());
}

void InspectorPageAgent::clearDeviceOrientationOverride(ErrorString*)
{
    m_deviceOrientation.clear();
}

void InspectorPageAgent::canOverrideDeviceOrientation(ErrorString*, bool* outParam)
{
#if ENABLE(DEVICE_ORIENTATION)
    *outParam = true;
#else
    *outParam = false;
#endif
}

DeviceOrientationData* InspectorPageAgent::overrideDeviceOrientation(DeviceOrientationData* deviceOrientation)
{
    if (m_deviceOrientation)
        deviceOrientation = m_deviceOrientation.get();
    return deviceOrientation;
}

void InspectorPageAgent::setTouchEmulationEnabled(ErrorString* error, bool enabled)
{
#if ENABLE(TOUCH_EVENTS)
    UNUSED_PARAM(error);
    updateTouchEventEmulationInPage(enabled);
#else
    *error = "Touch events emulation not supported";
    UNUSED_PARAM(enabled);
#endif
}

void InspectorPageAgent::setEmulatedMedia(ErrorString*, const String& media)
{
    if (media == m_emulatedMedia)
        return;

    m_emulatedMedia = media;
    Document* document = m_page->mainFrame().document();
    if (document) {
        document->styleResolverChanged(RecalcStyleImmediately);
        document->updateLayout();
    }
}

void InspectorPageAgent::applyEmulatedMedia(String* media)
{
    if (!m_emulatedMedia.isEmpty())
        *media = m_emulatedMedia;
}

void InspectorPageAgent::getCompositingBordersVisible(ErrorString*, bool* outParam)
{
    *outParam = m_page->settings().showDebugBorders() || m_page->settings().showRepaintCounter();
}

void InspectorPageAgent::setCompositingBordersVisible(ErrorString*, bool visible)
{
    m_page->settings().setShowDebugBorders(visible);
    m_page->settings().setShowRepaintCounter(visible);
}

void InspectorPageAgent::captureScreenshot(ErrorString* errorString, String* data)
{
    if (!m_client->captureScreenshot(data))
        *errorString = "Could not capture screenshot";
}

void InspectorPageAgent::handleJavaScriptDialog(ErrorString* errorString, bool accept, const String* promptText)
{
    if (!m_client->handleJavaScriptDialog(accept, promptText))
        *errorString = "Could not handle JavaScript dialog";
}

void InspectorPageAgent::archive(ErrorString* errorString, String* data)
{
    Frame* frame = mainFrame();
    if (!frame) {
        *errorString = "No main frame";
        return;
    }

#if ENABLE(WEB_ARCHIVE) && USE(CF)
    RefPtr<LegacyWebArchive> archive = LegacyWebArchive::create(frame);
    if (!archive) {
        *errorString = "Could not create web archive for main frame";
        return;
    }

    RetainPtr<CFDataRef> buffer = archive->rawDataRepresentation();
    *data = base64Encode(CFDataGetBytePtr(buffer.get()), CFDataGetLength(buffer.get()));
#else
    UNUSED_PARAM(data);
    *errorString = "No support for creating archives";
#endif
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
