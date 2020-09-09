/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "DateTimeSymbolicFieldElement.h"

#if ENABLE(DATE_AND_TIME_INPUT_TYPES)

#include <wtf/IsoMallocInlines.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/TextBreakIterator.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(DateTimeSymbolicFieldElement);

static AtomString makeVisibleEmptyValue(const Vector<String>& symbols)
{
    unsigned maximumLength = 0;
    for (auto& symbol : symbols)
        maximumLength = std::max(maximumLength, numGraphemeClusters(symbol));
    return makeString(pad('-', maximumLength, emptyString()));
}

DateTimeSymbolicFieldElement::DateTimeSymbolicFieldElement(Document& document, FieldOwner& fieldOwner, const Vector<String>& symbols)
    : DateTimeFieldElement(document, fieldOwner)
    , m_symbols(symbols)
    , m_visibleEmptyValue(makeVisibleEmptyValue(symbols))
    , m_typeAhead(this)
{
    ASSERT(!m_symbols.isEmpty());
}

bool DateTimeSymbolicFieldElement::hasValue() const
{
    return m_selectedIndex >= 0;
}

void DateTimeSymbolicFieldElement::initialize(const AtomString& pseudo)
{
    DateTimeFieldElement::initialize(pseudo);
}

void DateTimeSymbolicFieldElement::setEmptyValue(EventBehavior eventBehavior)
{
    m_selectedIndex = invalidIndex;
    updateVisibleValue(eventBehavior);
}

void DateTimeSymbolicFieldElement::setValueAsInteger(int newSelectedIndex, EventBehavior eventBehavior)
{
    m_selectedIndex = std::max(0, std::min(newSelectedIndex, static_cast<int>(m_symbols.size() - 1)));
    updateVisibleValue(eventBehavior);
}

void DateTimeSymbolicFieldElement::stepDown()
{
    int newValue = hasValue() ? m_selectedIndex - 1 : m_symbols.size() - 1;
    if (newValue < 0)
        newValue = m_symbols.size() - 1;
    setValueAsInteger(newValue, DispatchInputAndChangeEvents);
}

void DateTimeSymbolicFieldElement::stepUp()
{
    int newValue = hasValue() ? m_selectedIndex + 1 : 0;
    if (newValue >= static_cast<int>(m_symbols.size()))
        newValue = 0;
    setValueAsInteger(newValue, DispatchInputAndChangeEvents);
}

String DateTimeSymbolicFieldElement::value() const
{
    return hasValue() ? m_symbols[m_selectedIndex] : emptyString();
}

int DateTimeSymbolicFieldElement::valueAsInteger() const
{
    return m_selectedIndex;
}

String DateTimeSymbolicFieldElement::visibleEmptyValue() const
{
    return m_visibleEmptyValue;
}

String DateTimeSymbolicFieldElement::visibleValue() const
{
    return hasValue() ? m_symbols[m_selectedIndex] : visibleEmptyValue();
}

void DateTimeSymbolicFieldElement::handleKeyboardEvent(KeyboardEvent& keyboardEvent)
{
    if (keyboardEvent.type() != eventNames().keypressEvent)
        return;

    keyboardEvent.setDefaultHandled();

    int index = m_typeAhead.handleEvent(&keyboardEvent, TypeAhead::MatchPrefix | TypeAhead::CycleFirstChar | TypeAhead::MatchIndex);
    if (index < 0)
        return;
    setValueAsInteger(index, DispatchInputAndChangeEvents);
}

int DateTimeSymbolicFieldElement::indexOfSelectedOption() const
{
    return m_selectedIndex;
}

int DateTimeSymbolicFieldElement::optionCount() const
{
    return m_symbols.size();
}

String DateTimeSymbolicFieldElement::optionAtIndex(int index) const
{
    return m_symbols[index];
}

} // namespace WebCore

#endif // ENABLE(DATE_AND_TIME_INPUT_TYPES)
