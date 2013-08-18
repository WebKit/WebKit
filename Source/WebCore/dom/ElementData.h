/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
class StylePropertySet;
class UniqueElementData;

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

    const AtomicString& idForStyleResolution() const { return m_idForStyleResolution; }
    void setIdForStyleResolution(const AtomicString& newId) const { m_idForStyleResolution = newId; }

    const StylePropertySet* inlineStyle() const { return m_inlineStyle.get(); }
    const StylePropertySet* presentationAttributeStyle() const;

    unsigned length() const;
    bool isEmpty() const { return !length(); }

    const Attribute& attributeAt(unsigned index) const;
    const Attribute* findAttributeByName(const QualifiedName&) const;
    unsigned findAttributeIndexByName(const QualifiedName&) const;
    unsigned findAttributeIndexByName(const AtomicString& name, bool shouldIgnoreAttributeCase) const;
    unsigned findAttributeIndexByNameForAttributeNode(const Attr*, bool shouldIgnoreAttributeCase = false) const;

    bool hasID() const { return !m_idForStyleResolution.isNull(); }
    bool hasClass() const { return !m_classNames.isNull(); }
    bool hasName() const { return m_hasNameAttribute; }

    bool isEquivalent(const ElementData* other) const;

    bool isUnique() const { return m_isUnique; }

protected:
    ElementData();
    ElementData(unsigned arraySize);
    ElementData(const ElementData&, bool isUnique);

    unsigned m_isUnique : 1;
    unsigned m_arraySize : 27;
    mutable unsigned m_hasNameAttribute : 1;
    mutable unsigned m_presentationAttributeStyleIsDirty : 1;
    mutable unsigned m_styleAttributeIsDirty : 1;
#if ENABLE(SVG)
    mutable unsigned m_animatedSVGAttributesAreDirty : 1;
#endif

    mutable RefPtr<StylePropertySet> m_inlineStyle;
    mutable SpaceSplitString m_classNames;
    mutable AtomicString m_idForStyleResolution;

private:
    friend class Element;
    friend class StyledElement;
    friend class ShareableElementData;
    friend class UniqueElementData;
#if ENABLE(SVG)
    friend class SVGElement;
#endif

    const Attribute* attributeBase() const;
    const Attribute* findAttributeByName(const AtomicString& name, bool shouldIgnoreAttributeCase) const;
    unsigned findAttributeIndexByNameSlowCase(const AtomicString& name, bool shouldIgnoreAttributeCase) const;

    PassRefPtr<UniqueElementData> makeUniqueCopy() const;
};

#if COMPILER(MSVC)
#pragma warning(push)
#pragma warning(disable: 4200) // Disable "zero-sized array in struct/union" warning
#endif

class ShareableElementData : public ElementData {
public:
    static PassRefPtr<ShareableElementData> createWithAttributes(const Vector<Attribute>&);

    explicit ShareableElementData(const Vector<Attribute>&);
    explicit ShareableElementData(const UniqueElementData&);
    ~ShareableElementData();

    Attribute m_attributeArray[0];
};

#if COMPILER(MSVC)
#pragma warning(pop)
#endif

class UniqueElementData : public ElementData {
public:
    static PassRefPtr<UniqueElementData> create();
    PassRefPtr<ShareableElementData> makeShareableCopy() const;

    // These functions do no error/duplicate checking.
    void addAttribute(const QualifiedName&, const AtomicString&);
    void removeAttribute(unsigned index);

    Attribute& attributeAt(unsigned index);
    Attribute* findAttributeByName(const QualifiedName&);

    UniqueElementData();
    explicit UniqueElementData(const ShareableElementData&);
    explicit UniqueElementData(const UniqueElementData&);

    mutable RefPtr<StylePropertySet> m_presentationAttributeStyle;
    Vector<Attribute, 4> m_attributeVector;
};

inline unsigned ElementData::length() const
{
    if (isUnique())
        return static_cast<const UniqueElementData*>(this)->m_attributeVector.size();
    return m_arraySize;
}

inline const Attribute* ElementData::attributeBase() const
{
    if (m_isUnique)
        return static_cast<const UniqueElementData*>(this)->m_attributeVector.data();
    return static_cast<const ShareableElementData*>(this)->m_attributeArray;
}

inline const StylePropertySet* ElementData::presentationAttributeStyle() const
{
    if (!m_isUnique)
        return 0;
    return static_cast<const UniqueElementData*>(this)->m_presentationAttributeStyle.get();
}

inline const Attribute* ElementData::findAttributeByName(const AtomicString& name, bool shouldIgnoreAttributeCase) const
{
    unsigned index = findAttributeIndexByName(name, shouldIgnoreAttributeCase);
    if (index != attributeNotFound)
        return &attributeAt(index);
    return 0;
}

inline unsigned ElementData::findAttributeIndexByName(const QualifiedName& name) const
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
inline unsigned ElementData::findAttributeIndexByName(const AtomicString& name, bool shouldIgnoreAttributeCase) const
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

inline const Attribute* ElementData::findAttributeByName(const QualifiedName& name) const
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

}

#endif

