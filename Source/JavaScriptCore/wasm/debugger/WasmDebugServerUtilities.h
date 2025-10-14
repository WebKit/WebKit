/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

#include <JavaScriptCore/WasmIPIntGenerator.h>
#include <JavaScriptCore/WasmOps.h>
#include <wtf/DataLog.h>
#include <wtf/HexNumber.h>
#include <wtf/RawPointer.h>
#include <wtf/text/ASCIILiteral.h>
#include <wtf/text/StringToIntegerConversion.h>
#include <wtf/text/StringView.h>

namespace JSC {

class CallFrame;
class JSWebAssemblyInstance;

namespace Wasm {

class VirtualAddress;

enum class ProtocolError : uint8_t {
    None = 0,
    InvalidPacket = 1,
    InvalidAddress = 2,
    InvalidRegister = 3,
    MemoryError = 4,
    UnknownCommand = 5
};

struct Breakpoint {
    enum class Type : uint8_t {
        // User-set breakpoint (persistent, tracked by virtual address)
        Regular = 0,

        // One-time breakpoint (auto-removed after each stop)
        Interrupt = 1,
        Step = 2,
    };

    Breakpoint() = default;
    Breakpoint(uint8_t* pc, Type type)
        : type(type)
        , pc(pc)
        , originalBytecode(*pc)
    {
    }

    void patchBreakpoint() { *pc = 0x00; }
    void restorePatch() { *pc = originalBytecode; }

    bool isOneTimeBreakpoint() { return type != Type::Regular; }

    void dump(PrintStream& out) const
    {
        out.print("Breakpoint(type:", type);
        out.print(", pc:", RawPointer(pc));
        out.print(", *pc:", (int)*pc);
        out.print(", originalBytecode:", originalBytecode, ")");
    }

    Type type { Type::Regular };
    uint8_t* pc { nullptr };
    uint8_t originalBytecode { 0 };
};

template<typename T>
inline String toNativeEndianHex(const T& value)
{
    static_assert(sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8 || sizeof(T) == 16,
        "toNativeEndianHex only supports 1, 2, 4, 8, or 16 byte types");

    StringBuilder hexString;
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&value);
    for (size_t i = 0; i < sizeof(T); i++)
        hexString.append(hex(bytes[i], 2, Lowercase));
    return hexString.toString();
}

inline String stringToHex(StringView str)
{
    StringBuilder result;
    CString utf8 = str.utf8();
    for (size_t i = 0; i < utf8.length(); ++i)
        result.append(hex(static_cast<uint8_t>(utf8.data()[i]), 2, Lowercase));
    return result.toString();
}

inline void logWasmLocalValue(size_t index, const JSC::IPInt::IPIntLocal& local, const Wasm::Type& localType)
{
    dataLog("  Local[", index, "] (", localType, "): ");

    switch (localType.kind) {
    case TypeKind::I32:
        dataLogLn("i32=", local.i32, " [index ", index, "]");
        break;
    case TypeKind::I64:
        dataLogLn("i64=", local.i64, " [index ", index, "]");
        break;
    case TypeKind::F32:
        dataLogLn("f32=", local.f32, " [index ", index, "]");
        break;
    case TypeKind::F64:
        dataLogLn("f64=", local.f64, " [index ", index, "]");
        break;
    case TypeKind::V128:
        dataLogLn("v128=0x", hex(local.v128.u64x2[1], 16, Lowercase), hex(local.v128.u64x2[0], 16, Lowercase), " [index ", index, "]");
        break;
    case TypeKind::Ref:
    case TypeKind::RefNull:
        dataLogLn("ref=", local.ref, " [index ", index, "]");
        break;
    default:
        dataLogLn("raw=0x", hex(local.i64, 16, Lowercase), " [index ", index, "]");
        break;
    }
}

inline uint64_t parseHex(StringView str, uint64_t defaultValue = 0)
{
    if (str.isEmpty())
        return defaultValue;
    auto result = parseInteger<uint64_t>(str, 16);
    return result.value_or(defaultValue);
}

inline uint32_t parseDecimal(StringView str, uint32_t defaultValue = 0)
{
    if (str.isEmpty())
        return defaultValue;
    auto result = parseInteger<uint32_t>(str, 10);
    return result.value_or(defaultValue);
}

Vector<StringView> splitWithDelimiters(StringView packet, StringView delimiters);

bool getWasmReturnPC(CallFrame* currentFrame, uint8_t*& returnPC, VirtualAddress& virtualReturnPC);

} // namespace Wasm
} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(WEBASSEMBLY)
