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

#include "CachedResourceLoader.h"
#include "Document.h"
#include "HTMLDocumentParser.h"
#include "HTMLTokenizer.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "HTMLParserOptions.h"
#include "InputTypeNames.h"
#include "LinkRelAttribute.h"
#include "MediaList.h"
#include "MediaQueryEvaluator.h"

namespace WebCore {

using namespace HTMLNames;

class PreloadTask {
public:
    explicit PreloadTask(const AtomicString& tagName, const HTMLToken::AttributeList& attributes)
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
            && m_tagName != scriptTag
            && m_tagName != baseTag)
            return;

        for (HTMLToken::AttributeList::const_iterator iter = attributes.begin();
             iter != attributes.end(); ++iter) {
            AtomicString attributeName(iter->m_name.data(), iter->m_name.size());
            String attributeValue = StringImpl::create8BitIfPossible(iter->m_value.data(), iter->m_value.size());

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
            } else if (m_tagName == baseTag) {
                if (attributeName == hrefAttr)
                    m_baseElementHref = stripLeadingAndTrailingHTMLSpaces(attributeValue);
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

    void preload(Document* document, const KURL& baseURL)
    {
        if (m_urlToLoad.isEmpty())
            return;

        CachedResourceLoader* cachedResourceLoader = document->cachedResourceLoader();
        CachedResourceRequest request(ResourceRequest(document->completeURL(m_urlToLoad, baseURL)));
        request.setInitiator(tagName());
        if (m_tagName == scriptTag) {
            request.mutableResourceRequest().setAllowCookies(crossOriginModeAllowsCookies());
            cachedResourceLoader->preload(CachedResource::Script, request, m_charset);
        }
        else if (m_tagName == imgTag || (m_tagName == inputTag && m_inputIsImage))
            cachedResourceLoader->preload(CachedResource::ImageResource, request, String());
        else if (m_tagName == linkTag && m_linkIsStyleSheet && m_linkMediaAttributeIsScreen) 
            cachedResourceLoader->preload(CachedResource::CSSStyleSheet, request, m_charset);
    }

    const AtomicString& tagName() const { return m_tagName; }
    const String& baseElementHref() const { return m_baseElementHref; }

private:

    bool crossOriginModeAllowsCookies()
    {
        return m_crossOriginMode.isNull() || equalIgnoringCase(m_crossOriginMode, "use-credentials");
    }

    AtomicString m_tagName;
    String m_urlToLoad;
    String m_charset;
    String m_baseElementHref;
    String m_crossOriginMode;
    bool m_linkIsStyleSheet;
    bool m_linkMediaAttributeIsScreen;
    bool m_inputIsImage;
};

HTMLPreloadScanner::HTMLPreloadScanner(Document* document, const HTMLParserOptions& options)
    : m_document(document)
    , m_cssScanner(document)
    , m_tokenizer(HTMLTokenizer::create(options))
    , m_inStyle(false)
#if ENABLE(TEMPLATE_ELEMENT)
    , m_templateCount(0)
#endif
{
}

void HTMLPreloadScanner::appendToEnd(const SegmentedString& source)
{
    m_source.append(source);
}

void HTMLPreloadScanner::scan()
{
    // When we start scanning, our best prediction of the baseElementURL is the real one!
    m_predictedBaseElementURL = m_document->baseElementURL();

    // FIXME: We should save and re-use these tokens in HTMLDocumentParser if
    // the pending script doesn't end up calling document.write.
    while (m_tokenizer->nextToken(m_source, m_token)) {
        processToken();
        m_token.clear();
    }
}

void HTMLPreloadScanner::processToken()
{
    if (m_inStyle) {
        if (m_token.type() == HTMLTokenTypes::Character)
            m_cssScanner.scan(m_token);
        else if (m_token.type() == HTMLTokenTypes::EndTag) {
            m_inStyle = false;
            m_cssScanner.reset();
        }
    }

    if (m_token.type() != HTMLTokenTypes::StartTag) {
#if ENABLE(TEMPLATE_ELEMENT)
        if (m_templateCount && m_token.type() == HTMLTokenTypes::EndTag && AtomicString(m_token.name().data(), m_token.name().size()) == templateTag)
            m_templateCount--;
#endif
        return;
    }

    AtomicString tagName(m_token.name().data(), m_token.name().size());

#if ENABLE(TEMPLATE_ELEMENT)
    if (tagName == templateTag)
        m_templateCount++;

    if (m_templateCount)
        return;
#endif

    PreloadTask task(tagName, m_token.attributes());
    m_tokenizer->updateStateFor(task.tagName());

    if (task.tagName() == styleTag)
        m_inStyle = true;

    if (task.tagName() == baseTag)
        updatePredictedBaseElementURL(KURL(m_document->url(), task.baseElementHref()));

    task.preload(m_document, m_predictedBaseElementURL.isEmpty() ? m_document->baseURL() : m_predictedBaseElementURL);
}

void HTMLPreloadScanner::updatePredictedBaseElementURL(const KURL& baseElementURL)
{
    // The first <base> element is the one that wins.
    if (!m_predictedBaseElementURL.isEmpty())
        return;
    m_predictedBaseElementURL = baseElementURL;
}

}
