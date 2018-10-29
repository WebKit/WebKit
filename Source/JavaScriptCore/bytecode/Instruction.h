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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
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

#include "Opcode.h"
#include "OpcodeSize.h"

namespace JSC {

struct Instruction {

    struct Metadata { };

protected:
    Instruction()
    { }

private:
    template<OpcodeSize Width>
    class Impl {
    public:
        OpcodeID opcodeID() const { return static_cast<OpcodeID>(m_opcode); }

    private:
        typename TypeBySize<Width>::type m_opcode;
    };

public:
    OpcodeID opcodeID() const
    {
        if (isWide())
            return wide()->opcodeID();
        return narrow()->opcodeID();
    }

    const char* name() const
    {
        return opcodeNames[opcodeID()];
    }

    bool isWide() const
    {
        return narrow()->opcodeID() == op_wide;
    }

    size_t size() const
    {
        auto wide = isWide();
        auto padding = wide ? 1 : 0;
        auto size = wide ? 4 : 1;
        return opcodeLengths[opcodeID()] * size + padding;
    }

    template<class T>
    bool is() const
    {
        return opcodeID() == T::opcodeID;
    }

    template<class T>
    T as() const
    {
        ASSERT(is<T>());
        return T::decode(reinterpret_cast<const uint8_t*>(this));
    }

    template<class T>
    T* cast()
    {
        ASSERT(is<T>());
        return reinterpret_cast<T*>(this);
    }

    template<class T>
    const T* cast() const
    {
        ASSERT(is<T>());
        return reinterpret_cast<const T*>(this);
    }

    const Impl<OpcodeSize::Narrow>* narrow() const
    {
        return reinterpret_cast<const Impl<OpcodeSize::Narrow>*>(this);
    }

    const Impl<OpcodeSize::Wide>* wide() const
    {

        ASSERT(isWide());
        return reinterpret_cast<const Impl<OpcodeSize::Wide>*>((uintptr_t)this + 1);
    }
};

} // namespace JSC
