/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2008, 2012, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
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

#ifndef StyleProperties_h
#define StyleProperties_h

#include "CSSParserMode.h"
#include "CSSPrimitiveValue.h"
#include "CSSProperty.h"
#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include <memory>
#include <wtf/ListHashSet.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class CSSRule;
class CSSStyleDeclaration;
class ComputedStyleExtractor;
class ImmutableStyleProperties;
class URL;
class MutableStyleProperties;
class PropertySetCSSStyleDeclaration;
class StyledElement;
class StylePropertyShorthand;
class StyleSheetContents;

class StyleProperties : public RefCounted<StyleProperties> {
    friend class PropertyReference;
public:
    // Override RefCounted's deref() to ensure operator delete is called on
    // the appropriate subclass type.
    void deref();

    class PropertyReference {
    public:
        PropertyReference(const StyleProperties& propertySet, unsigned index)
            : m_propertySet(propertySet)
            , m_index(index)
        {
        }

        CSSPropertyID id() const { return static_cast<CSSPropertyID>(propertyMetadata().m_propertyID); }
        CSSPropertyID shorthandID() const { return propertyMetadata().shorthandID(); }

        bool isImportant() const { return propertyMetadata().m_important; }
        bool isInherited() const { return propertyMetadata().m_inherited; }
        bool isImplicit() const { return propertyMetadata().m_implicit; }

        String cssName() const;
        String cssText() const;

        const CSSValue* value() const { return propertyValue(); }
        // FIXME: We should try to remove this mutable overload.
        CSSValue* value() { return const_cast<CSSValue*>(propertyValue()); }

        // FIXME: Remove this.
        CSSProperty toCSSProperty() const { return CSSProperty(propertyMetadata(), const_cast<CSSValue*>(propertyValue())); }
        const StylePropertyMetadata& propertyMetadata() const;

    private:
        const CSSValue* propertyValue() const;

        const StyleProperties& m_propertySet;
        unsigned m_index;
    };

    unsigned propertyCount() const;
    bool isEmpty() const;
    PropertyReference propertyAt(unsigned index) const { return PropertyReference(*this, index); }

    PassRefPtr<CSSValue> getPropertyCSSValue(CSSPropertyID) const;
    String getPropertyValue(CSSPropertyID) const;
    bool propertyIsImportant(CSSPropertyID) const;
    String getPropertyShorthand(CSSPropertyID) const;
    bool isPropertyImplicit(CSSPropertyID) const;

    PassRef<MutableStyleProperties> copyBlockProperties() const;

    CSSParserMode cssParserMode() const { return static_cast<CSSParserMode>(m_cssParserMode); }

    void addSubresourceStyleURLs(ListHashSet<URL>&, StyleSheetContents* contextStyleSheet) const;

    PassRef<MutableStyleProperties> mutableCopy() const;
    PassRef<ImmutableStyleProperties> immutableCopyIfNeeded() const;

    PassRef<MutableStyleProperties> copyPropertiesInSet(const CSSPropertyID* set, unsigned length) const;
    
    String asText() const;

    bool isMutable() const { return m_isMutable; }
    bool hasCSSOMWrapper() const;

    bool hasFailedOrCanceledSubresources() const;

    static unsigned averageSizeInBytes();

#ifndef NDEBUG
    void showStyle();
#endif

    bool propertyMatches(CSSPropertyID, const CSSValue*) const;

protected:
    StyleProperties(CSSParserMode cssParserMode)
        : m_cssParserMode(cssParserMode)
        , m_isMutable(true)
        , m_arraySize(0)
    { }

    StyleProperties(CSSParserMode cssParserMode, unsigned immutableArraySize)
        : m_cssParserMode(cssParserMode)
        , m_isMutable(false)
        , m_arraySize(immutableArraySize)
    { }

    int findPropertyIndex(CSSPropertyID) const;

    unsigned m_cssParserMode : 2;
    mutable unsigned m_isMutable : 1;
    unsigned m_arraySize : 29;
    
private:
    String getShorthandValue(const StylePropertyShorthand&) const;
    String getCommonValue(const StylePropertyShorthand&) const;
    enum CommonValueMode { OmitUncommonValues, ReturnNullOnUncommonValues };
    String borderPropertyValue(CommonValueMode) const;
    String getLayeredShorthandValue(const StylePropertyShorthand&) const;
    String get4Values(const StylePropertyShorthand&) const;
    String borderSpacingValue(const StylePropertyShorthand&) const;
    String fontValue() const;
    void appendFontLonghandValueIfExplicit(CSSPropertyID, StringBuilder& result, String& value) const;

    friend class PropertySetCSSStyleDeclaration;
};

class ImmutableStyleProperties : public StyleProperties {
public:
    ~ImmutableStyleProperties();
    static PassRef<ImmutableStyleProperties> create(const CSSProperty* properties, unsigned count, CSSParserMode);

    unsigned propertyCount() const { return m_arraySize; }

    const CSSValue** valueArray() const;
    const StylePropertyMetadata* metadataArray() const;
    int findPropertyIndex(CSSPropertyID) const;

    void* m_storage;

private:
    ImmutableStyleProperties(const CSSProperty*, unsigned count, CSSParserMode);
};

inline const CSSValue** ImmutableStyleProperties::valueArray() const
{
    return reinterpret_cast<const CSSValue**>(const_cast<const void**>((&(this->m_storage))));
}

inline const StylePropertyMetadata* ImmutableStyleProperties::metadataArray() const
{
    return reinterpret_cast_ptr<const StylePropertyMetadata*>(&reinterpret_cast_ptr<const char*>(&(this->m_storage))[m_arraySize * sizeof(CSSValue*)]);
}

class MutableStyleProperties : public StyleProperties {
public:
    static PassRef<MutableStyleProperties> create(CSSParserMode = CSSQuirksMode);
    static PassRef<MutableStyleProperties> create(const CSSProperty* properties, unsigned count);

    ~MutableStyleProperties();

    unsigned propertyCount() const { return m_propertyVector.size(); }

    PropertySetCSSStyleDeclaration* cssStyleDeclaration();

    void addParsedProperties(const Vector<CSSProperty>&);
    void addParsedProperty(const CSSProperty&);

    // These expand shorthand properties into multiple properties.
    bool setProperty(CSSPropertyID, const String& value, bool important = false, StyleSheetContents* contextStyleSheet = 0);
    void setProperty(CSSPropertyID, PassRefPtr<CSSValue>, bool important = false);

    // These do not. FIXME: This is too messy, we can do better.
    bool setProperty(CSSPropertyID, CSSValueID identifier, bool important = false);
    bool setProperty(CSSPropertyID, CSSPropertyID identifier, bool important = false);
    void appendPrefixingVariantProperty(const CSSProperty&);
    void setPrefixingVariantProperty(const CSSProperty&);
    void setProperty(const CSSProperty&, CSSProperty* slot = 0);

    bool removeProperty(CSSPropertyID, String* returnText = 0);
    void removePrefixedOrUnprefixedProperty(CSSPropertyID);
    void removeBlockProperties();
    bool removePropertiesInSet(const CSSPropertyID* set, unsigned length);

    // FIXME: These two can be moved to EditingStyle.cpp
    void removeEquivalentProperties(const StyleProperties*);
    void removeEquivalentProperties(const ComputedStyleExtractor*);

    void mergeAndOverrideOnConflict(const StyleProperties&);

    void clear();
    void parseDeclaration(const String& styleDeclaration, StyleSheetContents* contextStyleSheet);

    CSSStyleDeclaration* ensureCSSStyleDeclaration();
    CSSStyleDeclaration* ensureInlineCSSStyleDeclaration(StyledElement* parentElement);

    int findPropertyIndex(CSSPropertyID) const;

    Vector<CSSProperty, 4> m_propertyVector;

private:
    explicit MutableStyleProperties(CSSParserMode);
    explicit MutableStyleProperties(const StyleProperties&);
    MutableStyleProperties(const CSSProperty* properties, unsigned count);

    bool removeShorthandProperty(CSSPropertyID);
    CSSProperty* findCSSPropertyWithID(CSSPropertyID);
    std::unique_ptr<PropertySetCSSStyleDeclaration> m_cssomWrapper;

    friend class StyleProperties;
};

TYPE_CASTS_BASE(MutableStyleProperties, StyleProperties, set, set->isMutable(), set.isMutable());

inline MutableStyleProperties* toMutableStyleProperties(const RefPtr<StyleProperties>& set)
{
    return toMutableStyleProperties(set.get());
}

TYPE_CASTS_BASE(ImmutableStyleProperties, StyleProperties, set, !set->isMutable(), !set.isMutable());

inline ImmutableStyleProperties* toImmutableStyleProperties(const RefPtr<StyleProperties>& set)
{
    return toImmutableStyleProperties(set.get());
}

inline const StylePropertyMetadata& StyleProperties::PropertyReference::propertyMetadata() const
{
    if (m_propertySet.isMutable())
        return static_cast<const MutableStyleProperties&>(m_propertySet).m_propertyVector.at(m_index).metadata();
    return static_cast<const ImmutableStyleProperties&>(m_propertySet).metadataArray()[m_index];
}

inline const CSSValue* StyleProperties::PropertyReference::propertyValue() const
{
    if (m_propertySet.isMutable())
        return static_cast<const MutableStyleProperties&>(m_propertySet).m_propertyVector.at(m_index).value();
    return static_cast<const ImmutableStyleProperties&>(m_propertySet).valueArray()[m_index];
}

inline unsigned StyleProperties::propertyCount() const
{
    if (m_isMutable)
        return static_cast<const MutableStyleProperties*>(this)->m_propertyVector.size();
    return m_arraySize;
}

inline bool StyleProperties::isEmpty() const
{
    return !propertyCount();
}

inline void StyleProperties::deref()
{
    if (!derefBase())
        return;

    if (m_isMutable)
        delete static_cast<MutableStyleProperties*>(this);
    else
        delete static_cast<ImmutableStyleProperties*>(this);
}

inline int StyleProperties::findPropertyIndex(CSSPropertyID propertyID) const
{
    if (m_isMutable)
        return toMutableStyleProperties(this)->findPropertyIndex(propertyID);
    return toImmutableStyleProperties(this)->findPropertyIndex(propertyID);
}

} // namespace WebCore

#endif // StyleProperties_h
