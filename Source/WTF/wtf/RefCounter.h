/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef RefCounter_h
#define RefCounter_h

#include <functional>
#include <wtf/Noncopyable.h>
#include <wtf/RefPtr.h>

namespace WTF {

class RefCounter {
    WTF_MAKE_NONCOPYABLE(RefCounter);

    class Count {
        WTF_MAKE_NONCOPYABLE(Count);
    public:
        WTF_EXPORT_PRIVATE void ref();
        WTF_EXPORT_PRIVATE void deref();

    private:
        friend class RefCounter;

        Count(RefCounter& refCounter)
            : m_refCounter(&refCounter)
            , m_value(0)
        {
        }

        RefCounter* m_refCounter;
        unsigned m_value;
    };

public:
    template<typename T>
    class Token  {
    public:
        Token() { }
        Token(std::nullptr_t) { }
        inline Token(const Token<T>&);
        inline Token(Token<T>&&);

        inline Token<T>& operator=(std::nullptr_t);
        inline Token<T>& operator=(const Token<T>&);
        inline Token<T>& operator=(Token<T>&&);

        explicit operator bool() const { return m_ptr; }

    private:
        friend class RefCounter;
        inline Token(Count* count);

        RefPtr<Count> m_ptr;
    };

    WTF_EXPORT_PRIVATE RefCounter(std::function<void(bool)> = [](bool) { });
    WTF_EXPORT_PRIVATE ~RefCounter();

    template<typename T>
    Token<T> token() const
    {
        return Token<T>(m_count);
    }

    unsigned value() const
    {
        return m_count->m_value;
    }

private:
    std::function<void(bool)> m_valueDidChange;
    Count* m_count;
};

template<class T>
inline RefCounter::Token<T>::Token(Count* count)
    : m_ptr(count)
{
}

template<class T>
inline RefCounter::Token<T>::Token(const RefCounter::Token<T>& token)
    : m_ptr(token.m_ptr)
{
}

template<class T>
inline RefCounter::Token<T>::Token(RefCounter::Token<T>&& token)
    : m_ptr(token.m_ptr)
{
}

template<class T>
inline RefCounter::Token<T>& RefCounter::Token<T>::operator=(std::nullptr_t)
{
    m_ptr = nullptr;
    return *this;
}

template<class T>
inline RefCounter::Token<T>& RefCounter::Token<T>::operator=(const RefCounter::Token<T>& token)
{
    m_ptr = token.m_ptr;
    return *this;
}

template<class T>
inline RefCounter::Token<T>& RefCounter::Token<T>::operator=(RefCounter::Token<T>&& token)
{
    m_ptr = token.m_ptr;
    return *this;
}

} // namespace WTF

using WTF::RefCounter;

#endif // RefCounter_h
