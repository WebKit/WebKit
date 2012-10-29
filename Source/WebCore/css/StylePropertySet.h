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
class ImmutableStylePropertySet;
class KURL;
class MutableStylePropertySet;
class PropertySetCSSStyleDeclaration;
class StyledElement;
class StylePropertyShorthand;
class StyleSheetContents;

class StylePropertySet : public RefCounted<StylePropertySet> {
    friend class PropertyReference;
public:
    ~StylePropertySet();

    // Override RefCounted's deref() to ensure operator delete is called on
    // the appropriate subclass type.
    void deref();

    static PassRefPtr<StylePropertySet> create(CSSParserMode = CSSQuirksMode);
    static PassRefPtr<StylePropertySet> create(const CSSProperty* properties, unsigned count);
    static PassRefPtr<StylePropertySet> createImmutable(const CSSProperty* properties, unsigned count, CSSParserMode);

    class PropertyReference {
    public:
        PropertyReference(const StylePropertySet& propertySet, unsigned index)
            : m_propertySet(propertySet)
            , m_index(index)
        { }

        CSSPropertyID id() const { return propertyInternal().id(); }
        bool isImportant() const { return propertyInternal().isImportant(); }
        bool isInherited() const { return propertyInternal().isInherited(); }

        String cssName() const { return propertyInternal().cssName(); }
        String cssText() const { return propertyInternal().cssText(); }

        const CSSValue* value() const { return propertyInternal().value(); }
        // FIXME: We should try to remove this mutable overload.
        CSSValue* value() { return const_cast<CSSValue*>(propertyInternal().value()); }

    private:
        const CSSProperty& propertyInternal() const { return m_propertySet.propertyAtInternal(m_index); }

        const StylePropertySet& m_propertySet;
        unsigned m_index;
    };

    unsigned propertyCount() const;
    bool isEmpty() const;
    PropertyReference propertyAt(unsigned index) const { return PropertyReference(*this, index); }

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

    CSSParserMode cssParserMode() const { return static_cast<CSSParserMode>(m_cssParserMode); }

    void addSubresourceStyleURLs(ListHashSet<KURL>&, StyleSheetContents* contextStyleSheet) const;

    PassRefPtr<StylePropertySet> copy() const;
    PassRefPtr<StylePropertySet> immutableCopyIfNeeded() const;

    void removeEquivalentProperties(const StylePropertySet*);
    void removeEquivalentProperties(const CSSStyleDeclaration*);

    PassRefPtr<StylePropertySet> copyPropertiesInSet(const CSSPropertyID* set, unsigned length) const;
    
    String asText() const;
    
    void clearParentElement(StyledElement*);

    CSSStyleDeclaration* ensureCSSStyleDeclaration();
    CSSStyleDeclaration* ensureInlineCSSStyleDeclaration(const StyledElement* parentElement);

    bool isMutable() const { return m_isMutable; }

    bool hasFailedOrCanceledSubresources() const;

    static unsigned averageSizeInBytes();
    void reportMemoryUsage(MemoryObjectInfo*) const;

#ifndef NDEBUG
    void showStyle();
#endif

    const CSSProperty* immutablePropertyArray() const;

protected:
    StylePropertySet(CSSParserMode cssParserMode)
        : m_cssParserMode(cssParserMode)
        , m_ownsCSSOMWrapper(false)
        , m_isMutable(true)
        , m_arraySize(0)
    { }

    StylePropertySet(CSSParserMode cssParserMode, unsigned immutableArraySize)
        : m_cssParserMode(cssParserMode)
        , m_ownsCSSOMWrapper(false)
        , m_isMutable(false)
        , m_arraySize(immutableArraySize)
    { }

    Vector<CSSProperty, 4>& mutablePropertyVector();
    const Vector<CSSProperty, 4>& mutablePropertyVector() const;

    unsigned m_cssParserMode : 2;
    mutable unsigned m_ownsCSSOMWrapper : 1;
    mutable unsigned m_isMutable : 1;
    unsigned m_arraySize : 28;
    
private:
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

    CSSProperty& propertyAtInternal(unsigned index);
    const CSSProperty& propertyAtInternal(unsigned index) const;

    friend class PropertySetCSSStyleDeclaration;
};

class ImmutableStylePropertySet : public StylePropertySet {
public:
    ImmutableStylePropertySet(const CSSProperty*, unsigned count, CSSParserMode);
    ~ImmutableStylePropertySet();

    void* m_propertyArray;
};

class MutableStylePropertySet : public StylePropertySet {
public:
    MutableStylePropertySet(CSSParserMode cssParserMode)
        : StylePropertySet(cssParserMode)
    { }
    MutableStylePropertySet(const CSSProperty* properties, unsigned count);
    MutableStylePropertySet(const StylePropertySet&);

    Vector<CSSProperty, 4> m_propertyVector;
};

inline CSSProperty& StylePropertySet::propertyAtInternal(unsigned index)
{
    if (m_isMutable)
        return mutablePropertyVector().at(index);
    return const_cast<CSSProperty*>(immutablePropertyArray())[index];
}

inline const CSSProperty& StylePropertySet::propertyAtInternal(unsigned index) const
{
    if (m_isMutable)
        return mutablePropertyVector().at(index);
    return immutablePropertyArray()[index];
}

inline Vector<CSSProperty, 4>& StylePropertySet::mutablePropertyVector()
{
    ASSERT(m_isMutable);
    return static_cast<MutableStylePropertySet*>(this)->m_propertyVector;
}

inline const Vector<CSSProperty, 4>& StylePropertySet::mutablePropertyVector() const
{
    ASSERT(m_isMutable);
    return static_cast<const MutableStylePropertySet*>(this)->m_propertyVector;
}

inline const CSSProperty* StylePropertySet::immutablePropertyArray() const
{
    ASSERT(!m_isMutable);
    return reinterpret_cast<const CSSProperty*>(&static_cast<const ImmutableStylePropertySet*>(this)->m_propertyArray);
}

inline unsigned StylePropertySet::propertyCount() const
{
    if (m_isMutable)
        return mutablePropertyVector().size();
    return m_arraySize;
}

inline bool StylePropertySet::isEmpty() const
{
    return !propertyCount();
}

inline void StylePropertySet::deref()
{
    if (!derefBase())
        return;

    if (m_isMutable)
        delete static_cast<MutableStylePropertySet*>(this);
    else
        delete static_cast<ImmutableStylePropertySet*>(this);
}

} // namespace WebCore

#endif // StylePropertySet_h
