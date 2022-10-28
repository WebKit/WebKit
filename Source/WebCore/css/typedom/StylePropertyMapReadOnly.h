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

#include "CSSStyleValue.h"
#include "CSSValue.h"
#include <wtf/RefCounted.h>
#include <wtf/UniqueRef.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class Document;
class Element;
class ScriptExecutionContext;
class StyledElement;

class StylePropertyMapReadOnly : public RefCounted<StylePropertyMapReadOnly> {
public:
    using StylePropertyMapEntry = KeyValuePair<String, Vector<RefPtr<CSSStyleValue>>>;
    class Iterator {
    public:
        explicit Iterator(StylePropertyMapReadOnly&, ScriptExecutionContext*);
        std::optional<StylePropertyMapEntry> next();

    private:
        Vector<StylePropertyMapEntry> m_values;
        size_t m_index { 0 };
    };
    Iterator createIterator(ScriptExecutionContext* context) { return Iterator(*this, context); }

    virtual ~StylePropertyMapReadOnly() = default;
    virtual ExceptionOr<RefPtr<CSSStyleValue>> get(ScriptExecutionContext&, const AtomString& property) const = 0;
    virtual ExceptionOr<Vector<RefPtr<CSSStyleValue>>> getAll(ScriptExecutionContext&, const AtomString&) const = 0;
    virtual ExceptionOr<bool> has(ScriptExecutionContext&, const AtomString&) const = 0;
    virtual unsigned size() const = 0;

    static RefPtr<CSSStyleValue> reifyValue(RefPtr<CSSValue>&&, Document&);
    static Vector<RefPtr<CSSStyleValue>> reifyValueToVector(RefPtr<CSSValue>&&, Document&);

protected:
    virtual Vector<StylePropertyMapEntry> entries(ScriptExecutionContext*) const = 0;
};

} // namespace WebCore
