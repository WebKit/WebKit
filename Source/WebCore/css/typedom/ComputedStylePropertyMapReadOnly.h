/*
 * Copyright (C) 2022 Igalia S.L.
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

#include "StylePropertyMapReadOnly.h"

namespace WebCore {
class Element;

class ComputedStylePropertyMapReadOnly final : public StylePropertyMapReadOnly {
public:
    static Ref<ComputedStylePropertyMapReadOnly> create(Element&);

private:
    explicit ComputedStylePropertyMapReadOnly(Element&);

    ExceptionOr<RefPtr<CSSStyleValue>> get(ScriptExecutionContext&, const AtomString&) const final;
    ExceptionOr<Vector<RefPtr<CSSStyleValue>>> getAll(ScriptExecutionContext&, const AtomString&) const final;
    ExceptionOr<bool> has(ScriptExecutionContext&, const AtomString&) const final;
    unsigned size() const final;
    Vector<StylePropertyMapReadOnly::StylePropertyMapEntry> entries(ScriptExecutionContext*) const final;

    Ref<Element> m_element;
};

} // namespace WebCore
