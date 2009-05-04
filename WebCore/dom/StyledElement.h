/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Peter Kelly (pmk@post.com)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef StyledElement_h
#define StyledElement_h

#include "CSSPrimitiveValue.h"
#include "Element.h"
#include "MappedAttributeEntry.h"
#include "NamedMappedAttrMap.h"

namespace WebCore {

class CSSMappedAttributeDeclaration;
class CSSMutableStyleDeclaration;
class MappedAttribute;

class StyledElement : public Element {
public:
    StyledElement(const QualifiedName&, Document*);
    virtual ~StyledElement();

    virtual bool isStyledElement() const { return true; }

    NamedMappedAttrMap* mappedAttributes() { return static_cast<NamedMappedAttrMap*>(namedAttrMap.get()); }
    const NamedMappedAttrMap* mappedAttributes() const { return static_cast<NamedMappedAttrMap*>(namedAttrMap.get()); }
    bool hasMappedAttributes() const { return namedAttrMap && mappedAttributes()->hasMappedAttributes(); }
    bool isMappedAttribute(const QualifiedName& name) const { MappedAttributeEntry res = eNone; mapToEntry(name, res); return res != eNone; }

    void addCSSLength(MappedAttribute* attr, int id, const String &value);
    void addCSSProperty(MappedAttribute* attr, int id, const String &value);
    void addCSSProperty(MappedAttribute* attr, int id, int value);
    void addCSSStringProperty(MappedAttribute* attr, int id, const String &value, CSSPrimitiveValue::UnitTypes = CSSPrimitiveValue::CSS_STRING);
    void addCSSImageProperty(MappedAttribute*, int propertyID, const String& url);
    void addCSSColor(MappedAttribute* attr, int id, const String &c);
    void createMappedDecl(MappedAttribute* attr);
    
    static CSSMappedAttributeDeclaration* getMappedAttributeDecl(MappedAttributeEntry type, const QualifiedName& name, const AtomicString& value);
    static void setMappedAttributeDecl(MappedAttributeEntry, const QualifiedName& name, const AtomicString& value, CSSMappedAttributeDeclaration*);
    static void removeMappedAttributeDecl(MappedAttributeEntry type, const QualifiedName& name, const AtomicString& value);

    static CSSMappedAttributeDeclaration* getMappedAttributeDecl(MappedAttributeEntry, Attribute*);
    static void setMappedAttributeDecl(MappedAttributeEntry, Attribute*, CSSMappedAttributeDeclaration*);
    
    CSSMutableStyleDeclaration* inlineStyleDecl() const { return m_inlineStyleDecl.get(); }
    virtual bool canHaveAdditionalAttributeStyleDecls() const { return false; }
    virtual void additionalAttributeStyleDecls(Vector<CSSMutableStyleDeclaration*>&) {};
    CSSMutableStyleDeclaration* getInlineStyleDecl();
    CSSStyleDeclaration* style();
    void createInlineStyleDecl();
    void destroyInlineStyleDecl();
    void invalidateStyleAttribute();
    virtual void updateStyleAttribute() const;
    
    const ClassNames& classNames() const { ASSERT(hasClass()); ASSERT(mappedAttributes()); return mappedAttributes()->classNames(); }

    virtual void attributeChanged(Attribute*, bool preserveDecls = false);
    virtual void parseMappedAttribute(MappedAttribute*);
    virtual bool mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const;
    virtual void createAttributeMap() const;
    virtual PassRefPtr<Attribute> createAttribute(const QualifiedName&, const AtomicString& value);

    virtual void copyNonAttributeProperties(const Element*);

    virtual void addSubresourceAttributeURLs(ListHashSet<KURL>&) const;

protected:
    // classAttributeChanged() exists to share code between
    // parseMappedAttribute (called via setAttribute()) and
    // svgAttributeChanged (called when element.className.baseValue is set)
    void classAttributeChanged(const AtomicString& newClassString);
    
    virtual void didMoveToNewOwnerDocument();

    RefPtr<CSSMutableStyleDeclaration> m_inlineStyleDecl;
};

} //namespace

#endif
