/*
 * Copyright (C) 2015, 2016 Apple Inc. All rights reserved.
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
#include "CustomElementRegistry.h"

#if ENABLE(CUSTOM_ELEMENTS)

#include "Document.h"
#include "Element.h"
#include "JSCustomElementInterface.h"
#include "MathMLNames.h"
#include "QualifiedName.h"
#include "SVGNames.h"
#include <runtime/JSCJSValueInlines.h>
#include <wtf/text/AtomicString.h>

namespace WebCore {

Ref<CustomElementRegistry> CustomElementRegistry::create()
{
    return adoptRef(*new CustomElementRegistry());
}

CustomElementRegistry::CustomElementRegistry()
{ }

CustomElementRegistry::~CustomElementRegistry()
{ }

void CustomElementRegistry::addElementDefinition(Ref<JSCustomElementInterface>&& elementInterface)
{
    AtomicString localName = elementInterface->name().localName();
    ASSERT(!m_nameMap.contains(localName));
    m_constructorMap.add(elementInterface->constructor(), elementInterface.ptr());
    m_nameMap.add(localName, elementInterface.copyRef());

    auto candidateList = m_upgradeCandidatesMap.find(localName);
    if (candidateList == m_upgradeCandidatesMap.end())
        return;

    Vector<RefPtr<Element>> list(WTFMove(candidateList->value));

    m_upgradeCandidatesMap.remove(localName);

    for (auto& candidate : list) {
        ASSERT(candidate);
        elementInterface->upgradeElement(*candidate);
    }

    // We should not be adding more upgrade candidate for this local name.
    ASSERT(!m_upgradeCandidatesMap.contains(localName));
}

void CustomElementRegistry::addUpgradeCandidate(Element& candidate)
{
    auto result = m_upgradeCandidatesMap.ensure(candidate.localName(), [] {
        return Vector<RefPtr<Element>>();
    });
    auto& nodeVector = result.iterator->value;
    ASSERT(!nodeVector.contains(&candidate));
    nodeVector.append(&candidate);
}

JSCustomElementInterface* CustomElementRegistry::findInterface(const QualifiedName& name) const
{
    auto it = m_nameMap.find(name.localName());
    return it == m_nameMap.end() || it->value->name() != name ? nullptr : const_cast<JSCustomElementInterface*>(it->value.ptr());
}

JSCustomElementInterface* CustomElementRegistry::findInterface(const AtomicString& name) const
{
    return m_nameMap.get(name);
}

JSCustomElementInterface* CustomElementRegistry::findInterface(const JSC::JSObject* constructor) const
{
    return m_constructorMap.get(constructor);
}

bool CustomElementRegistry::containsConstructor(const JSC::JSObject* constructor) const
{
    return m_constructorMap.contains(constructor);
}

}

#endif
