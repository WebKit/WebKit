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

    void invalidateStyleAttribute();

    const StyleProperties* inlineStyle() const { return elementData() ? elementData()->m_inlineStyle.get() : nullptr; }
    
    bool setInlineStyleProperty(CSSPropertyID, CSSValueID identifier, bool important = false);
    bool setInlineStyleProperty(CSSPropertyID, CSSPropertyID identifier, bool important = false);
    WEBCORE_EXPORT bool setInlineStyleProperty(CSSPropertyID, double value, CSSUnitType, bool important = false);
    WEBCORE_EXPORT bool setInlineStyleProperty(CSSPropertyID, const String& value, bool important = false);
    bool removeInlineStyleProperty(CSSPropertyID);
    void removeAllInlineStyleProperties();

    void synchronizeStyleAttributeInternal() const { const_cast<StyledElement*>(this)->synchronizeStyleAttributeInternalImpl(); }
    
    WEBCORE_EXPORT CSSStyleDeclaration& cssomStyle();
#if ENABLE(CSS_TYPED_OM)
    StylePropertyMap& ensureAttributeStyleMap();
#endif

    // https://html.spec.whatwg.org/#presentational-hints
    const StyleProperties* presentationalHintStyle() const;
    virtual void collectPresentationalHintsForAttribute(const QualifiedName&, const AtomString&, MutableStyleProperties&) { }
    virtual const StyleProperties* additionalPresentationalHintStyle() const { return nullptr; }
    virtual void collectExtraStyleForPresentationalHints(MutableStyleProperties&) { }

protected:
    StyledElement(const QualifiedName& name, Document& document, ConstructionType type)
        : Element(name, document, type)
    {
    }

    void attributeChanged(const QualifiedName&, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason = ModifiedDirectly) override;

    virtual bool hasPresentationalHintsForAttribute(const QualifiedName&) const { return false; }

    void addPropertyToPresentationalHintStyle(MutableStyleProperties&, CSSPropertyID, CSSValueID identifier);
    void addPropertyToPresentationalHintStyle(MutableStyleProperties&, CSSPropertyID, double value, CSSUnitType);
    void addPropertyToPresentationalHintStyle(MutableStyleProperties&, CSSPropertyID, const String& value);

    void addSubresourceAttributeURLs(ListHashSet<URL>&) const override;

private:
    void styleAttributeChanged(const AtomString& newStyleString, AttributeModificationReason);
    void synchronizeStyleAttributeInternalImpl();

    void inlineStyleChanged();
    PropertySetCSSStyleDeclaration* inlineStyleCSSOMWrapper();
    void setInlineStyleFromString(const AtomString&);
    MutableStyleProperties& ensureMutableInlineStyle();

    void rebuildPresentationalHintStyle();
};

inline const StyleProperties* StyledElement::presentationalHintStyle() const
{
    if (!elementData())
        return nullptr;
    if (elementData()->presentationalHintStyleIsDirty())
        const_cast<StyledElement&>(*this).rebuildPresentationalHintStyle();
    return elementData()->presentationalHintStyle();
}

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::StyledElement)
    static bool isType(const WebCore::Node& node) { return node.isStyledElement(); }
SPECIALIZE_TYPE_TRAITS_END()
