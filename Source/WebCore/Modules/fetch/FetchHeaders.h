/*
 * Copyright (C) 2016 Canon Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted, provided that the following conditions
 * are required to be met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Canon Inc. nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY CANON INC. AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL CANON INC. AND ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef FetchHeaders_h
#define FetchHeaders_h

#if ENABLE(FETCH_API)

#include "HTTPHeaderMap.h"

namespace WebCore {

typedef int ExceptionCode;

class FetchHeaders : public RefCounted<FetchHeaders> {
public:
    enum class Guard {
        None,
        Immutable,
        Request,
        RequestNoCors,
        Response
    };

    static Ref<FetchHeaders> create(Guard guard = Guard::None) { return adoptRef(*new FetchHeaders(guard)); }
    static Ref<FetchHeaders> create(const FetchHeaders& headers) { return adoptRef(*new FetchHeaders(headers.m_guard, headers.m_headers)); }

    void append(const String& name, const String& value, ExceptionCode&);
    void remove(const String&, ExceptionCode&);
    String get(const String&, ExceptionCode&) const;
    bool has(const String&, ExceptionCode&) const;
    void set(const String& name, const String& value, ExceptionCode&);

    void initializeWith(const FetchHeaders*, ExceptionCode&);
    void fill(const FetchHeaders*);

    String fastGet(HTTPHeaderName name) const { return m_headers.get(name); }
    void fastSet(HTTPHeaderName name, const String& value) { m_headers.set(name, value); }

    class Iterator {
    public:
        explicit Iterator(FetchHeaders&);
        bool next(String& nextKey, String& nextValue);

    private:
        Ref<FetchHeaders> m_headers;
        size_t m_currentIndex = 0;
        Vector<String> m_keys;
    };
    Iterator createIterator() { return Iterator(*this); }

private:
    FetchHeaders(Guard guard) : m_guard(guard) { }
    FetchHeaders(Guard guard, const HTTPHeaderMap& headers) : m_guard(guard), m_headers(headers) { }

    Guard m_guard;
    HTTPHeaderMap m_headers;
};

} // namespace WebCore

#endif // ENABLE(FETCH_API)

#endif // FetchHeaders_h
