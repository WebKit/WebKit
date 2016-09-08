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

#pragma once

#if ENABLE(WEBASSEMBLY)

namespace JSC {

namespace WASM {

#define FOR_EACH_WASM_SPECIAL_OP(macro) \
    macro(I32Const, 0x10, NA)

#define FOR_EACH_WASM_CONTROL_FLOW_OP(macro) \
    macro(Block, 0x01, NA) \
    macro(Return, 0x09, NA) \
    macro(End, 0x0f, NA)

#define FOR_EACH_WASM_UNARY_OP(macro) \
    macro(I32Clz, 0x57, Clz) \
    /* macro(I32Ctz, 0x58) */ \
    /* macro(I32PopCnt, 0x59) */ \
    /* macro(I32Eqz, 0x5a) */ \
    macro(I64Clz, 0x72, Clz) \
    /* macro(I64Ctz, 0x73) */ \
    /* macro(I64PopCnt, 0x74) */ \
    /* macro(I64Eqz, 0xba) */

#define FOR_EACH_WASM_BINARY_OP(macro) \
    macro(I32Add, 0x40, Add) \
    macro(I32Sub, 0x41, Sub) \
    macro(I32Mul, 0x42, Mul) \
    macro(I32DivS, 0x43, Div) \
    /* macro(I32DivU, 0x44) */ \
    macro(I32RemS, 0x45, Mod) \
    /* macro(I32RemU, 0x46, Mod) */ \
    macro(I32And, 0x47, BitAnd) \
    macro(I32Or, 0x48, BitOr) \
    macro(I32Xor, 0x49, BitXor) \
    macro(I32Shl, 0x4a, Shl) \
    macro(I32ShrU, 0x4b, SShr) \
    macro(I32ShrS, 0x4c, ZShr) \
    /* macro(I32RotR, 0xb6) */ \
    /* macro(I32RotL, 0xb7) */ \
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
    /* macro(I64DivU, 0x5f) */ \
    macro(I64RemS, 0x60, Mod) \
    /* macro(I64RemU, 0x61) */ \
    macro(I64And, 0x62, BitAnd) \
    macro(I64Or, 0x63, BitOr) \
    macro(I64Xor, 0x64, BitXor) \
    macro(I64Shl, 0x65, Shl) \
    macro(I64ShrU, 0x66, SShr) \
    macro(I64ShrS, 0x67, ZShr) \
    /* macro(I64RotR, 0xb8) */ \
    /* macro(I64RotL, 0xb9) */ \
    macro(I64Eq, 0x68, Equal) \
    macro(I64Ne, 0x69, NotEqual) \
    macro(I64LtS, 0x6a, LessThan) \
    macro(I64LeS, 0x6b, LessEqual) \
    macro(I64LtU, 0x6c, Below) \
    macro(I64LeU, 0x6d, BelowEqual) \


#define FOR_EACH_WASM_OP(macro) \
    FOR_EACH_WASM_SPECIAL_OP(macro) \
    FOR_EACH_WASM_CONTROL_FLOW_OP(macro) \
    FOR_EACH_WASM_UNARY_OP(macro) \
    FOR_EACH_WASM_BINARY_OP(macro)

#define CREATE_ENUM_VALUE(name, id, b3op) name = id,

enum OpType : uint8_t {
    FOR_EACH_WASM_OP(CREATE_ENUM_VALUE)
};

enum class BinaryOpType : uint8_t {
    FOR_EACH_WASM_BINARY_OP(CREATE_ENUM_VALUE)
};

enum class UnaryOpType : uint8_t {
    FOR_EACH_WASM_UNARY_OP(CREATE_ENUM_VALUE)
};

#undef CREATE_ENUM_VALUE

} // namespace WASM

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
