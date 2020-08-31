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

#include "HTMLDivElement.h"

#include <wtf/WeakPtr.h>

namespace WebCore {

class DateComponents;

class DateTimeFieldElement : public HTMLDivElement {
    WTF_MAKE_ISO_ALLOCATED(DateTimeFieldElement);
public:
    class FieldOwner : public CanMakeWeakPtr<FieldOwner> {
    public:
        virtual ~FieldOwner();
        virtual AtomString localeIdentifier() const = 0;
    };

    virtual bool hasValue() const = 0;
    virtual void setEmptyValue() = 0;
    virtual void setValueAsDate(const DateComponents&) = 0;
    virtual void setValueAsInteger(int) = 0;
    virtual String value() const = 0;
    virtual String visibleValue() const = 0;

protected:
    DateTimeFieldElement(Document&, FieldOwner&);
    void initialize(const AtomString& pseudo);
    Locale& localeForOwner() const;
    AtomString localeIdentifier() const;
    void updateVisibleValue();
    virtual int valueAsInteger() const = 0;

private:
    bool supportsFocus() const override;

    WeakPtr<FieldOwner> m_fieldOwner;
};

} // namespace WebCore

#endif // ENABLE(DATE_AND_TIME_INPUT_TYPES)
