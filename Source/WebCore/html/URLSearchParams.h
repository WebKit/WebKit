/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "ScriptExecutionContext.h"
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class DOMURL;
class ScriptExecutionContext;
template<typename> class ExceptionOr;

class URLSearchParams : public RefCounted<URLSearchParams> {
public:
    ~URLSearchParams();

    static ExceptionOr<Ref<URLSearchParams>> create(Variant<Vector<Vector<RefPtr<StringImpl>>>, Vector<KeyValuePair<RefPtr<StringImpl>, RefPtr<StringImpl>>>, RefPtr<StringImpl>>&&);
    static Ref<URLSearchParams> create(StringImpl* string, DOMURL* associatedURL)
    {
        return adoptRef(*new URLSearchParams(string, associatedURL));
    }

    size_t size() const { return m_pairs.size(); }

    void append(StringImpl* name, StringImpl* value);
    void remove(StringImpl* name, StringImpl* value = nullptr);
    NODELETE RefPtr<StringImpl> get(StringImpl* name) const;
    Vector<RefPtr<StringImpl>> getAll(StringImpl* name) const;
    NODELETE bool has(StringImpl* name, StringImpl* value = nullptr) const;
    void set(StringImpl* name, StringImpl* value);
    RefPtr<StringImpl> toString() const;
    void updateFromAssociatedURL();
    void sort();

    class Iterator {
    public:
        explicit Iterator(URLSearchParams&);
        NODELETE std::optional<KeyValuePair<RefPtr<StringImpl>, RefPtr<StringImpl>>> next();

    private:
        const Ref<URLSearchParams> m_target;
        size_t m_index { 0 };
    };
    Iterator createIterator(ScriptExecutionContext*) { return Iterator { *this }; }

private:
    const Vector<KeyValuePair<RefPtr<StringImpl>, RefPtr<StringImpl>>>& pairs() const LIFETIME_BOUND { return m_pairs; }
    URLSearchParams(StringImpl*, DOMURL*);
    URLSearchParams(const Vector<KeyValuePair<RefPtr<StringImpl>, RefPtr<StringImpl>>>&);
    void updateURL();

    WeakPtr<DOMURL> m_associatedURL;
    using PairType = KeyValuePair<RefPtr<StringImpl>, RefPtr<StringImpl>>;
    Vector<PairType> m_pairs;
};

} // namespace WebCore
