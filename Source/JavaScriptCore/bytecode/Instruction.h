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

struct JSOpcodeTraits {
    using OpcodeID = ::JSC::OpcodeID;
    static constexpr OpcodeID numberOfBytecodesWithCheckpoints = static_cast<OpcodeID>(NUMBER_OF_BYTECODE_WITH_CHECKPOINTS);
    static constexpr OpcodeID numberOfBytecodesWithMetadata = static_cast<OpcodeID>(NUMBER_OF_BYTECODE_WITH_METADATA);
    static_assert(numberOfBytecodesWithCheckpoints <= numberOfBytecodesWithMetadata);
    static constexpr OpcodeID wide16 = op_wide16;
    static constexpr OpcodeID wide32 = op_wide32;
    static constexpr const unsigned* opcodeLengths = ::JSC::opcodeLengths;
    static constexpr const char* const* opcodeNames = ::JSC::opcodeNames;
    static constexpr auto checkpointCountTable = bytecodeCheckpointCountTable;
};

struct WasmOpcodeTraits {
    using OpcodeID = WasmOpcodeID;
    static constexpr OpcodeID numberOfBytecodesWithCheckpoints = static_cast<OpcodeID>(NUMBER_OF_WASM_WITH_CHECKPOINTS);
    static constexpr OpcodeID numberOfBytecodesWithMetadata = static_cast<OpcodeID>(NUMBER_OF_WASM_WITH_METADATA);
    static_assert(numberOfBytecodesWithCheckpoints <= numberOfBytecodesWithMetadata);
    static constexpr OpcodeID wide16 = wasm_wide16;
    static constexpr OpcodeID wide32 = wasm_wide32;
    static constexpr const unsigned* opcodeLengths = wasmOpcodeLengths;
    static constexpr const char* const* opcodeNames = wasmOpcodeNames;
    static constexpr auto checkpointCountTable = wasmCheckpointCountTable;
};


template<typename Opcode>
struct BaseInstruction {

    struct Metadata { };

protected:
    BaseInstruction()
    { }

private:
    template<OpcodeSize Width>
    class Impl {
    public:
        template<typename Traits = JSOpcodeTraits>
        typename Traits::OpcodeID opcodeID() const { return static_cast<typename Traits::OpcodeID>(m_opcode); }

    private:
        typename TypeBySize<OpcodeSize::Narrow>::unsignedType m_opcode;
    };

public:
    template<typename Traits = JSOpcodeTraits>
    typename Traits::OpcodeID opcodeID() const
    {
        if (isWide32<Traits>())
            return wide32<Traits>()->template opcodeID<Traits>();
        if (isWide16<Traits>())
            return wide16<Traits>()->template opcodeID<Traits>();
        return narrow()->template opcodeID<Traits>();
    }

    template<typename Traits = JSOpcodeTraits>
    const char* name() const
    {
        return Traits::opcodeNames[opcodeID()];
    }

    template<typename Traits = JSOpcodeTraits>
    bool isWide16() const
    {
        return narrow()->template opcodeID<Traits>() == Traits::wide16;
    }

    template<typename Traits = JSOpcodeTraits>
    bool isWide32() const
    {
        return narrow()->template opcodeID<Traits>() == Traits::wide32;
    }

    template<typename Traits = JSOpcodeTraits>
    OpcodeSize width() const
    {
        if (isWide32<Traits>())
            return OpcodeSize::Wide32;
        return isWide16<Traits>() ? OpcodeSize::Wide16 : OpcodeSize::Narrow;
    }

    template<typename Traits = JSOpcodeTraits>
    bool hasMetadata() const
    {
        return opcodeID<Traits>() < Traits::numberOfBytecodesWithMetadata;
    }

    template<typename Traits = JSOpcodeTraits>
    bool hasCheckpoints() const
    {
        return opcodeID<Traits>() < Traits::numberOfBytecodesWithCheckpoints;
    }

    template<typename Traits = JSOpcodeTraits>
    unsigned numberOfCheckpoints() const
    {
        if (!hasCheckpoints<Traits>())
            return 1;
        return Traits::checkpointCountTable[opcodeID<Traits>()];
    }

    template<typename Traits = JSOpcodeTraits>
    int sizeShiftAmount() const
    {
        if (isWide32<Traits>())
            return 2;
        if (isWide16<Traits>())
            return 1;
        return 0;
    }

    template<typename Traits = JSOpcodeTraits>
    size_t size() const
    {
        auto sizeShiftAmount = this->sizeShiftAmount<Traits>();
        size_t prefixSize = sizeShiftAmount ? 1 : 0;
        size_t operandSize = static_cast<size_t>(1) << sizeShiftAmount;
        size_t sizeOfBytecode = 1;
        return sizeOfBytecode + (Traits::opcodeLengths[opcodeID<Traits>()] - 1) * operandSize + prefixSize;
    }

    template<class T, typename Traits = JSOpcodeTraits>
    bool is() const
    {
        return opcodeID<Traits>() == T::opcodeID;
    }

    template<class T, typename Traits = JSOpcodeTraits>
    T as() const
    {
        ASSERT((is<T, Traits>()));
        return T::decode(reinterpret_cast<const uint8_t*>(this));
    }

    template<class T, OpcodeSize width, typename Traits = JSOpcodeTraits>
    T asKnownWidth() const
    {
        ASSERT((is<T, Traits>()));
        return T(reinterpret_cast<const typename TypeBySize<width>::unsignedType*>(this + (width == OpcodeSize::Narrow ? 1 : 2)));
    }

    template<class T, typename Traits = JSOpcodeTraits>
    T* cast()
    {
        ASSERT((is<T, Traits>()));
        return bitwise_cast<T*>(this);
    }

    template<class T, typename Traits = JSOpcodeTraits>
    const T* cast() const
    {
        ASSERT((is<T, Traits>()));
        return reinterpret_cast<const T*>(this);
    }

    const Impl<OpcodeSize::Narrow>* narrow() const
    {
        return reinterpret_cast<const Impl<OpcodeSize::Narrow>*>(this);
    }

    template<typename Traits = JSOpcodeTraits>
    const Impl<OpcodeSize::Wide16>* wide16() const
    {

        ASSERT(isWide16<Traits>());
        return reinterpret_cast<const Impl<OpcodeSize::Wide16>*>(this + 1);
    }

    template<typename Traits = JSOpcodeTraits>
    const Impl<OpcodeSize::Wide32>* wide32() const
    {

        ASSERT(isWide32<Traits>());
        return reinterpret_cast<const Impl<OpcodeSize::Wide32>*>(this + 1);
    }
};

struct Instruction : public BaseInstruction<OpcodeID> {
};
static_assert(sizeof(Instruction) == 1, "So pointer math is the same as byte math");

struct WasmInstance : public BaseInstruction<WasmOpcodeID> {
};
static_assert(sizeof(WasmInstance) == 1, "So pointer math is the same as byte math");

} // namespace JSC
