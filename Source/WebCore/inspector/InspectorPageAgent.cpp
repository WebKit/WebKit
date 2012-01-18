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

#include "InspectorPageAgent.h"

#if ENABLE(INSPECTOR)

#include "Base64.h"
#include "CachedCSSStyleSheet.h"
#include "CachedResource.h"
#include "CachedResourceLoader.h"
#include "CachedScript.h"
#include "ContentSearchUtils.h"
#include "Cookie.h"
#include "CookieJar.h"
#include "DOMEditor.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "Frame.h"
#include "FrameLoadRequest.h"
#include "HTMLFrameOwnerElement.h"
#include "HTMLNames.h"
#include "IdentifiersFactory.h"
#include "InjectedScriptManager.h"
#include "InspectorFrontend.h"
#include "InspectorState.h"
#include "InspectorValues.h"
#include "InstrumentingAgents.h"
#include "MemoryCache.h"
#include "Page.h"
#include "RegularExpression.h"
#include "ScriptObject.h"
#include "SharedBuffer.h"
#include "TextEncoding.h"
#include "UserGestureIndicator.h"
#include "WindowFeatures.h"

#include <wtf/CurrentTime.h>
#include <wtf/ListHashSet.h>
#include <wtf/Vector.h>

using namespace std;

namespace WebCore {

namespace PageAgentState {
static const char pageAgentEnabled[] = "pageAgentEnabled";
static const char pageAgentScriptsToEvaluateOnLoad[] = "pageAgentScriptsToEvaluateOnLoad";
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
    return type == InspectorPageAgent::StylesheetResource || type == InspectorPageAgent::ScriptResource;
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

    if (cachedResource) {
        switch (cachedResource->type()) {
        case CachedResource::CSSStyleSheet:
            *result = static_cast<CachedCSSStyleSheet*>(cachedResource)->sheetText();
            return true;
        case CachedResource::Script:
            *result = static_cast<CachedScript*>(cachedResource)->script();
            return true;
        default:
            if (hasZeroSize) {
                *result = "";
                return true;
            }
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

PassOwnPtr<InspectorPageAgent> InspectorPageAgent::create(InstrumentingAgents* instrumentingAgents, Page* page, InspectorState* state, InjectedScriptManager* injectedScriptManager)
{
    return adoptPtr(new InspectorPageAgent(instrumentingAgents, page, state, injectedScriptManager));
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

String InspectorPageAgent::resourceTypeString(InspectorPageAgent::ResourceType resourceType)
{
    switch (resourceType) {
    case DocumentResource:
        return "Document";
    case ImageResource:
        return "Image";
    case FontResource:
        return "Font";
    case StylesheetResource:
        return "Stylesheet";
    case ScriptResource:
        return "Script";
    case XHRResource:
        return "XHR";
    case WebSocketResource:
        return "WebSocket";
    case OtherResource:
        return "Other";
    }
    return "Other";
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
    default:
        break;
    }
    return InspectorPageAgent::OtherResource;
}

String InspectorPageAgent::cachedResourceTypeString(const CachedResource& cachedResource)
{
    return resourceTypeString(cachedResourceType(cachedResource));
}

InspectorPageAgent::InspectorPageAgent(InstrumentingAgents* instrumentingAgents, Page* page, InspectorState* inspectorState, InjectedScriptManager* injectedScriptManager)
    : InspectorBaseAgent<InspectorPageAgent>("Page", instrumentingAgents, inspectorState)
    , m_page(page)
    , m_injectedScriptManager(injectedScriptManager)
    , m_frontend(0)
    , m_lastScriptIdentifier(0)
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
    }
}

void InspectorPageAgent::enable(ErrorString*)
{
    m_state->setBoolean(PageAgentState::pageAgentEnabled, true);
    m_instrumentingAgents->setInspectorPageAgent(this);
}

void InspectorPageAgent::disable(ErrorString*)
{
    m_state->setBoolean(PageAgentState::pageAgentEnabled, false);
    m_instrumentingAgents->setInspectorPageAgent(0);
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

void InspectorPageAgent::open(ErrorString*, const String& url, const bool* const inNewWindow)
{
    UserGestureIndicator indicator(DefinitelyProcessingUserGesture);

    Frame* mainFrame = m_page->mainFrame();
    Frame* frame;
    if (inNewWindow && *inNewWindow) {
        FrameLoadRequest request(mainFrame->document()->securityOrigin(), ResourceRequest(), "_blank");

        bool created;
        WindowFeatures windowFeatures;
        frame = WebCore::createWindow(mainFrame, mainFrame, request, windowFeatures, created);
        if (!frame)
            return;

        frame->loader()->setOpener(mainFrame);
        frame->page()->setOpenedByDOM();
    } else
        frame = mainFrame;

    // FIXME: Why does one use mainFrame and the other frame?
    frame->loader()->changeLocation(mainFrame->document()->securityOrigin(), frame->document()->completeURL(url), "", false, false);
}

static PassRefPtr<InspectorObject> buildObjectForCookie(const Cookie& cookie)
{
    RefPtr<InspectorObject> value = InspectorObject::create();
    value->setString("name", cookie.name);
    value->setString("value", cookie.value);
    value->setString("domain", cookie.domain);
    value->setString("path", cookie.path);
    value->setNumber("expires", cookie.expires);
    value->setNumber("size", (cookie.name.length() + cookie.value.length()));
    value->setBoolean("httpOnly", cookie.httpOnly);
    value->setBoolean("secure", cookie.secure);
    value->setBoolean("session", cookie.session);
    return value;
}

static PassRefPtr<InspectorArray> buildArrayForCookies(ListHashSet<Cookie>& cookiesList)
{
    RefPtr<InspectorArray> cookies = InspectorArray::create();

    ListHashSet<Cookie>::iterator end = cookiesList.end();
    ListHashSet<Cookie>::iterator it = cookiesList.begin();
    for (int i = 0; it != end; ++it, i++)
        cookies->pushObject(buildObjectForCookie(*it));

    return cookies;
}

static Vector<CachedResource*> cachedResourcesForFrame(Frame* frame)
{
    Vector<CachedResource*> result;

    const CachedResourceLoader::DocumentResourceMap& allResources = frame->document()->cachedResourceLoader()->allCachedResources();
    CachedResourceLoader::DocumentResourceMap::const_iterator end = allResources.end();
    for (CachedResourceLoader::DocumentResourceMap::const_iterator it = allResources.begin(); it != end; ++it)
        result.append(it->second.get());

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

void InspectorPageAgent::getCookies(ErrorString*, RefPtr<InspectorArray>& cookies, WTF::String* cookiesString)
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

    if (rawCookiesImplemented)
        cookies = buildArrayForCookies(rawCookiesList);
    else
        *cookiesString = stringCookiesList;
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

void InspectorPageAgent::getResourceTree(ErrorString*, RefPtr<InspectorObject>& object)
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

void InspectorPageAgent::searchInResource(ErrorString*, const String& frameId, const String& url, const String& query, const bool* const optionalCaseSensitive, const bool* const optionalIsRegex, RefPtr<InspectorArray>& results)
{
    results = InspectorArray::create();

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

static PassRefPtr<InspectorObject> buildObjectForSearchResult(const String& frameId, const String& url, int matchesCount)
{
    RefPtr<InspectorObject> result = InspectorObject::create();
    result->setString("frameId", frameId);
    result->setString("url", url);
    result->setNumber("matchesCount", matchesCount);

    return result;
}

void InspectorPageAgent::searchInResources(ErrorString*, const String& text, const bool* const optionalCaseSensitive, const bool* const optionalIsRegex, RefPtr<InspectorArray>& results)
{
    RefPtr<InspectorArray> searchResults = InspectorArray::create();

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
                    searchResults->pushValue(buildObjectForSearchResult(frameId(frame), cachedResource->url(), matchesCount));
            }
        }
        if (mainResourceContent(frame, false, &content)) {
            int matchesCount = ContentSearchUtils::countRegularExpressionMatches(regex, content);
            if (matchesCount)
                searchResults->pushValue(buildObjectForSearchResult(frameId(frame), frame->document()->url(), matchesCount));
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
    DOMEditor editor(document);
    editor.patchDocument(html);
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

PassRefPtr<InspectorObject> InspectorPageAgent::buildObjectForFrame(Frame* frame)
{
    RefPtr<InspectorObject> frameObject = InspectorObject::create();
    frameObject->setString("id", frameId(frame));
    if (frame->tree()->parent())
        frameObject->setString("parentId", frameId(frame->tree()->parent()));
    if (frame->ownerElement()) {
        String name = frame->ownerElement()->getAttribute(HTMLNames::nameAttr);
        if (name.isEmpty())
            name = frame->ownerElement()->getAttribute(HTMLNames::idAttr);
        frameObject->setString("name", name);
    }
    frameObject->setString("url", frame->document()->url().string());
    frameObject->setString("loaderId", loaderId(frame->loader()->documentLoader()));
    frameObject->setString("securityOrigin", frame->document()->securityOrigin()->toString());
    frameObject->setString("mimeType", frame->loader()->documentLoader()->responseMIMEType());

    return frameObject;
}

PassRefPtr<InspectorObject> InspectorPageAgent::buildObjectForFrameTree(Frame* frame)
{
    RefPtr<InspectorObject> result = InspectorObject::create();
    RefPtr<InspectorObject> frameObject = buildObjectForFrame(frame);
    result->setObject("frame", frameObject);

    RefPtr<InspectorArray> subresources = InspectorArray::create();
    result->setArray("resources", subresources);

    Vector<CachedResource*> allResources = cachedResourcesForFrame(frame);
    for (Vector<CachedResource*>::const_iterator it = allResources.begin(); it != allResources.end(); ++it) {
        CachedResource* cachedResource = *it;
        RefPtr<InspectorObject> resourceObject = InspectorObject::create();
        resourceObject->setString("url", cachedResource->url());
        resourceObject->setString("type", cachedResourceTypeString(*cachedResource));
        resourceObject->setString("mimeType", cachedResource->response().mimeType());
        subresources->pushValue(resourceObject);
    }

    RefPtr<InspectorArray> childrenArray;
    for (Frame* child = frame->tree()->firstChild(); child; child = child->tree()->nextSibling()) {
        if (!childrenArray) {
            childrenArray = InspectorArray::create();
            result->setArray("childFrames", childrenArray);
        }
        childrenArray->pushObject(buildObjectForFrameTree(child));
    }
    return result;
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
