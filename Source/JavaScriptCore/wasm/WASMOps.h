/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

// This file is auto-generated using wasm.json.

#pragma once

#if ENABLE(WEBASSEMBLY)

#include <cstdint>

namespace JSC { namespace WASM {

#define FOR_EACH_WASM_SPECIAL_OP(macro) \
    macro(I32Const, 0x10, Oops) \
    macro(I64Const, 0x11, Oops) \
    macro(F64Const, 0x12, Oops) \
    macro(F32Const, 0x13, Oops) \
    macro(GetLocal, 0x14, Oops) \
    macro(SetLocal, 0x15, Oops) \
    macro(TeeLocal, 0x19, Oops) \
    macro(GetGlobal, 0xbb, Oops) \
    macro(SetGlobal, 0xbc, Oops)

#define FOR_EACH_WASM_CONTROL_FLOW_OP(macro) \
    macro(Unreachable, 0x0, Oops) \
    macro(Block, 0x1, Oops) \
    macro(Loop, 0x2, Oops) \
    macro(If, 0x3, Oops) \
    macro(Else, 0x4, Oops) \
    macro(Select, 0x5, Oops) \
    macro(Br, 0x6, Oops) \
    macro(BrIf, 0x7, Oops) \
    macro(BrTable, 0x8, Oops) \
    macro(Return, 0x9, Oops) \
    macro(Drop, 0xb, Oops) \
    macro(Nop, 0xa, Oops) \
    macro(End, 0xf, Oops)

#define FOR_EACH_WASM_UNARY_OP(macro) \
    macro(I32Clz, 0x57, Clz) \
    macro(I32Ctz, 0x58, Oops) \
    macro(I32Popcnt, 0x59, Oops) \
    macro(I64Clz, 0x72, Clz) \
    macro(I64Ctz, 0x73, Oops) \
    macro(I64Popcnt, 0x74, Oops) \
    macro(F32Abs, 0x7b, Oops) \
    macro(F32Neg, 0x7c, Oops) \
    macro(F32Copysign, 0x7d, Oops) \
    macro(F32Ceil, 0x7e, Oops) \
    macro(F32Floor, 0x7f, Oops) \
    macro(F32Trunc, 0x80, Oops) \
    macro(F32Nearest, 0x81, Oops) \
    macro(F32Sqrt, 0x82, Oops) \
    macro(F64Abs, 0x8f, Oops) \
    macro(F64Neg, 0x90, Oops) \
    macro(F64Copysign, 0x91, Oops) \
    macro(F64Ceil, 0x92, Oops) \
    macro(F64Floor, 0x93, Oops) \
    macro(F64Trunc, 0x94, Oops) \
    macro(F64Nearest, 0x95, Oops) \
    macro(F64Sqrt, 0x96, Oops)

#define FOR_EACH_WASM_BINARY_OP(macro) \
    macro(I32Add, 0x40, Add) \
    macro(I32Sub, 0x41, Sub) \
    macro(I32Mul, 0x42, Mul) \
    macro(I32DivS, 0x43, Div) \
    macro(I32DivU, 0x44, Oops) \
    macro(I32RemS, 0x45, Mod) \
    macro(I32RemU, 0x46, Oops) \
    macro(I32And, 0x47, BitAnd) \
    macro(I32Or, 0x48, BitOr) \
    macro(I32Xor, 0x49, BitXor) \
    macro(I32Shl, 0x4a, Shl) \
    macro(I32ShrU, 0x4b, SShr) \
    macro(I32ShrS, 0x4c, ZShr) \
    macro(I32Rotr, 0xb6, Oops) \
    macro(I32Rotl, 0xb7, Oops) \
    macro(I32Eq, 0x4d, Equal) \
    macro(I32Ne, 0x4e, NotEqual) \
    macro(I32LtS, 0x4f, LessThan) \
    macro(I32LeS, 0x50, LessEqual) \
    macro(I32LtU, 0x51, Below) \
    macro(I32LeU, 0x52, BelowEqual) \
    macro(I32GtS, 0x53, GreaterThan) \
    macro(I32GeS, 0x54, GreaterEqual) \
    macro(I32GtU, 0x55, Above) \
    macro(I32GeU, 0x56, AboveEqual) \
    macro(I64Add, 0x5b, Add) \
    macro(I64Sub, 0x5c, Sub) \
    macro(I64Mul, 0x5d, Mul) \
    macro(I64DivS, 0x5e, Div) \
    macro(I64DivU, 0x5f, Oops) \
    macro(I64RemS, 0x60, Mod) \
    macro(I64RemU, 0x61, Oops) \
    macro(I64And, 0x62, BitAnd) \
    macro(I64Or, 0x63, BitOr) \
    macro(I64Xor, 0x64, BitXor) \
    macro(I64Shl, 0x65, Shl) \
    macro(I64ShrU, 0x66, SShr) \
    macro(I64ShrS, 0x67, ZShr) \
    macro(I64Rotr, 0xb8, Oops) \
    macro(I64Rotl, 0xb9, Oops) \
    macro(I64Eq, 0x68, Equal) \
    macro(I64Ne, 0x69, NotEqual) \
    macro(I64LtS, 0x6a, LessThan) \
    macro(I64LeS, 0x6b, LessEqual) \
    macro(I64LtU, 0x6c, Below) \
    macro(I64LeU, 0x6d, BelowEqual) \
    macro(I64GtS, 0x6e, GreaterThan) \
    macro(I64GeS, 0x6f, GreaterEqual) \
    macro(I64GtU, 0x70, Above) \
    macro(I64GeU, 0x71, AboveEqual) \
    macro(F32Add, 0x75, Oops) \
    macro(F32Sub, 0x76, Oops) \
    macro(F32Mul, 0x77, Oops) \
    macro(F32Div, 0x78, Oops) \
    macro(F32Min, 0x79, Oops) \
    macro(F32Max, 0x7a, Oops) \
    macro(F32Eq, 0x83, Oops) \
    macro(F32Ne, 0x84, Oops) \
    macro(F32Lt, 0x85, Oops) \
    macro(F32Le, 0x86, Oops) \
    macro(F32Gt, 0x87, Oops) \
    macro(F32Ge, 0x88, Oops) \
    macro(F64Add, 0x89, Oops) \
    macro(F64Sub, 0x8a, Oops) \
    macro(F64Mul, 0x8b, Oops) \
    macro(F64Div, 0x8c, Oops) \
    macro(F64Min, 0x8d, Oops) \
    macro(F64Max, 0x8e, Oops) \
    macro(F64Eq, 0x97, Oops) \
    macro(F64Ne, 0x98, Oops) \
    macro(F64Lt, 0x99, Oops) \
    macro(F64Le, 0x9a, Oops) \
    macro(F64Gt, 0x9b, Oops) \
    macro(F64Ge, 0x9c, Oops)

#define FOR_EACH_WASM_MEMORY_LOAD_OP(macro) \
    macro(I32Load8S, 0x20, Oops) \
    macro(I32Load8U, 0x21, Oops) \
    macro(I32Load16S, 0x22, Oops) \
    macro(I32Load16U, 0x23, Oops) \
    macro(I64Load8S, 0x24, Oops) \
    macro(I64Load8U, 0x25, Oops) \
    macro(I64Load16S, 0x26, Oops) \
    macro(I64Load16U, 0x27, Oops) \
    macro(I64Load32S, 0x28, Oops) \
    macro(I64Load32U, 0x29, Oops) \
    macro(I32Load, 0x2a, Oops) \
    macro(I64Load, 0x2b, Oops) \
    macro(F32Load, 0x2c, Oops) \
    macro(F64Load, 0x2d, Oops)

#define FOR_EACH_WASM_MEMORY_STORE_OP(macro) \
    macro(I32Store8, 0x2e, Oops) \
    macro(I32Store16, 0x2f, Oops) \
    macro(I64Store8, 0x30, Oops) \
    macro(I64Store16, 0x31, Oops) \
    macro(I64Store32, 0x32, Oops) \
    macro(I32Store, 0x33, Oops) \
    macro(I64Store, 0x34, Oops) \
    macro(F32Store, 0x35, Oops) \
    macro(F64Store, 0x36, Oops)



#define FOR_EACH_WASM_OP(macro) \
    FOR_EACH_WASM_SPECIAL_OP(macro) \
    FOR_EACH_WASM_CONTROL_FLOW_OP(macro) \
    FOR_EACH_WASM_UNARY_OP(macro) \
    FOR_EACH_WASM_BINARY_OP(macro) \
    FOR_EACH_WASM_MEMORY_LOAD_OP(macro) \
    FOR_EACH_WASM_MEMORY_STORE_OP(macro)

#define CREATE_ENUM_VALUE(name, id, b3op) name = id,

enum OpType : uint8_t {
    FOR_EACH_WASM_OP(CREATE_ENUM_VALUE)
};

template<typename Int>
inline bool isValidOpType(Int i)
{
    // Bitset of valid ops.
    static const uint8_t valid[] = { 0xff, 0x8f, 0xff, 0x2, 0xff, 0xff, 0x7f, 0xa, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x1f };
    return 0 <= i && i <= 188 && (valid[i / 8] & (1 << (i % 8)));
}

enum class BinaryOpType : uint8_t {
    FOR_EACH_WASM_BINARY_OP(CREATE_ENUM_VALUE)
};

enum class UnaryOpType : uint8_t {
    FOR_EACH_WASM_UNARY_OP(CREATE_ENUM_VALUE)
};

enum class LoadOpType : uint8_t {
    FOR_EACH_WASM_MEMORY_LOAD_OP(CREATE_ENUM_VALUE)
};

enum class StoreOpType : uint8_t {
    FOR_EACH_WASM_MEMORY_STORE_OP(CREATE_ENUM_VALUE)
};

#undef CREATE_ENUM_VALUE

inline bool isControlOp(OpType op)
{
    switch (op) {
#define CREATE_CASE(name, id, b3op) case OpType::name:
    FOR_EACH_WASM_CONTROL_FLOW_OP(CREATE_CASE)
        return true;
#undef CREATE_CASE
    default:
        break;
    }
    return false;
}

} } // namespace JSC::WASM

#endif // ENABLE(WEBASSEMBLY)

