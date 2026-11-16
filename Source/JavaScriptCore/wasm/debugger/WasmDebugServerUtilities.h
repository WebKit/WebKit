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

#include <wtf/Compiler.h>
#include <wtf/Platform.h>

#if ENABLE(WEBASSEMBLY_DEBUGGER)

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

#include <JavaScriptCore/CallFrame.h>
#include <JavaScriptCore/JSWebAssemblyInstance.h>
#include <JavaScriptCore/WasmExceptionType.h>
#include <JavaScriptCore/WasmIPIntGenerator.h>
#include <JavaScriptCore/WasmVirtualAddress.h>
#include <memory>
#include <wtf/HexNumber.h>
#include <wtf/RawHex.h>
#include <wtf/Ref.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringView.h>

namespace JSC {

namespace Wasm {

class VirtualAddress;
class IPIntCallee;
enum class TypeKind : int8_t;
struct Type;

enum class ProtocolError : uint8_t {
    None = 0,
    InvalidPacket = 1,
    InvalidAddress = 2,
    InvalidRegister = 3,
    MemoryError = 4,
    UnknownCommand = 5
};

class StepIntoEvent {
public:
    using BitField = uint8_t;

    enum Event : BitField {
        NoEvent = 0,
        StepIntoCall = 1 << 0, // Step into a function call
        StepIntoThrow = 1 << 1, // Step into an exception handler
    };

    StepIntoEvent() = default;

    void set(Event event)
    {
        RELEASE_ASSERT(!hasAny());
        RELEASE_ASSERT(event == StepIntoCall || event == StepIntoThrow);
        m_event = event;
    }

    bool take(Event event)
    {
        if (m_event == event) {
            m_event = NoEvent;
            return true;
        }
        return false;
    }

    bool hasAny() const { return m_event != NoEvent; }

private:
    BitField m_event { NoEvent };
};

// Why the VM stopped. Always set when isStopped. Drives GDB wire protocol signal and reason.
enum class DebugStopReason : uint8_t {
    // Debugger-imposed stop: passive VM (no WASM context) or WASM function prologue.
    // Also used for new module load stops (isNewModuleLoad flag is set in that case).
    Interrupted,
    Breakpoint, // A user-set breakpoint was hit (reason:breakpoint)
    Step, // A step breakpoint was hit (reason:trace)
    WasmTrap, // Wasm trap / exception
};

class Breakpoint final : public ThreadSafeRefCounted<Breakpoint> {
    WTF_MAKE_TZONE_ALLOCATED_EXPORT(Breakpoint, JS_EXPORT_PRIVATE);
public:
    static Ref<Breakpoint> create(const ModuleInformation& owner, uint8_t* pc)
    {
        return adoptRef(*new Breakpoint(owner, pc));
    }

    void patchBreakpoint() { *pc = static_cast<uint8_t>(OpType::Unreachable); }
    void restorePatch() { *pc = static_cast<uint8_t>(originalBytecode); }

    bool isUnused() const { return !siteCount && !oneTimeClaim; }

    void dump(PrintStream& out) const
    {
        out.print("Breakpoint(pc:", RawPointer(pc));
        out.print(", *pc:", (int)*pc);
        out.print(", originalBytecode:", RawHex(static_cast<unsigned>(originalBytecode)));
        out.print(", siteCount:", siteCount, ", oneTimeClaim:", oneTimeClaim, ")");
    }

    // Keeps the bytecode buffer alive.
    RefPtr<const ModuleInformation> owner;
    uint8_t* pc { nullptr };
    OpType originalBytecode { OpType::Unreachable };
    // Independent claims, able to coexist; the patch is restored once neither remains.
    unsigned siteCount { 0 };
    std::optional<DebugStopReason> oneTimeClaim; // Step or Interrupted while armed.

private:
    Breakpoint(const ModuleInformation& owner, uint8_t* pc)
        : owner(&owner)
        , pc(pc)
        , originalBytecode(static_cast<OpType>(*pc))
    {
    }
};

// WASM execution context snapshot captured when stopped at a debugging event.
// Present for Breakpoint/Step/Trap stops; null for Interrupted stops (no WASM execution context).
struct StopData {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(StopData);

    StopData(VirtualAddress, OpType originalBytecode, uint8_t* pc, uint8_t* mc, IPInt::IPIntStackEntry*, IPIntCallee*, JSWebAssemblyInstance*, CallFrame*);

    StopData(IPIntCallee*, JSWebAssemblyInstance*, CallFrame*); // Prologue: no pc/mc

    StopData(IPIntCallee*, JSWebAssemblyInstance*, CallFrame*, uint8_t* pc, uint8_t* mc, IPInt::IPIntStackEntry*, Wasm::ExceptionType); // Trap

    ~StopData();

    void dump(PrintStream&) const;

    VirtualAddress address;
    OpType originalBytecode { OpType::Unreachable };
    uint8_t* pc { nullptr };
    uint8_t* mc { nullptr };
    IPInt::IPIntStackEntry* stack { nullptr };
    RefPtr<IPIntCallee> callee;
    JSWebAssemblyInstance* instance { nullptr };
    CallFrame* callFrame { nullptr };
    std::optional<Wasm::ExceptionType> wasmTrapType;
};

// Per-VM debugging state machine with current stop information.
// Created on-demand via VM::debugState(), accessed only when VM is stopped.
struct DebugState {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(DebugState);

    using Reason = DebugStopReason;

    DebugState() = default;

    void setPrologueStopData(JSWebAssemblyInstance* instance, IPIntCallee* callee, CallFrame* callFrame)
    {
        stopReason = Reason::Interrupted;
        stopData = makeUnique<StopData>(callee, instance, callFrame);
    }

    void setAtomicsWaitStopData(IPIntCallee* callee, JSWebAssemblyInstance* instance, CallFrame* callFrame, uint8_t* pc, uint8_t* mc, IPInt::IPIntStackEntry* stack)
    {
        stopReason = Reason::Interrupted;
        // ExtAtomic by construction: the only callers are the memory_atomic_wait32/64 externs,
        // reached by dispatching the 0xFE prefix. *pc can be a sibling instance's patch instead.
        RELEASE_ASSERT(*pc == static_cast<uint8_t>(OpType::ExtAtomic) || *pc == static_cast<uint8_t>(OpType::Unreachable));
        stopData = makeUnique<StopData>(VirtualAddress::toVirtual(instance, callee->functionIndex(), pc), OpType::ExtAtomic, pc, mc, stack, callee, instance, callFrame);
    }

    void setBreakpointStopData(Reason reason, VirtualAddress address, OpType originalBytecode, uint8_t* pc, uint8_t* mc, IPInt::IPIntStackEntry* stack, IPIntCallee* callee, JSWebAssemblyInstance* instance, CallFrame* callFrame)
    {
        RELEASE_ASSERT(reason == Reason::Breakpoint || reason == Reason::Step || reason == Reason::Interrupted);
        stopReason = reason;
        stopData = makeUnique<StopData>(address, originalBytecode, pc, mc, stack, callee, instance, callFrame);
    }

    void setTrapStopData(IPIntCallee* callee, JSWebAssemblyInstance* instance, CallFrame* callFrame, uint8_t* pc, uint8_t* mc, IPInt::IPIntStackEntry* stack, Wasm::ExceptionType wasmTrapType)
    {
        stopReason = Reason::WasmTrap;
        stopData = makeUnique<StopData>(callee, instance, callFrame, pc, mc, stack, wasmTrapType);
    }

    // WHERE-based helpers — determined by stopData presence and pc:
    bool isStoppedAtSystemCall() const
    {
        bool result = !stopData;
        if (result)
            RELEASE_ASSERT(stopReason == Reason::Interrupted);
        return result;
    }
    bool isStoppedAtPrologue() const
    {
        bool result = stopData && !stopData->pc;
        if (result)
            RELEASE_ASSERT(stopReason == Reason::Interrupted || stopReason == Reason::WasmTrap);
        return result;
    }
    bool isStoppedAtBytecode() const
    {
        bool result = stopData && !!stopData->pc;
        if (result) {
            // FIXME: Reason::Interrupted is only valid here because setAtomicsWaitStopData uses it.
            // All other Interrupted stops have no stopData->pc. Consider introducing a dedicated
            // Reason::AtomicsWaitBlocked to keep that case distinct and tighten this assertion.
            RELEASE_ASSERT(stopReason == Reason::Breakpoint || stopReason == Reason::Step || stopReason == Reason::WasmTrap || stopReason == Reason::Interrupted);
        }
        return result;
    }
    bool isAtomicsWaitStop() const
    {
        if (!isStoppedAtBytecode())
            return false;
        if (stopData->originalBytecode != OpType::ExtAtomic)
            return false;
        auto extOp = static_cast<ExtAtomicOpType>(*(stopData->pc + 1));
        return extOp == ExtAtomicOpType::MemoryAtomicWait32 || extOp == ExtAtomicOpType::MemoryAtomicWait64;
    }

    // WHY-based helpers — determined by stopReason:
    bool isStoppedDueToWasmTrap() const
    {
        return stopReason == Reason::WasmTrap;
    }

    void clearStop()
    {
        stopReason = std::nullopt;
        isNewModuleLoad = false;
        isStopped = false;
        stopData = nullptr;
    }

    void setStopped()
    {
        // Sets Interrupted on VMs with no stop reason.
        if (!stopReason.has_value())
            stopReason = Reason::Interrupted;
        isStopped = true;
    }


    bool hasStepIntoEvent() { return stepIntoEvent.hasAny(); }
    void setStepIntoCall() { stepIntoEvent.set(StepIntoEvent::StepIntoCall); }
    bool takeStepIntoCall() { return stepIntoEvent.take(StepIntoEvent::StepIntoCall); }
    void setStepIntoThrow() { stepIntoEvent.set(StepIntoEvent::StepIntoThrow); }
    bool takeStepIntoThrow() { return stepIntoEvent.take(StepIntoEvent::StepIntoThrow); }

    std::optional<Reason> stopReason;
    bool isNewModuleLoad { false };
    bool isStopped { false }; // FIXME: There is better design (rdar://174508321) for code efficiency.
    std::unique_ptr<StopData> stopData { nullptr };

    // Step-into tracking (for step debugging behavior)
    StepIntoEvent stepIntoEvent;
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

String stringToHex(StringView);

void logWasmLocalValue(size_t index, const JSC::IPInt::IPIntLocal&, const Wasm::Type&);

std::optional<uint64_t> parseHexStrict(StringView);

uint32_t parseDecimal(StringView, uint32_t defaultValue = 0);

Vector<StringView> splitWithDelimiters(StringView packet, StringView delimiters);

// Caller resume location and enclosing instance.
struct WasmReturnSite {
    uint8_t* pc { nullptr };
    JSWebAssemblyInstance* instance { nullptr };

    explicit operator bool() const { return pc && instance; }
};

WasmReturnSite getWasmReturnPC(CallFrame* currentFrame);

struct FrameInfo {
    VirtualAddress address;
    CallFrame* wasmCallFrame { nullptr };
    RefPtr<IPIntCallee> wasmCallee;
    size_t inFlightCallFrameSize { 0 };

    bool isWasmFrame() const { return !!wasmCallee; }
};

Vector<FrameInfo> collectCallStack(VirtualAddress stopAddress, CallFrame* startFrame, VM&, unsigned maxFrames = 100);


inline StringView getErrorReply(ProtocolError error)
{
    switch (error) {
    case ProtocolError::InvalidPacket:
        return "E01"_s;
    case ProtocolError::InvalidAddress:
        return "E02"_s;
    case ProtocolError::InvalidRegister:
        return "E03"_s;
    case ProtocolError::MemoryError:
        return "E04"_s;
    case ProtocolError::UnknownCommand:
        return "E05"_s;
    default:
        return "E00"_s;
    }
}

} // namespace Wasm
} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(WEBASSEMBLY_DEBUGGER)
