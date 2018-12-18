/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Peter Kelly (pmk@post.com)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-2018 Apple Inc. All rights reserved.
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

#include "CSSPrimitiveValue.h"
#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "Element.h"

namespace WebCore {

class Attribute;
class MutableStyleProperties;
class PropertySetCSSStyleDeclaration;
class StyleProperties;
class StylePropertyMap;

class StyledElement : public Element {
    WTF_MAKE_ISO_ALLOCATED(StyledElement);
public:
    virtual ~StyledElement();

    virtual const StyleProperties* additionalPresentationAttributeStyle() const { return nullptr; }
    void invalidateStyleAttribute();

    const StyleProperties* inlineStyle() const { return elementData() ? elementData()->m_inlineStyle.get() : nullptr; }
    
    bool setInlineStyleProperty(CSSPropertyID, CSSValueID identifier, bool important = false);
    bool setInlineStyleProperty(CSSPropertyID, CSSPropertyID identifier, bool important = false);
    WEBCORE_EXPORT bool setInlineStyleProperty(CSSPropertyID, double value, CSSPrimitiveValue::UnitType, bool important = false);
    WEBCORE_EXPORT bool setInlineStyleProperty(CSSPropertyID, const String& value, bool important = false);
    bool removeInlineStyleProperty(CSSPropertyID);
    void removeAllInlineStyleProperties();

    static void synchronizeStyleAttributeInternal(StyledElement*);
    void synchronizeStyleAttributeInternal() const { StyledElement::synchronizeStyleAttributeInternal(const_cast<StyledElement*>(this)); }
    
    WEBCORE_EXPORT CSSStyleDeclaration& cssomStyle();
#if ENABLE(CSS_TYPED_OM)
    StylePropertyMap& ensureAttributeStyleMap();
#endif

    const StyleProperties* presentationAttributeStyle() const;
    virtual void collectStyleForPresentationAttribute(const QualifiedName&, const AtomicString&, MutableStyleProperties&) { }

protected:
    StyledElement(const QualifiedName& name, Document& document, ConstructionType type)
        : Element(name, document, type)
    {
    }

    void attributeChanged(const QualifiedName&, const AtomicString& oldValue, const AtomicString& newValue, AttributeModificationReason = ModifiedDirectly) override;

    virtual bool isPresentationAttribute(const QualifiedName&) const { return false; }

    void addPropertyToPresentationAttributeStyle(MutableStyleProperties&, CSSPropertyID, CSSValueID identifier);
    void addPropertyToPresentationAttributeStyle(MutableStyleProperties&, CSSPropertyID, double value, CSSPrimitiveValue::UnitType);
    void addPropertyToPresentationAttributeStyle(MutableStyleProperties&, CSSPropertyID, const String& value);

    void addSubresourceAttributeURLs(ListHashSet<URL>&) const override;

private:
    void styleAttributeChanged(const AtomicString& newStyleString, AttributeModificationReason);

    void inlineStyleChanged();
    PropertySetCSSStyleDeclaration* inlineStyleCSSOMWrapper();
    void setInlineStyleFromString(const AtomicString&);
    MutableStyleProperties& ensureMutableInlineStyle();

    void rebuildPresentationAttributeStyle();
};

inline const StyleProperties* StyledElement::presentationAttributeStyle() const
{
    if (!elementData())
        return nullptr;
    if (elementData()->presentationAttributeStyleIsDirty())
        const_cast<StyledElement&>(*this).rebuildPresentationAttributeStyle();
    return elementData()->presentationAttributeStyle();
}

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::StyledElement)
    static bool isType(const WebCore::Node& node) { return node.isStyledElement(); }
SPECIALIZE_TYPE_TRAITS_END()
