/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "MainThreadStylePropertyMapReadOnly.h"

namespace WebCore {

class CSSVariableReferenceValue;

class StylePropertyMap : public MainThreadStylePropertyMapReadOnly {
public:
    ExceptionOr<void> set(Document&, const AtomString& property, FixedVector<std::variant<RefPtr<CSSStyleValue>, String>>&& values);
    ExceptionOr<void> append(Document&, const AtomString& property, FixedVector<std::variant<RefPtr<CSSStyleValue>, String>>&& values);
    ExceptionOr<void> remove(Document&, const AtomString& property);
    virtual void clear() = 0;

protected:
    virtual void removeProperty(CSSPropertyID) = 0;
    virtual void removeCustomProperty(const AtomString&) = 0;
    virtual bool setShorthandProperty(CSSPropertyID, const String&) = 0;
    virtual bool setProperty(CSSPropertyID, Ref<CSSValue>&&) = 0;
    virtual bool setCustomProperty(Document&, const AtomString&, Ref<CSSVariableReferenceValue>&&) = 0;

private:
    RefPtr<CSSStyleValue> shorthandPropertyValue(Document&, CSSPropertyID) const;
};

} // namespace WebCore
