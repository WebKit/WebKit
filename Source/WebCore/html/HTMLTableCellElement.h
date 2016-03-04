/*
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2010 Apple Inc. All rights reserved.
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

#ifndef HTMLTableCellElement_h
#define HTMLTableCellElement_h

#include "HTMLTablePartElement.h"

namespace WebCore {

class HTMLTableCellElement : public HTMLTablePartElement {
public:
    int cellIndex() const;
    unsigned colSpan() const;
    unsigned colSpanForBindings() const;
    unsigned rowSpan() const;
    unsigned rowSpanForBindings() const;

    void setCellIndex(int);
    void setColSpanForBindings(unsigned);
    void setRowSpanForBindings(unsigned);

    String abbr() const;
    String axis() const;
    String headers() const;
    String scope() const;

    WEBCORE_EXPORT HTMLTableCellElement* cellAbove() const;

protected:
    HTMLTableCellElement(const QualifiedName&, Document&);

    void parseAttribute(const QualifiedName&, const AtomicString&) override;
    bool isPresentationAttribute(const QualifiedName&) const override;
    void collectStyleForPresentationAttribute(const QualifiedName&, const AtomicString&, MutableStyleProperties&) override;
    const StyleProperties* additionalPresentationAttributeStyle() override;

    bool isURLAttribute(const Attribute&) const override;

    void addSubresourceAttributeURLs(ListHashSet<URL>&) const override;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::HTMLTableCellElement)
    static bool isType(const WebCore::HTMLElement& element) { return element.hasTagName(WebCore::HTMLNames::tdTag) || element.hasTagName(WebCore::HTMLNames::thTag); }
    static bool isType(const WebCore::Node& node) { return is<WebCore::HTMLElement>(node) && isType(downcast<WebCore::HTMLElement>(node)); }
SPECIALIZE_TYPE_TRAITS_END()

#endif
