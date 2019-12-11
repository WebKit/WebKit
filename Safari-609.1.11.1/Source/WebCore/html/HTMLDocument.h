/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#pragma once

#include "Document.h"

namespace WebCore {

class HTMLDocument : public Document {
    WTF_MAKE_ISO_ALLOCATED(HTMLDocument);
public:
    static Ref<HTMLDocument> create(Frame*, const URL&);
    static Ref<HTMLDocument> createSynthesizedDocument(Frame&, const URL&);
    virtual ~HTMLDocument();

    WEBCORE_EXPORT int width();
    WEBCORE_EXPORT int height();
    
    Optional<Variant<RefPtr<WindowProxy>, RefPtr<Element>, RefPtr<HTMLCollection>>> namedItem(const AtomString&);
    Vector<AtomString> supportedPropertyNames() const;

    Element* documentNamedItem(const AtomStringImpl& name) const { return m_documentNamedItem.getElementByDocumentNamedItem(name, *this); }
    bool hasDocumentNamedItem(const AtomStringImpl& name) const { return m_documentNamedItem.contains(name); }
    bool documentNamedItemContainsMultipleElements(const AtomStringImpl& name) const { return m_documentNamedItem.containsMultiple(name); }
    void addDocumentNamedItem(const AtomStringImpl&, Element&);
    void removeDocumentNamedItem(const AtomStringImpl&, Element&);

    Element* windowNamedItem(const AtomStringImpl& name) const { return m_windowNamedItem.getElementByWindowNamedItem(name, *this); }
    bool hasWindowNamedItem(const AtomStringImpl& name) const { return m_windowNamedItem.contains(name); }
    bool windowNamedItemContainsMultipleElements(const AtomStringImpl& name) const { return m_windowNamedItem.containsMultiple(name); }
    void addWindowNamedItem(const AtomStringImpl&, Element&);
    void removeWindowNamedItem(const AtomStringImpl&, Element&);

    static bool isCaseSensitiveAttribute(const QualifiedName&);

protected:
    HTMLDocument(Frame*, const URL&, DocumentClassFlags = 0, unsigned constructionFlags = 0);

private:
    bool isFrameSet() const final;
    Ref<DocumentParser> createParser() override;
    Ref<Document> cloneDocumentWithoutChildren() const final;

    TreeScopeOrderedMap m_documentNamedItem;
    TreeScopeOrderedMap m_windowNamedItem;
};

inline Ref<HTMLDocument> HTMLDocument::create(Frame* frame, const URL& url)
{
    return adoptRef(*new HTMLDocument(frame, url, HTMLDocumentClass));
}

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::HTMLDocument)
    static bool isType(const WebCore::Document& document) { return document.isHTMLDocument(); }
    static bool isType(const WebCore::Node& node) { return is<WebCore::Document>(node) && isType(downcast<WebCore::Document>(node)); }
SPECIALIZE_TYPE_TRAITS_END()
