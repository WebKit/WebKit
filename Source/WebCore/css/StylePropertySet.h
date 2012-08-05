/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2008, 2012 Apple Inc. All rights reserved.
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
 */

#ifndef StylePropertySet_h
#define StylePropertySet_h

#include "CSSParserMode.h"
#include "CSSPrimitiveValue.h"
#include "CSSProperty.h"
#include "CSSPropertyNames.h"
#include <wtf/ListHashSet.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class CSSRule;
class CSSStyleDeclaration;
class KURL;
class MemoryObjectInfo;
class PropertySetCSSStyleDeclaration;
class StyledElement;
class StylePropertyShorthand;
class StyleSheetContents;

class StylePropertySet : public RefCounted<StylePropertySet> {
public:
    ~StylePropertySet();

    static PassRefPtr<StylePropertySet> create(CSSParserMode cssParserMode = CSSQuirksMode)
    {
        return adoptRef(new StylePropertySet(cssParserMode));
    }
    static PassRefPtr<StylePropertySet> create(const CSSProperty* properties, unsigned count)
    {
        return adoptRef(new StylePropertySet(properties, count, CSSStrictMode, /* makeMutable */ true));
    }
    static PassRefPtr<StylePropertySet> createImmutable(const CSSProperty* properties, unsigned count, CSSParserMode);

    unsigned propertyCount() const;
    bool isEmpty() const;
    const CSSProperty& propertyAt(unsigned index) const;
    CSSProperty& propertyAt(unsigned index);

    PassRefPtr<CSSValue> getPropertyCSSValue(CSSPropertyID) const;
    String getPropertyValue(CSSPropertyID) const;
    bool propertyIsImportant(CSSPropertyID) const;
    CSSPropertyID getPropertyShorthand(CSSPropertyID) const;
    bool isPropertyImplicit(CSSPropertyID) const;

    // These expand shorthand properties into multiple properties.
    bool setProperty(CSSPropertyID, const String& value, bool important = false, StyleSheetContents* contextStyleSheet = 0);
    void setProperty(CSSPropertyID, PassRefPtr<CSSValue>, bool important = false);

    // These do not. FIXME: This is too messy, we can do better.
    bool setProperty(CSSPropertyID, int identifier, bool important = false);
    void setProperty(const CSSProperty&, CSSProperty* slot = 0);
    
    bool removeProperty(CSSPropertyID, String* returnText = 0);

    void parseDeclaration(const String& styleDeclaration, StyleSheetContents* contextStyleSheet);

    void addParsedProperties(const Vector<CSSProperty>&);
    void addParsedProperty(const CSSProperty&);

    PassRefPtr<StylePropertySet> copyBlockProperties() const;
    void removeBlockProperties();
    bool removePropertiesInSet(const CSSPropertyID* set, unsigned length);

    void mergeAndOverrideOnConflict(const StylePropertySet*);

    void setCSSParserMode(CSSParserMode);
    CSSParserMode cssParserMode() const { return static_cast<CSSParserMode>(m_cssParserMode); }

    void addSubresourceStyleURLs(ListHashSet<KURL>&, StyleSheetContents* contextStyleSheet) const;

    PassRefPtr<StylePropertySet> copy() const;

    // Used by StyledElement::copyNonAttributeProperties().
    void copyPropertiesFrom(const StylePropertySet&);

    void removeEquivalentProperties(const StylePropertySet*);
    void removeEquivalentProperties(const CSSStyleDeclaration*);

    PassRefPtr<StylePropertySet> copyPropertiesInSet(const CSSPropertyID* set, unsigned length) const;
    
    String asText() const;
    
    void clearParentElement(StyledElement*);

    CSSStyleDeclaration* ensureCSSStyleDeclaration() const;
    CSSStyleDeclaration* ensureInlineCSSStyleDeclaration(const StyledElement* parentElement) const;

    bool isMutable() const { return m_isMutable; }

    bool hasFailedOrCanceledSubresources() const;

    static unsigned averageSizeInBytes();
    void reportMemoryUsage(MemoryObjectInfo*) const;

#ifndef NDEBUG
    void showStyle();
#endif
    
private:
    StylePropertySet(CSSParserMode);
    StylePropertySet(const CSSProperty* properties, unsigned count, CSSParserMode, bool makeMutable);
    StylePropertySet(const StylePropertySet&);

    void setNeedsStyleRecalc();

    String getShorthandValue(const StylePropertyShorthand&) const;
    String getCommonValue(const StylePropertyShorthand&) const;
    enum CommonValueMode { OmitUncommonValues, ReturnNullOnUncommonValues };
    String borderPropertyValue(CommonValueMode) const;
    String getLayeredShorthandValue(const StylePropertyShorthand&) const;
    String get4Values(const StylePropertyShorthand&) const;
    String borderSpacingValue(const StylePropertyShorthand&) const;
    String fontValue() const;
    bool appendFontLonghandValueIfExplicit(CSSPropertyID, StringBuilder& result) const;

    bool removeShorthandProperty(CSSPropertyID);
    bool propertyMatches(const CSSProperty*) const;

    const CSSProperty* findPropertyWithId(CSSPropertyID) const;
    CSSProperty* findPropertyWithId(CSSPropertyID);

    void append(const CSSProperty&);
    CSSProperty* array();
    const CSSProperty* array() const;

    unsigned m_cssParserMode : 2;
    mutable unsigned m_ownsCSSOMWrapper : 1;
    mutable unsigned m_isMutable : 1;
    unsigned m_arraySize : 28;

    union {
        Vector<CSSProperty>* m_mutablePropertyVector;
        void* m_properties;
    };
    
    friend class PropertySetCSSStyleDeclaration;
};

inline CSSProperty& StylePropertySet::propertyAt(unsigned index)
{
    if (isMutable())
        return m_mutablePropertyVector->at(index);
    return array()[index];
}

inline const CSSProperty& StylePropertySet::propertyAt(unsigned index) const
{
    if (isMutable())
        return m_mutablePropertyVector->at(index);
    return array()[index];
}

inline unsigned StylePropertySet::propertyCount() const
{
    if (isMutable())
        return m_mutablePropertyVector->size();
    return m_arraySize;
}

inline bool StylePropertySet::isEmpty() const
{
    return !propertyCount();
}

inline CSSProperty* StylePropertySet::array()
{
    ASSERT(!isMutable());
    return reinterpret_cast<CSSProperty*>(&m_properties);
}

inline const CSSProperty* StylePropertySet::array() const
{
    ASSERT(!isMutable());
    return reinterpret_cast<const CSSProperty*>(&m_properties);
}

} // namespace WebCore

#endif // StylePropertySet_h
