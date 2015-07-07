/*
 * Copyright (C) 2013, 2014 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ElementData_h
#define ElementData_h

#include "Attribute.h"
#include "SpaceSplitString.h"
#include <wtf/RefCounted.h>

namespace WebCore {

class Attr;
class ShareableElementData;
class StyleProperties;
class UniqueElementData;

class AttributeConstIterator {
public:
    AttributeConstIterator(const Attribute* array, unsigned offset)
        : m_array(array)
        , m_offset(offset)
    {
    }

    const Attribute& operator*() const { return m_array[m_offset]; }
    const Attribute* operator->() const { return &m_array[m_offset]; }
    AttributeConstIterator& operator++() { ++m_offset; return *this; }

    bool operator==(const AttributeConstIterator& other) const { return m_offset == other.m_offset; }
    bool operator!=(const AttributeConstIterator& other) const { return !(*this == other); }

private:
    const Attribute* m_array;
    unsigned m_offset;
};

class AttributeIteratorAccessor {
public:
    AttributeIteratorAccessor(const Attribute* array, unsigned size)
        : m_array(array)
        , m_size(size)
    {
    }

    AttributeConstIterator begin() const { return AttributeConstIterator(m_array, 0); }
    AttributeConstIterator end() const { return AttributeConstIterator(m_array, m_size); }

    unsigned attributeCount() const { return m_size; }

private:
    const Attribute* m_array;
    unsigned m_size;
};

class ElementData : public RefCounted<ElementData> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    // Override RefCounted's deref() to ensure operator delete is called on
    // the appropriate subclass type.
    void deref();

    static const unsigned attributeNotFound = static_cast<unsigned>(-1);

    void clearClass() const { m_classNames.clear(); }
    void setClass(const AtomicString& className, bool shouldFoldCase) const { m_classNames.set(className, shouldFoldCase); }
    const SpaceSplitString& classNames() const { return m_classNames; }
    static ptrdiff_t classNamesMemoryOffset() { return OBJECT_OFFSETOF(ElementData, m_classNames); }

    const AtomicString& idForStyleResolution() const { return m_idForStyleResolution; }
    static ptrdiff_t idForStyleResolutionMemoryOffset() { return OBJECT_OFFSETOF(ElementData, m_idForStyleResolution); }
    void setIdForStyleResolution(const AtomicString& newId) const { m_idForStyleResolution = newId; }

    const StyleProperties* inlineStyle() const { return m_inlineStyle.get(); }
    const StyleProperties* presentationAttributeStyle() const;

    unsigned length() const;
    bool isEmpty() const { return !length(); }

    AttributeIteratorAccessor attributesIterator() const;
    const Attribute& attributeAt(unsigned index) const;
    const Attribute* findAttributeByName(const QualifiedName&) const;
    unsigned findAttributeIndexByName(const QualifiedName&) const;
    unsigned findAttributeIndexByName(const AtomicString& name, bool shouldIgnoreAttributeCase) const;
    unsigned findAttributeIndexByNameForAttributeNode(const Attr*, bool shouldIgnoreAttributeCase = false) const;
    const Attribute* findLanguageAttribute() const;

    bool hasID() const { return !m_idForStyleResolution.isNull(); }
    bool hasClass() const { return !m_classNames.isEmpty(); }
    bool hasName() const { return m_arraySizeAndFlags & s_flagHasNameAttribute; }

    bool isEquivalent(const ElementData* other) const;

    bool isUnique() const { return m_arraySizeAndFlags & s_flagIsUnique; }
    static uint32_t isUniqueFlag() { return s_flagIsUnique; }

    static ptrdiff_t arraySizeAndFlagsMemoryOffset() { return OBJECT_OFFSETOF(ElementData, m_arraySizeAndFlags); }
    static inline uint32_t styleAttributeIsDirtyFlag() { return s_flagStyleAttributeIsDirty; }
    static uint32_t animatedSVGAttributesAreDirtyFlag() { return s_flagAnimatedSVGAttributesAreDirty; }

    static uint32_t arraySizeOffset() { return s_flagCount; }

private:
    mutable uint32_t m_arraySizeAndFlags;

    static const uint32_t s_arraySize = 27;
    static const uint32_t s_flagCount = 5;
    static const uint32_t s_flagIsUnique = 1;
    static const uint32_t s_flagHasNameAttribute = 1 << 1;
    static const uint32_t s_flagPresentationAttributeStyleIsDirty = 1 << 2;
    static const uint32_t s_flagStyleAttributeIsDirty = 1 << 3;
    static const uint32_t s_flagAnimatedSVGAttributesAreDirty = 1 << 4;
    static const uint32_t s_flagsMask = (1 << s_flagCount) - 1;

    inline void updateFlag(uint32_t flag, bool set) const
    {
        if (set)
            m_arraySizeAndFlags |= flag;
        else
            m_arraySizeAndFlags &= ~flag;
    }
    static inline uint32_t arraySizeAndFlagsFromOther(const ElementData& other, bool isUnique);

protected:
    ElementData();
    explicit ElementData(unsigned arraySize);
    ElementData(const ElementData&, bool isUnique);

    unsigned arraySize() const { return m_arraySizeAndFlags >> s_flagCount; }

    void setHasNameAttribute(bool hasName) const { updateFlag(s_flagHasNameAttribute, hasName); }

    bool styleAttributeIsDirty() const { return m_arraySizeAndFlags & s_flagStyleAttributeIsDirty; }
    void setStyleAttributeIsDirty(bool isDirty) const { updateFlag(s_flagStyleAttributeIsDirty, isDirty); }

    bool presentationAttributeStyleIsDirty() const { return m_arraySizeAndFlags & s_flagPresentationAttributeStyleIsDirty; }
    void setPresentationAttributeStyleIsDirty(bool isDirty) const { updateFlag(s_flagPresentationAttributeStyleIsDirty, isDirty); }

    bool animatedSVGAttributesAreDirty() const { return m_arraySizeAndFlags & s_flagAnimatedSVGAttributesAreDirty; }
    void setAnimatedSVGAttributesAreDirty(bool dirty) const { updateFlag(s_flagAnimatedSVGAttributesAreDirty, dirty); }

    mutable RefPtr<StyleProperties> m_inlineStyle;
    mutable SpaceSplitString m_classNames;
    mutable AtomicString m_idForStyleResolution;

private:
    friend class Element;
    friend class StyledElement;
    friend class ShareableElementData;
    friend class UniqueElementData;
    friend class SVGElement;

    void destroy();

    const Attribute* attributeBase() const;
    const Attribute* findAttributeByName(const AtomicString& name, bool shouldIgnoreAttributeCase) const;
    unsigned findAttributeIndexByNameSlowCase(const AtomicString& name, bool shouldIgnoreAttributeCase) const;

    PassRef<UniqueElementData> makeUniqueCopy() const;
};

#if COMPILER(MSVC)
#pragma warning(push)
#pragma warning(disable: 4200) // Disable "zero-sized array in struct/union" warning
#endif

class ShareableElementData : public ElementData {
public:
    static PassRef<ShareableElementData> createWithAttributes(const Vector<Attribute>&);

    explicit ShareableElementData(const Vector<Attribute>&);
    explicit ShareableElementData(const UniqueElementData&);
    ~ShareableElementData();

    static ptrdiff_t attributeArrayMemoryOffset() { return OBJECT_OFFSETOF(ShareableElementData, m_attributeArray); }

    Attribute m_attributeArray[0];
};

#if COMPILER(MSVC)
#pragma warning(pop)
#endif

class UniqueElementData : public ElementData {
public:
    static PassRef<UniqueElementData> create();
    PassRef<ShareableElementData> makeShareableCopy() const;

    // These functions do no error/duplicate checking.
    void addAttribute(const QualifiedName&, const AtomicString&);
    void removeAttribute(unsigned index);

    Attribute& attributeAt(unsigned index);
    Attribute* findAttributeByName(const QualifiedName&);

    UniqueElementData();
    explicit UniqueElementData(const ShareableElementData&);
    explicit UniqueElementData(const UniqueElementData&);

    static ptrdiff_t attributeVectorMemoryOffset() { return OBJECT_OFFSETOF(UniqueElementData, m_attributeVector); }

    mutable RefPtr<StyleProperties> m_presentationAttributeStyle;
    typedef Vector<Attribute, 4> AttributeVector;
    AttributeVector m_attributeVector;
};

inline void ElementData::deref()
{
    if (!derefBase())
        return;
    destroy();
}

inline unsigned ElementData::length() const
{
    if (isUnique())
        return static_cast<const UniqueElementData*>(this)->m_attributeVector.size();
    return arraySize();
}

inline const Attribute* ElementData::attributeBase() const
{
    if (isUnique())
        return static_cast<const UniqueElementData*>(this)->m_attributeVector.data();
    return static_cast<const ShareableElementData*>(this)->m_attributeArray;
}

inline const StyleProperties* ElementData::presentationAttributeStyle() const
{
    if (!isUnique())
        return 0;
    return static_cast<const UniqueElementData*>(this)->m_presentationAttributeStyle.get();
}

inline AttributeIteratorAccessor ElementData::attributesIterator() const
{
    if (isUnique()) {
        const Vector<Attribute, 4>& attributeVector = static_cast<const UniqueElementData*>(this)->m_attributeVector;
        return AttributeIteratorAccessor(attributeVector.data(), attributeVector.size());
    }
    return AttributeIteratorAccessor(static_cast<const ShareableElementData*>(this)->m_attributeArray, arraySize());
}

ALWAYS_INLINE const Attribute* ElementData::findAttributeByName(const AtomicString& name, bool shouldIgnoreAttributeCase) const
{
    unsigned index = findAttributeIndexByName(name, shouldIgnoreAttributeCase);
    if (index != attributeNotFound)
        return &attributeAt(index);
    return 0;
}

ALWAYS_INLINE unsigned ElementData::findAttributeIndexByName(const QualifiedName& name) const
{
    const Attribute* attributes = attributeBase();
    for (unsigned i = 0, count = length(); i < count; ++i) {
        if (attributes[i].name().matches(name))
            return i;
    }
    return attributeNotFound;
}

// We use a boolean parameter instead of calling shouldIgnoreAttributeCase so that the caller
// can tune the behavior (hasAttribute is case sensitive whereas getAttribute is not).
ALWAYS_INLINE unsigned ElementData::findAttributeIndexByName(const AtomicString& name, bool shouldIgnoreAttributeCase) const
{
    const Attribute* attributes = attributeBase();
    bool doSlowCheck = shouldIgnoreAttributeCase;
    const AtomicString& caseAdjustedName = shouldIgnoreAttributeCase ? name.lower() : name;

    // Optimize for the case where the attribute exists and its name exactly matches.
    for (unsigned i = 0, count = length(); i < count; ++i) {
        if (!attributes[i].name().hasPrefix()) {
            if (caseAdjustedName == attributes[i].localName())
                return i;
        } else
            doSlowCheck = true;
    }

    if (doSlowCheck)
        return findAttributeIndexByNameSlowCase(name, shouldIgnoreAttributeCase);
    return attributeNotFound;
}

ALWAYS_INLINE const Attribute* ElementData::findAttributeByName(const QualifiedName& name) const
{
    const Attribute* attributes = attributeBase();
    for (unsigned i = 0, count = length(); i < count; ++i) {
        if (attributes[i].name().matches(name))
            return &attributes[i];
    }
    return 0;
}

inline const Attribute& ElementData::attributeAt(unsigned index) const
{
    RELEASE_ASSERT(index < length());
    return attributeBase()[index];
}

inline void UniqueElementData::addAttribute(const QualifiedName& attributeName, const AtomicString& value)
{
    m_attributeVector.append(Attribute(attributeName, value));
}

inline void UniqueElementData::removeAttribute(unsigned index)
{
    m_attributeVector.remove(index);
}

inline Attribute& UniqueElementData::attributeAt(unsigned index)
{
    return m_attributeVector.at(index);
}

}

#endif

