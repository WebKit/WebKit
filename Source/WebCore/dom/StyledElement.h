/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Peter Kelly (pmk@post.com)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
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

#include "Element.h"
#include "StylePropertySet.h"

namespace WebCore {

class Attribute;

class StyledElement : public Element {
public:
    virtual ~StyledElement();

    void addCSSLength(int id, const String& value);
    void addCSSProperty(int id, const String& value);
    void addCSSProperty(int id, int value);
    void addCSSImageProperty(int propertyID, const String& url);
    void addCSSColor(int id, const String& color);
    void removeCSSProperties(int id1, int id2 = CSSPropertyInvalid, int id3 = CSSPropertyInvalid, int id4 = CSSPropertyInvalid, int id5 = CSSPropertyInvalid, int id6 = CSSPropertyInvalid, int id7 = CSSPropertyInvalid, int id8 = CSSPropertyInvalid);
    void removeCSSProperty(int id) { removeCSSProperties(id); }

    virtual PassRefPtr<StylePropertySet> additionalAttributeStyle() { return 0; }
    void invalidateStyleAttribute();

    StylePropertySet* inlineStyleDecl() const { return attributeData() ? attributeData()->inlineStyleDecl() : 0; }
    StylePropertySet* ensureInlineStyleDecl() { return ensureAttributeDataWithoutUpdate()->ensureInlineStyleDecl(this); }
    virtual CSSStyleDeclaration* style() OVERRIDE { return ensureInlineStyleDecl()->ensureCSSStyleDeclaration(); }

    StylePropertySet* attributeStyle() const { return attributeData() ? attributeData()->attributeStyle() : 0; }
    StylePropertySet* ensureAttributeStyle() { return ensureAttributeDataWithoutUpdate()->ensureAttributeStyle(this); }

    const SpaceSplitString& classNames() const;

protected:
    StyledElement(const QualifiedName& name, Document* document, ConstructionType type)
        : Element(name, document, type)
    {
    }

    virtual void attributeChanged(Attribute*) OVERRIDE;
    virtual void parseAttribute(Attribute*);
    virtual void copyNonAttributeProperties(const Element*);

    virtual void addSubresourceAttributeURLs(ListHashSet<KURL>&) const;

    // classAttributeChanged() exists to share code between
    // parseAttribute (called via setAttribute()) and
    // svgAttributeChanged (called when element.className.baseValue is set)
    void classAttributeChanged(const AtomicString& newClassString);

private:
    virtual void updateStyleAttribute() const;

    void destroyInlineStyleDecl()
    {
        if (attributeData())
            attributeData()->destroyInlineStyleDecl();
    }
};

inline const SpaceSplitString& StyledElement::classNames() const
{
    ASSERT(hasClass());
    ASSERT(attributeData());
    return attributeData()->classNames();
}

inline void StyledElement::invalidateStyleAttribute()
{
    clearIsStyleAttributeValid();
}

} //namespace

#endif
