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

#include "CSSParser.h"
#include "CSSParserMode.h"
#include "CSSPrimitiveValue.h"
#include "CSSProperty.h"
#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include <memory>
#include <wtf/ListHashSet.h>
#include <wtf/TypeCasts.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class CSSStyleDeclaration;
class CachedResource;
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
        PropertyReference(const StylePropertyMetadata& metadata, const CSSValue* value)
            : m_metadata(metadata)
            , m_value(value)
        { }

        CSSPropertyID id() const { return static_cast<CSSPropertyID>(m_metadata.m_propertyID); }
        CSSPropertyID shorthandID() const { return m_metadata.shorthandID(); }

        bool isImportant() const { return m_metadata.m_important; }
        bool isInherited() const { return m_metadata.m_inherited; }
        bool isImplicit() const { return m_metadata.m_implicit; }

        String cssName() const;
        String cssText() const;

        const CSSValue* value() const { return m_value; }
        // FIXME: We should try to remove this mutable overload.
        CSSValue* value() { return const_cast<CSSValue*>(m_value); }

        // FIXME: Remove this.
        CSSProperty toCSSProperty() const { return CSSProperty(id(), const_cast<CSSValue*>(m_value), isImportant(), m_metadata.m_isSetFromShorthand, m_metadata.m_indexInShorthandsVector, isImplicit()); }

    private:
        const StylePropertyMetadata& m_metadata;
        const CSSValue* m_value;
    };

    unsigned propertyCount() const;
    bool isEmpty() const { return !propertyCount(); }
    PropertyReference propertyAt(unsigned) const;

    WEBCORE_EXPORT PassRefPtr<CSSValue> getPropertyCSSValue(CSSPropertyID) const;
    WEBCORE_EXPORT String getPropertyValue(CSSPropertyID) const;
    bool propertyIsImportant(CSSPropertyID) const;
    String getPropertyShorthand(CSSPropertyID) const;
    bool isPropertyImplicit(CSSPropertyID) const;

    RefPtr<CSSValue> getCustomPropertyCSSValue(const String& propertyName) const;
    String getCustomPropertyValue(const String& propertyName) const;
    bool customPropertyIsImportant(const String& propertyName) const;

    Ref<MutableStyleProperties> copyBlockProperties() const;

    CSSParserMode cssParserMode() const { return static_cast<CSSParserMode>(m_cssParserMode); }

    void addSubresourceStyleURLs(ListHashSet<URL>&, StyleSheetContents* contextStyleSheet) const;

    WEBCORE_EXPORT Ref<MutableStyleProperties> mutableCopy() const;
    Ref<ImmutableStyleProperties> immutableCopyIfNeeded() const;

    Ref<MutableStyleProperties> copyPropertiesInSet(const CSSPropertyID* set, unsigned length) const;
    
    String asText() const;

    bool isMutable() const { return m_isMutable; }
    bool hasCSSOMWrapper() const;

    bool traverseSubresources(const std::function<bool (const CachedResource&)>& handler) const;

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
    int findCustomPropertyIndex(const String& propertyName) const;
    
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
    
    PassRefPtr<CSSValue> getPropertyCSSValueInternal(CSSPropertyID) const;
    
    friend class PropertySetCSSStyleDeclaration;
};

class ImmutableStyleProperties : public StyleProperties {
public:
    WEBCORE_EXPORT ~ImmutableStyleProperties();
    static Ref<ImmutableStyleProperties> create(const CSSProperty* properties, unsigned count, CSSParserMode);

    unsigned propertyCount() const { return m_arraySize; }
    bool isEmpty() const { return !propertyCount(); }
    PropertyReference propertyAt(unsigned index) const;

    const CSSValue** valueArray() const;
    const StylePropertyMetadata* metadataArray() const;
    int findPropertyIndex(CSSPropertyID) const;
    int findCustomPropertyIndex(const String& propertyName) const;
    
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
    WEBCORE_EXPORT static Ref<MutableStyleProperties> create(CSSParserMode = CSSQuirksMode);
    static Ref<MutableStyleProperties> create(const CSSProperty* properties, unsigned count);

    WEBCORE_EXPORT ~MutableStyleProperties();

    unsigned propertyCount() const { return m_propertyVector.size(); }
    bool isEmpty() const { return !propertyCount(); }
    PropertyReference propertyAt(unsigned index) const;

    PropertySetCSSStyleDeclaration* cssStyleDeclaration();

    bool addParsedProperties(const CSSParser::ParsedPropertyVector&);
    bool addParsedProperty(const CSSProperty&);

    // These expand shorthand properties into multiple properties.
    bool setProperty(CSSPropertyID, const String& value, bool important = false, StyleSheetContents* contextStyleSheet = 0);
    void setProperty(CSSPropertyID, PassRefPtr<CSSValue>, bool important = false);

    // These do not. FIXME: This is too messy, we can do better.
    bool setProperty(CSSPropertyID, CSSValueID identifier, bool important = false);
    bool setProperty(CSSPropertyID, CSSPropertyID identifier, bool important = false);
    bool appendPrefixingVariantProperty(const CSSProperty&);
    void setPrefixingVariantProperty(const CSSProperty&);
    bool setProperty(const CSSProperty&, CSSProperty* slot = nullptr);

    bool removeProperty(CSSPropertyID, String* returnText = nullptr);
    void removePrefixedOrUnprefixedProperty(CSSPropertyID);
    void removeBlockProperties();
    bool removePropertiesInSet(const CSSPropertyID* set, unsigned length);

    void mergeAndOverrideOnConflict(const StyleProperties&);

    void clear();
    void parseDeclaration(const String& styleDeclaration, StyleSheetContents* contextStyleSheet);

    WEBCORE_EXPORT CSSStyleDeclaration* ensureCSSStyleDeclaration();
    CSSStyleDeclaration* ensureInlineCSSStyleDeclaration(StyledElement* parentElement);

    int findPropertyIndex(CSSPropertyID) const;
    int findCustomPropertyIndex(const String& propertyName) const;
    
    Vector<CSSProperty, 4> m_propertyVector;

    // Methods for querying and altering CSS custom properties.
    bool setCustomProperty(const String& propertyName, const String& value, bool important = false, StyleSheetContents* contextStyleSheet = 0);
    bool removeCustomProperty(const String& propertyName, String* returnText = nullptr);

private:
    explicit MutableStyleProperties(CSSParserMode);
    explicit MutableStyleProperties(const StyleProperties&);
    MutableStyleProperties(const CSSProperty* properties, unsigned count);

    bool removeShorthandProperty(CSSPropertyID);
    CSSProperty* findCSSPropertyWithID(CSSPropertyID);
    CSSProperty* findCustomCSSPropertyWithName(const String&);
    std::unique_ptr<PropertySetCSSStyleDeclaration> m_cssomWrapper;

    friend class StyleProperties;
};

inline ImmutableStyleProperties::PropertyReference ImmutableStyleProperties::propertyAt(unsigned index) const
{
    return PropertyReference(metadataArray()[index], valueArray()[index]);
}

inline MutableStyleProperties::PropertyReference MutableStyleProperties::propertyAt(unsigned index) const
{
    const CSSProperty& property = m_propertyVector[index];
    return PropertyReference(property.metadata(), property.value());
}

inline StyleProperties::PropertyReference StyleProperties::propertyAt(unsigned index) const
{
    if (is<MutableStyleProperties>(*this))
        return downcast<MutableStyleProperties>(*this).propertyAt(index);
    return downcast<ImmutableStyleProperties>(*this).propertyAt(index);
}

inline unsigned StyleProperties::propertyCount() const
{
    if (is<MutableStyleProperties>(*this))
        return downcast<MutableStyleProperties>(*this).propertyCount();
    return downcast<ImmutableStyleProperties>(*this).propertyCount();
}

inline void StyleProperties::deref()
{
    if (!derefBase())
        return;

    if (is<MutableStyleProperties>(*this))
        delete downcast<MutableStyleProperties>(this);
    else
        delete downcast<ImmutableStyleProperties>(this);
}

inline int StyleProperties::findPropertyIndex(CSSPropertyID propertyID) const
{
    if (is<MutableStyleProperties>(*this))
        return downcast<MutableStyleProperties>(*this).findPropertyIndex(propertyID);
    return downcast<ImmutableStyleProperties>(*this).findPropertyIndex(propertyID);
}

inline int StyleProperties::findCustomPropertyIndex(const String& propertyName) const
{
    if (is<MutableStyleProperties>(*this))
        return downcast<MutableStyleProperties>(*this).findCustomPropertyIndex(propertyName);
    return downcast<ImmutableStyleProperties>(*this).findCustomPropertyIndex(propertyName);
}

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::MutableStyleProperties)
    static bool isType(const WebCore::StyleProperties& set) { return set.isMutable(); }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ImmutableStyleProperties)
    static bool isType(const WebCore::StyleProperties& set) { return !set.isMutable(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif // StyleProperties_h
