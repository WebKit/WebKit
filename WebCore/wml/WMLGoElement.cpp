/**
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved.
 *               http://www.torchmobile.com/
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"

#if ENABLE(WML)
#include "WMLGoElement.h"

#include "Frame.h"
#include "FrameLoader.h"
#include "HTMLNames.h"
#include "ResourceRequest.h"
#include "WMLCardElement.h"
#include "WMLDocument.h"
#include "WMLNames.h"
#include "WMLPageState.h"
#include "WMLPostfieldElement.h"
#include "WMLTimerElement.h"
#include "WMLVariables.h"

namespace WebCore {

using namespace WMLNames;

WMLGoElement::WMLGoElement(const QualifiedName& tagName, Document* doc)
    : WMLTaskElement(tagName, doc)
    , m_contentType("application/x-www-form-urlencoded")
    , m_isMultiPart(false)
    , m_isPostMethod(false)
{
}
 
void WMLGoElement::registerPostfieldElement(WMLPostfieldElement* postfield)
{
    m_postfieldElements.add(postfield);
}

void WMLGoElement::parseMappedAttribute(MappedAttribute* attr)
{
    if (attr->name() == HTMLNames::methodAttr) {
        const AtomicString& value = attr->value();

        if (value == "POST")
            m_isPostMethod = true;
        else if (value == "GET")
            m_isPostMethod = false;
    } else if (attr->name() == HTMLNames::enctypeAttr)
        parseContentType(parseValueSubstitutingVariableReferences(attr->value()));
    else if (attr->name() == HTMLNames::accept_charsetAttr)
        m_acceptCharset = parseValueForbiddingVariableReferences(attr->value());
    else
        WMLTaskElement::parseMappedAttribute(attr);
}

void WMLGoElement::executeTask(Event* event)
{
    Document* doc = document();
    WMLPageState* pageState = wmlPageStateForDocument(doc);
    if (!pageState)
        return;

    WMLCardElement* card = pageState->activeCard();
    if (!card)
        return;

    Frame* frame = doc->frame();
    if (!frame)
        return;

    FrameLoader* loader = frame->loader();
    if (!loader)
        return;

    String href = getAttribute(HTMLNames::hrefAttr);
    if (href.isEmpty())
        return;

    // Substitute variables within target url attribute value
    KURL url = doc->completeURL(substituteVariableReferences(href, doc, WMLVariableEscapingEscape));
    if (url.isEmpty())
        return;

    storeVariableState(pageState);

    // Stop the timer of the current card if it is active
    if (WMLTimerElement* eventTimer = card->eventTimer())
        eventTimer->stop();

    // FIXME: 'newcontext' handling not implemented for external cards
    bool inSameDeck = doc->url().path() == url.path();
    if (inSameDeck && url.hasRef()) {
        // Force frame loader to load the URL with fragment identifier
        loader->setForceReloadWmlDeck(true);

        if (WMLCardElement* card = WMLCardElement::findNamedCardInDocument(doc, url.ref())) {
            if (card->isNewContext())
                pageState->reset();
        }
    }

    // Prepare loading the destination url
    ResourceRequest request(url);

    if (getAttribute(sendrefererAttr) == "true")
        request.setHTTPReferrer(loader->outgoingReferrer());

    String cacheControl = getAttribute(cache_controlAttr);

    if (m_isPostMethod)
        preparePOSTRequest(request, inSameDeck, cacheControl);
    else {
        // Eventually display error message?
        if (m_isMultiPart)
            return;

        prepareGETRequest(request, url);
    }

    // Set HTTP cache-control header if needed
    if (!cacheControl.isEmpty()) {
        request.setHTTPHeaderField("cache-control", cacheControl);

        if (cacheControl == "no-cache")
            request.setCachePolicy(ReloadIgnoringCacheData);
    }

    loader->load(request);
}

void WMLGoElement::parseContentType(const String& type)
{
    if (type.contains("multipart", false) || type.contains("form-data", false)) {
        m_contentType = "multipart/form-data";
        m_isMultiPart = true;
    } else {
        m_contentType = "application/x-www-form-urlencoded";
        m_isMultiPart = false;
    }
}

void WMLGoElement::preparePOSTRequest(ResourceRequest& request, bool inSameDeck, const String& cacheControl)
{
    request.setHTTPMethod("POST");

    if (inSameDeck && cacheControl != "no-cache") {
        request.setCachePolicy(ReturnCacheDataDontLoad);
        return;
    }

    // FIXME: Implement POST method.
    /*
    RefPtr<FormData> data;

    if (m_isMultiPart) { // multipart/form-data
        Vector<char> boundary;
        getUniqueBoundaryString(boundary);
        data = createFormData(loader, boundary.data());
        request.setHTTPContentType(m_contentType + "; boundary=" + boundary.data());
    } else {
        // text/plain or application/x-www-form-urlencoded
        data = createFormData(loader, 0);
        request.setHTTPContentType(m_contentType);
    }

    request.setHTTPBody(data.get());
    */
}

void WMLGoElement::prepareGETRequest(ResourceRequest& request, const KURL& url)
{
    String queryString;

    HashSet<WMLPostfieldElement*>::iterator it = m_postfieldElements.begin();
    HashSet<WMLPostfieldElement*>::iterator end = m_postfieldElements.end();

    for (; it != end; ++it) {
        WMLPostfieldElement* postfield = (*it);

        if (!queryString.isEmpty())
            queryString += "&";

        queryString += encodeWithURLEscapeSequences(postfield->name()) + "m" +
                       encodeWithURLEscapeSequences(postfield->value());
    }

    KURL remoteURL(url);
    remoteURL.setQuery(queryString);

    request.setURL(remoteURL);
    request.setHTTPMethod("GET");


}

}

#endif
