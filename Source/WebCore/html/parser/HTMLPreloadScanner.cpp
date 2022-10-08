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

#include "HTMLImageElement.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "HTMLSrcsetParser.h"
#include "HTMLTokenizer.h"
#include "InputTypeNames.h"
#include "LinkLoader.h"
#include "LinkRelAttribute.h"
#include "Logging.h"
#include "MIMETypeRegistry.h"
#include "MediaList.h"
#include "MediaQueryEvaluator.h"
#include "MediaQueryParserContext.h"
#include "RenderView.h"
#include "SecurityPolicy.h"
#include "Settings.h"
#include "SizesAttributeParser.h"
#include <wtf/MainThread.h>

namespace WebCore {

using namespace HTMLNames;

TokenPreloadScanner::TagId TokenPreloadScanner::tagIdFor(const HTMLToken::DataVector& data)
{
    AtomString tagName(data);
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
    if (tagName == pictureTag)
        return TagId::Picture;
    if (tagName == sourceTag)
        return TagId::Source;
    return TagId::Unknown;
}

ASCIILiteral TokenPreloadScanner::initiatorFor(TagId tagId)
{
    switch (tagId) {
    case TagId::Source:
    case TagId::Img:
        return "img"_s;
    case TagId::Input:
        return "input"_s;
    case TagId::Link:
        return "link"_s;
    case TagId::Script:
        return "script"_s;
    case TagId::Unknown:
    case TagId::Style:
    case TagId::Base:
    case TagId::Template:
    case TagId::Meta:
    case TagId::Picture:
        ASSERT_NOT_REACHED();
        return "unknown"_s;
    }
    ASSERT_NOT_REACHED();
    return "unknown"_s;
}

class TokenPreloadScanner::StartTagScanner {
public:
    explicit StartTagScanner(Document& document, TagId tagId, float deviceScaleFactor = 1.0)
        : m_document(document)
        , m_tagId(tagId)
        , m_linkIsStyleSheet(false)
        , m_linkIsPreload(false)
        , m_metaIsViewport(false)
        , m_metaIsDisabledAdaptations(false)
        , m_inputIsImage(false)
        , m_deviceScaleFactor(deviceScaleFactor)
    {
    }

    void processAttributes(const HTMLToken::AttributeList& attributes, Vector<bool>& pictureState)
    {
        ASSERT(isMainThread());
        if (m_tagId >= TagId::Unknown)
            return;
        
        for (auto& attribute : attributes) {
            AtomString attributeName(attribute.name);
            AtomString attributeValue(attribute.value);
            processAttribute(WTFMove(attributeName), WTFMove(attributeValue), pictureState);
        }
        
        if (m_tagId == TagId::Source && !pictureState.isEmpty() && !pictureState.last() && m_mediaMatched && m_typeMatched && !m_srcSetAttribute.isEmpty()) {
            
            auto sourceSize = SizesAttributeParser(m_sizesAttribute, m_document).length();
            ImageCandidate imageCandidate = bestFitSourceForImageAttributes(m_deviceScaleFactor, AtomString { m_urlToLoad }, m_srcSetAttribute, sourceSize);
            if (!imageCandidate.isEmpty()) {
                pictureState.last() = true;
                setUrlToLoad(imageCandidate.string.toString(), true);
            }
        }
        
        // Resolve between src and srcSet if we have them and the tag is img.
        if (m_tagId == TagId::Img && !m_srcSetAttribute.isEmpty()) {
            auto sourceSize = SizesAttributeParser(m_sizesAttribute, m_document).length();
            ImageCandidate imageCandidate = bestFitSourceForImageAttributes(m_deviceScaleFactor, AtomString { m_urlToLoad }, m_srcSetAttribute, sourceSize);
            setUrlToLoad(imageCandidate.string.toString(), true);
        }

        if (m_metaIsViewport && !m_metaContent.isNull())
            m_document.processViewport(m_metaContent, ViewportArguments::ViewportMeta);

        if (m_metaIsDisabledAdaptations && !m_metaContent.isNull())
            m_document.processDisabledAdaptations(m_metaContent);
    }

    std::unique_ptr<PreloadRequest> createPreloadRequest(const URL& predictedBaseURL)
    {
        if (!shouldPreload())
            return nullptr;

        auto type = resourceType();
        if (!type)
            return nullptr;

        if (!LinkLoader::isSupportedType(type.value(), m_typeAttribute, m_document))
            return nullptr;

        // Do not preload if lazyload is possible but metadata fetch is disabled.
        if (HTMLImageElement::hasLazyLoadableAttributeValue(m_lazyloadAttribute))
            return nullptr;

        auto request = makeUnique<PreloadRequest>(initiatorFor(m_tagId), m_urlToLoad, predictedBaseURL, type.value(), m_mediaAttribute, m_scriptType, m_referrerPolicy);
        request->setCrossOriginMode(m_crossOriginMode);
        request->setNonce(m_nonceAttribute);
        request->setScriptIsAsync(m_scriptIsAsync);

        // According to the spec, the module tag ignores the "charset" attribute as the same to the worker's
        // importScript. But WebKit supports the "charset" for importScript intentionally. So to be consistent,
        // even for the module tags, we handle the "charset" attribute.
        request->setCharset(charset());
        return request;
    }

    static bool match(const AtomString& name, const QualifiedName& qName)
    {
        ASSERT(isMainThread());
        return qName.localName() == name;
    }

private:
    void processImageAndScriptAttribute(const AtomString& attributeName, const String& attributeValue)
    {
        if (match(attributeName, srcAttr))
            setUrlToLoad(attributeValue);
        else if (match(attributeName, crossoriginAttr))
            m_crossOriginMode = stripLeadingAndTrailingHTMLSpaces(attributeValue);
        else if (match(attributeName, charsetAttr))
            m_charset = attributeValue;
    }

    void processAttribute(AtomString&& attributeName, AtomString&& attributeValue, const Vector<bool>& pictureState)
    {
        bool inPicture = !pictureState.isEmpty();
        bool alreadyMatchedSource = inPicture && pictureState.last();

        switch (m_tagId) {
        case TagId::Img:
            if (inPicture && alreadyMatchedSource)
                break;
            if (match(attributeName, srcsetAttr) && m_srcSetAttribute.isNull()) {
                m_srcSetAttribute = WTFMove(attributeValue);
                break;
            }
            if (match(attributeName, sizesAttr) && m_sizesAttribute.isNull()) {
                m_sizesAttribute = WTFMove(attributeValue);
                break;
            }
            if (m_document.settings().lazyImageLoadingEnabled()) {
                if (match(attributeName, loadingAttr) && m_lazyloadAttribute.isNull()) {
                    m_lazyloadAttribute = WTFMove(attributeValue);
                    break;
                }
            }
            processImageAndScriptAttribute(attributeName, attributeValue);
            break;
        case TagId::Source:
            if (inPicture && alreadyMatchedSource)
                break;
            if (match(attributeName, srcsetAttr) && m_srcSetAttribute.isNull()) {
                m_srcSetAttribute = WTFMove(attributeValue);
                break;
            }
            if (match(attributeName, sizesAttr) && m_sizesAttribute.isNull()) {
                m_sizesAttribute = WTFMove(attributeValue);
                break;
            }
            if (match(attributeName, mediaAttr) && m_mediaAttribute.isNull()) {
                m_mediaAttribute = attributeValue;
                auto mediaSet = MediaQuerySet::create(attributeValue, MediaQueryParserContext(m_document));
                RefPtr documentElement = m_document.documentElement();
                LOG(MediaQueries, "HTMLPreloadScanner %p processAttribute evaluating media queries", this);
                m_mediaMatched = MediaQueryEvaluator { m_document.printing() ? "print"_s : "screen"_s, m_document, documentElement ? documentElement->computedStyle() : nullptr }.evaluate(mediaSet.get());
            }
            if (match(attributeName, typeAttr) && m_typeAttribute.isNull()) {
                // when multiple type attributes present: first value wins, ignore subsequent (to match ImageElement parser and Blink behaviours)
                m_typeAttribute = WTFMove(attributeValue);
                m_typeMatched &= MIMETypeRegistry::isSupportedImageVideoOrSVGMIMEType(m_typeAttribute);
            }
            break;
        case TagId::Script:
            if (match(attributeName, typeAttr)) {
                if (equalLettersIgnoringASCIICase(attributeValue, "module"_s))
                    m_scriptType = ScriptType::Module;
                else if (equalLettersIgnoringASCIICase(attributeValue, "importmap"_s))
                    m_scriptType = ScriptType::ImportMap;
                break;
            } else if (match(attributeName, nonceAttr)) {
                m_nonceAttribute = WTFMove(attributeValue);
                break;
            } else if (match(attributeName, referrerpolicyAttr)) {
                m_referrerPolicy = parseReferrerPolicy(attributeValue, ReferrerPolicySource::ReferrerPolicyAttribute).value_or(ReferrerPolicy::EmptyString);
                break;
            } else if (match(attributeName, nomoduleAttr)) {
                m_scriptIsNomodule = true;
                break;
            } else if (match(attributeName, asyncAttr)) {
                m_scriptIsAsync = true;
                break;
            }
            processImageAndScriptAttribute(attributeName, attributeValue);
            break;
        case TagId::Link:
            if (match(attributeName, hrefAttr))
                setUrlToLoad(attributeValue);
            else if (match(attributeName, relAttr)) {
                LinkRelAttribute parsedAttribute { m_document, attributeValue };
                m_linkIsStyleSheet = relAttributeIsStyleSheet(parsedAttribute);
                m_linkIsPreload = parsedAttribute.isLinkPreload;
            } else if (match(attributeName, mediaAttr))
                m_mediaAttribute = WTFMove(attributeValue);
            else if (match(attributeName, charsetAttr))
                m_charset = WTFMove(attributeValue);
            else if (match(attributeName, crossoriginAttr))
                m_crossOriginMode = stripLeadingAndTrailingHTMLSpaces(attributeValue);
            else if (match(attributeName, nonceAttr))
                m_nonceAttribute = WTFMove(attributeValue);
            else if (match(attributeName, asAttr))
                m_asAttribute = WTFMove(attributeValue);
            else if (match(attributeName, typeAttr))
                m_typeAttribute = WTFMove(attributeValue);
            else if (match(attributeName, referrerpolicyAttr))
                m_referrerPolicy = parseReferrerPolicy(attributeValue, ReferrerPolicySource::ReferrerPolicyAttribute).value_or(ReferrerPolicy::EmptyString);
            break;
        case TagId::Input:
            if (match(attributeName, srcAttr))
                setUrlToLoad(attributeValue);
            else if (match(attributeName, typeAttr))
                m_inputIsImage = equalLettersIgnoringASCIICase(attributeValue, "image"_s);
            break;
        case TagId::Meta:
            if (match(attributeName, contentAttr))
                m_metaContent = WTFMove(attributeValue);
            else if (match(attributeName, nameAttr))
                m_metaIsViewport = equalLettersIgnoringASCIICase(attributeValue, "viewport"_s);
            else if (m_document.settings().disabledAdaptationsMetaTagEnabled() && match(attributeName, nameAttr))
                m_metaIsDisabledAdaptations = equalLettersIgnoringASCIICase(attributeValue, "disabled-adaptations"_s);
            break;
        case TagId::Base:
        case TagId::Style:
        case TagId::Template:
        case TagId::Picture:
        case TagId::Unknown:
            break;
        }
    }

    static bool relAttributeIsStyleSheet(const LinkRelAttribute& parsedAttribute)
    {
        return parsedAttribute.isStyleSheet && !parsedAttribute.isAlternate && !parsedAttribute.iconType && !parsedAttribute.isDNSPrefetch;
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

    std::optional<CachedResource::Type> resourceType() const
    {
        switch (m_tagId) {
        case TagId::Script:
            return CachedResource::Type::Script;
        case TagId::Img:
        case TagId::Input:
        case TagId::Source:
            ASSERT(m_tagId != TagId::Input || m_inputIsImage);
            return CachedResource::Type::ImageResource;
        case TagId::Link:
            if (m_linkIsStyleSheet)
                return CachedResource::Type::CSSStyleSheet;
            if (m_linkIsPreload)
                return LinkLoader::resourceTypeFromAsAttribute(m_asAttribute, m_document);
            break;
        case TagId::Meta:
        case TagId::Unknown:
        case TagId::Style:
        case TagId::Base:
        case TagId::Template:
        case TagId::Picture:
            break;
        }
        ASSERT_NOT_REACHED();
        return CachedResource::Type::RawResource;
    }

    bool shouldPreload()
    {
        if (m_urlToLoad.isEmpty())
            return false;

        if (protocolIs(m_urlToLoad, "data"_s) || protocolIs(m_urlToLoad, "about"_s))
            return false;

        if (m_tagId == TagId::Link && !m_linkIsStyleSheet && !m_linkIsPreload)
            return false;

        if (m_tagId == TagId::Input && !m_inputIsImage)
            return false;

        if (m_tagId == TagId::Script && m_scriptType != ScriptType::Module && m_scriptIsNomodule)
            return false;

        return true;
    }

    Document& m_document;
    TagId m_tagId;
    String m_urlToLoad;
    AtomString m_srcSetAttribute;
    AtomString m_sizesAttribute;
    bool m_mediaMatched { true };
    bool m_typeMatched { true };
    String m_charset;
    String m_crossOriginMode;
    bool m_linkIsStyleSheet;
    bool m_linkIsPreload;
    AtomString m_mediaAttribute;
    AtomString m_nonceAttribute;
    String m_metaContent;
    AtomString m_asAttribute;
    AtomString m_typeAttribute;
    AtomString m_lazyloadAttribute;
    bool m_metaIsViewport;
    bool m_metaIsDisabledAdaptations;
    bool m_inputIsImage;
    bool m_scriptIsNomodule { false };
    bool m_scriptIsAsync { false };
    float m_deviceScaleFactor;
    ScriptType m_scriptType { ScriptType::Classic };
    ReferrerPolicy m_referrerPolicy { ReferrerPolicy::EmptyString };
};

TokenPreloadScanner::TokenPreloadScanner(const URL& documentURL, float deviceScaleFactor)
    : m_documentURL(documentURL)
    , m_deviceScaleFactor(deviceScaleFactor)
{
}

void TokenPreloadScanner::scan(const HTMLToken& token, Vector<std::unique_ptr<PreloadRequest>>& requests, Document& document)
{
    switch (token.type()) {
    case HTMLToken::Type::Character:
        if (!m_inStyle)
            return;
        m_cssScanner.scan(token.characters(), requests);
        return;

    case HTMLToken::Type::EndTag: {
        TagId tagId = tagIdFor(token.name());
        if (tagId == TagId::Template) {
            if (m_templateCount)
                --m_templateCount;
            return;
        }
        if (tagId == TagId::Style) {
            if (m_inStyle)
                m_cssScanner.reset();
            m_inStyle = false;
        } else if (tagId == TagId::Picture && !m_pictureSourceState.isEmpty())
            m_pictureSourceState.removeLast();

        return;
    }

    case HTMLToken::Type::StartTag: {
        if (m_templateCount)
            return;
        TagId tagId = tagIdFor(token.name());
        if (tagId == TagId::Template) {
            ++m_templateCount;
            return;
        }
        if (tagId == TagId::Style) {
            m_inStyle = true;
            return;
        }
        if (tagId == TagId::Base) {
            // The first <base> element is the one that wins.
            if (!m_predictedBaseElementURL.isEmpty())
                return;
            updatePredictedBaseURL(token, document.settings().shouldRestrictBaseURLSchemes());
            return;
        }
        if (tagId == TagId::Picture) {
            m_pictureSourceState.append(false);
            return;
        }

        StartTagScanner scanner(document, tagId, m_deviceScaleFactor);
        scanner.processAttributes(token.attributes(), m_pictureSourceState);
        if (auto request = scanner.createPreloadRequest(m_predictedBaseElementURL))
            requests.append(WTFMove(request));
        return;
    }

    default:
        return;
    }
}

void TokenPreloadScanner::updatePredictedBaseURL(const HTMLToken& token, bool shouldRestrictBaseURLSchemes)
{
    ASSERT(m_predictedBaseElementURL.isEmpty());
    static constexpr UChar hrefAsUChar[] = { 'h', 'r', 'e', 'f' };
    auto* hrefAttribute = findAttribute(token.attributes(), hrefAsUChar);
    if (!hrefAttribute)
        return;
    URL temp { m_documentURL, stripLeadingAndTrailingHTMLSpaces(StringImpl::create8BitIfPossible(hrefAttribute->value)) };
    if (!shouldRestrictBaseURLSchemes || SecurityPolicy::isBaseURLSchemeAllowed(temp))
        m_predictedBaseElementURL = WTFMove(temp).isolatedCopy();
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
        if (token->type() == HTMLToken::Type::StartTag)
            m_tokenizer.updateStateFor(AtomString(token->name()));
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
    scanner.appendToEnd(String("<meta name=viewport content='width=400'>"_s));
    scanner.scan(preloader, *document);
    return (document->viewportArguments().width == 400);
}

}
