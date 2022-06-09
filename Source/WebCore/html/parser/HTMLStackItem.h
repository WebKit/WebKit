/*
 * Copyright (C) 2012 Company 100, Inc. All rights reserved.
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#pragma once

#include "AtomHTMLToken.h"
#include "DocumentFragment.h"
#include "Element.h"
#include "HTMLNames.h"
#include "MathMLNames.h"
#include "SVGNames.h"

namespace WebCore {

class HTMLStackItem : public RefCounted<HTMLStackItem> {
public:
    // Normal HTMLElementStack and HTMLFormattingElementList items.
    static Ref<HTMLStackItem> create(Ref<Element>&&, AtomHTMLToken&&, const AtomString& namespaceURI = HTMLNames::xhtmlNamespaceURI);
    static Ref<HTMLStackItem> create(Ref<Element>&&, const AtomString&, Vector<Attribute>&&);

    // Document fragment or element for parsing context.
    static Ref<HTMLStackItem> create(Element&);
    static Ref<HTMLStackItem> create(DocumentFragment&);

    bool isElement() const;
    bool isDocumentFragment() const;

    ContainerNode& node() const;
    Element& element() const;

    const AtomString& namespaceURI() const;
    const AtomString& localName() const;

    const Vector<Attribute>& attributes() const;
    const Attribute* findAttribute(const QualifiedName& attributeName) const;

    bool hasTagName(const QualifiedName&) const;
    bool matchesHTMLTag(const AtomString&) const;

private:
    HTMLStackItem(Ref<Element>&&, AtomHTMLToken&&, const AtomString& namespaceURI);
    HTMLStackItem(Ref<Element>&&, const AtomString& localName, const AtomString& namespaceURI, Vector<Attribute>&&);
    explicit HTMLStackItem(Element&);
    explicit HTMLStackItem(DocumentFragment&);

    const Ref<ContainerNode> m_node;
    const AtomString m_namespaceURI;
    const AtomString m_localName;
    const Vector<Attribute> m_attributes;
};

bool isInHTMLNamespace(const HTMLStackItem&);
bool isNumberedHeaderElement(const HTMLStackItem&);
bool isSpecialNode(const HTMLStackItem&);

inline HTMLStackItem::HTMLStackItem(Ref<Element>&& element, AtomHTMLToken&& token, const AtomString& namespaceURI = HTMLNames::xhtmlNamespaceURI)
    : m_node(WTFMove(element))
    , m_namespaceURI(namespaceURI)
    , m_localName(token.name())
    , m_attributes(WTFMove(token.attributes()))
{
}

inline Ref<HTMLStackItem> HTMLStackItem::create(Ref<Element>&& element, AtomHTMLToken&& token, const AtomString& namespaceURI)
{
    return adoptRef(*new HTMLStackItem(WTFMove(element), WTFMove(token), namespaceURI));
}

inline HTMLStackItem::HTMLStackItem(Ref<Element>&& element, const AtomString& localName, const AtomString& namespaceURI, Vector<Attribute>&& attributes)
    : m_node(WTFMove(element))
    , m_namespaceURI(namespaceURI)
    , m_localName(localName)
    , m_attributes(WTFMove(attributes))
{
}

inline Ref<HTMLStackItem> HTMLStackItem::create(Ref<Element>&& element, const AtomString& localName, Vector<Attribute>&& attributes)
{
    auto& namespaceURI = element.get().namespaceURI();
    return adoptRef(*new HTMLStackItem(WTFMove(element), localName, namespaceURI, WTFMove(attributes)));
}

inline HTMLStackItem::HTMLStackItem(Element& element)
    : m_node(element)
    , m_namespaceURI(element.namespaceURI())
    , m_localName(element.localName())
{
}

inline Ref<HTMLStackItem> HTMLStackItem::create(Element& element)
{
    return adoptRef(*new HTMLStackItem(element));
}

inline HTMLStackItem::HTMLStackItem(DocumentFragment& fragment)
    : m_node(fragment)
{
}

inline Ref<HTMLStackItem> HTMLStackItem::create(DocumentFragment& fragment)
{
    return adoptRef(*new HTMLStackItem(fragment));
}

inline ContainerNode& HTMLStackItem::node() const
{
    return m_node.get();
}

inline Element& HTMLStackItem::element() const
{
    return downcast<Element>(node());
}

inline bool HTMLStackItem::isDocumentFragment() const
{
    return m_localName.isNull();
}

inline bool HTMLStackItem::isElement() const
{
    return !isDocumentFragment();
}

inline const AtomString& HTMLStackItem::namespaceURI() const
{
    return m_namespaceURI;
}

inline const AtomString& HTMLStackItem::localName() const
{
    return m_localName;
}

inline const Vector<Attribute>& HTMLStackItem::attributes() const
{
    ASSERT(isElement());
    return m_attributes;
}

inline const Attribute* HTMLStackItem::findAttribute(const QualifiedName& attributeName) const
{
    return WebCore::findAttribute(const_cast<Vector<Attribute>&>(attributes()), attributeName);
}

inline bool HTMLStackItem::hasTagName(const QualifiedName& name) const
{
    return m_localName == name.localName() && m_namespaceURI == name.namespaceURI();
}

inline bool HTMLStackItem::matchesHTMLTag(const AtomString& name) const
{
    return m_localName == name && m_namespaceURI == HTMLNames::xhtmlNamespaceURI;
}

inline bool isInHTMLNamespace(const HTMLStackItem& item)
{
    // A DocumentFragment takes the place of the document element when parsing
    // fragments and thus should be treated as if it was in the HTML namespace.
    // FIXME: Is this also needed for a ShadowRoot that might be a non-HTML element?
    return item.namespaceURI() == HTMLNames::xhtmlNamespaceURI || item.isDocumentFragment();
}

inline bool isNumberedHeaderElement(const HTMLStackItem& item)
{
    return item.namespaceURI() == HTMLNames::xhtmlNamespaceURI
        && (item.localName() == HTMLNames::h1Tag
            || item.localName() == HTMLNames::h2Tag
            || item.localName() == HTMLNames::h3Tag
            || item.localName() == HTMLNames::h4Tag
            || item.localName() == HTMLNames::h5Tag
            || item.localName() == HTMLNames::h6Tag);
}

// http://www.whatwg.org/specs/web-apps/current-work/multipage/parsing.html#special
inline bool isSpecialNode(const HTMLStackItem& item)
{
    if (item.isDocumentFragment())
        return true;
    const AtomString& tagName = item.localName();
    if (item.namespaceURI() == HTMLNames::xhtmlNamespaceURI) {
        return tagName == HTMLNames::addressTag
            || tagName == HTMLNames::appletTag
            || tagName == HTMLNames::areaTag
            || tagName == HTMLNames::articleTag
            || tagName == HTMLNames::asideTag
            || tagName == HTMLNames::baseTag
            || tagName == HTMLNames::basefontTag
            || tagName == HTMLNames::bgsoundTag
            || tagName == HTMLNames::blockquoteTag
            || tagName == HTMLNames::bodyTag
            || tagName == HTMLNames::brTag
            || tagName == HTMLNames::buttonTag
            || tagName == HTMLNames::captionTag
            || tagName == HTMLNames::centerTag
            || tagName == HTMLNames::colTag
            || tagName == HTMLNames::colgroupTag
            || tagName == HTMLNames::commandTag
            || tagName == HTMLNames::ddTag
            || tagName == HTMLNames::detailsTag
            || tagName == HTMLNames::dirTag
            || tagName == HTMLNames::divTag
            || tagName == HTMLNames::dlTag
            || tagName == HTMLNames::dtTag
            || tagName == HTMLNames::embedTag
            || tagName == HTMLNames::fieldsetTag
            || tagName == HTMLNames::figcaptionTag
            || tagName == HTMLNames::figureTag
            || tagName == HTMLNames::footerTag
            || tagName == HTMLNames::formTag
            || tagName == HTMLNames::frameTag
            || tagName == HTMLNames::framesetTag
            || tagName == HTMLNames::h1Tag
            || tagName == HTMLNames::h2Tag
            || tagName == HTMLNames::h3Tag
            || tagName == HTMLNames::h4Tag
            || tagName == HTMLNames::h5Tag
            || tagName == HTMLNames::h6Tag
            || tagName == HTMLNames::headTag
            || tagName == HTMLNames::headerTag
            || tagName == HTMLNames::hgroupTag
            || tagName == HTMLNames::hrTag
            || tagName == HTMLNames::htmlTag
            || tagName == HTMLNames::iframeTag
            || tagName == HTMLNames::imgTag
            || tagName == HTMLNames::inputTag
            || tagName == HTMLNames::liTag
            || tagName == HTMLNames::linkTag
            || tagName == HTMLNames::listingTag
            || tagName == HTMLNames::mainTag
            || tagName == HTMLNames::marqueeTag
            || tagName == HTMLNames::menuTag
            || tagName == HTMLNames::metaTag
            || tagName == HTMLNames::navTag
            || tagName == HTMLNames::noembedTag
            || tagName == HTMLNames::noframesTag
            || tagName == HTMLNames::noscriptTag
            || tagName == HTMLNames::objectTag
            || tagName == HTMLNames::olTag
            || tagName == HTMLNames::pTag
            || tagName == HTMLNames::paramTag
            || tagName == HTMLNames::plaintextTag
            || tagName == HTMLNames::preTag
            || tagName == HTMLNames::scriptTag
            || tagName == HTMLNames::sectionTag
            || tagName == HTMLNames::selectTag
            || tagName == HTMLNames::styleTag
            || tagName == HTMLNames::summaryTag
            || tagName == HTMLNames::tableTag
            || tagName == HTMLNames::tbodyTag
            || tagName == HTMLNames::tdTag
            || tagName == HTMLNames::templateTag
            || tagName == HTMLNames::textareaTag
            || tagName == HTMLNames::tfootTag
            || tagName == HTMLNames::thTag
            || tagName == HTMLNames::theadTag
            || tagName == HTMLNames::titleTag
            || tagName == HTMLNames::trTag
            || tagName == HTMLNames::ulTag
            || tagName == HTMLNames::wbrTag
            || tagName == HTMLNames::xmpTag;
    }
    if (item.namespaceURI() == MathMLNames::mathmlNamespaceURI) {
        return tagName == MathMLNames::annotation_xmlTag
            || tagName == MathMLNames::miTag
            || tagName == MathMLNames::moTag
            || tagName == MathMLNames::mnTag
            || tagName == MathMLNames::msTag
            || tagName == MathMLNames::mtextTag;
    }
    if (item.namespaceURI() == SVGNames::svgNamespaceURI) {
        return tagName == SVGNames::descTag
            || tagName == SVGNames::foreignObjectTag
            || tagName == SVGNames::titleTag;
    }
    return false;
}

} // namespace WebCore
