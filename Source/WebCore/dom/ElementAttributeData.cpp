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

#include "WebCoreMemoryInstrumentation.h"
#include <wtf/MemoryInstrumentationVector.h>

namespace WebCore {

struct SameSizeAsElementAttributeData : public RefCounted<SameSizeAsElementAttributeData> {
    unsigned bitfield;
    void* refPtrs[3];
};

COMPILE_ASSERT(sizeof(ElementAttributeData) == sizeof(SameSizeAsElementAttributeData), element_attribute_data_should_stay_small);

static size_t sizeForImmutableElementAttributeDataWithAttributeCount(unsigned count)
{
    return sizeof(ImmutableElementAttributeData) - sizeof(void*) + sizeof(Attribute) * count;
}

PassRefPtr<ElementAttributeData> ElementAttributeData::createImmutable(const Vector<Attribute>& attributes)
{
    void* slot = WTF::fastMalloc(sizeForImmutableElementAttributeDataWithAttributeCount(attributes.size()));
    return adoptRef(new (slot) ImmutableElementAttributeData(attributes));
}

PassRefPtr<ElementAttributeData> ElementAttributeData::create()
{
    return adoptRef(new MutableElementAttributeData);
}

ImmutableElementAttributeData::ImmutableElementAttributeData(const Vector<Attribute>& attributes)
    : ElementAttributeData(attributes.size())
{
    for (unsigned i = 0; i < m_arraySize; ++i)
        new (&reinterpret_cast<Attribute*>(&m_attributeArray)[i]) Attribute(attributes[i]);
}

ImmutableElementAttributeData::~ImmutableElementAttributeData()
{
    for (unsigned i = 0; i < m_arraySize; ++i)
        (reinterpret_cast<Attribute*>(&m_attributeArray)[i]).~Attribute();
}

ImmutableElementAttributeData::ImmutableElementAttributeData(const MutableElementAttributeData& other)
    : ElementAttributeData(other, false)
{
    ASSERT(!other.m_presentationAttributeStyle);

    if (other.m_inlineStyle) {
        ASSERT(!other.m_inlineStyle->hasCSSOMWrapper());
        m_inlineStyle = other.m_inlineStyle->immutableCopyIfNeeded();
    }

    for (unsigned i = 0; i < m_arraySize; ++i)
        new (&reinterpret_cast<Attribute*>(&m_attributeArray)[i]) Attribute(*other.attributeItem(i));
}

ElementAttributeData::ElementAttributeData(const ElementAttributeData& other, bool isMutable)
    : m_isMutable(isMutable)
    , m_arraySize(isMutable ? 0 : other.length())
    , m_presentationAttributeStyleIsDirty(other.m_presentationAttributeStyleIsDirty)
    , m_styleAttributeIsDirty(other.m_styleAttributeIsDirty)
#if ENABLE(SVG)
    , m_animatedSVGAttributesAreDirty(other.m_animatedSVGAttributesAreDirty)
#endif
    , m_classNames(other.m_classNames)
    , m_idForStyleResolution(other.m_idForStyleResolution)
{
    // NOTE: The inline style is copied by the subclass copy constructor since we don't know what to do with it here.
}

MutableElementAttributeData::MutableElementAttributeData(const MutableElementAttributeData& other)
    : ElementAttributeData(other, true)
    , m_presentationAttributeStyle(other.m_presentationAttributeStyle)
    , m_attributeVector(other.m_attributeVector)
{
    m_inlineStyle = other.m_inlineStyle ? other.m_inlineStyle->copy() : 0;
}

MutableElementAttributeData::MutableElementAttributeData(const ImmutableElementAttributeData& other)
    : ElementAttributeData(other, true)
{
    // An ImmutableElementAttributeData should never have a mutable inline StylePropertySet attached.
    ASSERT(!other.m_inlineStyle || !other.m_inlineStyle->isMutable());
    m_inlineStyle = other.m_inlineStyle;

    m_attributeVector.reserveCapacity(other.length());
    for (unsigned i = 0; i < other.length(); ++i)
        m_attributeVector.uncheckedAppend(other.immutableAttributeArray()[i]);
}

PassRefPtr<ElementAttributeData> ElementAttributeData::makeMutableCopy() const
{
    if (isMutable())
        return adoptRef(new MutableElementAttributeData(static_cast<const MutableElementAttributeData&>(*this)));
    return adoptRef(new MutableElementAttributeData(static_cast<const ImmutableElementAttributeData&>(*this)));
}

PassRefPtr<ElementAttributeData> ElementAttributeData::makeImmutableCopy() const
{
    ASSERT(isMutable());
    void* slot = WTF::fastMalloc(sizeForImmutableElementAttributeDataWithAttributeCount(mutableAttributeVector().size()));
    return adoptRef(new (slot) ImmutableElementAttributeData(static_cast<const MutableElementAttributeData&>(*this)));
}

void ElementAttributeData::addAttribute(const Attribute& attribute)
{
    ASSERT(isMutable());
    mutableAttributeVector().append(attribute);
}

void ElementAttributeData::removeAttribute(size_t index)
{
    ASSERT(isMutable());
    ASSERT(index < length());
    mutableAttributeVector().remove(index);
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

void ElementAttributeData::reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    size_t actualSize = m_isMutable ? sizeof(ElementAttributeData) : sizeForImmutableElementAttributeDataWithAttributeCount(m_arraySize);
    MemoryClassInfo info(memoryObjectInfo, this, WebCoreMemoryTypes::DOM, actualSize);
    info.addMember(m_inlineStyle);
    info.addMember(m_classNames);
    info.addMember(m_idForStyleResolution);
    if (m_isMutable) {
        info.addMember(presentationAttributeStyle());
        info.addMember(mutableAttributeVector());
    }
    for (unsigned i = 0, len = length(); i < len; i++)
        info.addMember(*attributeItem(i));
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

}
