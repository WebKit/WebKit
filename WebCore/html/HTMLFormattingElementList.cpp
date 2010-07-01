/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "HTMLFormattingElementList.h"

#include "Element.h"
#include "NotImplemented.h"

namespace WebCore {

HTMLFormattingElementList::Entry::Entry(Element* element)
    : m_element(element)
{
    ASSERT(element);
}

HTMLFormattingElementList::Entry::Entry(MarkerEntryType)
{
}

HTMLFormattingElementList::Entry::~Entry()
{
}

bool HTMLFormattingElementList::Entry::isMarker() const
{
    return !m_element;
}

Element* HTMLFormattingElementList::Entry::element() const
{
    // The fact that !m_element == isMarker() is an implementation detail
    // callers should check isMarker() before calling element().
    ASSERT(m_element);
    return m_element.get();
}

void HTMLFormattingElementList::Entry::replaceElement(PassRefPtr<Element> element)
{
    ASSERT(m_element); // Once a marker, always a marker.
    m_element = element;
}

HTMLFormattingElementList::HTMLFormattingElementList()
{
}

HTMLFormattingElementList::~HTMLFormattingElementList()
{
}

void HTMLFormattingElementList::append(Element* element)
{
    m_entries.append(element);
}

void HTMLFormattingElementList::clearToLastMarker()
{
    while (m_entries.size() && !m_entries.last().isMarker())
        m_entries.removeLast();
}

}
