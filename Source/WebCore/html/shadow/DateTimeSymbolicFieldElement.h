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

#pragma once

#if ENABLE(DATE_AND_TIME_INPUT_TYPES)

#include "DateTimeFieldElement.h"

namespace WebCore {

class DateTimeSymbolicFieldElement : public DateTimeFieldElement {
    WTF_MAKE_ISO_ALLOCATED(DateTimeSymbolicFieldElement);
protected:
    DateTimeSymbolicFieldElement(Document&, FieldOwner&, const Vector<String>&);
    size_t symbolsSize() const { return m_symbols.size(); }
    bool hasValue() const final;
    void initialize(const AtomString& pseudo);
    void setEmptyValue() final;
    void setValueAsInteger(int) final;
    int valueAsInteger() const final;

private:
    static constexpr int invalidIndex = -1;

    String visibleEmptyValue() const;

    // DateTimeFieldElement functions:
    String value() const final;
    String visibleValue() const final;

    const Vector<String> m_symbols;

    const AtomString m_visibleEmptyValue;
    int m_selectedIndex { invalidIndex };
};

} // namespace WebCore

#endif // ENABLE(DATE_AND_TIME_INPUT_TYPES)
