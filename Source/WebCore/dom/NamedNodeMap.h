/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Peter Kelly (pmk@post.com)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2008, 2010, 2013 Apple Inc. All rights reserved.
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

#include "Attr.h"

namespace WebCore {

class NamedNodeMap : public ScriptWrappable {
    WTF_MAKE_FAST_ALLOCATED;
    friend class Element;
public:
    explicit NamedNodeMap(Element& element)
        : m_element(element)
    {
        // Only supports NamedNodeMaps with Element associated, DocumentType.entities and DocumentType.notations are not supported yet.
    }

    WEBCORE_EXPORT void ref();
    WEBCORE_EXPORT void deref();

    // Public DOM interface.

    WEBCORE_EXPORT RefPtr<Attr> getNamedItem(const AtomicString&) const;
    WEBCORE_EXPORT RefPtr<Attr> removeNamedItem(const AtomicString& name, ExceptionCode&);
    Vector<String> supportedPropertyNames();

    WEBCORE_EXPORT RefPtr<Attr> getNamedItemNS(const AtomicString& namespaceURI, const AtomicString& localName) const;
    WEBCORE_EXPORT RefPtr<Attr> removeNamedItemNS(const AtomicString& namespaceURI, const AtomicString& localName, ExceptionCode&);

    RefPtr<Attr> setNamedItem(Attr&, ExceptionCode&);
    WEBCORE_EXPORT RefPtr<Attr> setNamedItem(Node&, ExceptionCode&); // for legacy bindings.

    WEBCORE_EXPORT RefPtr<Attr> item(unsigned index) const;
    WEBCORE_EXPORT unsigned length() const;

    Element& element() const { return m_element; }

private:
    Element& m_element;
};

} // namespace WebCore
