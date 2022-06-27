/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include <wtf/Vector.h>

namespace IPC {

template <typename... Types> class ArrayReferenceTuple;

template<typename T0, typename T1>
class ArrayReferenceTuple<T0, T1> {
public:
    ArrayReferenceTuple() = default;
    ArrayReferenceTuple(const T0* data0, const T1* data1, size_t size)
        : m_size(size)
        , m_data0(size ? data0 : nullptr)
        , m_data1(size ? data1 : nullptr)
    {
    }
    bool isEmpty() const { return !m_size; }
    size_t size() const { return m_size; }
    template<int I>
    auto data() const
    {
        if constexpr(I)
            return m_data1;
        else if constexpr(!I)
            return m_data0;
    }
private:
    size_t m_size { 0 };
    const T0* m_data0 { nullptr };
    const T1* m_data1 { nullptr };
};

template<typename T0, typename T1, typename T2>
class ArrayReferenceTuple<T0, T1, T2> : public ArrayReferenceTuple<T0, T1> {
public:
    ArrayReferenceTuple() = default;
    ArrayReferenceTuple(const T0* data0, const T1* data1, const T2* data2, size_t size)
        : ArrayReferenceTuple<T0, T1>(data0, data1, size)
        , m_data2(size ? data2 : nullptr)
    {
    }
    template<int I>
    auto data() const
    {
        if constexpr(I == 2)
            return m_data2;
        else
            return ArrayReferenceTuple<T0, T1>::template data<I>();
    }
private:
    const T2* m_data2 { nullptr };
};

template<typename T0, typename T1, typename T2, typename T3>
class ArrayReferenceTuple<T0, T1, T2, T3> : public ArrayReferenceTuple<T0, T1, T2> {
public:
    ArrayReferenceTuple() = default;
    ArrayReferenceTuple(const T0* data0, const T1* data1, const T2* data2, const T3* data3, size_t size)
        : ArrayReferenceTuple<T0, T1, T2>(data0, data1, data2, size)
        , m_data3(size ? data3 : nullptr)
    {
    }
    template<int I>
    auto data() const
    {
        if constexpr(I == 3)
            return m_data3;
        else
            return ArrayReferenceTuple<T0, T1, T2>::template data<I>();
    }
private:
    const T3* m_data3 { nullptr };
};

template<typename T0, typename T1, typename T2, typename T3, typename T4>
class ArrayReferenceTuple<T0, T1, T2, T3, T4> : public ArrayReferenceTuple<T0, T1, T2, T3> {
public:
    ArrayReferenceTuple() = default;
    ArrayReferenceTuple(const T0* data0, const T1* data1, const T2* data2, const T3* data3, const T4* data4, size_t size)
        : ArrayReferenceTuple<T0, T1, T2, T3>(data0, data1, data2, data3, size)
        , m_data4(size ? data4 : nullptr)
    {
    }
    template<int I>
    auto data() const
    {
        if constexpr(I == 4)
            return m_data4;
        else
            return ArrayReferenceTuple<T0, T1, T2, T3>::template data<I>();
    }
private:
    const T4* m_data4 { nullptr };
};

template<typename T0, typename T1>
ArrayReferenceTuple(const T0*, const T1*, size_t) -> ArrayReferenceTuple<T0, T1>;

template<typename T0, typename T1, typename T2>
ArrayReferenceTuple(const T0*, const T1*, const T2*, size_t) -> ArrayReferenceTuple<T0, T1, T2>;

template<typename T0, typename T1, typename T2, typename T3>
ArrayReferenceTuple(const T0*, const T1*, const T2*, const T3*, size_t) -> ArrayReferenceTuple<T0, T1, T2, T3>;

template<typename T0, typename T1, typename T2, typename T3, typename T4>
ArrayReferenceTuple(const T0*, const T1*, const T2*, const T3*, const T4*, size_t) -> ArrayReferenceTuple<T0, T1, T2, T3, T4>;

}
