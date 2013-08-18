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

#include "config.h"
#include "ElementData.h"

#include "Attr.h"
#include "StylePropertySet.h"

namespace WebCore {

void ElementData::deref()
{
    if (!derefBase())
        return;

    if (m_isUnique)
        delete static_cast<UniqueElementData*>(this);
    else
        delete static_cast<ShareableElementData*>(this);
}

ElementData::ElementData()
    : m_isUnique(true)
    , m_arraySize(0)
    , m_hasNameAttribute(false)
    , m_presentationAttributeStyleIsDirty(false)
    , m_styleAttributeIsDirty(false)
#if ENABLE(SVG)
    , m_animatedSVGAttributesAreDirty(false)
#endif
{
}

ElementData::ElementData(unsigned arraySize)
    : m_isUnique(false)
    , m_arraySize(arraySize)
    , m_hasNameAttribute(false)
    , m_presentationAttributeStyleIsDirty(false)
    , m_styleAttributeIsDirty(false)
#if ENABLE(SVG)
    , m_animatedSVGAttributesAreDirty(false)
#endif
{
}

struct SameSizeAsElementData : public RefCounted<SameSizeAsElementData> {
    unsigned bitfield;
    void* refPtrs[3];
};

COMPILE_ASSERT(sizeof(ElementData) == sizeof(SameSizeAsElementData), element_attribute_data_should_stay_small);

static size_t sizeForShareableElementDataWithAttributeCount(unsigned count)
{
    return sizeof(ShareableElementData) + sizeof(Attribute) * count;
}

PassRefPtr<ShareableElementData> ShareableElementData::createWithAttributes(const Vector<Attribute>& attributes)
{
    void* slot = WTF::fastMalloc(sizeForShareableElementDataWithAttributeCount(attributes.size()));
    return adoptRef(new (NotNull, slot) ShareableElementData(attributes));
}

PassRefPtr<UniqueElementData> UniqueElementData::create()
{
    return adoptRef(new UniqueElementData);
}

ShareableElementData::ShareableElementData(const Vector<Attribute>& attributes)
    : ElementData(attributes.size())
{
    for (unsigned i = 0; i < m_arraySize; ++i)
        new (NotNull, &m_attributeArray[i]) Attribute(attributes[i]);
}

ShareableElementData::~ShareableElementData()
{
    for (unsigned i = 0; i < m_arraySize; ++i)
        m_attributeArray[i].~Attribute();
}

ShareableElementData::ShareableElementData(const UniqueElementData& other)
    : ElementData(other, false)
{
    ASSERT(!other.m_presentationAttributeStyle);

    if (other.m_inlineStyle) {
        ASSERT(!other.m_inlineStyle->hasCSSOMWrapper());
        m_inlineStyle = other.m_inlineStyle->immutableCopyIfNeeded();
    }

    for (unsigned i = 0; i < m_arraySize; ++i)
        new (NotNull, &m_attributeArray[i]) Attribute(other.m_attributeVector.at(i));
}

ElementData::ElementData(const ElementData& other, bool isUnique)
    : m_isUnique(isUnique)
    , m_arraySize(isUnique ? 0 : other.length())
    , m_hasNameAttribute(other.m_hasNameAttribute)
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

UniqueElementData::UniqueElementData()
{
}

UniqueElementData::UniqueElementData(const UniqueElementData& other)
    : ElementData(other, true)
    , m_presentationAttributeStyle(other.m_presentationAttributeStyle)
    , m_attributeVector(other.m_attributeVector)
{
    m_inlineStyle = other.m_inlineStyle ? other.m_inlineStyle->mutableCopy() : 0;
}

UniqueElementData::UniqueElementData(const ShareableElementData& other)
    : ElementData(other, true)
{
    // An ShareableElementData should never have a mutable inline StylePropertySet attached.
    ASSERT(!other.m_inlineStyle || !other.m_inlineStyle->isMutable());
    m_inlineStyle = other.m_inlineStyle;

    m_attributeVector.reserveCapacity(other.length());
    for (unsigned i = 0; i < other.length(); ++i)
        m_attributeVector.uncheckedAppend(other.m_attributeArray[i]);
}

PassRefPtr<UniqueElementData> ElementData::makeUniqueCopy() const
{
    if (isUnique())
        return adoptRef(new UniqueElementData(static_cast<const UniqueElementData&>(*this)));
    return adoptRef(new UniqueElementData(static_cast<const ShareableElementData&>(*this)));
}

PassRefPtr<ShareableElementData> UniqueElementData::makeShareableCopy() const
{
    void* slot = WTF::fastMalloc(sizeForShareableElementDataWithAttributeCount(m_attributeVector.size()));
    return adoptRef(new (NotNull, slot) ShareableElementData(*this));
}

void UniqueElementData::addAttribute(const QualifiedName& attributeName, const AtomicString& value)
{
    m_attributeVector.append(Attribute(attributeName, value));
}

void UniqueElementData::removeAttribute(unsigned index)
{
    ASSERT_WITH_SECURITY_IMPLICATION(index < length());
    m_attributeVector.remove(index);
}

bool ElementData::isEquivalent(const ElementData* other) const
{
    if (!other)
        return isEmpty();

    unsigned len = length();
    if (len != other->length())
        return false;

    for (unsigned i = 0; i < len; i++) {
        const Attribute& attribute = attributeAt(i);
        const Attribute* otherAttr = other->findAttributeByName(attribute.name());
        if (!otherAttr || attribute.value() != otherAttr->value())
            return false;
    }

    return true;
}

unsigned ElementData::findAttributeIndexByNameSlowCase(const AtomicString& name, bool shouldIgnoreAttributeCase) const
{
    // Continue to checking case-insensitively and/or full namespaced names if necessary:
    for (unsigned i = 0; i < length(); ++i) {
        const Attribute& attribute = attributeAt(i);
        if (!attribute.name().hasPrefix()) {
            if (shouldIgnoreAttributeCase && equalIgnoringCase(name, attribute.localName()))
                return i;
        } else {
            // FIXME: Would be faster to do this comparison without calling toString, which
            // generates a temporary string by concatenation. But this branch is only reached
            // if the attribute name has a prefix, which is rare in HTML.
            if (equalPossiblyIgnoringCase(name, attribute.name().toString(), shouldIgnoreAttributeCase))
                return i;
        }
    }
    return attributeNotFound;
}

unsigned ElementData::findAttributeIndexByNameForAttributeNode(const Attr* attr, bool shouldIgnoreAttributeCase) const
{
    ASSERT(attr);
    const Attribute* attributes = attributeBase();
    unsigned count = length();
    for (unsigned i = 0; i < count; ++i) {
        if (attributes[i].name().matchesIgnoringCaseForLocalName(attr->qualifiedName(), shouldIgnoreAttributeCase))
            return i;
    }
    return attributeNotFound;
}

Attribute* UniqueElementData::findAttributeByName(const QualifiedName& name)
{
    unsigned count = length();
    for (unsigned i = 0; i < count; ++i) {
        if (m_attributeVector.at(i).name().matches(name))
            return &m_attributeVector.at(i);
    }
    return 0;
}

Attribute& UniqueElementData::attributeAt(unsigned index)
{
    ASSERT_WITH_SECURITY_IMPLICATION(index < length());
    return m_attributeVector.at(index);
}

}

