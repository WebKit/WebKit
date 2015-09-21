/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
 *
 * =========================================================================
 *
 * Copyright (c) 2015 by the repository authors of
 * WebAssembly/polyfill-prototype-1.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef WASMConstants_h
#define WASMConstants_h

#if ENABLE(WEBASSEMBLY)

namespace JSC {

static const uint32_t wasmMagicNumber = 0x6d736177;

enum class WASMOpKind {
    Statement,
    Expression
};

enum class WASMOpStatement : uint8_t {
    SetLocal,
    SetGlobal,
    I32Store8,
    I32StoreWithOffset8,
    I32Store16,
    I32StoreWithOffset16,
    I32Store32,
    I32StoreWithOffset32,
    F32Store,
    F32StoreWithOffset,
    F64Store,
    F64StoreWithOffset,
    CallInternal,
    CallIndirect,
    CallImport,
    Return,
    Block,
    If,
    IfElse,
    While,
    Do,
    Label,
    Break,
    BreakLabel,
    Continue,
    ContinueLabel,
    Switch,
    NumberOfWASMOpStatements
};

enum class WASMOpStatementWithImmediate : uint8_t {
    SetLocal,
    SetGlobal,
    NumberOfWASMOpStatementWithImmediates
};

enum class WASMOpExpressionI32 : uint8_t {
    ConstantPoolIndex,
    Immediate,
    GetLocal,
    GetGlobal,
    SetLocal,
    SetGlobal,
    SLoad8,
    SLoadWithOffset8,
    ULoad8,
    ULoadWithOffset8,
    SLoad16,
    SLoadWithOffset16,
    ULoad16,
    ULoadWithOffset16,
    Load32,
    LoadWithOffset32,
    Store8,
    StoreWithOffset8,
    Store16,
    StoreWithOffset16,
    Store32,
    StoreWithOffset32,
    CallInternal,
    CallIndirect,
    CallImport,
    Conditional,
    Comma,
    FromF32,
    FromF64,
    Negate,
    Add,
    Sub,
    Mul,
    SDiv,
    UDiv,
    SMod,
    UMod,
    BitNot,
    BitOr,
    BitAnd,
    BitXor,
    LeftShift,
    ArithmeticRightShift,
    LogicalRightShift,
    CountLeadingZeros,
    LogicalNot,
    EqualI32,
    EqualF32,
    EqualF64,
    NotEqualI32,
    NotEqualF32,
    NotEqualF64,
    SLessThanI32,
    ULessThanI32,
    LessThanF32,
    LessThanF64,
    SLessThanOrEqualI32,
    ULessThanOrEqualI32,
    LessThanOrEqualF32,
    LessThanOrEqualF64,
    SGreaterThanI32,
    UGreaterThanI32,
    GreaterThanF32,
    GreaterThanF64,
    SGreaterThanOrEqualI32,
    UGreaterThanOrEqualI32,
    GreaterThanOrEqualF32,
    GreaterThanOrEqualF64,
    SMin,
    UMin,
    SMax,
    UMax,
    Abs,
    NumberOfWASMOpExpressionI32s
};

enum class WASMOpExpressionI32WithImmediate : uint8_t {
    ConstantPoolIndex,
    Immediate,
    GetLocal,
    NumberOfWASMOpExpressionI32WithImmediates
};

enum class WASMOpExpressionF32 : uint8_t {
    ConstantPoolIndex,
    Immediate,
    GetLocal,
    GetGlobal,
    SetLocal,
    SetGlobal,
    Load,
    LoadWithOffset,
    Store,
    StoreWithOffset,
    CallInternal,
    CallIndirect,
    Conditional,
    Comma,
    FromS32,
    FromU32,
    FromF64,
    Negate,
    Add,
    Sub,
    Mul,
    Div,
    Abs,
    Ceil,
    Floor,
    Sqrt,
    NumberOfWASMOpExpressionF32s
};

enum class WASMOpExpressionF32WithImmediate : uint8_t {
    ConstantPoolIndex,
    GetLocal,
    NumberOfWASMOpExpressionF32WithImmediates
};

enum class WASMOpExpressionF64 : uint8_t {
    ConstantPoolIndex,
    Immediate,
    GetLocal,
    GetGlobal,
    SetLocal,
    SetGlobal,
    Load,
    LoadWithOffset,
    Store,
    StoreWithOffset,
    CallInternal,
    CallIndirect,
    CallImport,
    Conditional,
    Comma,
    FromS32,
    FromU32,
    FromF32,
    Negate,
    Add,
    Sub,
    Mul,
    Div,
    Mod,
    Min,
    Max,
    Abs,
    Ceil,
    Floor,
    Sqrt,
    Cos,
    Sin,
    Tan,
    ACos,
    ASin,
    ATan,
    ATan2,
    Exp,
    Ln,
    Pow,
    NumberOfWASMOpExpressionF64s
};

enum class WASMOpExpressionF64WithImmediate : uint8_t {
    ConstantPoolIndex,
    GetLocal,
    NumberOfWASMOpExpressionF64WithImmediates
};

enum class WASMOpExpressionVoid : uint8_t {
    CallInternal,
    CallIndirect,
    CallImport,
    NumberOfWASMOpExpressionVoids
};

enum class WASMVariableTypes : uint8_t {
    I32 = 1 << 0,
    F32 = 1 << 1,
    F64 = 1 << 2,
    NumberOfVariableTypes = 8
};

enum class WASMVariableTypesWithImmediate : uint8_t {
    I32,
    NumberOfVariableTypesWithImmediates
};

enum class WASMSwitchCase : uint8_t {
    CaseWithNoStatements,
    CaseWithStatement,
    CaseWithBlockStatement,
    DefaultWithNoStatements,
    DefaultWithStatement,
    DefaultWithBlockStatement,
    NumberOfSwitchCases
};

enum class WASMExportFormat : uint8_t {
    Default,
    Record,
    NumberOfExportFormats
};

enum class WASMTypeConversion {
    ConvertSigned,
    ConvertUnsigned,
    Promote,
    Demote,
};

enum class WASMMemoryType {
    I8,
    I16,
    I32,
    F32,
    F64
};

enum class MemoryAccessOffsetMode { NoOffset, WithOffset };
enum class MemoryAccessConversion { NoConversion, SignExtend, ZeroExtend };

static const uint8_t hasImmediateInOpFlag = 0x80;

static const unsigned opWithImmediateBits = 2;
static const uint32_t opWithImmediateLimit = 1 << opWithImmediateBits;

static const unsigned immediateBits = 5;
static const uint32_t immediateLimit = 1 << immediateBits;

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)

#endif // WASMConstants_h
