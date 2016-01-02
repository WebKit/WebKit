/*
 * Copyright (C) 2008, 2014 Apple Inc. All Rights Reserved.
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
#include "HTMLSrcsetParser.h"
#include "HTMLTokenizer.h"
#include "InputTypeNames.h"
#include "LinkRelAttribute.h"
#include "SourceSizeList.h"
#include <wtf/MainThread.h>

namespace WebCore {

using namespace HTMLNames;

TokenPreloadScanner::TagId TokenPreloadScanner::tagIdFor(const HTMLToken::DataVector& data)
{
    AtomicString tagName(data);
    if (tagName == iframeTag)
        return TagId::Iframe;
    if (tagName == imgTag)
        return TagId::Img;
    if (tagName == inputTag)
        return TagId::Input;
    if (tagName == linkTag)
        return TagId::Link;
    if (tagName == scriptTag)
        return TagId::Script;
    if (tagName == styleTag)
        return TagId::Style;
    if (tagName == baseTag)
        return TagId::Base;
    if (tagName == templateTag)
        return TagId::Template;
    if (tagName == metaTag)
        return TagId::Meta;
    return TagId::Unknown;
}

String TokenPreloadScanner::initiatorFor(TagId tagId)
{
    switch (tagId) {
    case TagId::Iframe:
        return "iframe";
    case TagId::Img:
        return "img";
    case TagId::Input:
        return "input";
    case TagId::Link:
        return "link";
    case TagId::Script:
        return "script";
    case TagId::Unknown:
    case TagId::Style:
    case TagId::Base:
    case TagId::Template:
    case TagId::Meta:
        ASSERT_NOT_REACHED();
        return "unknown";
    }
    ASSERT_NOT_REACHED();
    return "unknown";
}

class TokenPreloadScanner::StartTagScanner {
public:
    explicit StartTagScanner(TagId tagId, float deviceScaleFactor = 1.0)
        : m_tagId(tagId)
        , m_linkIsStyleSheet(false)
        , m_metaIsViewport(false)
        , m_inputIsImage(false)
        , m_deviceScaleFactor(deviceScaleFactor)
    {
    }

    void processAttributes(const HTMLToken::AttributeList& attributes, Document& document)
    {
        ASSERT(isMainThread());
        if (m_tagId >= TagId::Unknown)
            return;

        for (auto& attribute : attributes) {
            AtomicString attributeName(attribute.name);
            String attributeValue = StringImpl::create8BitIfPossible(attribute.value);
            processAttribute(attributeName, attributeValue);
        }

        // Resolve between src and srcSet if we have them and the tag is img.
        if (m_tagId == TagId::Img && !m_srcSetAttribute.isEmpty()) {
            float sourceSize = 0;
            sourceSize = parseSizesAttribute(m_sizesAttribute, document.renderView(), document.frame());
            ImageCandidate imageCandidate = bestFitSourceForImageAttributes(m_deviceScaleFactor, m_urlToLoad, m_srcSetAttribute, sourceSize);
            setUrlToLoad(imageCandidate.string.toString(), true);
        }

        if (m_metaIsViewport && !m_metaContent.isNull())
            document.processViewport(m_metaContent, ViewportArguments::ViewportMeta);
    }

    std::unique_ptr<PreloadRequest> createPreloadRequest(const URL& predictedBaseURL)
    {
        if (!shouldPreload())
            return nullptr;

        auto request = std::make_unique<PreloadRequest>(initiatorFor(m_tagId), m_urlToLoad, predictedBaseURL, resourceType(), m_mediaAttribute);

        request->setCrossOriginModeAllowsCookies(crossOriginModeAllowsCookies());
        request->setCharset(charset());
        return request;
    }

    static bool match(const AtomicString& name, const QualifiedName& qName)
    {
        ASSERT(isMainThread());
        return qName.localName() == name;
    }

private:
    void processImageAndScriptAttribute(const AtomicString& attributeName, const String& attributeValue)
    {
        if (match(attributeName, srcAttr))
            setUrlToLoad(attributeValue);
        else if (match(attributeName, crossoriginAttr) && !attributeValue.isNull())
            m_crossOriginMode = stripLeadingAndTrailingHTMLSpaces(attributeValue);
        else if (match(attributeName, charsetAttr))
            m_charset = attributeValue;
    }

    void processAttribute(const AtomicString& attributeName, const String& attributeValue)
    {
        switch (m_tagId) {
        case TagId::Iframe:
            if (match(attributeName, srcAttr))
                setUrlToLoad(attributeValue);
            break;
        case TagId::Img:
            if (match(attributeName, srcsetAttr) && m_srcSetAttribute.isNull()) {
                m_srcSetAttribute = attributeValue;
                break;
            }
            if (match(attributeName, sizesAttr) && m_sizesAttribute.isNull()) {
                m_sizesAttribute = attributeValue;
                break;
            }
            processImageAndScriptAttribute(attributeName, attributeValue);
            break;
        case TagId::Script:
            processImageAndScriptAttribute(attributeName, attributeValue);
            break;
        case TagId::Link:
            if (match(attributeName, hrefAttr))
                setUrlToLoad(attributeValue);
            else if (match(attributeName, relAttr))
                m_linkIsStyleSheet = relAttributeIsStyleSheet(attributeValue);
            else if (match(attributeName, mediaAttr))
                m_mediaAttribute = attributeValue;
            else if (match(attributeName, charsetAttr))
                m_charset = attributeValue;
            break;
        case TagId::Input:
            if (match(attributeName, srcAttr))
                setUrlToLoad(attributeValue);
            else if (match(attributeName, typeAttr))
                m_inputIsImage = equalIgnoringCase(attributeValue, InputTypeNames::image());
            break;
        case TagId::Meta:
            if (match(attributeName, contentAttr))
                m_metaContent = attributeValue;
            else if (match(attributeName, nameAttr))
                m_metaIsViewport = equalIgnoringCase(attributeValue, "viewport");
            break;
        case TagId::Base:
        case TagId::Style:
        case TagId::Template:
        case TagId::Unknown:
            break;
        }
    }

    static bool relAttributeIsStyleSheet(const String& attributeValue)
    {
        LinkRelAttribute parsedAttribute { attributeValue };
        return parsedAttribute.isStyleSheet && !parsedAttribute.isAlternate && parsedAttribute.iconType == InvalidIcon && !parsedAttribute.isDNSPrefetch;
    }

    void setUrlToLoad(const String& value, bool allowReplacement = false)
    {
        // We only respect the first src/href, per HTML5:
        // http://www.whatwg.org/specs/web-apps/current-work/multipage/tokenization.html#attribute-name-state
        if (!allowReplacement && !m_urlToLoad.isEmpty())
            return;
        String url = stripLeadingAndTrailingHTMLSpaces(value);
        if (url.isEmpty())
            return;
        m_urlToLoad = url;
    }

    const String& charset() const
    {
        return m_charset;
    }

    CachedResource::Type resourceType() const
    {
        switch (m_tagId) {
        case TagId::Iframe:
            return CachedResource::MainResource;
        case TagId::Script:
            return CachedResource::Script;
        case TagId::Img:
        case TagId::Input:
            ASSERT(m_tagId != TagId::Input || m_inputIsImage);
            return CachedResource::ImageResource;
        case TagId::Link:
            ASSERT(m_linkIsStyleSheet);
            return CachedResource::CSSStyleSheet;
        case TagId::Meta:
        case TagId::Unknown:
        case TagId::Style:
        case TagId::Base:
        case TagId::Template:
            break;
        }
        ASSERT_NOT_REACHED();
        return CachedResource::RawResource;
    }

    bool shouldPreload()
    {
        if (m_urlToLoad.isEmpty())
            return false;

        if (protocolIs(m_urlToLoad, "data") || protocolIs(m_urlToLoad, "about"))
            return false;

        if (m_tagId == TagId::Link && !m_linkIsStyleSheet)
            return false;

        if (m_tagId == TagId::Input && !m_inputIsImage)
            return false;

        return true;
    }

    bool crossOriginModeAllowsCookies()
    {
        return m_crossOriginMode.isNull() || equalIgnoringCase(m_crossOriginMode, "use-credentials");
    }

    TagId m_tagId;
    String m_urlToLoad;
    String m_srcSetAttribute;
    String m_sizesAttribute;
    String m_charset;
    String m_crossOriginMode;
    bool m_linkIsStyleSheet;
    String m_mediaAttribute;
    String m_metaContent;
    bool m_metaIsViewport;
    bool m_inputIsImage;
    float m_deviceScaleFactor;
};

TokenPreloadScanner::TokenPreloadScanner(const URL& documentURL, float deviceScaleFactor)
    : m_documentURL(documentURL)
    , m_deviceScaleFactor(deviceScaleFactor)
{
}

void TokenPreloadScanner::scan(const HTMLToken& token, Vector<std::unique_ptr<PreloadRequest>>& requests, Document& document)
{
    switch (token.type()) {
    case HTMLToken::Character:
        if (!m_inStyle)
            return;
        m_cssScanner.scan(token.characters(), requests);
        return;

    case HTMLToken::EndTag: {
        TagId tagId = tagIdFor(token.name());
#if ENABLE(TEMPLATE_ELEMENT)
        if (tagId == TagId::Template) {
            if (m_templateCount)
                --m_templateCount;
            return;
        }
#endif
        if (tagId == TagId::Style) {
            if (m_inStyle)
                m_cssScanner.reset();
            m_inStyle = false;
        }
        return;
    }

    case HTMLToken::StartTag: {
#if ENABLE(TEMPLATE_ELEMENT)
        if (m_templateCount)
            return;
#endif
        TagId tagId = tagIdFor(token.name());
#if ENABLE(TEMPLATE_ELEMENT)
        if (tagId == TagId::Template) {
            ++m_templateCount;
            return;
        }
#endif
        if (tagId == TagId::Style) {
            m_inStyle = true;
            return;
        }
        if (tagId == TagId::Base) {
            // The first <base> element is the one that wins.
            if (!m_predictedBaseElementURL.isEmpty())
                return;
            updatePredictedBaseURL(token);
            return;
        }

        StartTagScanner scanner(tagId, m_deviceScaleFactor);
        scanner.processAttributes(token.attributes(), document);
        if (auto request = scanner.createPreloadRequest(m_predictedBaseElementURL))
            requests.append(WTFMove(request));
        return;
    }

    default:
        return;
    }
}

void TokenPreloadScanner::updatePredictedBaseURL(const HTMLToken& token)
{
    ASSERT(m_predictedBaseElementURL.isEmpty());
    if (auto* hrefAttribute = findAttribute(token.attributes(), hrefAttr.localName().string()))
        m_predictedBaseElementURL = URL(m_documentURL, stripLeadingAndTrailingHTMLSpaces(StringImpl::create8BitIfPossible(hrefAttribute->value))).isolatedCopy();
}

HTMLPreloadScanner::HTMLPreloadScanner(const HTMLParserOptions& options, const URL& documentURL, float deviceScaleFactor)
    : m_scanner(documentURL, deviceScaleFactor)
    , m_tokenizer(options)
{
}

void HTMLPreloadScanner::appendToEnd(const SegmentedString& source)
{
    m_source.append(source);
}

void HTMLPreloadScanner::scan(HTMLResourcePreloader& preloader, Document& document)
{
    ASSERT(isMainThread()); // HTMLTokenizer::updateStateFor only works on the main thread.

    const URL& startingBaseElementURL = document.baseElementURL();

    // When we start scanning, our best prediction of the baseElementURL is the real one!
    if (!startingBaseElementURL.isEmpty())
        m_scanner.setPredictedBaseElementURL(startingBaseElementURL);

    PreloadRequestStream requests;

    while (auto token = m_tokenizer.nextToken(m_source)) {
        if (token->type() == HTMLToken::StartTag)
            m_tokenizer.updateStateFor(AtomicString(token->name()));
        m_scanner.scan(*token, requests, document);
    }

    preloader.preload(WTFMove(requests));
}

bool testPreloadScannerViewportSupport(Document* document)
{
    ASSERT(document);
    HTMLParserOptions options(*document);
    HTMLPreloadScanner scanner(options, document->url());
    HTMLResourcePreloader preloader(*document);
    scanner.appendToEnd(String("<meta name=viewport content='width=400'>"));
    scanner.scan(preloader, *document);
    return (document->viewportArguments().width == 400);
}

}
