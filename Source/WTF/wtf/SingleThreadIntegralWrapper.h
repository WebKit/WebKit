/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#include <wtf/Threading.h>

namespace WTF {

template <typename IntegralType>
class SingleThreadIntegralWrapper {
public:
    SingleThreadIntegralWrapper(IntegralType);

    operator IntegralType() const;
    explicit operator bool() const;
    SingleThreadIntegralWrapper& operator=(IntegralType);
    SingleThreadIntegralWrapper& operator++();
    SingleThreadIntegralWrapper& operator--();

private:
#if ASSERT_ENABLED && !USE(WEB_THREAD)
    void assertThread() const { ASSERT(m_thread.ptr() == &Thread::current()); }
#else
    constexpr void assertThread() const { }
#endif

    IntegralType m_value;
#if ASSERT_ENABLED && !USE(WEB_THREAD)
    Ref<Thread> m_thread;
#endif
};

template <typename IntegralType>
inline SingleThreadIntegralWrapper<IntegralType>::SingleThreadIntegralWrapper(IntegralType value)
    : m_value { value }
#if ASSERT_ENABLED && !USE(WEB_THREAD)
    , m_thread { Thread::current() }
#endif
{ }

template <typename IntegralType>
inline SingleThreadIntegralWrapper<IntegralType>::operator IntegralType() const
{
    assertThread();
    return m_value;
}

template <typename IntegralType>
inline SingleThreadIntegralWrapper<IntegralType>::operator bool() const
{
    assertThread();
    return m_value;
}

template <typename IntegralType>
inline SingleThreadIntegralWrapper<IntegralType>& SingleThreadIntegralWrapper<IntegralType>::operator=(IntegralType value)
{
    assertThread();
    m_value = value;
    return *this;
}

template <typename IntegralType>
inline SingleThreadIntegralWrapper<IntegralType>& SingleThreadIntegralWrapper<IntegralType>::operator++()
{
    assertThread();
    m_value++;
    return *this;
}

template <typename IntegralType>
inline SingleThreadIntegralWrapper<IntegralType>& SingleThreadIntegralWrapper<IntegralType>::operator--()
{
    assertThread();
    m_value--;
    return *this;
}

} // namespace WTF

using WTF::SingleThreadIntegralWrapper;
