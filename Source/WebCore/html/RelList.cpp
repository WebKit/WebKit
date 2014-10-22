/*
 * Copyright (C) 2014 Dhi Aurrahman <diorahman@rockybars.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RelList.h"

#include "Element.h"
#include "HTMLNames.h"

namespace WebCore {

RelList::RelList(Element& element) 
    : m_element(element)
    , m_relAttributeValue(SpaceSplitString(element.fastGetAttribute(HTMLNames::relAttr), false))
{
}

void RelList::ref()
{
    m_element.ref();
}

void RelList::deref()
{
    m_element.deref();
}

unsigned RelList::length() const
{
    return m_relAttributeValue.size();
}

const AtomicString RelList::item(unsigned index) const
{
    if (index >= length())
        return nullAtom;
    return m_relAttributeValue[index];
}

Element* RelList::element() const
{
    return &m_element;
}

void RelList::updateRelAttribute(const AtomicString& value)
{
    m_relAttributeValue.set(value, false);
}

bool RelList::containsInternal(const AtomicString& token) const
{
    return m_relAttributeValue.contains(token);
}

AtomicString RelList::value() const
{
    return m_element.fastGetAttribute(HTMLNames::relAttr);
}

void RelList::setValue(const AtomicString& value)
{
    m_element.setAttribute(HTMLNames::relAttr, value);
}

} // namespace WebCore

