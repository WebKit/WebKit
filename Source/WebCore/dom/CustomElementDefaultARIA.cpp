/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include "config.h"
#include "CustomElementDefaultARIA.h"

#include "Element.h"
#include "ElementInlines.h"
#include "HTMLNames.h"
#include "SpaceSplitString.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(CustomElementDefaultARIA);

CustomElementDefaultARIA::CustomElementDefaultARIA() = default;
CustomElementDefaultARIA::~CustomElementDefaultARIA() = default;

void CustomElementDefaultARIA::setValueForAttribute(const QualifiedName& name, const AtomString& value)
{
    m_map.set(name, value);
}

static bool isElementVisible(const Element& element, const Element& thisElement)
{
    return !element.isConnected() || element.isInDocumentTree() || thisElement.isDescendantOrShadowDescendantOf(element.protectedRootNode());
}

const AtomString& CustomElementDefaultARIA::valueForAttribute(const Element& thisElement, const QualifiedName& name) const
{
    auto it = m_map.find(name);
    if (it == m_map.end())
        return nullAtom();

    return std::visit(WTF::makeVisitor([&](const AtomString& stringValue) -> const AtomString& {
        return stringValue;
    }, [&](const WeakPtr<Element, WeakPtrImplWithEventTargetData>& weakElementValue) -> const AtomString& {
        RefPtr elementValue = weakElementValue.get();
        if (elementValue && isElementVisible(*elementValue, thisElement))
            return elementValue->attributeWithoutSynchronization(HTMLNames::idAttr);
        return nullAtom();
    }, [&](const Vector<WeakPtr<Element, WeakPtrImplWithEventTargetData>>& elements) -> const AtomString& {
        StringBuilder idList;
        for (auto& weakElement : elements) {
            RefPtr element = weakElement.get();
            if (element && isElementVisible(*element, thisElement)) {
                if (idList.length())
                    idList.append(' ');
                idList.append(element->attributeWithoutSynchronization(HTMLNames::idAttr));
            }
        }
        // FIXME: This should probably be using the idList we just built.
        return nullAtom();
    }), it->value);
}

bool CustomElementDefaultARIA::hasAttribute(const QualifiedName& name) const
{
    return m_map.find(name) != m_map.end();
}

RefPtr<Element> CustomElementDefaultARIA::elementForAttribute(const Element& thisElement, const QualifiedName& name) const
{
    auto it = m_map.find(name);
    if (it == m_map.end())
        return nullptr;

    RefPtr<Element> result;
    std::visit(WTF::makeVisitor([&](const AtomString& stringValue) {
        if (thisElement.isInTreeScope())
            result = thisElement.treeScope().getElementById(stringValue);
    }, [&](const WeakPtr<Element, WeakPtrImplWithEventTargetData>& weakElementValue) {
        RefPtr elementValue = weakElementValue.get();
        if (elementValue && isElementVisible(*elementValue, thisElement))
            result = WTFMove(elementValue);
    }, [&](const Vector<WeakPtr<Element, WeakPtrImplWithEventTargetData>>&) {
        RELEASE_ASSERT_NOT_REACHED();
    }), it->value);
    return result;
}

void CustomElementDefaultARIA::setElementForAttribute(const QualifiedName& name, Element* element)
{
    m_map.set(name, WeakPtr<Element, WeakPtrImplWithEventTargetData> { element });
}

Vector<RefPtr<Element>> CustomElementDefaultARIA::elementsForAttribute(const Element& thisElement, const QualifiedName& name) const
{
    Vector<RefPtr<Element>> result;
    auto it = m_map.find(name);
    if (it == m_map.end())
        return result;
    std::visit(WTF::makeVisitor([&](const AtomString& stringValue) {
        SpaceSplitString idList { stringValue, SpaceSplitString::ShouldFoldCase::No };
        result.reserveCapacity(idList.size());
        if (thisElement.isInTreeScope()) {
            for (unsigned i = 0; i < idList.size(); ++i)
                result.append(thisElement.treeScope().getElementById(idList[i]));
        }
    }, [&](const WeakPtr<Element, WeakPtrImplWithEventTargetData>& weakElementValue) {
        RefPtr element = weakElementValue.get();
        if (element && isElementVisible(*element, thisElement))
            result.append(WTFMove(element));
    }, [&](const Vector<WeakPtr<Element, WeakPtrImplWithEventTargetData>>& elements) {
        result.reserveCapacity(elements.size());
        for (auto& weakElement : elements) {
            if (RefPtr element = weakElement.get(); element && isElementVisible(*element, thisElement))
                result.append(WTFMove(element));
        }
    }), it->value);
    return result;
}

void CustomElementDefaultARIA::setElementsForAttribute(const QualifiedName& name, std::optional<Vector<RefPtr<Element>>>&& values)
{
    Vector<WeakPtr<Element, WeakPtrImplWithEventTargetData>> elements;
    if (values) {
        for (auto& element : *values) {
            ASSERT(element);
            elements.append(WeakPtr<Element, WeakPtrImplWithEventTargetData> { *element });
        }
    }
    m_map.set(name, WTFMove(elements));
}

} // namespace WebCore
