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

#include "Base64.h"
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
#include "DOMNodeHighlighter.h"
#include "DOMPatchSupport.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "Frame.h"
#include "FrameView.h"
#include "HTMLFrameOwnerElement.h"
#include "HTMLNames.h"
#include "IdentifiersFactory.h"
#include "InjectedScriptManager.h"
#include "InspectorClient.h"
#include "InspectorFrontend.h"
#include "InspectorInstrumentation.h"
#include "InspectorState.h"
#include "InspectorValues.h"
#include "InstrumentingAgents.h"
#include "MemoryCache.h"
#include "Page.h"
#include "RegularExpression.h"
#include "ScriptObject.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "SharedBuffer.h"
#include "TextEncoding.h"
#include "TextResourceDecoder.h"
#include "UserGestureIndicator.h"

#include <wtf/CurrentTime.h>
#include <wtf/ListHashSet.h>
#include <wtf/Vector.h>

using namespace std;

namespace WebCore {

namespace PageAgentState {
static const char pageAgentEnabled[] = "pageAgentEnabled";
static const char pageAgentScriptExecutionDisabled[] = "pageAgentScriptExecutionDisabled";
static const char pageAgentScriptsToEvaluateOnLoad[] = "pageAgentScriptsToEvaluateOnLoad";
static const char pageAgentScreenWidthOverride[] = "pageAgentScreenWidthOverride";
static const char pageAgentScreenHeightOverride[] = "pageAgentScreenHeightOverride";
static const char pageAgentFontScaleFactorOverride[] = "pageAgentFontScaleFactorOverride";
static const char pageAgentFitWindow[] = "pageAgentFitWindow";
static const char showPaintRects[] = "showPaintRects";
}

static bool decodeSharedBuffer(PassRefPtr<SharedBuffer> buffer, const String& textEncodingName, String* result)
{
    if (buffer) {
        TextEncoding encoding(textEncodingName);
        if (!encoding.isValid())
            encoding = WindowsLatin1Encoding();
        *result = encoding.decode(buffer->data(), buffer->size());
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
    return type == InspectorPageAgent::StylesheetResource || type == InspectorPageAgent::ScriptResource || type == InspectorPageAgent::XHRResource;
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
        RefPtr<SharedBuffer> buffer = hasZeroSize ? SharedBuffer::create() : cachedResource->data();

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
            SharedBuffer* buffer = cachedResource->data();
            if (!buffer)
                return false;
            RefPtr<TextResourceDecoder> decoder = createXHRTextDecoder(cachedResource->response().mimeType(), cachedResource->response().textEncodingName());
            // We show content for raw resources only for certain mime types (text, html and xml). Otherwise decoder will be null.
            if (!decoder)
                return false;
            String content = decoder->decode(buffer->data(), buffer->size());
            content += decoder->flush();
            *result = content;
            return true;
        }
        default:
            return decodeSharedBuffer(cachedResource->data(), cachedResource->encoding(), result);
        }
    }
    return false;
}

static bool mainResourceContent(Frame* frame, bool withBase64Encode, String* result)
{
    RefPtr<SharedBuffer> buffer = frame->loader()->documentLoader()->mainResourceData();
    if (!buffer)
        return false;
    String textEncodingName = frame->document()->inputEncoding();

    return InspectorPageAgent::sharedBufferContent(buffer, textEncodingName, withBase64Encode, result);
}

// static
bool InspectorPageAgent::sharedBufferContent(PassRefPtr<SharedBuffer> buffer, const String& textEncodingName, bool withBase64Encode, String* result)
{
    if (withBase64Encode) {
        *result = base64Encode(buffer->data(), buffer->size());
        return true;
    }

    return decodeSharedBuffer(buffer, textEncodingName, result);
}

PassOwnPtr<InspectorPageAgent> InspectorPageAgent::create(InstrumentingAgents* instrumentingAgents, Page* page, InspectorState* state, InjectedScriptManager* injectedScriptManager, InspectorClient* client)
{
    return adoptPtr(new InspectorPageAgent(instrumentingAgents, page, state, injectedScriptManager, client));
}

// static
void InspectorPageAgent::resourceContent(ErrorString* errorString, Frame* frame, const KURL& url, String* result, bool* base64Encoded)
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

CachedResource* InspectorPageAgent::cachedResource(Frame* frame, const KURL& url)
{
    CachedResource* cachedResource = frame->document()->cachedResourceLoader()->cachedResource(url);
    if (!cachedResource)
        cachedResource = memoryCache()->resourceForURL(url);
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
    default:
        break;
    }
    return InspectorPageAgent::OtherResource;
}

TypeBuilder::Page::ResourceType::Enum InspectorPageAgent::cachedResourceTypeJson(const CachedResource& cachedResource)
{
    return resourceTypeJson(cachedResourceType(cachedResource));
}

InspectorPageAgent::InspectorPageAgent(InstrumentingAgents* instrumentingAgents, Page* page, InspectorState* inspectorState, InjectedScriptManager* injectedScriptManager, InspectorClient* client)
    : InspectorBaseAgent<InspectorPageAgent>("Page", instrumentingAgents, inspectorState)
    , m_page(page)
    , m_injectedScriptManager(injectedScriptManager)
    , m_client(client)
    , m_frontend(0)
    , m_lastScriptIdentifier(0)
    , m_lastPaintContext(0)
    , m_didLoadEventFire(false)
{
}

void InspectorPageAgent::setFrontend(InspectorFrontend* frontend)
{
    m_frontend = frontend->page();
}

void InspectorPageAgent::clearFrontend()
{
    ErrorString error;
    disable(&error);
    m_frontend = 0;
}

void InspectorPageAgent::restore()
{
    if (m_state->getBoolean(PageAgentState::pageAgentEnabled)) {
        ErrorString error;
        enable(&error);

        // When restoring the agent, override values are restored into the FrameView.
        int width = static_cast<int>(m_state->getLong(PageAgentState::pageAgentScreenWidthOverride));
        int height = static_cast<int>(m_state->getLong(PageAgentState::pageAgentScreenHeightOverride));
        double fontScaleFactor = m_state->getDouble(PageAgentState::pageAgentFontScaleFactorOverride);
        bool fitWindow = m_state->getBoolean(PageAgentState::pageAgentFitWindow);
        updateViewMetrics(width, height, fontScaleFactor, fitWindow);
    }
}

void InspectorPageAgent::enable(ErrorString*)
{
    m_state->setBoolean(PageAgentState::pageAgentEnabled, true);
    bool scriptExecutionDisabled = m_state->getBoolean(PageAgentState::pageAgentScriptExecutionDisabled);
    setScriptExecutionDisabled(0, scriptExecutionDisabled);
    m_instrumentingAgents->setInspectorPageAgent(this);
}

void InspectorPageAgent::disable(ErrorString*)
{
    m_state->setBoolean(PageAgentState::pageAgentEnabled, false);
    m_instrumentingAgents->setInspectorPageAgent(0);

    setScriptExecutionDisabled(0, false);

    // When disabling the agent, reset the override values.
    m_state->setLong(PageAgentState::pageAgentScreenWidthOverride, 0);
    m_state->setLong(PageAgentState::pageAgentScreenHeightOverride, 0);
    m_state->setDouble(PageAgentState::pageAgentFontScaleFactorOverride, 1);
    m_state->setBoolean(PageAgentState::pageAgentFitWindow, false);
    updateViewMetrics(0, 0, 1, false);
}

void InspectorPageAgent::addScriptToEvaluateOnLoad(ErrorString*, const String& source, String* identifier)
{
    RefPtr<InspectorObject> scripts = m_state->getObject(PageAgentState::pageAgentScriptsToEvaluateOnLoad);
    if (!scripts) {
        scripts = InspectorObject::create();
        m_state->setObject(PageAgentState::pageAgentScriptsToEvaluateOnLoad, scripts);
    }
    // Assure we don't override existing ids -- m_lastScriptIdentifier could get out of sync WRT actual
    // scripts once we restored the scripts from the cookie during navigation.
    do {
        *identifier = String::number(++m_lastScriptIdentifier);
    } while (scripts->find(*identifier) != scripts->end());
    scripts->setString(*identifier, source);
}

void InspectorPageAgent::removeScriptToEvaluateOnLoad(ErrorString* error, const String& identifier)
{
    RefPtr<InspectorObject> scripts = m_state->getObject(PageAgentState::pageAgentScriptsToEvaluateOnLoad);
    if (!scripts || scripts->find(identifier) == scripts->end()) {
        *error = "Script not found";
        return;
    }
    scripts->remove(identifier);
}

void InspectorPageAgent::reload(ErrorString*, const bool* const optionalIgnoreCache, const String* optionalScriptToEvaluateOnLoad)
{
    m_pendingScriptToEvaluateOnLoadOnce = optionalScriptToEvaluateOnLoad ? *optionalScriptToEvaluateOnLoad : "";
    m_page->mainFrame()->loader()->reload(optionalIgnoreCache ? *optionalIgnoreCache : false);
}

void InspectorPageAgent::navigate(ErrorString*, const String& url)
{
    UserGestureIndicator indicator(DefinitelyProcessingUserGesture);
    Frame* frame = m_page->mainFrame();
    frame->loader()->changeLocation(frame->document()->securityOrigin(), frame->document()->completeURL(url), "", false, false);
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

static PassRefPtr<TypeBuilder::Array<TypeBuilder::Page::Cookie> > buildArrayForCookies(ListHashSet<Cookie>& cookiesList)
{
    RefPtr<TypeBuilder::Array<TypeBuilder::Page::Cookie> > cookies = TypeBuilder::Array<TypeBuilder::Page::Cookie>::create();

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
        CachedResource* cachedResource = it->second.get();

        switch (cachedResource->type()) {
        case CachedResource::ImageResource:
            // Skip images that were not auto loaded (images disabled in the user agent).
            if (static_cast<CachedImage*>(cachedResource)->stillNeedsLoad())
                continue;
            break;
        case CachedResource::FontResource:
            // Skip fonts that were referenced in CSS but never used/downloaded.
            if (static_cast<CachedFont*>(cachedResource)->stillNeedsLoad())
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

static Vector<KURL> allResourcesURLsForFrame(Frame* frame)
{
    Vector<KURL> result;

    result.append(frame->loader()->documentLoader()->url());

    Vector<CachedResource*> allResources = cachedResourcesForFrame(frame);
    for (Vector<CachedResource*>::const_iterator it = allResources.begin(); it != allResources.end(); ++it)
        result.append((*it)->url());

    return result;
}

void InspectorPageAgent::getCookies(ErrorString*, RefPtr<TypeBuilder::Array<TypeBuilder::Page::Cookie> >& cookies, WTF::String* cookiesString)
{
    // If we can get raw cookies.
    ListHashSet<Cookie> rawCookiesList;

    // If we can't get raw cookies - fall back to String representation
    String stringCookiesList;

    // Return value to getRawCookies should be the same for every call because
    // the return value is platform/network backend specific, and the call will
    // always return the same true/false value.
    bool rawCookiesImplemented = false;

    for (Frame* frame = mainFrame(); frame; frame = frame->tree()->traverseNext(mainFrame())) {
        Document* document = frame->document();
        Vector<KURL> allURLs = allResourcesURLsForFrame(frame);
        for (Vector<KURL>::const_iterator it = allURLs.begin(); it != allURLs.end(); ++it) {
            Vector<Cookie> docCookiesList;
            rawCookiesImplemented = getRawCookies(document, KURL(ParsedURLString, *it), docCookiesList);
            if (!rawCookiesImplemented) {
                // FIXME: We need duplication checking for the String representation of cookies.
                ExceptionCode ec = 0;
                stringCookiesList += document->cookie(ec);
                // Exceptions are thrown by cookie() in sandboxed frames. That won't happen here
                // because "document" is the document of the main frame of the page.
                ASSERT(!ec);
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
        *cookiesString = stringCookiesList;
    }
}

void InspectorPageAgent::deleteCookie(ErrorString*, const String& cookieName, const String& domain)
{
    for (Frame* frame = m_page->mainFrame(); frame; frame = frame->tree()->traverseNext(m_page->mainFrame())) {
        Document* document = frame->document();
        if (document->url().host() != domain)
            continue;

        Vector<KURL> allURLs = allResourcesURLsForFrame(frame);
        for (Vector<KURL>::const_iterator it = allURLs.begin(); it != allURLs.end(); ++it)
            WebCore::deleteCookie(document, KURL(ParsedURLString, *it), cookieName);
    }
}

void InspectorPageAgent::getResourceTree(ErrorString*, RefPtr<TypeBuilder::Page::FrameResourceTree>& object)
{
    object = buildObjectForFrameTree(m_page->mainFrame());
}

void InspectorPageAgent::getResourceContent(ErrorString* errorString, const String& frameId, const String& url, String* content, bool* base64Encoded)
{
    Frame* frame = assertFrame(errorString, frameId);
    if (!frame)
        return;

    resourceContent(errorString, frame, KURL(ParsedURLString, url), content, base64Encoded);
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

void InspectorPageAgent::searchInResource(ErrorString*, const String& frameId, const String& url, const String& query, const bool* const optionalCaseSensitive, const bool* const optionalIsRegex, RefPtr<TypeBuilder::Array<TypeBuilder::Page::SearchMatch> >& results)
{
    results = TypeBuilder::Array<TypeBuilder::Page::SearchMatch>::create();

    bool isRegex = optionalIsRegex ? *optionalIsRegex : false;
    bool caseSensitive = optionalCaseSensitive ? *optionalCaseSensitive : false;

    Frame* frame = frameForId(frameId);
    KURL kurl(ParsedURLString, url);

    FrameLoader* frameLoader = frame ? frame->loader() : 0;
    DocumentLoader* loader = frameLoader ? frameLoader->documentLoader() : 0;
    if (!loader)
        return;

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

void InspectorPageAgent::searchInResources(ErrorString*, const String& text, const bool* const optionalCaseSensitive, const bool* const optionalIsRegex, RefPtr<TypeBuilder::Array<TypeBuilder::Page::SearchResult> >& results)
{
    RefPtr<TypeBuilder::Array<TypeBuilder::Page::SearchResult> > searchResults = TypeBuilder::Array<TypeBuilder::Page::SearchResult>::create();

    bool isRegex = optionalIsRegex ? *optionalIsRegex : false;
    bool caseSensitive = optionalCaseSensitive ? *optionalCaseSensitive : false;
    RegularExpression regex = ContentSearchUtils::createSearchRegex(text, caseSensitive, isRegex);

    for (Frame* frame = m_page->mainFrame(); frame; frame = frame->tree()->traverseNext(m_page->mainFrame())) {
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

    // These two always fit an int.
    int currentWidth = static_cast<int>(m_state->getLong(PageAgentState::pageAgentScreenWidthOverride));
    int currentHeight = static_cast<int>(m_state->getLong(PageAgentState::pageAgentScreenHeightOverride));
    double currentFontScaleFactor = m_state->getDouble(PageAgentState::pageAgentFontScaleFactorOverride);
    bool currentFitWindow = m_state->getBoolean(PageAgentState::pageAgentFitWindow);

    if (width == currentWidth && height == currentHeight && fontScaleFactor == currentFontScaleFactor && fitWindow == currentFitWindow)
        return;

    m_state->setLong(PageAgentState::pageAgentScreenWidthOverride, width);
    m_state->setLong(PageAgentState::pageAgentScreenHeightOverride, height);
    m_state->setDouble(PageAgentState::pageAgentFontScaleFactorOverride, fontScaleFactor);
    m_state->setBoolean(PageAgentState::pageAgentFitWindow, fitWindow);

    updateViewMetrics(width, height, fontScaleFactor, fitWindow);
}

void InspectorPageAgent::setShowPaintRects(ErrorString*, bool show)
{
    m_state->setBoolean(PageAgentState::showPaintRects, show);
    if (!show)
        m_page->mainFrame()->view()->invalidate();
}

void InspectorPageAgent::getScriptExecutionStatus(ErrorString*, PageCommandHandler::Result::Enum* status)
{
    bool disabledByScriptController = false;
    bool disabledInSettings = false;
    Frame* frame = mainFrame();
    if (frame) {
        disabledByScriptController = !frame->script()->canExecuteScripts(NotAboutToExecuteScript);
        if (frame->settings())
            disabledInSettings = !frame->settings()->isScriptEnabled();
    }

    if (!disabledByScriptController) {
        *status = PageCommandHandler::Result::Allowed;
        return;
    }

    if (disabledInSettings)
        *status = PageCommandHandler::Result::Disabled;
    else
        *status = PageCommandHandler::Result::Forbidden;
}

void InspectorPageAgent::setScriptExecutionDisabled(ErrorString*, bool value)
{
    m_state->setBoolean(PageAgentState::pageAgentScriptExecutionDisabled, value);
    if (!mainFrame())
        return;

    Settings* settings = mainFrame()->settings();
    if (settings)
        settings->setScriptEnabled(!value);
}

void InspectorPageAgent::didClearWindowObjectInWorld(Frame* frame, DOMWrapperWorld* world)
{
    if (world != mainThreadNormalWorld())
        return;

    if (frame == m_page->mainFrame())
        m_injectedScriptManager->discardInjectedScripts();

    if (!m_frontend)
        return;

    RefPtr<InspectorObject> scripts = m_state->getObject(PageAgentState::pageAgentScriptsToEvaluateOnLoad);
    if (scripts) {
        InspectorObject::const_iterator end = scripts->end();
        for (InspectorObject::const_iterator it = scripts->begin(); it != end; ++it) {
            String scriptText;
            if (it->second->asString(&scriptText))
                m_injectedScriptManager->injectScript(scriptText, mainWorldScriptState(frame));
        }
    }
    if (!m_scriptToEvaluateOnLoadOnce.isEmpty())
        m_injectedScriptManager->injectScript(m_scriptToEvaluateOnLoadOnce, mainWorldScriptState(frame));
}

void InspectorPageAgent::domContentEventFired()
{
    m_didLoadEventFire = true;
    m_frontend->domContentEventFired(currentTime());
}

void InspectorPageAgent::loadEventFired()
{
    m_frontend->loadEventFired(currentTime());
}

void InspectorPageAgent::frameNavigated(DocumentLoader* loader)
{
    if (loader->frame() == m_page->mainFrame()) {
        m_scriptToEvaluateOnLoadOnce = m_pendingScriptToEvaluateOnLoadOnce;
        m_pendingScriptToEvaluateOnLoadOnce = String();
    }
    m_frontend->frameNavigated(buildObjectForFrame(loader->frame()));
}

void InspectorPageAgent::frameDetached(Frame* frame)
{
    HashMap<Frame*, String>::iterator iterator = m_frameToIdentifier.find(frame);
    if (iterator != m_frameToIdentifier.end()) {
        m_frontend->frameDetached(iterator->second);
        m_identifierToFrame.remove(iterator->second);
        m_frameToIdentifier.remove(iterator);
    }
}

Frame* InspectorPageAgent::mainFrame()
{
    return m_page->mainFrame();
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

Frame* InspectorPageAgent::assertFrame(ErrorString* errorString, String frameId)
{
    Frame* frame = frameForId(frameId);
    if (!frame)
        *errorString = "No frame for given id found";

    return frame;
}

// static
DocumentLoader* InspectorPageAgent::assertDocumentLoader(ErrorString* errorString, Frame* frame)
{
    FrameLoader* frameLoader = frame->loader();
    DocumentLoader* documentLoader = frameLoader ? frameLoader->documentLoader() : 0;
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

void InspectorPageAgent::applyScreenWidthOverride(long* width)
{
    long widthOverride = m_state->getLong(PageAgentState::pageAgentScreenWidthOverride);
    if (widthOverride)
        *width = widthOverride;
}

void InspectorPageAgent::applyScreenHeightOverride(long* height)
{
    long heightOverride = m_state->getLong(PageAgentState::pageAgentScreenHeightOverride);
    if (heightOverride)
        *height = heightOverride;
}

void InspectorPageAgent::willPaint(GraphicsContext* context, const LayoutRect& rect)
{
    if (m_state->getBoolean(PageAgentState::showPaintRects)) {
        m_lastPaintContext = context;
        m_lastPaintRect = rect;
        m_lastPaintRect.inflate(-1);
    }
}

void InspectorPageAgent::didPaint()
{
    if (!m_lastPaintContext || !m_state->getBoolean(PageAgentState::showPaintRects))
        return;

    static int colorSelector = 0;
    const Color colors[] = {
        Color(0xFF, 0, 0, 0x3F),
        Color(0xFF, 0, 0xFF, 0x3F),
        Color(0, 0, 0xFF, 0x3F),
    };

    DOMNodeHighlighter::drawOutline(*m_lastPaintContext, m_lastPaintRect, colors[colorSelector++ % WTF_ARRAY_LENGTH(colors)]);

    m_lastPaintContext = 0;
}

void InspectorPageAgent::didLayout()
{
    if (!m_didLoadEventFire)
        return;

    m_didLoadEventFire = false;
    int currentWidth = static_cast<int>(m_state->getLong(PageAgentState::pageAgentScreenWidthOverride));
    int currentHeight = static_cast<int>(m_state->getLong(PageAgentState::pageAgentScreenHeightOverride));

    if (currentWidth && currentHeight)
        m_client->autoZoomPageToFitWidth();
}

PassRefPtr<TypeBuilder::Page::Frame> InspectorPageAgent::buildObjectForFrame(Frame* frame)
{
    RefPtr<TypeBuilder::Page::Frame> frameObject = TypeBuilder::Page::Frame::create()
         .setId(frameId(frame))
         .setLoaderId(loaderId(frame->loader()->documentLoader()))
         .setUrl(frame->document()->url().string())
         .setMimeType(frame->loader()->documentLoader()->responseMIMEType());
    if (frame->tree()->parent())
        frameObject->setParentId(frameId(frame->tree()->parent()));
    if (frame->ownerElement()) {
        String name = frame->ownerElement()->getNameAttribute();
        if (name.isEmpty())
            name = frame->ownerElement()->getAttribute(HTMLNames::idAttr);
        frameObject->setName(name);
    }
    // FIXME: Make this field non-optional. https://bugs.webkit.org/show_bug.cgi?id=80857
    frameObject->setSecurityOrigin(frame->document()->securityOrigin()->toString());

    return frameObject;
}

PassRefPtr<TypeBuilder::Page::FrameResourceTree> InspectorPageAgent::buildObjectForFrameTree(Frame* frame)
{
    RefPtr<TypeBuilder::Page::Frame> frameObject = buildObjectForFrame(frame);
    RefPtr<TypeBuilder::Array<TypeBuilder::Page::FrameResourceTree::Resources> > subresources = TypeBuilder::Array<TypeBuilder::Page::FrameResourceTree::Resources>::create();
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
        if (cachedResource->status() == CachedResource::LoadError)
            resourceObject->setFailed(true);
        if (cachedResource->status() == CachedResource::Canceled)
            resourceObject->setCanceled(true);
        subresources->addItem(resourceObject);
    }

    RefPtr<TypeBuilder::Array<TypeBuilder::Page::FrameResourceTree> > childrenArray;
    for (Frame* child = frame->tree()->firstChild(); child; child = child->tree()->nextSibling()) {
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
    document->styleResolverChanged(RecalcStyleImmediately);
    InspectorInstrumentation::mediaQueryResultChanged(document);
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
