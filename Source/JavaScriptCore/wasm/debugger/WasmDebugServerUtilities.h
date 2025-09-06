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

#include "WasmIPIntGenerator.h"
#include "WasmOps.h"
#include <wtf/DataLog.h>
#include <wtf/HexNumber.h>
#include <wtf/RawPointer.h>
#include <wtf/text/ASCIILiteral.h>
#include <wtf/text/StringView.h>

namespace JSC {

class CallFrame;
class JSWebAssemblyInstance;

namespace Wasm {

class VirtualAddress;

// Error codes for GDB Remote Protocol debugging
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

    Type type { Type::Regular }; // Regular vs Interrupt

    uint8_t* pc { nullptr }; // Physical address of pc
    uint8_t originalBytecode { 0 }; // Original instruction of pc(for restoration)
};


template<typename T>
inline String toLittleEndianHex(const T& value)
{
    static_assert(sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8 || sizeof(T) == 16,
        "toLittleEndianHex only supports 1, 2, 4, 8, or 16 byte types");

    StringBuilder hexString;
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&value);

    for (size_t i = 0; i < sizeof(T); i++) {
        hexString.append(hex(bytes[i], 2, Lowercase));
    }
    return hexString.toString();
}

// Convert string to hex-encoded representation for GDB Remote Protocol
inline String stringToHex(StringView str)
{
    StringBuilder result;
    CString utf8 = str.utf8();
    for (size_t i = 0; i < utf8.length(); ++i) {
        result.append(hex(static_cast<uint8_t>(utf8.data()[i]), 2, Lowercase));
    }
    return result.toString();
}

inline void logWasmLocalValue(size_t index, const JSC::IPInt::IPIntLocal& local, const Wasm::Type& localType)
{
    dataLog("  Local[", index, "] (", localType, "): ");

    switch (localType.kind) {
    case TypeKind::I32:
        dataLogF("i32=%d", local.i32);
        break;
    case TypeKind::I64:
        dataLogF("i64=%" PRId64, local.i64);
        break;
    case TypeKind::F32:
        dataLogF("f32=%f", local.f32);
        break;
    case TypeKind::F64:
        dataLogF("f64=%f", local.f64);
        break;
    case TypeKind::V128:
        dataLogF("v128=0x%016" PRIx64 "%016" PRIx64, local.v128.u64x2[1], local.v128.u64x2[0]);
        break;
    case TypeKind::Ref:
    case TypeKind::RefNull:
        dataLogF("ref=%p", reinterpret_cast<void*>(local.ref));
        break;
    default:
        dataLogF("raw=0x%" PRIx64, local.i64);
        break;
    }

    dataLogF(" [index %zu]\n", index);
}

// Parse hexadecimal string to uint64_t with default value fallback
// Handles empty strings gracefully and uses standard C library functions
inline uint64_t parseHex(StringView str, uint64_t defaultValue = 0)
{
    if (str.isEmpty())
        return defaultValue;
    CString cstr = str.utf8();
    return strtoull(cstr.data(), nullptr, 16);
}

// Parse decimal string to uint32_t with default value fallback
// Handles empty strings gracefully and uses standard C library functions
inline uint32_t parseDecimal(StringView str, uint32_t defaultValue = 0)
{
    if (str.isEmpty())
        return defaultValue;
    CString cstr = str.utf8();
    return static_cast<uint32_t>(strtoul(cstr.data(), nullptr, 10));
}

Vector<StringView> packetSplit(StringView packet, StringView delimiters);

// WebAssembly call stack utilities
bool getWasmReturnPC(CallFrame* currentFrame, uint8_t*& returnPC, VirtualAddress& virtualReturnPC);

} // namespace Wasm
} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(WEBASSEMBLY)
