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
struct PresentationAttributeCacheKey;

class StyledElement : public Element {
public:
    virtual ~StyledElement();

    virtual StylePropertySet* additionalAttributeStyle() { return 0; }
    void invalidateStyleAttribute();

    const StylePropertySet* inlineStyle() const { return attributeData() ? attributeData()->inlineStyle() : 0; }
    const StylePropertySet* ensureInlineStyle() { return ensureAttributeData()->ensureInlineStyle(this); }
    
    // Unlike StylePropertySet setters, these implement invalidation.
    bool setInlineStyleProperty(CSSPropertyID, int identifier, bool important = false);
    bool setInlineStyleProperty(CSSPropertyID, double value, CSSPrimitiveValue::UnitTypes, bool important = false);
    bool setInlineStyleProperty(CSSPropertyID, const String& value, bool important = false);
    bool removeInlineStyleProperty(CSSPropertyID);
    
    virtual CSSStyleDeclaration* style() OVERRIDE;

    StylePropertySet* attributeStyle();

    const SpaceSplitString& classNames() const;

    virtual void collectStyleForAttribute(const Attribute&, StylePropertySet*) { }

protected:
    StyledElement(const QualifiedName& name, Document* document, ConstructionType type)
        : Element(name, document, type)
    {
    }

    virtual void attributeChanged(const Attribute&) OVERRIDE;
    virtual void parseAttribute(const Attribute&);
    virtual void copyNonAttributeProperties(const Element*);

    virtual bool isPresentationAttribute(const QualifiedName&) const { return false; }

    void addPropertyToAttributeStyle(StylePropertySet*, CSSPropertyID, int identifier);
    void addPropertyToAttributeStyle(StylePropertySet*, CSSPropertyID, double value, CSSPrimitiveValue::UnitTypes);
    void addPropertyToAttributeStyle(StylePropertySet*, CSSPropertyID, const String& value);

    virtual void addSubresourceAttributeURLs(ListHashSet<KURL>&) const;

    // classAttributeChanged() exists to share code between
    // parseAttribute (called via setAttribute()) and
    // svgAttributeChanged (called when element.className.baseValue is set)
    void classAttributeChanged(const AtomicString& newClassString);

private:
    virtual void updateStyleAttribute() const;
    void inlineStyleChanged();

    void makePresentationAttributeCacheKey(PresentationAttributeCacheKey&) const;
    void updateAttributeStyle();

    void destroyInlineStyle()
    {
        if (attributeData())
            attributeData()->destroyInlineStyle(this);
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

inline StylePropertySet* StyledElement::attributeStyle()
{
    if (attributeStyleDirty())
        updateAttributeStyle();
    return attributeData() ? attributeData()->attributeStyle() : 0;
}

} //namespace

#endif
