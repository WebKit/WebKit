/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef WTF_FilterIterator_h
#define WTF_FilterIterator_h

#include <wtf/Forward.h>
#include <wtf/PassRefPtr.h>
#include <wtf/Vector.h>

namespace WTF {

template<typename Predicate, typename Cast, typename Iterator>
class FilterIterator {
public:
    FilterIterator(Predicate pred, Cast cast, Iterator begin, Iterator end)
        : m_pred(std::move(pred))
        , m_cast(std::move(cast))
        , m_iter(std::move(begin))
        , m_end(std::move(end))
    {
        while (m_iter != m_end && !m_pred(*m_iter))
            ++m_iter;
    }

    FilterIterator& operator++()
    {
        while (m_iter != m_end) {
            ++m_iter;
            if (m_iter == m_end || m_pred(*m_iter))
                break;
        }
        return *this;
    }

    const decltype(std::declval<Cast>()(*std::declval<Iterator>())) operator*() const
    {
        ASSERT(m_iter != m_end);
        ASSERT(m_pred(*m_iter));
        return m_cast(*m_iter);
    }

    inline bool operator==(FilterIterator& other) const { return m_iter == other.m_iter; }
    inline bool operator!=(FilterIterator& other) const { return m_iter != other.m_iter; }

private:
    const Predicate m_pred;
    const Cast m_cast;
    Iterator m_iter;
    Iterator m_end;
};

} // namespace WTF

#endif // WTF_FilterIterator_h
