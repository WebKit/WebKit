/*
 * Copyright (c) 2012 Motorola Mobility, Inc. All rights reserved.
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY MOTOROLA MOBILITY, INC. AND ITS CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MOTOROLA MOBILITY, INC. OR ITS
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RadioNodeList.h"

#include "HTMLFormElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HTMLObjectElement.h"
#include "NodeRareData.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

using namespace HTMLNames;

WTF_MAKE_ISO_ALLOCATED_IMPL(RadioNodeList);

RadioNodeList::RadioNodeList(ContainerNode& rootNode, const AtomString& name)
    : CachedLiveNodeList(rootNode, InvalidateForFormControls)
    , m_name(name)
    , m_isRootedAtDocument(is<HTMLFormElement>(ownerNode()))
{
}

RadioNodeList::~RadioNodeList()
{
    ownerNode().nodeLists()->removeCacheWithAtomName(*this, m_name);
}

static inline RefPtr<HTMLInputElement> toRadioButtonInputElement(HTMLElement& element)
{
    if (!is<HTMLInputElement>(element))
        return nullptr;

    auto& inputElement = downcast<HTMLInputElement>(element);
    if (!inputElement.isRadioButton() || inputElement.value().isEmpty())
        return nullptr;
    return &inputElement;
}

String RadioNodeList::value() const
{
    auto length = this->length();
    for (unsigned i = 0; i < length; ++i) {
        auto inputElement = toRadioButtonInputElement(*item(i));
        if (!inputElement || !inputElement->checked())
            continue;
        return inputElement->value();
    }
    return String();
}

void RadioNodeList::setValue(const String& value)
{
    auto length = this->length();
    for (unsigned i = 0; i < length; ++i) {
        auto inputElement = toRadioButtonInputElement(*item(i));
        if (!inputElement || inputElement->value() != value)
            continue;
        inputElement->setChecked(true);
        return;
    }
}

bool RadioNodeList::checkElementMatchesRadioNodeListFilter(const Element& testElement) const
{
    ASSERT(is<HTMLObjectElement>(testElement) || is<HTMLFormControlElement>(testElement));
    if (is<HTMLFormElement>(ownerNode())) {
        RefPtr<HTMLFormElement> formElement;
        if (testElement.hasTagName(objectTag))
            formElement = downcast<HTMLObjectElement>(testElement).form();
        else
            formElement = downcast<HTMLFormControlElement>(testElement).form();
        if (!formElement || formElement != &ownerNode())
            return false;
    }

    return testElement.getIdAttribute() == m_name || testElement.getNameAttribute() == m_name;
}

bool RadioNodeList::elementMatches(Element& testElement) const
{
    if (!is<HTMLObjectElement>(testElement) && !is<HTMLFormControlElement>(testElement))
        return false;

    if (is<HTMLInputElement>(testElement) && downcast<HTMLInputElement>(testElement).isImageButton())
        return false;

    return checkElementMatchesRadioNodeListFilter(testElement);
}

} // namspace

