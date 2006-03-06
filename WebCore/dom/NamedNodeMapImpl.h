/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifndef DOM_NamedNodeMapImpl_h
#define DOM_NamedNodeMapImpl_h

#include "Shared.h"
#include "PlatformString.h"
#include <kxmlcore/PassRefPtr.h>

namespace WebCore {

class NodeImpl;
class QualifiedName;

typedef int ExceptionCode;

// Generic NamedNodeMap interface
// Other classes implement this for more specific situations e.g. attributes of an element.
class NamedNodeMapImpl : public Shared<NamedNodeMapImpl>
{
public:
    NamedNodeMapImpl() { }
    virtual ~NamedNodeMapImpl() { }

    PassRefPtr<NodeImpl> getNamedItem(const DOMString& name) const { return getNamedItemNS(DOMString(), name); }
    PassRefPtr<NodeImpl> removeNamedItem(const DOMString& name, ExceptionCode& ec) { return removeNamedItemNS(DOMString(), name, ec); }

    virtual PassRefPtr<NodeImpl> getNamedItemNS(const DOMString &namespaceURI, const DOMString &localName) const = 0;
    PassRefPtr<NodeImpl> setNamedItemNS(NodeImpl* arg, ExceptionCode& ec) { return setNamedItem(arg, ec); }
    virtual PassRefPtr<NodeImpl> removeNamedItemNS(const String& namespaceURI, const String& localName, ExceptionCode&) = 0;

    // DOM methods & attributes for NamedNodeMap
    virtual PassRefPtr<NodeImpl> getNamedItem(const QualifiedName& attrName) const = 0;
    virtual PassRefPtr<NodeImpl> removeNamedItem (const QualifiedName& attrName, ExceptionCode&) = 0;
    virtual PassRefPtr<NodeImpl> setNamedItem (NodeImpl* arg, ExceptionCode&) = 0;

    virtual PassRefPtr<NodeImpl> item(unsigned index) const = 0;
    virtual unsigned length() const = 0;

    // Other methods (not part of DOM)
    virtual bool isReadOnly() { return false; }
};

} //namespace

#endif
