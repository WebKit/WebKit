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
#if ENABLE(INPUT_TYPE_TIME_MULTIPLE_FIELDS)
#include "DateTimeSymbolicFieldElement.h"

#include "KeyboardEvent.h"
#include <wtf/unicode/Unicode.h>

namespace WebCore {

DateTimeSymbolicFieldElement::DateTimeSymbolicFieldElement(Document* document, FieldEventHandler& fieldEventHandler, const Vector<String>& symbols)
    : DateTimeFieldElement(document, fieldEventHandler)
    , m_symbols(symbols)
    , m_selectedIndex(-1)
{
    ASSERT(!symbols.isEmpty());
}

void DateTimeSymbolicFieldElement::handleKeyboardEvent(KeyboardEvent* keyboardEvent)
{
    if (keyboardEvent->type() != eventNames().keypressEvent)
        return;

    const UChar charCode = WTF::Unicode::toLower(keyboardEvent->charCode());
    if (charCode < ' ')
        return;

    keyboardEvent->setDefaultHandled();
    for (unsigned index = 0; index < m_symbols.size(); ++index) {
        if (!m_symbols[index].isEmpty() && WTF::Unicode::toLower(m_symbols[index][0]) == charCode) {
            setValueAsInteger(index, DispatchEvent);
            return;
        }
    }
}

bool DateTimeSymbolicFieldElement::hasValue() const
{
    return m_selectedIndex >= 0;
}

void DateTimeSymbolicFieldElement::setEmptyValue(const DateComponents&, EventBehavior eventBehavior)
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
    const int size = m_symbols.size();
    m_selectedIndex = hasValue() ? (m_selectedIndex + size - 1) % size : size - 1;
    updateVisibleValue(DispatchEvent);
}

void DateTimeSymbolicFieldElement::stepUp()
{
    m_selectedIndex = hasValue() ? (m_selectedIndex + 1) % m_symbols.size() : 0;
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

String DateTimeSymbolicFieldElement::visibleValue() const
{
    return hasValue() ? m_symbols[m_selectedIndex] : "--";
}

} // namespace WebCore

#endif
