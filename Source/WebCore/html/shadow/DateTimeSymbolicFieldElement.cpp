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

#include "FontCache.h"
#include "KeyboardEvent.h"
#include "RenderStyle.h"
#include "StyleResolver.h"
#include "TextBreakIterator.h"
#include "TextRun.h"
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

DateTimeSymbolicFieldElement::DateTimeSymbolicFieldElement(Document* document, FieldOwner& fieldOwner, const Vector<String>& symbols)
    : DateTimeFieldElement(document, fieldOwner)
    , m_symbols(symbols)
    , m_visibleEmptyValue(makeVisibleEmptyValue(symbols))
    , m_selectedIndex(-1)
    , m_typeAhead(this)
{
    ASSERT(!symbols.isEmpty());
    setHasCustomCallbacks();
}

PassRefPtr<RenderStyle> DateTimeSymbolicFieldElement::customStyleForRenderer()
{
    FontCachePurgePreventer fontCachePurgePreventer;
    RefPtr<RenderStyle> originalStyle = document()->styleResolver()->styleForElement(this);
    RefPtr<RenderStyle> style = RenderStyle::clone(originalStyle.get());
    float maximumWidth = style->font().width(visibleEmptyValue());
    for (unsigned index = 0; index < m_symbols.size(); ++index)
        maximumWidth = std::max(maximumWidth, style->font().width(m_symbols[index]));
    style->setMinWidth(Length(maximumWidth, Fixed));
    return style.release();
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
    return static_cast<int>(m_symbols.size());
}

int DateTimeSymbolicFieldElement::minimum() const
{
    return 1;
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
