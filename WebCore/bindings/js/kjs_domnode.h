/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef KJS_DOMNODE_H
#define KJS_DOMNODE_H

#include <kjs_binding.h>
#include <kjs/lookup.h>

namespace WebCore {
    class JSNode;
}

namespace KJS {

KJS_DEFINE_PROTOTYPE(DOMNodeProto)

class DOMNode : public DOMObject {
public:
    virtual ~DOMNode();
    virtual bool toBoolean(ExecState*) const;
    virtual bool getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&);
    JSValue *getValueProperty(ExecState*, int token) const;
    virtual void mark();
    virtual void put(ExecState*, const Identifier &propertyName, JSValue*, int attr = None);
    void putValueProperty(ExecState*, int token, JSValue*, int attr);
    WebCore::Node *impl() const { return m_impl.get(); }
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    
    virtual JSValue *toPrimitive(ExecState*, JSType preferred = UndefinedType) const;
    virtual UString toString(ExecState*) const;
    
    enum { NodeName, NodeValue, NodeType, ParentNode, ParentElement,
        ChildNodes, FirstChild, LastChild, PreviousSibling, NextSibling, Item,
        Attributes, NamespaceURI, Prefix, LocalName, OwnerDocument, InsertBefore,
        ReplaceChild, RemoveChild, AppendChild, HasAttributes, HasChildNodes,
        CloneNode, Normalize, IsSupported, Contains, IsSameNode, IsEqualNode, TextContent,
        IsDefaultNamespace, LookupNamespaceURI, LookupPrefix,
    };
    
protected:
    RefPtr<WebCore::Node> m_impl;

private:
    // Don't use this class directly -- use JSNode instead
    friend class WebCore::JSNode;
    DOMNode();
    DOMNode(ExecState*, WebCore::Node *n);
    DOMNode(WebCore::Node *n);
};

} // namespace KJS

#endif
