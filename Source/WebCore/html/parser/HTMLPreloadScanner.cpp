/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2009 Torch Mobile, Inc. http://www.torchmobile.com/
 * Copyright (C) 2010 Google Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "HTMLPreloadScanner.h"

#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "HTMLParserOptions.h"
#include "HTMLTokenizer.h"
#include "InputTypeNames.h"
#include "LinkRelAttribute.h"
#include "MediaList.h"
#include "MediaQueryEvaluator.h"
#include <wtf/Functional.h>
#include <wtf/MainThread.h>

namespace WebCore {

using namespace HTMLNames;

static bool isStartTag(const HTMLToken& token)
{
    return token.type() == HTMLToken::StartTag;
}

static bool isStartOrEndTag(const HTMLToken& token)
{
    return token.type() == HTMLToken::EndTag || isStartTag(token);
}

class StartTagScanner {
public:
    StartTagScanner(const AtomicString& tagName, const HTMLToken::AttributeList& attributes)
        : m_tagName(tagName)
        , m_linkIsStyleSheet(false)
        , m_linkMediaAttributeIsScreen(true)
        , m_inputIsImage(false)
    {
        processAttributes(attributes);
    }

    void processAttributes(const HTMLToken::AttributeList& attributes)
    {
        if (m_tagName != imgTag
            && m_tagName != inputTag
            && m_tagName != linkTag
            && m_tagName != scriptTag)
            return;

        for (HTMLToken::AttributeList::const_iterator iter = attributes.begin();
             iter != attributes.end(); ++iter) {
            AtomicString attributeName(iter->name);
            String attributeValue = StringImpl::create8BitIfPossible(iter->value);

            if (attributeName == charsetAttr)
                m_charset = attributeValue;

            if (m_tagName == scriptTag || m_tagName == imgTag) {
                if (attributeName == srcAttr)
                    setUrlToLoad(attributeValue);
                else if (attributeName == crossoriginAttr && !attributeValue.isNull())
                    m_crossOriginMode = stripLeadingAndTrailingHTMLSpaces(attributeValue);
            } else if (m_tagName == linkTag) {
                if (attributeName == hrefAttr)
                    setUrlToLoad(attributeValue);
                else if (attributeName == relAttr)
                    m_linkIsStyleSheet = relAttributeIsStyleSheet(attributeValue);
                else if (attributeName == mediaAttr)
                    m_linkMediaAttributeIsScreen = linkMediaAttributeIsScreen(attributeValue);
            } else if (m_tagName == inputTag) {
                if (attributeName == srcAttr)
                    setUrlToLoad(attributeValue);
                else if (attributeName == typeAttr)
                    m_inputIsImage = equalIgnoringCase(attributeValue, InputTypeNames::image());
            }
        }
    }

    static bool relAttributeIsStyleSheet(const String& attributeValue)
    {
        LinkRelAttribute rel(attributeValue);
        return rel.m_isStyleSheet && !rel.m_isAlternate && rel.m_iconType == InvalidIcon && !rel.m_isDNSPrefetch;
    }

    static bool linkMediaAttributeIsScreen(const String& attributeValue)
    {
        if (attributeValue.isEmpty())
            return true;
        RefPtr<MediaQuerySet> mediaQueries = MediaQuerySet::createAllowingDescriptionSyntax(attributeValue);
    
        // Only preload screen media stylesheets. Used this way, the evaluator evaluates to true for any 
        // rules containing complex queries (full evaluation is possible but it requires a frame and a style selector which
        // may be problematic here).
        MediaQueryEvaluator mediaQueryEvaluator("screen");
        return mediaQueryEvaluator.eval(mediaQueries.get());
    }

    void setUrlToLoad(const String& attributeValue)
    {
        // We only respect the first src/href, per HTML5:
        // http://www.whatwg.org/specs/web-apps/current-work/multipage/tokenization.html#attribute-name-state
        if (!m_urlToLoad.isEmpty())
            return;
        m_urlToLoad = stripLeadingAndTrailingHTMLSpaces(attributeValue);
    }

    const String& charset() const
    {
        // FIXME: Its not clear that this if is needed, the loader probably ignores charset for image requests anyway.
        if (m_tagName == imgTag)
            return emptyString();
        return m_charset;
    }

    CachedResource::Type resourceType() const
    {
        if (m_tagName == scriptTag)
            return CachedResource::Script;
        if (m_tagName == imgTag || (m_tagName == inputTag && m_inputIsImage))
            return CachedResource::ImageResource;
        if (m_tagName == linkTag && m_linkIsStyleSheet && m_linkMediaAttributeIsScreen)
            return CachedResource::CSSStyleSheet;
        ASSERT_NOT_REACHED();
        return CachedResource::RawResource;
    }

    bool shouldPreload()
    {
        if (m_urlToLoad.isEmpty())
            return false;

        if (m_tagName == linkTag && (!m_linkIsStyleSheet || !m_linkMediaAttributeIsScreen))
            return false;

        if (m_tagName == inputTag && !m_inputIsImage)
            return false;
        return true;
    }

    PassOwnPtr<PreloadRequest> createPreloadRequest(const KURL& predictedBaseURL)
    {
        if (!shouldPreload())
            return nullptr;

        OwnPtr<PreloadRequest> request = PreloadRequest::create(m_tagName, m_urlToLoad, predictedBaseURL, resourceType());
        request->setCrossOriginModeAllowsCookies(crossOriginModeAllowsCookies());
        request->setCharset(charset());
        return request.release();
    }

    const AtomicString& tagName() const { return m_tagName; }

private:

    bool crossOriginModeAllowsCookies()
    {
        return m_crossOriginMode.isNull() || equalIgnoringCase(m_crossOriginMode, "use-credentials");
    }

    AtomicString m_tagName;
    String m_urlToLoad;
    String m_charset;
    String m_crossOriginMode;
    bool m_linkIsStyleSheet;
    bool m_linkMediaAttributeIsScreen;
    bool m_inputIsImage;
};

HTMLPreloadScanner::HTMLPreloadScanner(const HTMLParserOptions& options, const KURL& documentURL)
    : m_tokenizer(HTMLTokenizer::create(options))
    , m_inStyle(false)
    , m_documentURL(documentURL)
#if ENABLE(TEMPLATE_ELEMENT)
    , m_templateCount(0)
#endif
{
}

void HTMLPreloadScanner::appendToEnd(const SegmentedString& source)
{
    m_source.append(source);
}

// This function exists for convenience on the main thread and is not used by the background-thread preload scanner.
void HTMLPreloadScanner::scan(HTMLResourcePreloader* preloader, const KURL& startingBaseElementURL)
{
    ASSERT(isMainThread()); // HTMLTokenizer::updateStateFor only works on the main thread.
    // When we start scanning, our best prediction of the baseElementURL is the real one!
    if (!startingBaseElementURL.isEmpty())
        m_predictedBaseElementURL = startingBaseElementURL;

    Vector<OwnPtr<PreloadRequest> > requests;
    // Note: m_token is only used from this function and for the main thread.
    // All other functions are passed a token.
    while (m_tokenizer->nextToken(m_source, m_token)) {
        if (isStartTag(m_token))
            m_tokenizer->updateStateFor(AtomicString(m_token.name()));
        processToken(m_token, requests);
        m_token.clear();
    }
    for (size_t i = 0; i < requests.size(); i++)
        preloader->preload(requests[i].release());
}

#if ENABLE(TEMPLATE_ELEMENT)
bool HTMLPreloadScanner::processPossibleTemplateTag(const AtomicString& tagName, const HTMLToken& token)
{
    if (isStartOrEndTag(token) && tagName == templateTag) {
        if (isStartTag(token))
            m_templateCount++;
        else
            m_templateCount--;
        return true; // Twas our token.
    }
    // If we're in a template we "consume" all tokens.
    return m_templateCount > 0;
}
#endif

bool HTMLPreloadScanner::processPossibleStyleTag(const AtomicString& tagName, const HTMLToken& token)
{
    ASSERT(isStartOrEndTag(token));
    if (tagName != styleTag)
        return false;

    m_inStyle = isStartTag(token);

    if (!m_inStyle)
        m_cssScanner.reset();

    return true;
}

bool HTMLPreloadScanner::processPossibleBaseTag(const AtomicString& tagName, const HTMLToken& token)
{
    ASSERT(isStartTag(token));
    if (tagName != baseTag)
        return false;

    // The first <base> element is the one that wins.
    if (!m_predictedBaseElementURL.isEmpty())
        return true;

    for (HTMLToken::AttributeList::const_iterator iter = token.attributes().begin(); iter != token.attributes().end(); ++iter) {
        AtomicString attributeName(iter->name);
        if (attributeName == hrefAttr) {
            String hrefValue = StringImpl::create8BitIfPossible(iter->value);
            m_predictedBaseElementURL = KURL(m_documentURL, stripLeadingAndTrailingHTMLSpaces(hrefValue));
            break;
        }
    }
    return true;
}

void HTMLPreloadScanner::processToken(const HTMLToken& token, Vector<OwnPtr<PreloadRequest> >& requests)
{
    // <style> is the only place we search for urls in non-start/end-tag tokens.
    if (m_inStyle) {
        if (token.type() != HTMLToken::Character)
            return;
        return m_cssScanner.scan(token, requests);
    }
    if (!isStartOrEndTag(token))
        return;

    AtomicString tagName(token.name());
#if ENABLE(TEMPLATE_ELEMENT)
    if (processPossibleTemplateTag(tagName, token))
        return;
#endif
    if (processPossibleStyleTag(tagName, token))
        return;
    if (!isStartTag(token))
        return;
    if (processPossibleBaseTag(tagName, token))
        return;

    StartTagScanner scanner(tagName, token.attributes());
    OwnPtr<PreloadRequest> request =  scanner.createPreloadRequest(m_predictedBaseElementURL);
    if (request)
        requests.append(request.release());
}

}
