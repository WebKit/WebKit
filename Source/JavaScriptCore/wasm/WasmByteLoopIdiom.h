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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(WEBASSEMBLY)

#include "Options.h"
#include "WasmOps.h"
#include "WasmParser.h"

namespace JSC { namespace Wasm {

// Pattern matches the byte-at-a-time copy and fill loops that a toolchain without the bulk memory
// instructions emits for memcpy, memmove and memset, so that they can run as a single memory.copy or
// memory.fill instead of roughly six instructions per byte.
//
// Matching is on raw bytecode rather than on IR, which covers an inlined callee just as well and is
// cheap enough to run at every loop. Only memory 0 at a zero static offset matches, never a
// multi-memory access, and only the three shapes below, exactly; anything else is left alone.
class ByteLoopIdiom {
public:
    enum class Kind : uint8_t {
        // for (; n; --n) mem[d++] = mem[s++];
        CopyForward,
        // for (; n; --n) mem[--d] = mem[--s];
        CopyBackward,
        // for (; n; --n) mem[d++] = v;
        Fill,
    };

    Kind kind;
    uint32_t destinationLocal { 0 };
    // sourceLocal for a copy, the byte to store for a fill.
    uint32_t operandLocal { 0 };
    uint32_t countLocal { 0 };

    bool isFill() const { return kind == Kind::Fill; }
    bool isBackward() const { return kind == Kind::CopyBackward; }

    static std::optional<ByteLoopIdiom> match(std::span<const uint8_t> source, size_t bodyOffset)
    {
        if (!Options::useWasmByteLoopReplacement()) [[unlikely]]
            return std::nullopt;
        if (auto idiom = matchCopyForward(source, bodyOffset))
            return idiom;
        if (auto idiom = matchCopyBackward(source, bodyOffset))
            return idiom;
        return matchFill(source, bodyOffset);
    }

private:
    class Cursor final : public Parser<void> {
    public:
        Cursor(std::span<const uint8_t> source, size_t offset)
            : Parser<void>(source)
        {
            m_offset = offset;
        }

        bool opcode(OpType expected)
        {
            uint8_t byte;
            return parseUInt8(byte) && byte == expected;
        }

        bool opcodeWithImmediate(OpType expected, uint32_t& result)
        {
            return opcode(expected) && parseVarUInt32(result);
        }

        bool constantOne()
        {
            int32_t value;
            return opcode(OpType::I32Const) && parseVarInt32(value) && value == 1;
        }

        // Only a plain access to memory 0 at offset 0 is matched; a memory index follows the
        // alignment when its bit 6 is set.
        bool byteAccess(OpType expected)
        {
            uint32_t alignment;
            uint64_t offset;
            return opcode(expected)
                && parseVarUInt32(alignment)
                && !(alignment & (1 << 6))
                && parseVarUInt64(offset)
                && !offset;
        }

        // local.get x; i32.const 1; <arithmetic>; local.set x  (or local.tee x)
        bool updateLocalByOne(OpType arithmetic, OpType write, uint32_t& local)
        {
            uint32_t read;
            uint32_t written;
            if (!opcodeWithImmediate(OpType::GetLocal, read)
                || !constantOne()
                || !opcode(arithmetic)
                || !opcodeWithImmediate(write, written)
                || read != written)
                return false;
            local = read;
            return true;
        }

        // The loop body must end right here, so that nothing else in it can have an effect.
        bool continueThenEndOfBody()
        {
            uint32_t target;
            return opcodeWithImmediate(OpType::BrIf, target) && !target && opcode(OpType::End);
        }
    };

    bool localsAreDistinct() const
    {
        return destinationLocal != operandLocal && countLocal != destinationLocal && countLocal != operandLocal;
    }

    static std::optional<ByteLoopIdiom> matchCopyForward(std::span<const uint8_t> source, size_t bodyOffset)
    {
        Cursor cursor(source, bodyOffset);
        ByteLoopIdiom idiom { Kind::CopyForward };
        uint32_t firstAdvanced;
        uint32_t secondAdvanced;
        if (!cursor.opcodeWithImmediate(OpType::GetLocal, idiom.destinationLocal)
            || !cursor.opcodeWithImmediate(OpType::GetLocal, idiom.operandLocal)
            || !cursor.byteAccess(OpType::I32Load8U)
            || !cursor.byteAccess(OpType::I32Store8)
            || !cursor.updateLocalByOne(OpType::I32Add, OpType::SetLocal, firstAdvanced)
            || !cursor.updateLocalByOne(OpType::I32Add, OpType::SetLocal, secondAdvanced)
            || !cursor.updateLocalByOne(OpType::I32Sub, OpType::TeeLocal, idiom.countLocal)
            || !cursor.continueThenEndOfBody())
            return std::nullopt;
        // The two pointers may be advanced in either order.
        if (std::minmax(firstAdvanced, secondAdvanced) != std::minmax(idiom.destinationLocal, idiom.operandLocal))
            return std::nullopt;
        if (!idiom.localsAreDistinct())
            return std::nullopt;
        return idiom;
    }

    static std::optional<ByteLoopIdiom> matchCopyBackward(std::span<const uint8_t> source, size_t bodyOffset)
    {
        Cursor cursor(source, bodyOffset);
        ByteLoopIdiom idiom { Kind::CopyBackward };
        if (!cursor.updateLocalByOne(OpType::I32Sub, OpType::TeeLocal, idiom.destinationLocal)
            || !cursor.updateLocalByOne(OpType::I32Sub, OpType::TeeLocal, idiom.operandLocal)
            || !cursor.byteAccess(OpType::I32Load8U)
            || !cursor.byteAccess(OpType::I32Store8)
            || !cursor.updateLocalByOne(OpType::I32Sub, OpType::TeeLocal, idiom.countLocal)
            || !cursor.continueThenEndOfBody()
            || !idiom.localsAreDistinct())
            return std::nullopt;
        return idiom;
    }

    static std::optional<ByteLoopIdiom> matchFill(std::span<const uint8_t> source, size_t bodyOffset)
    {
        Cursor cursor(source, bodyOffset);
        ByteLoopIdiom idiom { Kind::Fill };
        uint32_t advanced;
        if (!cursor.opcodeWithImmediate(OpType::GetLocal, idiom.destinationLocal)
            || !cursor.opcodeWithImmediate(OpType::GetLocal, idiom.operandLocal)
            || !cursor.byteAccess(OpType::I32Store8)
            || !cursor.updateLocalByOne(OpType::I32Add, OpType::SetLocal, advanced)
            || !cursor.updateLocalByOne(OpType::I32Sub, OpType::TeeLocal, idiom.countLocal)
            || !cursor.continueThenEndOfBody()
            || advanced != idiom.destinationLocal
            || !idiom.localsAreDistinct())
            return std::nullopt;
        return idiom;
    }
};

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
