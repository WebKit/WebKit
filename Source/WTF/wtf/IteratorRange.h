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

#pragma once

#include <iterator>

namespace WTF {

template<typename Iterator>
class IteratorRange {
    WTF_MAKE_FAST_ALLOCATED;
public:
    using reverse_iterator = std::reverse_iterator<Iterator>;

    IteratorRange(Iterator begin, Iterator end)
        : m_begin(WTFMove(begin))
        , m_end(WTFMove(end))
    {
    }

    auto begin() const { return m_begin; }
    auto end() const { return m_end; }
    auto rbegin() const { return reverse_iterator { m_end }; }
    auto rend() const { return reverse_iterator { m_begin }; }

private:
    Iterator m_begin;
    Iterator m_end;
};

template<typename Iterator>
IteratorRange<Iterator> makeIteratorRange(Iterator&& begin, Iterator&& end)
{
    return IteratorRange<Iterator>(std::forward<Iterator>(begin), std::forward<Iterator>(end));
}

template<typename Container>
IteratorRange<typename Container::reverse_iterator> makeReversedRange(Container& container)
{
    return makeIteratorRange(std::rbegin(container), std::rend(container));
}

template<typename Container>
IteratorRange<typename Container::const_reverse_iterator> makeReversedRange(const Container& container)
{
    return makeIteratorRange(std::crbegin(container), std::crend(container));
}

template<typename Container, typename Iterator>
class SizedIteratorRange {
    WTF_MAKE_FAST_ALLOCATED;
public:
    SizedIteratorRange(const Container& container, Iterator begin, Iterator end)
        : m_container(container)
        , m_begin(WTFMove(begin))
        , m_end(WTFMove(end))
    {
    }

    auto size() const -> decltype(std::declval<Container>().size()) { return m_container.size(); }
    bool isEmpty() const { return m_container.isEmpty(); }
    Iterator begin() const { return m_begin; }
    Iterator end() const { return m_end; }

private:
    const Container& m_container;
    Iterator m_begin;
    Iterator m_end;
};

template<typename Container, typename Iterator>
SizedIteratorRange<Container, Iterator> makeSizedIteratorRange(const Container& container, Iterator&& begin, Iterator&& end)
{
    return SizedIteratorRange<Container, Iterator>(container, std::forward<Iterator>(begin), std::forward<Iterator>(end));
}

} // namespace WTF

using WTF::IteratorRange;
using WTF::makeReversedRange;
