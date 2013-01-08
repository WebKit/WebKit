/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#if ENABLE(INPUT_MULTIPLE_FIELDS_UI)
#include "DateTimeSymbolicFieldElement.h"

#include "Font.h"
#include "KeyboardEvent.h"
#include "TextBreakIterator.h"
#include <wtf/text/StringBuilder.h>
#include <wtf/unicode/Unicode.h>

namespace WebCore {

static AtomicString makeVisibleEmptyValue(const Vector<String>& symbols)
{
    unsigned maximumLength = 0;
    for (unsigned index = 0; index < symbols.size(); ++index)
        maximumLength = std::max(maximumLength, numGraphemeClusters(symbols[index]));
    StringBuilder builder;
    builder.reserveCapacity(maximumLength);
    for (unsigned length = 0; length < maximumLength; ++length)
        builder.append('-');
    return builder.toAtomicString();
}

DateTimeSymbolicFieldElement::DateTimeSymbolicFieldElement(Document* document, FieldOwner& fieldOwner, const Vector<String>& symbols, int minimum, int maximum)
    : DateTimeFieldElement(document, fieldOwner)
    , m_symbols(symbols)
    , m_visibleEmptyValue(makeVisibleEmptyValue(symbols))
    , m_selectedIndex(-1)
    , m_typeAhead(this)
    , m_minimumIndex(minimum)
    , m_maximumIndex(maximum)
{
    ASSERT(!symbols.isEmpty());
    ASSERT(m_minimumIndex >= 0);
    ASSERT(m_maximumIndex < static_cast<int>(m_symbols.size()));
    ASSERT(m_minimumIndex <= m_maximumIndex);
}

float DateTimeSymbolicFieldElement::maximumWidth(const Font& font)
{
    float maximumWidth = font.width(visibleEmptyValue());
    for (unsigned index = 0; index < m_symbols.size(); ++index)
        maximumWidth = std::max(maximumWidth, font.width(m_symbols[index]));
    return maximumWidth + DateTimeFieldElement::maximumWidth(font);
}

void DateTimeSymbolicFieldElement::handleKeyboardEvent(KeyboardEvent* keyboardEvent)
{
    if (keyboardEvent->type() != eventNames().keypressEvent)
        return;

    const UChar charCode = WTF::Unicode::toLower(keyboardEvent->charCode());
    if (charCode < ' ')
        return;

    keyboardEvent->setDefaultHandled();

    int index = m_typeAhead.handleEvent(keyboardEvent, TypeAhead::MatchPrefix | TypeAhead::CycleFirstChar | TypeAhead::MatchIndex);
    if (index < 0)
        return;
    setValueAsInteger(index, DispatchEvent);
}

bool DateTimeSymbolicFieldElement::hasValue() const
{
    return m_selectedIndex >= 0;
}

int DateTimeSymbolicFieldElement::maximum() const
{
    return m_maximumIndex + 1;
}

int DateTimeSymbolicFieldElement::minimum() const
{
    return m_minimumIndex + 1;
}

void DateTimeSymbolicFieldElement::setEmptyValue(EventBehavior eventBehavior)
{
    if (isReadOnly())
        return;
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
    if (hasValue()) {
        if (!isInRange(--m_selectedIndex))
            m_selectedIndex = m_maximumIndex;
    } else
        m_selectedIndex = m_maximumIndex;
    updateVisibleValue(DispatchEvent);
}

void DateTimeSymbolicFieldElement::stepUp()
{
    if (hasValue()) {
        if (!isInRange(++m_selectedIndex))
            m_selectedIndex = m_minimumIndex;
    } else
        m_selectedIndex = m_minimumIndex;
    updateVisibleValue(DispatchEvent);
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

#endif
