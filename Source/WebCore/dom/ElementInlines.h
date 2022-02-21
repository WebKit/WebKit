/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "Document.h"
#include "Element.h"
#include "ElementData.h"
#include "HTMLNames.h"
#include "RenderStyle.h"
#include "StyleChange.h"

namespace WebCore {

inline AttributeIteratorAccessor Element::attributesIterator() const
{
    return elementData()->attributesIterator();
}

inline unsigned Element::findAttributeIndexByName(const QualifiedName& name) const
{
    return elementData()->findAttributeIndexByName(name);
}

inline unsigned Element::findAttributeIndexByName(const AtomString& name, bool shouldIgnoreAttributeCase) const
{
    return elementData()->findAttributeIndexByName(name, shouldIgnoreAttributeCase);
}

inline bool Node::hasAttributes() const
{
    return is<Element>(*this) && downcast<Element>(*this).hasAttributes();
}

inline NamedNodeMap* Node::attributes() const
{
    return is<Element>(*this) ? &downcast<Element>(*this).attributes() : nullptr;
}

inline Element* Node::parentElement() const
{
    return dynamicDowncast<Element>(parentNode());
}

inline const Element* Element::rootElement() const
{
    if (isConnected())
        return document().documentElement();

    const Element* highest = this;
    while (highest->parentElement())
        highest = highest->parentElement();
    return highest;
}

inline bool Element::hasAttributeWithoutSynchronization(const QualifiedName& name) const
{
    ASSERT(fastAttributeLookupAllowed(name));
    return elementData() && findAttributeByName(name);
}

inline const AtomString& Element::attributeWithoutSynchronization(const QualifiedName& name) const
{
    if (elementData()) {
        if (const Attribute* attribute = findAttributeByName(name))
            return attribute->value();
    }
    return nullAtom();
}

inline bool Element::hasAttributesWithoutUpdate() const
{
    return elementData() && !elementData()->isEmpty();
}

inline const AtomString& Element::idForStyleResolution() const
{
    return hasID() ? elementData()->idForStyleResolution() : nullAtom();
}

inline const AtomString& Element::getIdAttribute() const
{
    if (hasID())
        return elementData()->findAttributeByName(HTMLNames::idAttr)->value();
    return nullAtom();
}

inline const AtomString& Element::getNameAttribute() const
{
    if (hasName())
        return elementData()->findAttributeByName(HTMLNames::nameAttr)->value();
    return nullAtom();
}

inline void Element::setIdAttribute(const AtomString& value)
{
    setAttributeWithoutSynchronization(HTMLNames::idAttr, value);
}

inline const SpaceSplitString& Element::classNames() const
{
    ASSERT(hasClass());
    ASSERT(elementData());
    return elementData()->classNames();
}

inline unsigned Element::attributeCount() const
{
    ASSERT(elementData());
    return elementData()->length();
}

inline const Attribute& Element::attributeAt(unsigned index) const
{
    ASSERT(elementData());
    return elementData()->attributeAt(index);
}

inline const Attribute* Element::findAttributeByName(const QualifiedName& name) const
{
    ASSERT(elementData());
    return elementData()->findAttributeByName(name);
}

inline bool Element::hasID() const
{
    return elementData() && elementData()->hasID();
}

inline bool Element::hasClass() const
{
    return elementData() && elementData()->hasClass();
}

inline bool Element::hasName() const
{
    return elementData() && elementData()->hasName();
}

inline UniqueElementData& Element::ensureUniqueElementData()
{
    if (!elementData() || !elementData()->isUnique())
        createUniqueElementData();
    return static_cast<UniqueElementData&>(*m_elementData);
}

inline bool shouldIgnoreAttributeCase(const Element& element)
{
    return element.isHTMLElement() && element.document().isHTMLDocument();
}

template<typename... QualifiedNames>
inline const AtomString& Element::getAttribute(const QualifiedName& name, const QualifiedNames&... names) const
{
    const AtomString& value = getAttribute(name);
    if (!value.isNull())
        return value;
    return getAttribute(names...);
}

inline bool isInTopLayerOrBackdrop(const RenderStyle& style, const Element* element)
{
    return (element && element->isInTopLayer()) || style.styleType() == PseudoId::Backdrop;
}


}
