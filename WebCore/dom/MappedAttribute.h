/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Peter Kelly (pmk@post.com)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.
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

#ifndef MappedAttribute_h
#define MappedAttribute_h

#include "Attribute.h"
#include "CSSMappedAttributeDeclaration.h"

namespace WebCore {

class Attr;
class CSSStyleDeclaration;
class CSSStyleSelector;
class Element;
class NamedAttrMap;

class MappedAttribute : public Attribute
{
public:
    MappedAttribute(const QualifiedName& name, const AtomicString& value, CSSMappedAttributeDeclaration* decl = 0)
    : Attribute(name, value), m_styleDecl(decl)
    {
    }

    MappedAttribute(const AtomicString& name, const AtomicString& value, CSSMappedAttributeDeclaration* decl = 0)
    : Attribute(name, value), m_styleDecl(decl)
    {
    }

    virtual Attribute* clone(bool preserveDecl=true) const;

    virtual CSSStyleDeclaration* style() const { return m_styleDecl.get(); }

    CSSMappedAttributeDeclaration* decl() const { return m_styleDecl.get(); }
    void setDecl(CSSMappedAttributeDeclaration* decl) { m_styleDecl = decl; }

private:
    RefPtr<CSSMappedAttributeDeclaration> m_styleDecl;
};

} //namespace

#endif
