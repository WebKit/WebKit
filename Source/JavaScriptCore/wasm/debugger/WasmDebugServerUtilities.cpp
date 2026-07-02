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
#include "CodeBlock.h"
#include "InlineCallFrame.h"
#include "JSWebAssemblyInstance.h"
#include "NativeCallee.h"
#include "VM.h"
#include "VMEntryRecord.h"
#include "WasmCallee.h"
#include "WasmIPIntGenerator.h"
#include "WasmOps.h"
#include "WasmVirtualAddress.h"
#include <cstring>
#include <wtf/DataLog.h>
#include <wtf/StdLibExtras.h>
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

// Raw call-site return PC stored at frame−8 by IPInt's saveIPIntRegisters().
static const uint8_t* savedWasmToWasmPC(CallFrame* frame)
{
    return WTF::unalignedLoad<const uint8_t*>(reinterpret_cast<const uint8_t*>(frame) - sizeof(Register));
}

// IPInt PC saved at WasmToJSIPIntReturnPCSlot[cfr] by both the JIT and no-JIT WasmToJS stubs.
static const uint8_t* savedWasmToJSPC(CallFrame* wasmToJSFrame)
{
    return WTF::unalignedLoad<const uint8_t*>(reinterpret_cast<const uint8_t*>(wasmToJSFrame) + Wasm::WasmToJSIPIntReturnPCSlot);
}

bool getWasmReturnPC(CallFrame* currentFrame, uint8_t*& returnPC, VirtualAddress& virtualReturnPC)
{
    // Safe to use the non-EntryFrame overload: IPInt WASM frames are always entered via a
    // JSToWasm trampoline (a normal CallFrame), never directly from C++, so no EntryFrame
    // boundary can appear immediately above an IPInt frame.
    CallFrame* callerFrame = currentFrame->callerFrame();

    if (!callerFrame->callee().isNativeCallee())
        return false;

    RefPtr caller = callerFrame->callee().asNativeCallee();
    if (caller->category() != NativeCallee::Category::Wasm)
        return false;

    RefPtr wasmCaller = uncheckedDowncast<const Wasm::Callee>(caller.get());
    if (wasmCaller->compilationMode() != Wasm::CompilationMode::IPIntMode)
        return false;

    RefPtr ipintCaller = uncheckedDowncast<const Wasm::IPIntCallee>(wasmCaller.get());
    returnPC = const_cast<uint8_t*>(savedWasmToWasmPC(currentFrame));
    virtualReturnPC = VirtualAddress::toVirtual(callerFrame->wasmInstance(), ipintCaller->functionIndex(), returnPC);
    return true;
}


// Walk the full CallFrame chain from a WASM breakpoint, collecting virtual addresses for
// every WASM and JS frame. The result is consumed by qWasmCallStack to give LLDB a
// complete backtrace even when WASM and JS frames are interleaved.
//
// Supported call chain topologies (-> = calls, W = IPInt WASM, J = JS, JW = JSToWasm*, WJ = WasmToJS):
//
//   J -> JW -> W                                   (WASM called from JS)
//   J -> JW -> W -> W                              (WASM->WASM, called from JS)
//   J -> JW -> W -> WJ -> J -> JW -> W -> ...      (WASM<->JS interleaved)
//   J -> JW -> W -> WJ -> J -> J -> J -> JW -> W   (multiple JS frames between WASM segments)
//
// Algorithm — three steps per iteration:
//
//   Step A: emit `frame` if it is a JS frame (one per inline depth for DFG/FTL; one otherwise).
//           WASM frames are naturally skipped since their callee().isNativeCallee() is true.
//   Step B: advance to caller = frame->callerFrame(). Stop if null.
//   Step C: dispatch on the caller's type — JS, IPIntMode (WASM->WASM), JSToWasm* (skip
//           trampoline), WasmToJS (recover outer WASM call-site PC from WasmToJSIPIntReturnPCSlot).
//           Both the JIT and no-JIT WasmToJS stubs save the IPInt PC into
//           WasmToJSIPIntReturnPCSlot[cfr] on entry, before JS JIT can clobber the register.
Vector<FrameInfo> collectCallStack(VirtualAddress stopAddress, CallFrame* startFrame, VM& vm, unsigned maxFrames)
{
    Vector<FrameInfo> frames;

    // Frame 0 — the breakpoint frame. startFrame is always an IPInt WASM frame.
    {
        RELEASE_ASSERT(startFrame->callee().isNativeCallee());
        RefPtr startNativeCallee = startFrame->callee().asNativeCallee();
        RELEASE_ASSERT(startNativeCallee->category() == NativeCallee::Category::Wasm);
        RefPtr startWasmCallee = uncheckedDowncast<const Wasm::Callee>(startNativeCallee.get());
        RELEASE_ASSERT(startWasmCallee->compilationMode() == Wasm::CompilationMode::IPIntMode);
        RefPtr startIPIntCallee = uncheckedDowncast<const Wasm::IPIntCallee>(startWasmCallee.get());
        frames.append({ stopAddress, startFrame, WTF::move(startIPIntCallee) });
    }

    if (Options::verboseWasmDebugger()) {
        dataLogLn("[collectCallStack] frames:");
        dataLog("  [0] ");
        if (stopAddress.value() == VirtualAddress::JS_FRAME_BASE)
            dataLog("[JS] ");
        else if (stopAddress.value() == VirtualAddress::INVALID_BASE)
            dataLog("[System] ");
        else
            dataLog("[WASM] ");
        dataLogLn(stopAddress);
    }

    EntryFrame* entryFrame = vm.topEntryFrame;
    CallFrame* frame = startFrame;

    while (frames.size() < maxFrames) {
        // Step A — emit `frame` if it is a JS frame.
        // startFrame (WASM) is naturally skipped because its callee().isNativeCallee() is true.
        // Every path in Step C changes `frame`, so Step A always processes a fresh unemitted frame.
        if (!frame->callee().isNativeCallee()) {
#if ENABLE(DFG_JIT)
            if (frame->codeBlock() && frame->codeBlock()->canGetCodeOrigin(frame->callSiteIndex())) {
                // DFG/FTL: emit one JS_FRAME_BASE per inline depth (innermost to outermost).
                frame->codeBlock()->codeOrigin(frame->callSiteIndex()).walkUpInlineStack([&](CodeOrigin origin) {
                    if (frames.size() < maxFrames) {
                        if (Options::verboseWasmDebugger()) {
                            if (auto* inlineCallFrame = origin.inlineCallFrame())
                                dataLogLn("  [", frames.size(), "] [JS][Inlined] ", inlineCallFrame->inferredNameWithHash());
                            else
                                dataLogLn("  [", frames.size(), "] [JS] ", frame->codeBlock()->inferredNameWithHash());
                        }
                        frames.append({ VirtualAddress(VirtualAddress::JS_FRAME_BASE), nullptr, { } });
                    }
                });
            } else {
#endif
                dataLogLnIf(Options::verboseWasmDebugger(), "  [", frames.size(), "] [JS] ", frame->codeBlock()->inferredNameWithHash());
                frames.append({ VirtualAddress(VirtualAddress::JS_FRAME_BASE), nullptr, { } });
#if ENABLE(DFG_JIT)
            }
#endif
        }

        // Step B — advance to the caller.
        EntryFrame* callerEntryFrame = entryFrame;
        CallFrame* caller = frame->callerFrame(callerEntryFrame);
        if (!caller)
            break;

        // Step C — dispatch on the caller's type.
        if (!caller->callee().isNativeCallee()) {
            // JS frame above — advance; Step A will emit it next iteration.
            entryFrame = callerEntryFrame;
            frame = caller;
            continue;
        }

        RefPtr callerCallee = caller->callee().asNativeCallee();
        if (callerCallee->category() != NativeCallee::Category::Wasm)
            break; // C++ entry or InlineCache — stop.

        RefPtr wasmCallerCallee = uncheckedDowncast<const Wasm::Callee>(callerCallee.get());
        switch (wasmCallerCallee->compilationMode()) {

        case Wasm::CompilationMode::IPIntMode: {
            // WASM->WASM: emit exact bytecode return PC saved at frame−8 by IPInt.
            RefPtr ipintCaller = uncheckedDowncast<const Wasm::IPIntCallee>(wasmCallerCallee.get());
            VirtualAddress virtualReturnPC = VirtualAddress::toVirtual(caller->wasmInstance(), ipintCaller->functionIndex(), savedWasmToWasmPC(frame));
            dataLogLnIf(Options::verboseWasmDebugger(), "  [", frames.size(), "] [WASM][IPInt] ", virtualReturnPC);
            frames.append({ virtualReturnPC, caller, ipintCaller });
            entryFrame = callerEntryFrame;
            frame = caller;
            break;
        }

        case Wasm::CompilationMode::JSToWasmMode:
        case Wasm::CompilationMode::JSToWasmICMode:
        case Wasm::CompilationMode::WasmBuiltinMode:
            // JSToWasm trampoline or WasmBuiltin — skip; advance to the frame beyond it.
            // Step A will emit it in the next iteration if it is a JS frame.
            dataLogLnIf(Options::verboseWasmDebugger(), "    [WASM][", wasmCallerCallee->compilationMode(), "][SKIPPED]");
            entryFrame = callerEntryFrame;
            frame = caller->callerFrame(callerEntryFrame);
            RELEASE_ASSERT(frame);
            break;

        case Wasm::CompilationMode::WasmToJSMode: {
            // WasmToJS trampoline — `frame` (the JS frame directly below it) was already
            // emitted by Step A. Recover the outer WASM call-site PC from the dedicated slot
            // saved by both the JIT and no-JIT WasmToJS stubs before the JS call: this value
            // is the IPInt PC advanced past the 'call' opcode, stored at
            // WasmToJSIPIntReturnPCSlot[cfr] of the WasmToJS frame.
            CallFrame* outerFrame = caller->callerFrame(callerEntryFrame);
            RELEASE_ASSERT(outerFrame && outerFrame->callee().isNativeCallee());
            RefPtr outerCallee = outerFrame->callee().asNativeCallee();
            RELEASE_ASSERT(outerCallee->category() == NativeCallee::Category::Wasm);
            RefPtr outerWasmCallee = uncheckedDowncast<const Wasm::Callee>(outerCallee.get());
            RELEASE_ASSERT(outerWasmCallee->compilationMode() == Wasm::CompilationMode::IPIntMode);

            RefPtr outerIPIntCallee = uncheckedDowncast<const Wasm::IPIntCallee>(outerWasmCallee.get());
            VirtualAddress callSiteAddr = VirtualAddress::toVirtual(outerFrame->wasmInstance(), outerIPIntCallee->functionIndex(), savedWasmToJSPC(caller));
            dataLogLnIf(Options::verboseWasmDebugger(), "  [", frames.size(), "] [WASM][WasmToJS]", callSiteAddr);
            frames.append({ callSiteAddr, outerFrame, outerIPIntCallee });
            entryFrame = callerEntryFrame;
            frame = outerFrame;
            break;
        }

        default:
            // BBQ/OMG/OMGForOSREntry: disabled when the WASM debugger is active; stop.
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    }

    return frames;
}

StopData::StopData(IPIntCallee* callee, JSWebAssemblyInstance* instance, CallFrame* callFrame)
    : address(VirtualAddress::toVirtual(instance, callee->functionIndex(), callee->bytecode()))
    , callee(callee)
    , instance(instance)
    , callFrame(callFrame)
{
}

StopData::StopData(VirtualAddress address, uint8_t originalBytecode, uint8_t* pc, uint8_t* mc, IPInt::IPIntStackEntry* stack, IPIntCallee* callee, JSWebAssemblyInstance* instance, CallFrame* callFrame)
    : address(address)
    , originalBytecode(originalBytecode)
    , pc(pc)
    , mc(mc)
    , stack(stack)
    , callee(callee)
    , instance(instance)
    , callFrame(callFrame)
{
}

StopData::StopData(IPIntCallee* callee, JSWebAssemblyInstance* instance, CallFrame* callFrame, uint8_t* pc, uint8_t* mc, IPInt::IPIntStackEntry* stack, Wasm::ExceptionType type)
    : StopData(VirtualAddress::toVirtual(instance, callee->functionIndex(), pc), 0, pc, mc, stack, callee, instance, callFrame)
{
    wasmTrapType = type;
}

StopData::~StopData() = default;

void StopData::dump(PrintStream& out) const
{
    out.print("StopData(address:", address);
    out.print(", originalBytecode:", originalBytecode);
    out.print(", pc:", RawPointer(pc));
    out.print(", mc:", RawPointer(mc));
    out.print(", stack:", RawPointer(stack));
    out.print(", callee:", RawPointer(callee.get()));
    out.print(", instance:", RawPointer(instance));
    out.print(", callFrame:", RawPointer(callFrame), ")");
}

} // namespace Wasm
} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(WEBASSEMBLY_DEBUGGER)
