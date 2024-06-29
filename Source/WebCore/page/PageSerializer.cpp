/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
 * Copyright (C) 2011-2013 Google Inc. All rights reserved.
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
#include "PageSerializer.h"

#include "CSSFontFaceRule.h"
#include "CSSImageValue.h"
#include "CSSImportRule.h"
#include "CSSStyleRule.h"
#include "CachedImage.h"
#include "CommonAtomStrings.h"
#include "DocumentInlines.h"
#include "ElementInlines.h"
#include "HTMLFrameOwnerElement.h"
#include "HTMLHeadElement.h"
#include "HTMLImageElement.h"
#include "HTMLLinkElement.h"
#include "HTMLMetaCharsetParser.h"
#include "HTMLMetaElement.h"
#include "HTMLNames.h"
#include "HTMLObjectElement.h"
#include "HTMLStyleElement.h"
#include "HTTPParsers.h"
#include "Image.h"
#include "LocalFrame.h"
#include "MarkupAccumulator.h"
#include "Page.h"
#include "RenderElement.h"
#include "StyleCachedImage.h"
#include "StyleImage.h"
#include "StylePropertiesInlines.h"
#include "StyleRule.h"
#include "StyleSheetContents.h"
#include "Text.h"
#include <pal/text/TextEncoding.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

static bool isCharsetSpecifyingNode(const HTMLMetaElement& element)
{
    if (!element.hasAttributes())
        return false;
    Vector<std::pair<StringView, StringView>> attributes;
    for (auto& attribute : element.attributesIterator()) {
        if (attribute.name().hasPrefix())
            continue;
        attributes.append({ StringView { attribute.name().localName() }, StringView { attribute.value() } });
    }
    return HTMLMetaCharsetParser::encodingFromMetaAttributes(attributes).isValid();
}

template<typename NodeType> static bool isCharsetSpecifyingNode(const NodeType& node)
{
    auto* metaElement = dynamicDowncast<HTMLMetaElement>(node);
    return metaElement && isCharsetSpecifyingNode(*metaElement);
}

static bool shouldIgnoreElement(const Element& element)
{
    return element.hasTagName(HTMLNames::scriptTag) || element.hasTagName(HTMLNames::noscriptTag) || isCharsetSpecifyingNode(element);
}

static const QualifiedName& frameOwnerURLAttributeName(const HTMLFrameOwnerElement& frameOwner)
{
    // FIXME: We should support all frame owners including applets.
    return is<HTMLObjectElement>(frameOwner) ? HTMLNames::dataAttr : HTMLNames::srcAttr;
}

class PageSerializer::SerializerMarkupAccumulator final : public MarkupAccumulator {
public:
    SerializerMarkupAccumulator(PageSerializer&, Document&, Vector<Ref<Node>>*);

private:
    PageSerializer& m_serializer;
    Document& m_document;

    void appendText(StringBuilder&, const Text&) override;
    void appendStartTag(StringBuilder&, const Element&, Namespaces*) override;
    void appendCustomAttributes(StringBuilder&, const Element&, Namespaces*) override;
    void appendEndTag(StringBuilder&, const Element&) override;
};

PageSerializer::SerializerMarkupAccumulator::SerializerMarkupAccumulator(PageSerializer& serializer, Document& document, Vector<Ref<Node>>* nodes)
    : MarkupAccumulator(nodes, ResolveURLs::Yes, document.isHTMLDocument() ? SerializationSyntax::HTML : SerializationSyntax::XML)
    , m_serializer(serializer)
    , m_document(document)
{
    // MarkupAccumulator does not serialize the <?xml ... line, so we add it explicitly to ensure the right encoding is specified.
    if (m_document.isXMLDocument() || m_document.xmlStandalone())
        append("<?xml version=\""_s, m_document.xmlVersion(), "\" encoding=\""_s, m_document.charset(), "\"?>"_s);
}

void PageSerializer::SerializerMarkupAccumulator::appendText(StringBuilder& out, const Text& text)
{
    Element* parent = text.parentElement();
    if (parent && !shouldIgnoreElement(*parent))
        MarkupAccumulator::appendText(out, text);
}

void PageSerializer::SerializerMarkupAccumulator::appendStartTag(StringBuilder& out, const Element& element, Namespaces* namespaces)
{
    if (!shouldIgnoreElement(element))
        MarkupAccumulator::appendStartTag(out, element, namespaces);

    if (element.hasTagName(HTMLNames::headTag))
        out.append("<meta charset=\""_s, m_document.charset(), "\">"_s);

    // FIXME: For object (plugins) tags and video tag we could replace them by an image of their current contents.
}

void PageSerializer::SerializerMarkupAccumulator::appendCustomAttributes(StringBuilder& out, const Element& element, Namespaces* namespaces)
{
    RefPtr frameOwner = dynamicDowncast<HTMLFrameOwnerElement>(element);
    if (!frameOwner)
        return;

    auto* frame = dynamicDowncast<LocalFrame>(frameOwner->contentFrame());
    if (!frame)
        return;

    auto url = frame->document()->url();
    if (url.isValid() && !url.protocolIsAbout())
        return;

    // We need to give a fake location to blank frames so they can be referenced by the serialized frame.
    url = m_serializer.urlForBlankFrame(frame);
    appendAttribute(out, element, Attribute(frameOwnerURLAttributeName(*frameOwner), AtomString { url.string() }), namespaces);
}

void PageSerializer::SerializerMarkupAccumulator::appendEndTag(StringBuilder& out, const Element& element)
{
    if (!shouldIgnoreElement(element))
        MarkupAccumulator::appendEndTag(out, element);
}

PageSerializer::PageSerializer(Vector<PageSerializer::Resource>& resources)
    : m_resources(resources)
{
}

void PageSerializer::serialize(Page& page)
{
    if (auto* localMainFrame = dynamicDowncast<LocalFrame>(page.mainFrame()))
        serializeFrame(localMainFrame);
}

void PageSerializer::serializeFrame(LocalFrame* frame)
{
    Document* document = frame->document();
    URL url = document->url();
    if (!url.isValid() || url.protocolIsAbout()) {
        // For blank frames we generate a fake URL so they can be referenced by their containing frame.
        url = urlForBlankFrame(frame);
    }

    if (m_resourceURLs.contains(url)) {
        // FIXME: We could have 2 frame with the same URL but which were dynamically changed and have now
        // different content. So we should serialize both and somehow rename the frame src in the containing
        // frame. Arg!
        return;
    }

    PAL::TextEncoding textEncoding(document->charset());
    if (!textEncoding.isValid()) {
        // FIXME: iframes used as images trigger this. We should deal with them correctly.
        return;
    }

    Vector<Ref<Node>> serializedNodes;
    SerializerMarkupAccumulator accumulator(*this, *document, &serializedNodes);
    String text = accumulator.serializeNodes(*document->protectedDocumentElement(), SerializedNodes::SubtreeIncludingNode);
    m_resources.append({ url, document->suggestedMIMEType(), SharedBuffer::create(textEncoding.encode(text, PAL::UnencodableHandling::Entities)) });
    m_resourceURLs.add(url);

    for (auto&& node : WTFMove(serializedNodes)) {
        RefPtr element = dynamicDowncast<Element>(WTFMove(node));
        if (!element)
            continue;
        // We have to process in-line style as it might contain some resources (typically background images).
        if (RefPtr styledElement = dynamicDowncast<StyledElement>(*element))
            retrieveResourcesForProperties(styledElement->protectedInlineStyle().get(), document);

        if (RefPtr imageElement = dynamicDowncast<HTMLImageElement>(*element)) {
            auto url = document->completeURL(imageElement->attributeWithoutSynchronization(HTMLNames::srcAttr));
            auto* cachedImage = imageElement->cachedImage();
            addImageToResources(cachedImage, imageElement->renderer(), url);
        } else if (RefPtr linkElement = dynamicDowncast<HTMLLinkElement>(*element)) {
            if (RefPtr sheet = linkElement->sheet()) {
                auto url = document->completeURL(linkElement->attributeWithoutSynchronization(HTMLNames::hrefAttr));
                serializeCSSStyleSheet(sheet.get(), url);
                ASSERT(m_resourceURLs.contains(url));
            }
        } else if (RefPtr styleElement = dynamicDowncast<HTMLStyleElement>(*element)) {
            if (RefPtr sheet = styleElement->sheet())
                serializeCSSStyleSheet(sheet.get(), URL());
        }
    }

    for (auto* childFrame = frame->tree().firstChild(); childFrame; childFrame = childFrame->tree().nextSibling()) {
        auto* localFrame = dynamicDowncast<LocalFrame>(childFrame);
        if (!localFrame)
            continue;
        serializeFrame(localFrame);
    }
}

void PageSerializer::serializeCSSStyleSheet(CSSStyleSheet* styleSheet, const URL& url)
{
    StringBuilder cssText;
    for (unsigned i = 0; i < styleSheet->length(); ++i) {
        CSSRule* rule = styleSheet->item(i);
        String itemText = rule->cssText();
        if (!itemText.isEmpty()) {
            cssText.append(itemText);
            if (i < styleSheet->length() - 1)
                cssText.append("\n\n"_s);
        }
        Document* document = styleSheet->ownerDocument();
        // Some rules have resources associated with them that we need to retrieve.
        if (RefPtr importRule = dynamicDowncast<CSSImportRule>(*rule)) {
            auto importURL = document->completeURL(importRule->href());
            if (m_resourceURLs.contains(importURL))
                continue;
            serializeCSSStyleSheet(importRule->protectedStyleSheet().get(), importURL);
        } else if (is<CSSFontFaceRule>(*rule)) {
            // FIXME: Add support for font face rule. It is not clear to me at this point if the actual otf/eot file can
            // be retrieved from the CSSFontFaceRule object.
        } else if (RefPtr styleRule = dynamicDowncast<CSSStyleRule>(*rule))
            retrieveResourcesForRule(styleRule->styleRule(), document);
    }

    if (url.isValid() && !m_resourceURLs.contains(url)) {
        // FIXME: We should check whether a charset has been specified and if none was found add one.
        PAL::TextEncoding textEncoding(styleSheet->contents().charset());
        ASSERT(textEncoding.isValid());
        m_resources.append({ url, cssContentTypeAtom(), SharedBuffer::create(textEncoding.encode(cssText.toString(), PAL::UnencodableHandling::Entities)) });
        m_resourceURLs.add(url);
    }
}

void PageSerializer::addImageToResources(CachedImage* image, RenderElement* imageRenderer, const URL& url)
{
    if (!url.isValid() || m_resourceURLs.contains(url))
        return;

    if (!image || image->image() == &Image::nullImage())
        return;

    RefPtr<FragmentedSharedBuffer> data = imageRenderer ? image->imageForRenderer(imageRenderer)->data() : 0;
    if (!data)
        data = image->image()->data();

    if (!data) {
        LOG_ERROR("No data for image %s", url.string().utf8().data());
        return;
    }

    m_resources.append({ url, image->response().mimeType(), data->makeContiguous() });
    m_resourceURLs.add(url);
}

void PageSerializer::retrieveResourcesForRule(StyleRule& rule, Document* document)
{
    retrieveResourcesForProperties(rule.protectedProperties().ptr(), document);
}

void PageSerializer::retrieveResourcesForProperties(const StyleProperties* styleDeclaration, Document* document)
{
    if (!styleDeclaration)
        return;

    // The background-image and list-style-image (for ul or ol) are the CSS properties
    // that make use of images. We iterate to make sure we include any other
    // image properties there might be.
    for (auto property : *styleDeclaration) {
        RefPtr cssValue = dynamicDowncast<CSSImageValue>(property.value());
        if (!cssValue)
            continue;

        auto* image = cssValue->cachedImage();
        if (!image)
            continue;

        addImageToResources(image, nullptr, document->completeURL(image->url().string()));
    }
}

URL PageSerializer::urlForBlankFrame(LocalFrame* frame)
{
    auto iterator = m_blankFrameURLs.find(frame);
    if (iterator != m_blankFrameURLs.end())
        return iterator->value;
    URL fakeURL { makeString("wyciwyg://frame/"_s, m_blankFrameCounter++) };
    m_blankFrameURLs.add(frame, fakeURL);
    return fakeURL;
}

}
