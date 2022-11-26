/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004-2022 Apple Inc. All rights reserved.
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

#pragma once

#include "CSSParserContext.h"
#include "CSSParserTokenRange.h"
#include "CSSProperty.h"
#include "CSSValueKeywords.h"
#include <memory>
#include <wtf/Function.h>
#include <wtf/TypeCasts.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class CSSStyleDeclaration;
class CachedResource;
class Color;
class ImmutableStyleProperties;
class MutableStyleProperties;
class PropertySetCSSStyleDeclaration;
class StyledElement;
class StylePropertyShorthand;
class StyleSheetContents;

enum StylePropertiesType { ImmutablePropertiesType, MutablePropertiesType };

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(StyleProperties);
class StyleProperties : public RefCounted<StyleProperties> {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(StyleProperties);
    friend class PropertyReference;
public:
    // Override RefCounted's deref() to ensure operator delete is called on
    // the appropriate subclass type.
    void deref() const;

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

    StylePropertiesType type() const { return static_cast<StylePropertiesType>(m_type); }

    unsigned propertyCount() const;
    bool isEmpty() const { return !propertyCount(); }
    PropertyReference propertyAt(unsigned) const;

    WEBCORE_EXPORT RefPtr<CSSValue> getPropertyCSSValue(CSSPropertyID) const;
    WEBCORE_EXPORT String getPropertyValue(CSSPropertyID) const;

    WEBCORE_EXPORT std::optional<Color> propertyAsColor(CSSPropertyID) const;
    WEBCORE_EXPORT std::optional<CSSValueID> propertyAsValueID(CSSPropertyID) const;

    bool propertyIsImportant(CSSPropertyID) const;
    String getPropertyShorthand(CSSPropertyID) const;
    bool isPropertyImplicit(CSSPropertyID) const;

    RefPtr<CSSValue> getCustomPropertyCSSValue(const String& propertyName) const;
    String getCustomPropertyValue(const String& propertyName) const;
    bool customPropertyIsImportant(const String& propertyName) const;

    Ref<MutableStyleProperties> copyBlockProperties() const;

    CSSParserMode cssParserMode() const { return static_cast<CSSParserMode>(m_cssParserMode); }

    WEBCORE_EXPORT Ref<MutableStyleProperties> mutableCopy() const;
    Ref<ImmutableStyleProperties> immutableCopyIfNeeded() const;

    Ref<MutableStyleProperties> copyPropertiesInSet(const CSSPropertyID* set, unsigned length) const;
    
    String asText() const;
    AtomString asTextAtom() const;

    bool hasCSSOMWrapper() const;
    bool isMutable() const { return type() == MutablePropertiesType; }

    bool traverseSubresources(const Function<bool(const CachedResource&)>& handler) const;

    static unsigned averageSizeInBytes();

#ifndef NDEBUG
    void showStyle();
#endif

    bool propertyMatches(CSSPropertyID, const CSSValue*) const;

protected:
    StyleProperties(CSSParserMode cssParserMode, StylePropertiesType type)
        : m_cssParserMode(cssParserMode)
        , m_type(type)
        , m_arraySize(0)
    { }

    StyleProperties(CSSParserMode cssParserMode, unsigned immutableArraySize)
        : m_cssParserMode(cssParserMode)
        , m_type(ImmutablePropertiesType)
        , m_arraySize(immutableArraySize)
    { }

    int findPropertyIndex(CSSPropertyID) const;
    int findCustomPropertyIndex(const String& propertyName) const;

    unsigned m_cssParserMode : 3;
    mutable unsigned m_type : 2;
    unsigned m_arraySize : 27;

private:
    String getGridShorthandValue(const StylePropertyShorthand&) const;
    String getGridRowColumnShorthandValue(const StylePropertyShorthand&) const;
    String getGridAreaShorthandValue() const;
    String getGridTemplateValue() const;
    String getGridValue() const;
    String getShorthandValue(const StylePropertyShorthand&, const char* separator = " ") const;
    String getCommonValue(const StylePropertyShorthand&) const;
    String getAlignmentShorthandValue(const StylePropertyShorthand&) const;
    String borderImagePropertyValue(const StylePropertyShorthand&) const;
    String borderPropertyValue(const StylePropertyShorthand&, const StylePropertyShorthand&, const StylePropertyShorthand&) const;
    String pageBreakPropertyValue(const StylePropertyShorthand&) const;
    String getLayeredShorthandValue(const StylePropertyShorthand&) const;
    String get2Values(const StylePropertyShorthand&) const;
    String get4Values(const StylePropertyShorthand&) const;
    String borderSpacingValue(const StylePropertyShorthand&) const;
    String fontValue(const StylePropertyShorthand&) const;
    String fontVariantValue(const StylePropertyShorthand&) const;
    String fontSynthesisValue() const;
    String textDecorationSkipValue() const;
    String offsetValue() const;
    String commonShorthandChecks(const StylePropertyShorthand&) const;
    StringBuilder asTextInternal() const;

    friend class PropertySetCSSStyleDeclaration;
};

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(ImmutableStyleProperties);
class ImmutableStyleProperties final : public StyleProperties {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(ImmutableStyleProperties);
public:
    WEBCORE_EXPORT ~ImmutableStyleProperties();
    static Ref<ImmutableStyleProperties> create(const CSSProperty* properties, unsigned count, CSSParserMode);

    unsigned propertyCount() const { return m_arraySize; }
    bool isEmpty() const { return !propertyCount(); }
    PropertyReference propertyAt(unsigned index) const;

    int findPropertyIndex(CSSPropertyID) const;
    int findCustomPropertyIndex(const String& propertyName) const;
    
    void* m_storage;

private:
    PackedPtr<const CSSValue>* valueArray() const;
    const StylePropertyMetadata* metadataArray() const;
    ImmutableStyleProperties(const CSSProperty*, unsigned count, CSSParserMode);
};

inline PackedPtr<const CSSValue>* ImmutableStyleProperties::valueArray() const
{
    return bitwise_cast<PackedPtr<const CSSValue>*>(bitwise_cast<const uint8_t*>(metadataArray()) + (m_arraySize * sizeof(StylePropertyMetadata)));
}

inline const StylePropertyMetadata* ImmutableStyleProperties::metadataArray() const
{
    return reinterpret_cast<const StylePropertyMetadata*>(const_cast<const void**>((&(this->m_storage))));
}

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(MutableStyleProperties);
class MutableStyleProperties final : public StyleProperties {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(MutableStyleProperties);
public:
    WEBCORE_EXPORT static Ref<MutableStyleProperties> create(CSSParserMode = HTMLQuirksMode);
    static Ref<MutableStyleProperties> create(Vector<CSSProperty>&&);

    WEBCORE_EXPORT ~MutableStyleProperties();

    unsigned propertyCount() const { return m_propertyVector.size(); }
    bool isEmpty() const { return !propertyCount(); }
    PropertyReference propertyAt(unsigned index) const;

    PropertySetCSSStyleDeclaration* cssStyleDeclaration();

    bool addParsedProperties(const ParsedPropertyVector&);
    bool addParsedProperty(const CSSProperty&);

    // These expand shorthand properties into multiple properties.
    bool setProperty(CSSPropertyID, const String& value, bool important, CSSParserContext, bool* didFailParsing = nullptr);
    bool setProperty(CSSPropertyID, const String& value, bool important = false, bool* didFailParsing = nullptr);
    void setProperty(CSSPropertyID, RefPtr<CSSValue>&&, bool important = false);

    // These do not. FIXME: This is too messy, we can do better.
    bool setProperty(CSSPropertyID, CSSValueID identifier, bool important = false);
    bool setProperty(CSSPropertyID, CSSPropertyID identifier, bool important = false);
    bool setProperty(const CSSProperty&, CSSProperty* slot = nullptr);

    bool removeProperty(CSSPropertyID, String* returnText = nullptr);
    void removeBlockProperties();
    bool removePropertiesInSet(const CSSPropertyID* set, unsigned length);

    void mergeAndOverrideOnConflict(const StyleProperties&);

    void clear();
    bool parseDeclaration(const String& styleDeclaration, CSSParserContext);

    WEBCORE_EXPORT CSSStyleDeclaration& ensureCSSStyleDeclaration();
    CSSStyleDeclaration& ensureInlineCSSStyleDeclaration(StyledElement& parentElement);

    int findPropertyIndex(CSSPropertyID) const;
    int findCustomPropertyIndex(const String& propertyName) const;
    
    Vector<CSSProperty, 4> m_propertyVector;

    // Methods for querying and altering CSS custom properties.
    bool setCustomProperty(const Document*, const String& propertyName, const String& value, bool important, CSSParserContext);
    bool removeCustomProperty(const String& propertyName, String* returnText = nullptr);

private:
    explicit MutableStyleProperties(CSSParserMode);
    explicit MutableStyleProperties(const StyleProperties&);
    MutableStyleProperties(Vector<CSSProperty>&&);

    bool removeShorthandProperty(CSSPropertyID);
    CSSProperty* findCSSPropertyWithID(CSSPropertyID);
    CSSProperty* findCustomCSSPropertyWithName(const String&);
    std::unique_ptr<PropertySetCSSStyleDeclaration> m_cssomWrapper;
    bool canUpdateInPlace(const CSSProperty&, CSSProperty* toReplace) const;

    friend class StyleProperties;
};

inline ImmutableStyleProperties::PropertyReference ImmutableStyleProperties::propertyAt(unsigned index) const
{
    return PropertyReference(metadataArray()[index], valueArray()[index].get());
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

inline void StyleProperties::deref() const
{
    if (!derefBase())
        return;

    if (is<MutableStyleProperties>(*this))
        delete downcast<MutableStyleProperties>(this);
    else if (is<ImmutableStyleProperties>(*this))
        delete downcast<ImmutableStyleProperties>(this);
    else
        RELEASE_ASSERT_NOT_REACHED();
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
    static bool isType(const WebCore::StyleProperties& set) { return set.type() == WebCore::MutablePropertiesType; }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ImmutableStyleProperties)
    static bool isType(const WebCore::StyleProperties& set) { return set.type() == WebCore::ImmutablePropertiesType; }
SPECIALIZE_TYPE_TRAITS_END()

