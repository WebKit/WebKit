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

#include "CSSHelper.h"
#include "CachedResourceLoader.h"
#include "Document.h"
#include "HTMLTokenizer.h"
#include "HTMLTreeBuilder.h"
#include "HTMLLinkElement.h"
#include "HTMLNames.h"

namespace WebCore {

using namespace HTMLNames;

namespace {

class PreloadTask {
public:
    PreloadTask(const HTMLToken& token)
        : m_tagName(token.name().data(), token.name().size())
        , m_linkIsStyleSheet(false)
    {
        processAttributes(token.attributes());
    }

    void processAttributes(const HTMLToken::AttributeList& attributes)
    {
        if (m_tagName != scriptTag && m_tagName != imgTag && m_tagName != linkTag)
            return;

        for (HTMLToken::AttributeList::const_iterator iter = attributes.begin();
             iter != attributes.end(); ++iter) {
            AtomicString attributeName(iter->m_name.data(), iter->m_name.size());
            String attributeValue(iter->m_value.data(), iter->m_value.size());

            if (attributeName == charsetAttr)
                m_charset = attributeValue;

            if (m_tagName == scriptTag || m_tagName == imgTag) {
                if (attributeName == srcAttr)
                    setUrlToLoad(attributeValue);
            } else if (m_tagName == linkTag) {
                if (attributeName == hrefAttr)
                    setUrlToLoad(attributeValue);
                else if (attributeName == relAttr)
                    m_linkIsStyleSheet = relAttributeIsStyleSheet(attributeValue);
            }
        }
    }

    bool relAttributeIsStyleSheet(const String& attributeValue)
    {
        ASSERT(m_tagName == linkTag);
        HTMLLinkElement::RelAttribute rel;
        HTMLLinkElement::tokenizeRelAttribute(attributeValue, rel);
        return rel.m_isStyleSheet && !rel.m_isAlternate && !rel.m_isIcon && !rel.m_isDNSPrefetch;
    }

    void setUrlToLoad(const String& attributeValue)
    {
        // We only respect the first src/href, per HTML5:
        // http://www.whatwg.org/specs/web-apps/current-work/multipage/tokenization.html#attribute-name-state
        if (!m_urlToLoad.isEmpty())
            return;
        m_urlToLoad = deprecatedParseURL(attributeValue);
    }

    void preload(Document* document, bool scanningBody)
    {
        if (m_urlToLoad.isEmpty())
            return;

        CachedResourceLoader* cachedResourceLoader = document->cachedResourceLoader();
        if (m_tagName == scriptTag)
            cachedResourceLoader->preload(CachedResource::Script, m_urlToLoad, m_charset, scanningBody);
        else if (m_tagName == imgTag) 
            cachedResourceLoader->preload(CachedResource::ImageResource, m_urlToLoad, String(), scanningBody);
        else if (m_tagName == linkTag && m_linkIsStyleSheet) 
            cachedResourceLoader->preload(CachedResource::CSSStyleSheet, m_urlToLoad, m_charset, scanningBody);
    }

    const AtomicString& tagName() const { return m_tagName; }

private:
    AtomicString m_tagName;
    String m_urlToLoad;
    String m_charset;
    bool m_linkIsStyleSheet;
};

} // namespace

HTMLPreloadScanner::HTMLPreloadScanner(Document* document)
    : m_document(document)
    , m_cssScanner(document)
    , m_tokenizer(HTMLTokenizer::create())
    , m_bodySeen(false)
    , m_inStyle(false)
{
}

void HTMLPreloadScanner::appendToEnd(const SegmentedString& source)
{
    m_source.append(source);
}

void HTMLPreloadScanner::scan()
{
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
        if (m_token.type() == HTMLToken::Character)
            m_cssScanner.scan(m_token, scanningBody());
        else if (m_token.type() == HTMLToken::EndTag) {
            m_inStyle = false;
            m_cssScanner.reset();
        }
    }

    if (m_token.type() != HTMLToken::StartTag)
        return;

    PreloadTask task(m_token);
    m_tokenizer->setState(HTMLTreeBuilder::adjustedLexerState(m_tokenizer->state(), task.tagName(), m_document->frame()));
    if (task.tagName() == scriptTag) {
        // The tree builder handles scriptTag separately from the other tokenizer
        // state adjustments, so we need to handle it separately too.
        ASSERT(m_tokenizer->state() == HTMLTokenizer::DataState);
        m_tokenizer->setState(HTMLTokenizer::ScriptDataState);
    }

    if (task.tagName() == bodyTag)
        m_bodySeen = true;

    if (task.tagName() == styleTag)
        m_inStyle = true;

    task.preload(m_document, scanningBody());
}

bool HTMLPreloadScanner::scanningBody() const
{
    return m_document->body() || m_bodySeen;
}

}
