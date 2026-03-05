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

#include <wtf/Platform.h>

#if ENABLE(WEBASSEMBLY_DEBUGGER)

#include <JavaScriptCore/JSExportMacros.h>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <wtf/HexNumber.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/WTFString.h>

namespace JSC {

class JSWebAssemblyInstance;

namespace Wasm {

enum class ProtocolError : uint8_t;
class ModuleManager;
class FunctionCodeIndex;

/**
 * VirtualAddress - WebAssembly virtual address encoding for LLDB debugging
 *
 * Encodes 64-bit virtual addresses for WebAssembly debugging with LLDB.
 * Separates module code addresses from instance memory addresses.
 *
 * Address Format (64-bit):
 * - Bits 63-62: Address Type (2 bits)
 * - Bits 61-32: ID (30 bits) - ModuleID for code, InstanceID for memory
 * - Bits 31-0:  Offset (32 bits)
 *
 * Address Types:
 * - 0x00 (Memory): Instance linear memory - uses InstanceID
 * - 0x01 (Module): Module code/bytecode - uses ModuleID
 * - 0x02 (Invalid): Invalid/unmapped regions
 * - 0x03 (Invalid2): Invalid/unmapped regions
 *
 * Virtual Memory Layout:
 * - 0x0000000000000000 - 0x3FFFFFFFFFFFFFFF: Memory regions
 * - 0x4000000000000000 - 0x7FFFFFFFFFFFFFFF: Module regions
 * - 0x8000000000000000 - 0xFFFFFFFFFFFFFFFF: Invalid regions
 *
 * Example:
 *
 *     Module A (ID=0): Code at 0x4000000000000000
 *     ├── Instance 1 (ID=0): Memory at 0x0000000000000000
 *     ├── Instance 2 (ID=1): Memory at 0x0000000100000000
 *     └── Instance 3 (ID=2): Memory at 0x0000000200000000
 *
 *     Module B (ID=1): Code at 0x4000000100000000
 *     └── Instance 4 (ID=3): Memory at 0x0000000300000000
 *
 * Memory Region Example:
 *
 *     [0x0000000000000000-0x0000000001010000) rw- wasm_memory_0_0
 *     [0x0000000001010000-0x4000000000000000) ---
 *     [0x4000000000000000-0x40000000000013f1) r-x wasm_module_0
 *     [0x40000000000013f1-0xffffffffffffffff) ---
 */
class VirtualAddress {
public:
    enum class Type : uint8_t {
        Memory = 0x00, // Instance linear memory (uses InstanceID)
        Module = 0x01, // Module code/bytecode (uses ModuleID)
        Invalid = 0x02, // Invalid/unmapped regions
        Invalid2 = 0x03 // Invalid/unmapped regions
    };

    static constexpr uint64_t MEMORY_BASE = 0x0000000000000000ULL;
    static constexpr uint64_t MEMORY_END = 0x3FFFFFFFFFFFFFFFULL;
    static constexpr uint64_t MODULE_BASE = 0x4000000000000000ULL;
    static constexpr uint64_t MODULE_END = 0x7FFFFFFFFFFFFFFFULL;
    static constexpr uint64_t INVALID_BASE = 0x8000000000000000ULL;
    static constexpr uint64_t INVALID_END = 0xFFFFFFFFFFFFFFFFULL;

    VirtualAddress()
        : m_value(0)
    {
    }
    explicit VirtualAddress(uint64_t addr)
        : m_value(addr)
    {
    }

    static VirtualAddress createMemory(uint32_t instanceId, uint32_t offset = 0)
    {
        return VirtualAddress(encode(Type::Memory, instanceId, offset));
    }

    static VirtualAddress createModule(uint32_t moduleId, uint32_t offset = 0)
    {
        return VirtualAddress(encode(Type::Module, moduleId, offset));
    }

    Type type() const { return static_cast<Type>((m_value & 0xC000000000000000ULL) >> 62); }
    uint32_t id() const { return static_cast<uint32_t>((m_value & 0x3FFFFFFF00000000ULL) >> 32); }
    uint32_t offset() const { return static_cast<uint32_t>(m_value & 0x00000000FFFFFFFFULL); }
    String hex() const { return makeString(WTF::hex(m_value, WTF::Lowercase)); }
    uint64_t value() const { return m_value; }

    bool isInvalidType() const
    {
        Type addressType = type();
        return addressType == Type::Invalid || addressType == Type::Invalid2;
    }

    JS_EXPORT_PRIVATE static VirtualAddress toVirtual(JSWebAssemblyInstance*, FunctionCodeIndex, const uint8_t* pc);
    JS_EXPORT_PRIVATE uint8_t* toPhysicalPC(const ModuleManager&);

    operator uint64_t() const { return m_value; }

    JS_EXPORT_PRIVATE void dump(PrintStream&) const;

private:
    static uint64_t encode(Type type, uint32_t id, uint32_t offset)
    {
        return (((uint64_t)type << 62) | ((uint64_t)id << 32) | ((uint64_t)offset << 0));
    }

    uint64_t m_value;
};

} // namespace Wasm
} // namespace JSC

namespace WTF {

template<> struct DefaultHash<JSC::Wasm::VirtualAddress> {
    static unsigned hash(const JSC::Wasm::VirtualAddress& address)
    {
        return DefaultHash<uint64_t>::hash(static_cast<uint64_t>(address));
    }
    static bool equal(const JSC::Wasm::VirtualAddress& a, const JSC::Wasm::VirtualAddress& b)
    {
        return static_cast<uint64_t>(a) == static_cast<uint64_t>(b);
    }
    static constexpr bool safeToCompareToEmptyOrDeleted = true;
};

template<> struct HashTraits<JSC::Wasm::VirtualAddress> : GenericHashTraits<JSC::Wasm::VirtualAddress> {
    static JSC::Wasm::VirtualAddress emptyValue() { return JSC::Wasm::VirtualAddress(); }
    static void constructDeletedValue(JSC::Wasm::VirtualAddress& slot) { slot = JSC::Wasm::VirtualAddress(std::numeric_limits<uint64_t>::max()); }
    static bool isDeletedValue(const JSC::Wasm::VirtualAddress& value) { return static_cast<uint64_t>(value) == std::numeric_limits<uint64_t>::max(); }
};

} // namespace WTF

#endif // ENABLE(WEBASSEMBLY_DEBUGGER)
