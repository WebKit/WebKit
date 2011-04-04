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

#include "CachedResourceLoader.h"
#include "Cookie.h"
#include "CookieJar.h"
#include "DocumentLoader.h"
#include "Frame.h"
#include "FrameLoadRequest.h"
#include "InjectedScriptManager.h"
#include "InspectorFrontend.h"
#include "InspectorValues.h"
#include "InstrumentingAgents.h"
#include "Page.h"
#include "ScriptObject.h"
#include "UserGestureIndicator.h"
#include "WindowFeatures.h"
#include <wtf/CurrentTime.h>
#include <wtf/ListHashSet.h>

namespace WebCore {

PassOwnPtr<InspectorPageAgent> InspectorPageAgent::create(InstrumentingAgents* instrumentingAgents, Page* inspectedPage, InjectedScriptManager* injectedScriptManager)
{
    return adoptPtr(new InspectorPageAgent(instrumentingAgents, inspectedPage, injectedScriptManager));
}

InspectorPageAgent::InspectorPageAgent(InstrumentingAgents* instrumentingAgents, Page* inspectedPage, InjectedScriptManager* injectedScriptManager)
    : m_instrumentingAgents(instrumentingAgents)
    , m_inspectedPage(inspectedPage)
    , m_injectedScriptManager(injectedScriptManager)
    , m_frontend(0)
{
}

void InspectorPageAgent::setFrontend(InspectorFrontend* frontend)
{
    m_frontend = frontend;
    m_instrumentingAgents->setInspectorPageAgent(this);

    // Initialize Web Inspector title.
    m_frontend->page()->inspectedURLChanged(m_inspectedPage->mainFrame()->document()->url().string());

}

void InspectorPageAgent::clearFrontend()
{
    m_instrumentingAgents->setInspectorPageAgent(0);
    m_userAgentOverride = "";
    m_frontend = 0;
}

void InspectorPageAgent::addScriptToEvaluateOnLoad(ErrorString*, const String& source)
{
    m_scriptsToEvaluateOnLoad.append(source);
}

void InspectorPageAgent::removeAllScriptsToEvaluateOnLoad(ErrorString*)
{
    m_scriptsToEvaluateOnLoad.clear();
}

void InspectorPageAgent::reloadPage(ErrorString*, const bool* const optionalIgnoreCache)
{
    m_inspectedPage->mainFrame()->loader()->reload(optionalIgnoreCache ? *optionalIgnoreCache : false);
}

void InspectorPageAgent::openInInspectedWindow(ErrorString*, const String& url)
{
    Frame* mainFrame = m_inspectedPage->mainFrame();

    FrameLoadRequest request(mainFrame->document()->securityOrigin(), ResourceRequest(), "_blank");

    bool created;
    WindowFeatures windowFeatures;
    Frame* newFrame = WebCore::createWindow(mainFrame, mainFrame, request, windowFeatures, created);
    if (!newFrame)
        return;

    UserGestureIndicator indicator(DefinitelyProcessingUserGesture);
    newFrame->loader()->setOpener(mainFrame);
    newFrame->page()->setOpenedByDOM();
    newFrame->loader()->changeLocation(mainFrame->document()->securityOrigin(), newFrame->loader()->completeURL(url), "", false, false);
}

void InspectorPageAgent::setUserAgentOverride(ErrorString*, const String& userAgent)
{
    m_userAgentOverride = userAgent;
}

void InspectorPageAgent::applyUserAgentOverride(String* userAgent) const
{
    if (!m_userAgentOverride.isEmpty())
        *userAgent = m_userAgentOverride;
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

void InspectorPageAgent::getCookies(ErrorString*, RefPtr<InspectorArray>* cookies, WTF::String* cookiesString)
{
    // If we can get raw cookies.
    ListHashSet<Cookie> rawCookiesList;

    // If we can't get raw cookies - fall back to String representation
    String stringCookiesList;

    // Return value to getRawCookies should be the same for every call because
    // the return value is platform/network backend specific, and the call will
    // always return the same true/false value.
    bool rawCookiesImplemented = false;

    for (Frame* frame = m_inspectedPage->mainFrame(); frame; frame = frame->tree()->traverseNext(m_inspectedPage->mainFrame())) {
        Document* document = frame->document();
        const CachedResourceLoader::DocumentResourceMap& allResources = document->cachedResourceLoader()->allCachedResources();
        CachedResourceLoader::DocumentResourceMap::const_iterator end = allResources.end();
        for (CachedResourceLoader::DocumentResourceMap::const_iterator it = allResources.begin(); it != end; ++it) {
            Vector<Cookie> docCookiesList;
            rawCookiesImplemented = getRawCookies(document, KURL(ParsedURLString, it->second->url()), docCookiesList);

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
        *cookies = buildArrayForCookies(rawCookiesList);
    else
        *cookiesString = stringCookiesList;
}

void InspectorPageAgent::deleteCookie(ErrorString*, const String& cookieName, const String& domain)
{
    for (Frame* frame = m_inspectedPage->mainFrame(); frame; frame = frame->tree()->traverseNext(m_inspectedPage->mainFrame())) {
        Document* document = frame->document();
        if (document->url().host() != domain)
            continue;
        const CachedResourceLoader::DocumentResourceMap& allResources = document->cachedResourceLoader()->allCachedResources();
        CachedResourceLoader::DocumentResourceMap::const_iterator end = allResources.end();
        for (CachedResourceLoader::DocumentResourceMap::const_iterator it = allResources.begin(); it != end; ++it)
            WebCore::deleteCookie(document, KURL(ParsedURLString, it->second->url()), cookieName);
    }
}

void InspectorPageAgent::inspectedURLChanged(const String& url)
{
    m_frontend->page()->inspectedURLChanged(url);
}

void InspectorPageAgent::restore()
{
    inspectedURLChanged(m_inspectedPage->mainFrame()->document()->url().string());
}

void InspectorPageAgent::didCommitLoad(const String& url)
{
    inspectedURLChanged(url);
}

void InspectorPageAgent::domContentEventFired()
{
     m_frontend->page()->domContentEventFired(currentTime());
}

void InspectorPageAgent::loadEventFired()
{
     m_frontend->page()->loadEventFired(currentTime());
}

void InspectorPageAgent::didClearWindowObjectInWorld(Frame* frame, DOMWrapperWorld* world)
{
    if (world != mainThreadNormalWorld())
        return;

    if (frame == m_inspectedPage->mainFrame())
        m_injectedScriptManager->discardInjectedScripts();

    if (m_scriptsToEvaluateOnLoad.size()) {
        ScriptState* scriptState = mainWorldScriptState(frame);
        for (Vector<String>::iterator it = m_scriptsToEvaluateOnLoad.begin();
             it != m_scriptsToEvaluateOnLoad.end(); ++it) {
            m_injectedScriptManager->injectScript(*it, scriptState);
        }
    }
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
