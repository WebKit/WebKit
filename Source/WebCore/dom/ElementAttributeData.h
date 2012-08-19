/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Peter Kelly (pmk@post.com)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2008, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
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

#ifndef ElementAttributeData_h
#define ElementAttributeData_h

#include "Attribute.h"
#include "SpaceSplitString.h"
#include "StylePropertySet.h"
#include <wtf/NotFound.h>

namespace WebCore {

class Attr;
class Element;
class MemoryObjectInfo;

enum SynchronizationOfLazyAttribute { NotInSynchronizationOfLazyAttribute, InSynchronizationOfLazyAttribute };

class ElementAttributeData {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static PassOwnPtr<ElementAttributeData> create() { return adoptPtr(new ElementAttributeData); }
    static PassOwnPtr<ElementAttributeData> createImmutable(const Vector<Attribute>&);
    ~ElementAttributeData();

    void clearClass() { m_classNames.clear(); }
    void setClass(const AtomicString& className, bool shouldFoldCase) const { m_classNames.set(className, shouldFoldCase); }
    const SpaceSplitString& classNames() const { return m_classNames; }

    const AtomicString& idForStyleResolution() const { return m_idForStyleResolution; }
    void setIdForStyleResolution(const AtomicString& newId) const { m_idForStyleResolution = newId; }

    const StylePropertySet* inlineStyle() const { return m_inlineStyleDecl.get(); }
    StylePropertySet* inlineStyle() { return m_inlineStyleDecl.get(); }
    StylePropertySet* ensureInlineStyle(StyledElement*);
    StylePropertySet* ensureMutableInlineStyle(StyledElement*);
    void updateInlineStyleAvoidingMutation(StyledElement*, const String& text) const;
    void destroyInlineStyle(StyledElement*) const;

    const StylePropertySet* attributeStyle() const { return m_attributeStyle.get(); }
    void setAttributeStyle(PassRefPtr<StylePropertySet> style) const { m_attributeStyle = style; }

    size_t length() const;
    bool isEmpty() const { return !length(); }

    PassRefPtr<Attr> getAttributeNode(const String&, bool shouldIgnoreAttributeCase, Element*) const;
    PassRefPtr<Attr> getAttributeNode(const QualifiedName&, Element*) const;

    // Internal interface.
    const Attribute* attributeItem(unsigned index) const;
    const Attribute* getAttributeItem(const QualifiedName&) const;
    Attribute* attributeItem(unsigned index);
    Attribute* getAttributeItem(const QualifiedName&);
    size_t getAttributeItemIndex(const QualifiedName&) const;
    size_t getAttributeItemIndex(const AtomicString& name, bool shouldIgnoreAttributeCase) const;

    // These functions do no error checking.
    void addAttribute(const Attribute&, Element*, SynchronizationOfLazyAttribute = NotInSynchronizationOfLazyAttribute);
    void removeAttribute(size_t index, Element*, SynchronizationOfLazyAttribute = NotInSynchronizationOfLazyAttribute);
    PassRefPtr<Attr> takeAttribute(size_t index, Element*);

    bool hasID() const { return !m_idForStyleResolution.isNull(); }
    bool hasClass() const { return !m_classNames.isNull(); }

    bool isEquivalent(const ElementAttributeData* other) const;

    void setAttr(Element*, const QualifiedName&, Attr*) const;
    void removeAttr(Element*, const QualifiedName&) const;
    PassRefPtr<Attr> attrIfExists(Element*, const QualifiedName&) const;
    PassRefPtr<Attr> ensureAttr(Element*, const QualifiedName&) const;
    void detachAttrObjectsFromElement(Element*) const;

    void reportMemoryUsage(MemoryObjectInfo*) const;

private:
    friend class Element;
    friend class HTMLConstructionSite;

    ElementAttributeData();
    ElementAttributeData(const ElementAttributeData&);
    ElementAttributeData(const Vector<Attribute>&);

    Attribute* getAttributeItem(const AtomicString& name, bool shouldIgnoreAttributeCase);
    const Attribute* getAttributeItem(const AtomicString& name, bool shouldIgnoreAttributeCase) const;
    size_t getAttributeItemIndexSlowCase(const AtomicString& name, bool shouldIgnoreAttributeCase) const;
    void cloneDataFrom(const ElementAttributeData& sourceData, const Element& sourceElement, Element& targetElement);
    void clearAttributes(Element*);
    void replaceAttribute(size_t index, const Attribute&, Element*);

    bool isMutable() const { return m_isMutable; }
    PassOwnPtr<ElementAttributeData> makeMutable() const { return adoptPtr(new ElementAttributeData(*this)); }

    mutable RefPtr<StylePropertySet> m_inlineStyleDecl;
    mutable RefPtr<StylePropertySet> m_attributeStyle;
    mutable SpaceSplitString m_classNames;
    mutable AtomicString m_idForStyleResolution;

    unsigned m_isMutable : 1;
    unsigned m_arraySize : 31;

    union {
        Vector<Attribute, 4>* m_mutableAttributeVector;
        void* m_attributes;
    };
};

inline size_t ElementAttributeData::length() const
{
    if (isMutable())
        return m_mutableAttributeVector->size();
    return m_arraySize;
}

inline Attribute* ElementAttributeData::getAttributeItem(const AtomicString& name, bool shouldIgnoreAttributeCase)
{
    size_t index = getAttributeItemIndex(name, shouldIgnoreAttributeCase);
    if (index != notFound)
        return attributeItem(index);
    return 0;
}

inline const Attribute* ElementAttributeData::getAttributeItem(const AtomicString& name, bool shouldIgnoreAttributeCase) const
{
    size_t index = getAttributeItemIndex(name, shouldIgnoreAttributeCase);
    if (index != notFound)
        return attributeItem(index);
    return 0;
}

inline size_t ElementAttributeData::getAttributeItemIndex(const QualifiedName& name) const
{
    for (unsigned i = 0; i < length(); ++i) {
        if (attributeItem(i)->name().matches(name))
            return i;
    }
    return notFound;
}

// We use a boolean parameter instead of calling shouldIgnoreAttributeCase so that the caller
// can tune the behavior (hasAttribute is case sensitive whereas getAttribute is not).
inline size_t ElementAttributeData::getAttributeItemIndex(const AtomicString& name, bool shouldIgnoreAttributeCase) const
{
    unsigned len = length();
    bool doSlowCheck = shouldIgnoreAttributeCase;

    // Optimize for the case where the attribute exists and its name exactly matches.
    for (unsigned i = 0; i < len; ++i) {
        const Attribute* attribute = attributeItem(i);
        if (!attribute->name().hasPrefix()) {
            if (name == attribute->localName())
                return i;
        } else
            doSlowCheck = true;
    }

    if (doSlowCheck)
        return getAttributeItemIndexSlowCase(name, shouldIgnoreAttributeCase);
    return notFound;
}

inline const Attribute* ElementAttributeData::getAttributeItem(const QualifiedName& name) const
{
    for (unsigned i = 0; i < length(); ++i) {
        if (attributeItem(i)->name().matches(name))
            return attributeItem(i);
    }
    return 0;
}

inline Attribute* ElementAttributeData::getAttributeItem(const QualifiedName& name)
{
    for (unsigned i = 0; i < length(); ++i) {
        if (attributeItem(i)->name().matches(name))
            return attributeItem(i);
    }
    return 0;
}

inline const Attribute* ElementAttributeData::attributeItem(unsigned index) const
{
    ASSERT(index < length());
    if (isMutable())
        return &m_mutableAttributeVector->at(index);
    const Attribute* buffer = reinterpret_cast<const Attribute*>(&m_attributes);
    return &buffer[index];
}

inline Attribute* ElementAttributeData::attributeItem(unsigned index)
{
    ASSERT(isMutable());
    ASSERT(index < length());
    return &m_mutableAttributeVector->at(index);
}

}

#endif // ElementAttributeData_h
