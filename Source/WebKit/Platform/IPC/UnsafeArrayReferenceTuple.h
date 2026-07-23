/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include "ArrayReferenceTuple.h"

namespace IPC {

// Like ArrayReferenceTuple, except that stream IPC receivers will have spans
// pointing into the stream IPC message buffer. These spans are susceptible to
// TOCTOU bugs, since the stream IPC message buffer may be modified after the
// receiver has validated the received data.
//
// UnsafeArrayReferenceTuple must only be used in cases where no validation need
// be performed on the received data.
template<typename... Types>
class UnsafeArrayReferenceTuple {
public:
    UnsafeArrayReferenceTuple() = default;

    UnsafeArrayReferenceTuple(const Types*... data, size_t size)
        : m_tuple(data..., size)
    {
    }

    UnsafeArrayReferenceTuple(ArrayReferenceTuple<Types...> tuple)
        : m_tuple(tuple)
    {
    }

    ArrayReferenceTuple<Types...> tuple() const { return m_tuple; }

    bool isEmpty() const { return m_tuple.isEmpty(); }
    size_t size() const { return m_tuple.size(); }

    template<unsigned I>
    auto data() const
    {
        return m_tuple.template data<I>();
    }

    template<unsigned I>
    auto span() const
    {
        return m_tuple.template span<I>();
    }

private:
    ArrayReferenceTuple<Types...> m_tuple;
};

}
