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

class CSSDeferredParser;
class CSSStyleDeclaration;
class CachedResource;
class Color;
class ImmutableStyleProperties;
class MutableStyleProperties;
class PropertySetCSSStyleDeclaration;
class StyledElement;
class StylePropertyShorthand;
class StyleSheetContents;

enum StylePropertiesType { ImmutablePropertiesType, MutablePropertiesType, DeferredPropertiesType };
    
class StylePropertiesBase : public RefCounted<StylePropertiesBase> {
public:
    // Override RefCounted's deref() to ensure operator delete is called on
    // the appropriate subclass type.
    void deref();
    
    StylePropertiesType type() const { return static_cast<StylePropertiesType>(m_type); }

    CSSParserMode cssParserMode() const { return static_cast<CSSParserMode>(m_cssParserMode); }

protected:
    StylePropertiesBase(CSSParserMode cssParserMode, StylePropertiesType type)
        : m_cssParserMode(cssParserMode)
        , m_type(type)
        , m_arraySize(0)
    { }
    
    StylePropertiesBase(CSSParserMode cssParserMode, unsigned immutableArraySize)
        : m_cssParserMode(cssParserMode)
        , m_type(ImmutablePropertiesType)
        , m_arraySize(immutableArraySize)
    { }
    
    unsigned m_cssParserMode : 3;
    mutable unsigned m_type : 2;
    unsigned m_arraySize : 27;
};

class StyleProperties : public StylePropertiesBase {
    friend class PropertyReference;
public:
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

    WEBCORE_EXPORT RefPtr<CSSValue> getPropertyCSSValue(CSSPropertyID) const;
    WEBCORE_EXPORT String getPropertyValue(CSSPropertyID) const;

    WEBCORE_EXPORT Optional<Color> propertyAsColor(CSSPropertyID) const;
    WEBCORE_EXPORT CSSValueID propertyAsValueID(CSSPropertyID) const;

    bool propertyIsImportant(CSSPropertyID) const;
    String getPropertyShorthand(CSSPropertyID) const;
    bool isPropertyImplicit(CSSPropertyID) const;

    RefPtr<CSSValue> getCustomPropertyCSSValue(const String& propertyName) const;
    String getCustomPropertyValue(const String& propertyName) const;
    bool customPropertyIsImportant(const String& propertyName) const;

    Ref<MutableStyleProperties> copyBlockProperties() const;

    WEBCORE_EXPORT Ref<MutableStyleProperties> mutableCopy() const;
    Ref<ImmutableStyleProperties> immutableCopyIfNeeded() const;

    Ref<MutableStyleProperties> copyPropertiesInSet(const CSSPropertyID* set, unsigned length) const;
    
    String asText() const;

    bool hasCSSOMWrapper() const;
    bool isMutable() const { return type() == MutablePropertiesType; }

    bool traverseSubresources(const WTF::Function<bool (const CachedResource&)>& handler) const;

    static unsigned averageSizeInBytes();

#ifndef NDEBUG
    void showStyle();
#endif

    bool propertyMatches(CSSPropertyID, const CSSValue*) const;

protected:
    StyleProperties(CSSParserMode cssParserMode, StylePropertiesType type)
        : StylePropertiesBase(cssParserMode, type)
    { }

    StyleProperties(CSSParserMode cssParserMode, unsigned immutableArraySize)
        : StylePropertiesBase(cssParserMode, immutableArraySize)
    { }

    int findPropertyIndex(CSSPropertyID) const;
    int findCustomPropertyIndex(const String& propertyName) const;

private:
    String getShorthandValue(const StylePropertyShorthand&) const;
    String getCommonValue(const StylePropertyShorthand&) const;
    String getAlignmentShorthandValue(const StylePropertyShorthand&) const;
    String borderPropertyValue() const;
    String getLayeredShorthandValue(const StylePropertyShorthand&) const;
    String get4Values(const StylePropertyShorthand&) const;
    String borderSpacingValue(const StylePropertyShorthand&) const;
    String fontValue() const;
    void appendFontLonghandValueIfExplicit(CSSPropertyID, StringBuilder& result, String& value) const;
    
    RefPtr<CSSValue> getPropertyCSSValueInternal(CSSPropertyID) const;
    
    friend class PropertySetCSSStyleDeclaration;
};

class ImmutableStyleProperties final : public StyleProperties {
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

class MutableStyleProperties final : public StyleProperties {
public:
    WEBCORE_EXPORT static Ref<MutableStyleProperties> create(CSSParserMode = HTMLQuirksMode);
    static Ref<MutableStyleProperties> create(const CSSProperty* properties, unsigned count);

    WEBCORE_EXPORT ~MutableStyleProperties();

    unsigned propertyCount() const { return m_propertyVector.size(); }
    bool isEmpty() const { return !propertyCount(); }
    PropertyReference propertyAt(unsigned index) const;

    PropertySetCSSStyleDeclaration* cssStyleDeclaration();

    bool addParsedProperties(const ParsedPropertyVector&);
    bool addParsedProperty(const CSSProperty&);

    // These expand shorthand properties into multiple properties.
    bool setProperty(CSSPropertyID, const String& value, bool important, CSSParserContext);
    bool setProperty(CSSPropertyID, const String& value, bool important = false);
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
    MutableStyleProperties(const CSSProperty* properties, unsigned count);

    bool removeShorthandProperty(CSSPropertyID);
    CSSProperty* findCSSPropertyWithID(CSSPropertyID);
    CSSProperty* findCustomCSSPropertyWithName(const String&);
    std::unique_ptr<PropertySetCSSStyleDeclaration> m_cssomWrapper;

    friend class StyleProperties;
};

class DeferredStyleProperties final : public StylePropertiesBase {
public:
    WEBCORE_EXPORT ~DeferredStyleProperties();
    static Ref<DeferredStyleProperties> create(const CSSParserTokenRange&, CSSDeferredParser&);

    Ref<ImmutableStyleProperties> parseDeferredProperties();
    
private:
    DeferredStyleProperties(const CSSParserTokenRange&, CSSDeferredParser&);
    
    Vector<CSSParserToken> m_tokens;
    Ref<CSSDeferredParser> m_parser;
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

inline void StylePropertiesBase::deref()
{
    if (!derefBase())
        return;

    if (is<MutableStyleProperties>(*this))
        delete downcast<MutableStyleProperties>(this);
    else if (is<ImmutableStyleProperties>(*this))
        delete downcast<ImmutableStyleProperties>(this);
    else
        delete downcast<DeferredStyleProperties>(this);
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

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::StyleProperties)
    static bool isType(const WebCore::StylePropertiesBase& set) { return set.type() != WebCore::DeferredPropertiesType; }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::MutableStyleProperties)
    static bool isType(const WebCore::StylePropertiesBase& set) { return set.type() == WebCore::MutablePropertiesType; }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ImmutableStyleProperties)
    static bool isType(const WebCore::StylePropertiesBase& set) { return set.type() == WebCore::ImmutablePropertiesType; }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::DeferredStyleProperties)
    static bool isType(const WebCore::StylePropertiesBase& set) { return set.type() == WebCore::DeferredPropertiesType; }
SPECIALIZE_TYPE_TRAITS_END()
