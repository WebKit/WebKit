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
#include "ElementInternals.h"

#include "ElementRareData.h"
#include "ShadowRoot.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

using namespace HTMLNames;

WTF_MAKE_ISO_ALLOCATED_IMPL(ElementInternals);

ShadowRoot* ElementInternals::shadowRoot() const
{
    RefPtr element = m_element.get();
    if (!element)
        return nullptr;
    auto* shadowRoot = element->shadowRoot();
    if (!shadowRoot || !shadowRoot->isAvailableToElementInternals())
        return nullptr;
    return shadowRoot;
}

void ElementInternals::setRole(const AtomString& role)
{
    setAriaValueForAttribute(roleAttr->localName(), role);
}

const AtomString& ElementInternals::role()
{
    return ariaValueForAttribute(roleAttr->localName());
}

void ElementInternals::setAriaRoleDescription(const AtomString& description)
{
    setAriaValueForAttribute(aria_roledescriptionAttr->localName(), description);
}

const AtomString& ElementInternals::ariaRoleDescription()
{
    return ariaValueForAttribute(aria_roledescriptionAttr->localName());
}

void ElementInternals::setAriaLabel(const AtomString& label)
{
    setAriaValueForAttribute(aria_labelAttr->localName(), label);
}

const AtomString& ElementInternals::ariaLabel()
{
    return ariaValueForAttribute(aria_labelAttr->localName());
}

void ElementInternals::setAriaValueForAttribute(const AtomString& key, const AtomString& value)
{
    m_element->customElementDefaultARIA().setValueForAttribute(key, value);
}

const AtomString& ElementInternals::ariaValueForAttribute(const AtomString& key)
{
    auto* defaultARIA = m_element->customElementDefaultARIAIfExists();
    if (!defaultARIA)
        return nullAtom();
    return defaultARIA->valueForAttribute(key);
}

} // namespace WebCore
