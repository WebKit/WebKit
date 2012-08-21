/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Peter Kelly (pmk@post.com)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2008, 2010, 2012 Apple Inc. All rights reserved.
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

#include "config.h"
#include "ElementAttributeData.h"

#include "Attr.h"
#include "CSSStyleSheet.h"
#include "MemoryInstrumentation.h"
#include "StyledElement.h"

namespace WebCore {

static size_t immutableElementAttributeDataSize(unsigned count)
{
    return sizeof(ElementAttributeData) - sizeof(void*) + sizeof(Attribute) * count;
}

PassOwnPtr<ElementAttributeData> ElementAttributeData::createImmutable(const Vector<Attribute>& attributes)
{
    void* slot = WTF::fastMalloc(immutableElementAttributeDataSize(attributes.size()));
    return adoptPtr(new (slot) ElementAttributeData(attributes));
}

ElementAttributeData::ElementAttributeData()
    : m_isMutable(true)
    , m_arraySize(0)
    , m_mutableAttributeVector(new Vector<Attribute, 4>)
{
}

ElementAttributeData::ElementAttributeData(const Vector<Attribute>& attributes)
    : m_isMutable(false)
    , m_arraySize(attributes.size())
{
    Attribute* buffer = reinterpret_cast<Attribute*>(&m_attributes);
    for (unsigned i = 0; i < attributes.size(); ++i)
        new (&buffer[i]) Attribute(attributes[i]);
}

ElementAttributeData::ElementAttributeData(const ElementAttributeData& other)
    : m_inlineStyleDecl(other.m_inlineStyleDecl)
    , m_attributeStyle(other.m_attributeStyle)
    , m_classNames(other.m_classNames)
    , m_idForStyleResolution(other.m_idForStyleResolution)
    , m_isMutable(true)
    , m_arraySize(0)
    , m_mutableAttributeVector(new Vector<Attribute, 4>)
{
    // This copy constructor should only be used by makeMutable() to go from immutable to mutable.
    ASSERT(!other.m_isMutable);

    const Attribute* otherBuffer = reinterpret_cast<const Attribute*>(&other.m_attributes);
    for (unsigned i = 0; i < other.m_arraySize; ++i)
        m_mutableAttributeVector->append(otherBuffer[i]);
}

ElementAttributeData::~ElementAttributeData()
{
    if (isMutable()) {
        ASSERT(!m_arraySize);
        delete m_mutableAttributeVector;
    } else {
        Attribute* buffer = reinterpret_cast<Attribute*>(&m_attributes);
        for (unsigned i = 0; i < m_arraySize; ++i)
            buffer[i].~Attribute();
    }
}

typedef Vector<RefPtr<Attr> > AttrList;
typedef HashMap<Element*, OwnPtr<AttrList> > AttrListMap;

static AttrListMap& attrListMap()
{
    DEFINE_STATIC_LOCAL(AttrListMap, map, ());
    return map;
}

static AttrList* attrListForElement(Element* element)
{
    ASSERT(element);
    if (!element->hasAttrList())
        return 0;
    ASSERT(attrListMap().contains(element));
    return attrListMap().get(element);
}

static AttrList* ensureAttrListForElement(Element* element)
{
    ASSERT(element);
    if (element->hasAttrList()) {
        ASSERT(attrListMap().contains(element));
        return attrListMap().get(element);
    }
    ASSERT(!attrListMap().contains(element));
    element->setHasAttrList();
    AttrListMap::AddResult result = attrListMap().add(element, adoptPtr(new AttrList));
    return result.iterator->second.get();
}

static void removeAttrListForElement(Element* element)
{
    ASSERT(element);
    ASSERT(element->hasAttrList());
    ASSERT(attrListMap().contains(element));
    attrListMap().remove(element);
    element->clearHasAttrList();
}

static Attr* findAttrInList(AttrList* attrList, const QualifiedName& name)
{
    for (unsigned i = 0; i < attrList->size(); ++i) {
        if (attrList->at(i)->qualifiedName() == name)
            return attrList->at(i).get();
    }
    return 0;
}

PassRefPtr<Attr> ElementAttributeData::attrIfExists(Element* element, const QualifiedName& name) const
{
    if (AttrList* attrList = attrListForElement(element))
        return findAttrInList(attrList, name);
    return 0;
}

PassRefPtr<Attr> ElementAttributeData::ensureAttr(Element* element, const QualifiedName& name) const
{
    AttrList* attrList = ensureAttrListForElement(element);
    RefPtr<Attr> attr = findAttrInList(attrList, name);
    if (!attr) {
        attr = Attr::create(element, name);
        attrList->append(attr);
    }
    return attr.release();
}

void ElementAttributeData::setAttr(Element* element, const QualifiedName& name, Attr* attr) const
{
    AttrList* attrList = ensureAttrListForElement(element);

    if (findAttrInList(attrList, name))
        return;

    attrList->append(attr);
    attr->attachToElement(element);
}

void ElementAttributeData::removeAttr(Element* element, const QualifiedName& name) const
{
    AttrList* attrList = attrListForElement(element);
    ASSERT(attrList);

    for (unsigned i = 0; i < attrList->size(); ++i) {
        if (attrList->at(i)->qualifiedName() == name) {
            attrList->remove(i);
            if (attrList->isEmpty())
                removeAttrListForElement(element);
            return;
        }
    }

    ASSERT_NOT_REACHED();
}

StylePropertySet* ElementAttributeData::ensureInlineStyle(StyledElement* element)
{
    ASSERT(isMutable());
    if (!m_inlineStyleDecl) {
        ASSERT(element->isStyledElement());
        m_inlineStyleDecl = StylePropertySet::create(strictToCSSParserMode(element->isHTMLElement() && !element->document()->inQuirksMode()));
    }
    return m_inlineStyleDecl.get();
}

StylePropertySet* ElementAttributeData::ensureMutableInlineStyle(StyledElement* element)
{
    ASSERT(isMutable());
    if (m_inlineStyleDecl && !m_inlineStyleDecl->isMutable()) {
        m_inlineStyleDecl = m_inlineStyleDecl->copy();
        return m_inlineStyleDecl.get();
    }
    return ensureInlineStyle(element);
}
    
void ElementAttributeData::updateInlineStyleAvoidingMutation(StyledElement* element, const String& text) const
{
    // We reconstruct the property set instead of mutating if there is no CSSOM wrapper.
    // This makes wrapperless property sets immutable and so cacheable.
    if (m_inlineStyleDecl && !m_inlineStyleDecl->isMutable())
        m_inlineStyleDecl.clear();
    if (!m_inlineStyleDecl)
        m_inlineStyleDecl = StylePropertySet::create(strictToCSSParserMode(element->isHTMLElement() && !element->document()->inQuirksMode()));
    m_inlineStyleDecl->parseDeclaration(text, element->document()->elementSheet()->contents());
}

void ElementAttributeData::destroyInlineStyle(StyledElement* element) const
{
    if (!m_inlineStyleDecl)
        return;
    m_inlineStyleDecl->clearParentElement(element);
    m_inlineStyleDecl = 0;
}

void ElementAttributeData::addAttribute(const Attribute& attribute, Element* element, SynchronizationOfLazyAttribute inSynchronizationOfLazyAttribute)
{
    ASSERT(isMutable());

    if (element && inSynchronizationOfLazyAttribute == NotInSynchronizationOfLazyAttribute)
        element->willModifyAttribute(attribute.name(), nullAtom, attribute.value());

    m_mutableAttributeVector->append(attribute);

    if (element && inSynchronizationOfLazyAttribute == NotInSynchronizationOfLazyAttribute)
        element->didAddAttribute(attribute);
}

void ElementAttributeData::removeAttribute(size_t index, Element* element, SynchronizationOfLazyAttribute inSynchronizationOfLazyAttribute)
{
    ASSERT(isMutable());
    ASSERT(index < length());

    Attribute& attribute = m_mutableAttributeVector->at(index);
    QualifiedName name = attribute.name();

    if (element && inSynchronizationOfLazyAttribute == NotInSynchronizationOfLazyAttribute)
        element->willRemoveAttribute(name, attribute.value());

    if (RefPtr<Attr> attr = attrIfExists(element, name))
        attr->detachFromElementWithValue(attribute.value());

    m_mutableAttributeVector->remove(index);

    if (element && inSynchronizationOfLazyAttribute == NotInSynchronizationOfLazyAttribute)
        element->didRemoveAttribute(name);
}

bool ElementAttributeData::isEquivalent(const ElementAttributeData* other) const
{
    if (!other)
        return isEmpty();

    unsigned len = length();
    if (len != other->length())
        return false;

    for (unsigned i = 0; i < len; i++) {
        const Attribute* attribute = attributeItem(i);
        const Attribute* otherAttr = other->getAttributeItem(attribute->name());
        if (!otherAttr || attribute->value() != otherAttr->value())
            return false;
    }

    return true;
}

void ElementAttributeData::detachAttrObjectsFromElement(Element* element) const
{
    ASSERT(element->hasAttrList());

    for (unsigned i = 0; i < length(); ++i) {
        const Attribute* attribute = attributeItem(i);
        if (RefPtr<Attr> attr = attrIfExists(element, attribute->name()))
            attr->detachFromElementWithValue(attribute->value());
    }

    // The loop above should have cleaned out this element's Attr map.
    ASSERT(!element->hasAttrList());
}

void ElementAttributeData::reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    size_t actualSize = m_isMutable ? sizeof(ElementAttributeData) : immutableElementAttributeDataSize(m_arraySize);
    MemoryClassInfo info(memoryObjectInfo, this, MemoryInstrumentation::DOM, actualSize);
    info.addInstrumentedMember(m_inlineStyleDecl);
    info.addInstrumentedMember(m_attributeStyle);
    info.addMember(m_classNames);
    info.addInstrumentedMember(m_idForStyleResolution);
    if (m_isMutable)
        info.addVectorPtr(m_mutableAttributeVector);
    for (unsigned i = 0, len = length(); i < len; i++)
        info.addInstrumentedMember(*attributeItem(i));
}

size_t ElementAttributeData::getAttributeItemIndexSlowCase(const AtomicString& name, bool shouldIgnoreAttributeCase) const
{
    // Continue to checking case-insensitively and/or full namespaced names if necessary:
    for (unsigned i = 0; i < length(); ++i) {
        const Attribute* attribute = attributeItem(i);
        if (!attribute->name().hasPrefix()) {
            if (shouldIgnoreAttributeCase && equalIgnoringCase(name, attribute->localName()))
                return i;
        } else {
            // FIXME: Would be faster to do this comparison without calling toString, which
            // generates a temporary string by concatenation. But this branch is only reached
            // if the attribute name has a prefix, which is rare in HTML.
            if (equalPossiblyIgnoringCase(name, attribute->name().toString(), shouldIgnoreAttributeCase))
                return i;
        }
    }
    return notFound;
}

void ElementAttributeData::cloneDataFrom(const ElementAttributeData& sourceData, const Element& sourceElement, Element& targetElement)
{
    // FIXME: Cloned elements could start out with immutable attribute data.
    ASSERT(isMutable());

    const AtomicString& oldID = targetElement.getIdAttribute();
    const AtomicString& newID = sourceElement.getIdAttribute();

    if (!oldID.isNull() || !newID.isNull())
        targetElement.updateId(oldID, newID);

    const AtomicString& oldName = targetElement.getNameAttribute();
    const AtomicString& newName = sourceElement.getNameAttribute();

    if (!oldName.isNull() || !newName.isNull())
        targetElement.updateName(oldName, newName);

    clearAttributes(&targetElement);

    if (sourceData.isMutable())
        *m_mutableAttributeVector = *sourceData.m_mutableAttributeVector;
    else {
        ASSERT(m_mutableAttributeVector->isEmpty());
        m_mutableAttributeVector->reserveInitialCapacity(sourceData.m_arraySize);
        const Attribute* sourceBuffer = reinterpret_cast<const Attribute*>(&sourceData.m_attributes);
        for (unsigned i = 0; i < sourceData.m_arraySize; ++i)
            m_mutableAttributeVector->uncheckedAppend(sourceBuffer[i]);
    }

    for (unsigned i = 0; i < length(); ++i) {
        const Attribute& attribute = m_mutableAttributeVector->at(i);
        if (targetElement.isStyledElement() && attribute.name() == HTMLNames::styleAttr) {
            static_cast<StyledElement&>(targetElement).styleAttributeChanged(attribute.value(), StyledElement::DoNotReparseStyleAttribute);
            continue;
        }
        targetElement.attributeChanged(attribute);
    }

    if (targetElement.isStyledElement() && sourceData.m_inlineStyleDecl) {
        StylePropertySet* inlineStyle = ensureMutableInlineStyle(static_cast<StyledElement*>(&targetElement));
        inlineStyle->copyPropertiesFrom(*sourceData.m_inlineStyleDecl);
        inlineStyle->setCSSParserMode(sourceData.m_inlineStyleDecl->cssParserMode());
        targetElement.setIsStyleAttributeValid(sourceElement.isStyleAttributeValid());
    }
}

void ElementAttributeData::clearAttributes(Element* element)
{
    ASSERT(isMutable());

    if (element->hasAttrList())
        detachAttrObjectsFromElement(element);

    clearClass();
    m_mutableAttributeVector->clear();
}

void ElementAttributeData::replaceAttribute(size_t index, const Attribute& attribute, Element* element)
{
    ASSERT(isMutable());
    ASSERT(element);
    ASSERT(index < length());

    element->willModifyAttribute(attribute.name(), m_mutableAttributeVector->at(index).value(), attribute.value());
    (*m_mutableAttributeVector)[index] = attribute;
    element->didModifyAttribute(attribute);
}

PassRefPtr<Attr> ElementAttributeData::getAttributeNode(const String& name, bool shouldIgnoreAttributeCase, Element* element) const
{
    ASSERT(element);
    const Attribute* attribute = getAttributeItem(name, shouldIgnoreAttributeCase);
    if (!attribute)
        return 0;
    return ensureAttr(element, attribute->name());
}

PassRefPtr<Attr> ElementAttributeData::getAttributeNode(const QualifiedName& name, Element* element) const
{
    ASSERT(element);
    const Attribute* attribute = getAttributeItem(name);
    if (!attribute)
        return 0;
    return ensureAttr(element, attribute->name());
}

}
