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

#include <wtf/text/ASCIILiteral.h>

#if ENABLE(WEBASSEMBLY)

namespace JSC {

namespace Wasm {

#define FOR_EACH_EXCEPTION(macro) \
    macro(OutOfBoundsMemoryAccess,  "Out of bounds memory access"_s) \
    macro(OutOfBoundsTableAccess, "Out of bounds table access"_s) \
    macro(OutOfBoundsCallIndirect, "Out of bounds call_indirect"_s) \
    macro(NullTableEntry,  "call_indirect to a null table entry"_s) \
    macro(NullReference,  "call_ref to a null reference"_s) \
    macro(NullI31Get, "i31.get_<sx> to a null reference"_s) \
    macro(BadSignature, "call_indirect to a signature that does not match"_s) \
    macro(OutOfBoundsTrunc, "Out of bounds Trunc operation"_s) \
    macro(Unreachable, "Unreachable code should not be executed"_s) \
    macro(DivisionByZero, "Division by zero"_s) \
    macro(IntegerOverflow, "Integer overflow"_s) \
    macro(StackOverflow, "Stack overflow"_s) \
    macro(FuncrefNotWasm, "Funcref must be an exported wasm function"_s) \
    macro(InvalidGCTypeUse, "Unsupported use of struct or array type"_s) \
    macro(OutOfBoundsArrayGet, "Out of bounds array.get"_s) \
    macro(OutOfBoundsArraySet, "Out of bounds array.set"_s) \
    macro(NullArrayGet, "array.get to a null reference"_s) \
    macro(NullArraySet, "array.set to a null reference"_s) \
    macro(NullArrayLen, "array.len to a null reference"_s) \
    macro(NullStructGet, "struct.get to a null reference"_s) \
    macro(NullStructSet, "struct.set to a null reference"_s) \
    macro(TypeErrorInvalidV128Use, "an exported wasm function cannot contain a v128 parameter or return value"_s) \
    macro(NullRefAsNonNull, "ref.as_non_null to a null reference"_s) \
    macro(CastFailure, "ref.cast failed to cast reference to target heap type"_s) \
    macro(OutOfBoundsDataSegmentAccess, "Offset + array length would exceed the size of a data segment"_s) \
    macro(OutOfBoundsElementSegmentAccess, "Offset + array length would exceed the length of an element segment"_s)

enum class ExceptionType : uint32_t {
#define MAKE_ENUM(enumName, error) enumName,
    FOR_EACH_EXCEPTION(MAKE_ENUM)
#undef MAKE_ENUM
};

#define JSC_COUNT_EXCEPTION_TYPES(name, message) + 1
static constexpr unsigned numberOfExceptionTypes = 0 FOR_EACH_EXCEPTION(JSC_COUNT_EXCEPTION_TYPES);
#undef JSC_COUNT_EXCEPTION_TYPES

ALWAYS_INLINE ASCIILiteral errorMessageForExceptionType(ExceptionType type)
{
    switch (type) {
#define SWITCH_CASE(enumName, error) \
    case ExceptionType::enumName: return error;

    FOR_EACH_EXCEPTION(SWITCH_CASE)
#undef SWITCH_CASE
    }
    ASSERT_NOT_REACHED();
    return ""_s;
}

ALWAYS_INLINE bool isTypeErrorExceptionType(ExceptionType type)
{
    switch (type) {
    case ExceptionType::OutOfBoundsMemoryAccess:
    case ExceptionType::OutOfBoundsTableAccess:
    case ExceptionType::OutOfBoundsDataSegmentAccess:
    case ExceptionType::OutOfBoundsElementSegmentAccess:
    case ExceptionType::OutOfBoundsCallIndirect:
    case ExceptionType::NullTableEntry:
    case ExceptionType::NullReference:
    case ExceptionType::NullI31Get:
    case ExceptionType::BadSignature:
    case ExceptionType::OutOfBoundsTrunc:
    case ExceptionType::Unreachable:
    case ExceptionType::DivisionByZero:
    case ExceptionType::IntegerOverflow:
    case ExceptionType::StackOverflow:
    case ExceptionType::OutOfBoundsArrayGet:
    case ExceptionType::OutOfBoundsArraySet:
    case ExceptionType::NullArrayGet:
    case ExceptionType::NullArraySet:
    case ExceptionType::NullArrayLen:
    case ExceptionType::NullStructGet:
    case ExceptionType::NullStructSet:
    case ExceptionType::NullRefAsNonNull:
    case ExceptionType::CastFailure:
        return false;
    case ExceptionType::FuncrefNotWasm:
    case ExceptionType::InvalidGCTypeUse:
    case ExceptionType::TypeErrorInvalidV128Use:
        return true;
    }
    return false;
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
