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

#include "config.h"
#include "WasmDebugServerUtilities.h"

#if ENABLE(WEBASSEMBLY_DEBUGGER)

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

#include "CallFrame.h"
#include "JSWebAssemblyInstance.h"
#include "NativeCallee.h"
#include "WasmCallee.h"
#include "WasmIPIntGenerator.h"
#include "WasmOps.h"
#include "WasmVirtualAddress.h"
#include <cstring>
#include <wtf/DataLog.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/Vector.h>
#include <wtf/text/StringToIntegerConversion.h>

namespace JSC {
namespace Wasm {

WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(DebugState);
WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(StopData);

String stringToHex(StringView str)
{
    StringBuilder result;
    CString utf8 = str.utf8();
    for (size_t i = 0; i < utf8.length(); ++i)
        result.append(hex(static_cast<uint8_t>(utf8.data()[i]), 2, Lowercase));
    return result.toString();
}

void logWasmLocalValue(size_t index, const JSC::IPInt::IPIntLocal& local, const Wasm::Type& localType)
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

uint64_t parseHex(StringView str, uint64_t defaultValue)
{
    if (str.isEmpty())
        return defaultValue;
    auto result = parseInteger<uint64_t>(str, 16);
    return result.value_or(defaultValue);
}

uint32_t parseDecimal(StringView str, uint32_t defaultValue)
{
    if (str.isEmpty())
        return defaultValue;
    auto result = parseInteger<uint32_t>(str, 10);
    return result.value_or(defaultValue);
}

// Splits a string using a sequence of delimiters with exact matching.
// Returns empty vector if any delimiter is missing.
//
// Examples:
//   splitWithDelimiters("Z0,400000000000018b,1", ",,") -> ["Z0", "400000000000018b", "1"]
//   splitWithDelimiters("qWasmLocal:0:5", "::") -> ["qWasmLocal", "0", "5"]
//   splitWithDelimiters("invalid", ",,") -> [] (missing delimiters)
Vector<StringView> splitWithDelimiters(StringView packet, StringView delimiters)
{
    Vector<StringView> result;

    if (packet.isEmpty() || delimiters.isEmpty())
        return result;

    StringView current = packet;

    // Split on each delimiter in sequence - must find ALL delimiters for exact matching
    for (size_t i = 0; i < delimiters.length(); ++i) {
        char delimiter = delimiters[i];
        size_t pos = current.find(delimiter);
        if (pos == notFound)
            return Vector<StringView>();

        result.append(current.substring(0, pos));
        current = current.substring(pos + 1);
    }

    result.append(current);
    return result;
}

bool getWasmReturnPC(CallFrame* currentFrame, uint8_t*& returnPC, VirtualAddress& virtualReturnPC)
{
    CallFrame* callerFrame = currentFrame->callerFrame();

    if (!callerFrame->callee().isNativeCallee())
        return false;

    RefPtr caller = callerFrame->callee().asNativeCallee();
    if (caller->category() != NativeCallee::Category::Wasm)
        return false;

    RefPtr wasmCaller = uncheckedDowncast<const Wasm::Callee>(caller.get());
    if (wasmCaller->compilationMode() != Wasm::CompilationMode::IPIntMode)
        return false;

    // Read the WebAssembly return PC from IPInt's saved PC location (cfr-8)
    // This contains the WebAssembly bytecode address where execution should continue in the caller
    uint8_t* pcLocation = reinterpret_cast<uint8_t*>(currentFrame) - 8;
    returnPC = WTF::unalignedLoad<uint8_t*>(pcLocation);

    JSWebAssemblyInstance* callerInstance = callerFrame->wasmInstance();
    RefPtr ipintCaller = uncheckedDowncast<const Wasm::IPIntCallee>(wasmCaller.get());
    virtualReturnPC = VirtualAddress::toVirtual(callerInstance, ipintCaller->functionIndex(), returnPC);
    return true;
}

StopData::StopData(IPIntCallee* callee, JSWebAssemblyInstance* instance)
    : code(Code::Stop)
    , location(Location::Prologue)
    , address(VirtualAddress::toVirtual(instance, callee->functionIndex(), callee->bytecode()))
    , callee(callee)
    , instance(instance)
{
}

StopData::StopData(Breakpoint::Type type, VirtualAddress address, uint8_t originalBytecode, uint8_t* pc, uint8_t* mc, IPInt::IPIntLocal* locals, IPInt::IPIntStackEntry* stack, IPIntCallee* callee, JSWebAssemblyInstance* instance, CallFrame* callFrame)
    : location(Location::Breakpoint)
    , address(address)
    , originalBytecode(originalBytecode)
    , pc(pc)
    , mc(mc)
    , locals(locals)
    , stack(stack)
    , callee(callee)
    , instance(instance)
    , callFrame(callFrame)
{
    setCode(type);
}

StopData::~StopData() = default;

void StopData::setCode(Breakpoint::Type type)
{
    switch (type) {
    case Breakpoint::Type::Interrupt:
        code = Code::Stop;
        break;
    case Breakpoint::Type::Step:
        code = Code::Trace;
        break;
    case Breakpoint::Type::Regular:
        code = Code::Breakpoint;
        break;
    default:
        break;
    }
}

void StopData::dump(PrintStream& out) const
{
    out.print("StopData(Code:", code);
    out.print(", location:", location);
    out.print(", address:", address);
    out.print(", originalBytecode:", originalBytecode);
    out.print(", pc:", RawPointer(pc));
    out.print(", mc:", RawPointer(mc));
    out.print(", locals:", RawPointer(locals));
    out.print(", stack:", RawPointer(stack));
    out.print(", callee:", RawPointer(callee.get()));
    out.print(", instance:", RawPointer(instance));
    out.print(", callFrame:", RawPointer(callFrame), ")");
}

} // namespace Wasm
} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(WEBASSEMBLY_DEBUGGER)
