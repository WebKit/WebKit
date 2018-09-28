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
#include "PageSerializer.h"

#include "CSSFontFaceRule.h"
#include "CSSImageValue.h"
#include "CSSImportRule.h"
#include "CSSStyleRule.h"
#include "CachedImage.h"
#include "Document.h"
#include "Element.h"
#include "Frame.h"
#include "HTMLFrameOwnerElement.h"
#include "HTMLHeadElement.h"
#include "HTMLImageElement.h"
#include "HTMLLinkElement.h"
#include "HTMLMetaCharsetParser.h"
#include "HTMLNames.h"
#include "HTMLObjectElement.h"
#include "HTMLStyleElement.h"
#include "HTTPParsers.h"
#include "Image.h"
#include "MarkupAccumulator.h"
#include "Page.h"
#include "RenderElement.h"
#include "StyleCachedImage.h"
#include "StyleImage.h"
#include "StyleProperties.h"
#include "StyleRule.h"
#include "StyleSheetContents.h"
#include "Text.h"
#include "TextEncoding.h"
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

static bool isCharsetSpecifyingNode(const Node& node)
{
    if (!is<HTMLElement>(node))
        return false;

    const HTMLElement& element = downcast<HTMLElement>(node);
    if (!element.hasTagName(HTMLNames::metaTag))
        return false;
    HTMLMetaCharsetParser::AttributeList attributes;
    if (element.hasAttributes()) {
        for (const Attribute& attribute : element.attributesIterator()) {
            // FIXME: We should deal appropriately with the attribute if they have a namespace.
            attributes.append(std::make_pair(attribute.name().toString(), attribute.value().string()));
        }
    }
    return HTMLMetaCharsetParser::encodingFromMetaAttributes(attributes).isValid();
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

class PageSerializer::SerializerMarkupAccumulator final : public WebCore::MarkupAccumulator {
public:
    SerializerMarkupAccumulator(PageSerializer&, Document&, Vector<Node*>*);

private:
    PageSerializer& m_serializer;
    Document& m_document;

    void appendText(StringBuilder&, const Text&) override;
    void appendElement(StringBuilder&, const Element&, Namespaces*) override;
    void appendCustomAttributes(StringBuilder&, const Element&, Namespaces*) override;
    void appendEndTag(const Element&) override;
};

PageSerializer::SerializerMarkupAccumulator::SerializerMarkupAccumulator(PageSerializer& serializer, Document& document, Vector<Node*>* nodes)
    : MarkupAccumulator(nodes, ResolveURLs::Yes)
    , m_serializer(serializer)
    , m_document(document)
{
    // MarkupAccumulator does not serialize the <?xml ... line, so we add it explicitely to ensure the right encoding is specified.
    if (m_document.isXMLDocument() || m_document.xmlStandalone())
        appendString("<?xml version=\"" + m_document.xmlVersion() + "\" encoding=\"" + m_document.charset() + "\"?>");
}

void PageSerializer::SerializerMarkupAccumulator::appendText(StringBuilder& out, const Text& text)
{
    Element* parent = text.parentElement();
    if (parent && !shouldIgnoreElement(*parent))
        MarkupAccumulator::appendText(out, text);
}

void PageSerializer::SerializerMarkupAccumulator::appendElement(StringBuilder& out, const Element& element, Namespaces* namespaces)
{
    if (!shouldIgnoreElement(element))
        MarkupAccumulator::appendElement(out, element, namespaces);

    if (element.hasTagName(HTMLNames::headTag)) {
        out.appendLiteral("<meta charset=\"");
        out.append(m_document.charset());
        out.appendLiteral("\">");
    }

    // FIXME: For object (plugins) tags and video tag we could replace them by an image of their current contents.
}

void PageSerializer::SerializerMarkupAccumulator::appendCustomAttributes(StringBuilder& out, const Element& element, Namespaces* namespaces)
{
    if (!is<HTMLFrameOwnerElement>(element))
        return;

    const HTMLFrameOwnerElement& frameOwner = downcast<HTMLFrameOwnerElement>(element);
    Frame* frame = frameOwner.contentFrame();
    if (!frame)
        return;

    URL url = frame->document()->url();
    if (url.isValid() && !url.isBlankURL())
        return;

    // We need to give a fake location to blank frames so they can be referenced by the serialized frame.
    url = m_serializer.urlForBlankFrame(frame);
    appendAttribute(out, element, Attribute(frameOwnerURLAttributeName(frameOwner), url.string()), namespaces);
}

void PageSerializer::SerializerMarkupAccumulator::appendEndTag(const Element& element)
{
    if (!shouldIgnoreElement(element))
        MarkupAccumulator::appendEndTag(element);
}

PageSerializer::PageSerializer(Vector<PageSerializer::Resource>& resources)
    : m_resources(resources)
{
}

void PageSerializer::serialize(Page& page)
{
    serializeFrame(&page.mainFrame());
}

void PageSerializer::serializeFrame(Frame* frame)
{
    Document* document = frame->document();
    URL url = document->url();
    if (!url.isValid() || url.isBlankURL()) {
        // For blank frames we generate a fake URL so they can be referenced by their containing frame.
        url = urlForBlankFrame(frame);
    }

    if (m_resourceURLs.contains(url)) {
        // FIXME: We could have 2 frame with the same URL but which were dynamically changed and have now
        // different content. So we should serialize both and somehow rename the frame src in the containing
        // frame. Arg!
        return;
    }

    Vector<Node*> nodes;
    SerializerMarkupAccumulator accumulator(*this, *document, &nodes);
    TextEncoding textEncoding(document->charset());
    CString data;
    if (!textEncoding.isValid()) {
        // FIXME: iframes used as images trigger this. We should deal with them correctly.
        return;
    }
    String text = accumulator.serializeNodes(*document->documentElement(), SerializedNodes::SubtreeIncludingNode);
    m_resources.append({ url, document->suggestedMIMEType(), SharedBuffer::create(textEncoding.encode(text, UnencodableHandling::Entities)) });
    m_resourceURLs.add(url);

    for (auto& node : nodes) {
        if (!is<Element>(*node))
            continue;

        Element& element = downcast<Element>(*node);
        // We have to process in-line style as it might contain some resources (typically background images).
        if (is<StyledElement>(element))
            retrieveResourcesForProperties(downcast<StyledElement>(element).inlineStyle(), document);

        if (is<HTMLImageElement>(element)) {
            HTMLImageElement& imageElement = downcast<HTMLImageElement>(element);
            URL url = document->completeURL(imageElement.attributeWithoutSynchronization(HTMLNames::srcAttr));
            CachedImage* cachedImage = imageElement.cachedImage();
            addImageToResources(cachedImage, imageElement.renderer(), url);
        } else if (is<HTMLLinkElement>(element)) {
            HTMLLinkElement& linkElement = downcast<HTMLLinkElement>(element);
            if (CSSStyleSheet* sheet = linkElement.sheet()) {
                URL url = document->completeURL(linkElement.attributeWithoutSynchronization(HTMLNames::hrefAttr));
                serializeCSSStyleSheet(sheet, url);
                ASSERT(m_resourceURLs.contains(url));
            }
        } else if (is<HTMLStyleElement>(element)) {
            if (CSSStyleSheet* sheet = downcast<HTMLStyleElement>(element).sheet())
                serializeCSSStyleSheet(sheet, URL());
        }
    }

    for (Frame* childFrame = frame->tree().firstChild(); childFrame; childFrame = childFrame->tree().nextSibling())
        serializeFrame(childFrame);
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
                cssText.appendLiteral("\n\n");
        }
        Document* document = styleSheet->ownerDocument();
        // Some rules have resources associated with them that we need to retrieve.
        if (is<CSSImportRule>(*rule)) {
            CSSImportRule& importRule = downcast<CSSImportRule>(*rule);
            URL importURL = document->completeURL(importRule.href());
            if (m_resourceURLs.contains(importURL))
                continue;
            serializeCSSStyleSheet(importRule.styleSheet(), importURL);
        } else if (is<CSSFontFaceRule>(*rule)) {
            // FIXME: Add support for font face rule. It is not clear to me at this point if the actual otf/eot file can
            // be retrieved from the CSSFontFaceRule object.
        } else if (is<CSSStyleRule>(*rule))
            retrieveResourcesForRule(downcast<CSSStyleRule>(*rule).styleRule(), document);
    }

    if (url.isValid() && !m_resourceURLs.contains(url)) {
        // FIXME: We should check whether a charset has been specified and if none was found add one.
        TextEncoding textEncoding(styleSheet->contents().charset());
        ASSERT(textEncoding.isValid());
        m_resources.append({ url, "text/css"_s, SharedBuffer::create(textEncoding.encode(cssText.toString(), UnencodableHandling::Entities)) });
        m_resourceURLs.add(url);
    }
}

void PageSerializer::addImageToResources(CachedImage* image, RenderElement* imageRenderer, const URL& url)
{
    if (!url.isValid() || m_resourceURLs.contains(url))
        return;

    if (!image || image->image() == &Image::nullImage())
        return;

    RefPtr<SharedBuffer> data = imageRenderer ? image->imageForRenderer(imageRenderer)->data() : 0;
    if (!data)
        data = image->image()->data();

    if (!data) {
        LOG_ERROR("No data for image %s", url.string().utf8().data());
        return;
    }

    m_resources.append({ url, image->response().mimeType(), WTFMove(data) });
    m_resourceURLs.add(url);
}

void PageSerializer::retrieveResourcesForRule(StyleRule& rule, Document* document)
{
    retrieveResourcesForProperties(&rule.properties(), document);
}

void PageSerializer::retrieveResourcesForProperties(const StyleProperties* styleDeclaration, Document* document)
{
    if (!styleDeclaration)
        return;

    // The background-image and list-style-image (for ul or ol) are the CSS properties
    // that make use of images. We iterate to make sure we include any other
    // image properties there might be.
    unsigned propertyCount = styleDeclaration->propertyCount();
    for (unsigned i = 0; i < propertyCount; ++i) {
        RefPtr<CSSValue> cssValue = styleDeclaration->propertyAt(i).value();
        if (!is<CSSImageValue>(*cssValue))
            continue;

        auto* image = downcast<CSSImageValue>(*cssValue).cachedImage();
        if (!image)
            continue;

        addImageToResources(image, nullptr, document->completeURL(image->url()));
    }
}

URL PageSerializer::urlForBlankFrame(Frame* frame)
{
    auto iter = m_blankFrameURLs.find(frame);
    if (iter != m_blankFrameURLs.end())
        return iter->value;
    String url = "wyciwyg://frame/" + String::number(m_blankFrameCounter++);
    URL fakeURL(ParsedURLString, url);
    m_blankFrameURLs.add(frame, fakeURL);
    return fakeURL;
}

}
