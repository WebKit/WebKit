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

#include "ExceptionOr.h"
#include "ScriptWrappable.h"

namespace WebCore {

class Attr;
class Element;

class NamedNodeMap final : public ScriptWrappable {
    WTF_MAKE_ISO_ALLOCATED(NamedNodeMap);
public:
    explicit NamedNodeMap(Element& element)
        : m_element(element)
    {
    }

    WEBCORE_EXPORT void ref();
    WEBCORE_EXPORT void deref();

    WEBCORE_EXPORT unsigned length() const;
    WEBCORE_EXPORT RefPtr<Attr> item(unsigned index) const;
    WEBCORE_EXPORT RefPtr<Attr> getNamedItem(const AtomString&) const;
    WEBCORE_EXPORT RefPtr<Attr> getNamedItemNS(const AtomString& namespaceURI, const AtomString& localName) const;
    WEBCORE_EXPORT ExceptionOr<RefPtr<Attr>> setNamedItem(Attr&);
    WEBCORE_EXPORT ExceptionOr<Ref<Attr>> removeNamedItem(const AtomString& name);
    WEBCORE_EXPORT ExceptionOr<Ref<Attr>> removeNamedItemNS(const AtomString& namespaceURI, const AtomString& localName);

    Vector<String> supportedPropertyNames() const;

    Element& element() { return m_element; }

private:
    Element& m_element;
};

} // namespace WebCore
