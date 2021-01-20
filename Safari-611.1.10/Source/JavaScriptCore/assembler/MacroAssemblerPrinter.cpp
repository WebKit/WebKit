/*
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
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
#include "MacroAssemblerPrinter.h"

#if ENABLE(ASSEMBLER)
#if ENABLE(MASM_PROBE)

#include "MacroAssembler.h"
#include <inttypes.h>

namespace JSC {

namespace Printer {

using CPUState = Probe::CPUState;
using RegisterID = MacroAssembler::RegisterID;
using FPRegisterID = MacroAssembler::FPRegisterID;

template<typename T> T nextID(T id) { return static_cast<T>(id + 1); }

void printAllRegisters(PrintStream& out, Context& context)
{
    auto& cpu = context.probeContext.cpu;
    unsigned charsToIndent = context.data.as<unsigned>();

    auto indent = [&] () {
        for (unsigned i = 0; i < charsToIndent; ++i)
            out.print(" ");
    };
#define INDENT indent()

    INDENT; out.print("cpu: {\n");

#if USE(JSVALUE32_64)
    #define INTPTR_HEX_VALUE_FORMAT "0x%08" PRIxPTR
#else
    #define INTPTR_HEX_VALUE_FORMAT "0x%016" PRIxPTR
#endif

    for (auto id = MacroAssembler::firstRegister(); id <= MacroAssembler::lastRegister(); id = nextID(id)) {
        intptr_t value = static_cast<intptr_t>(cpu.gpr(id));
        INDENT; out.printf("    %6s: " INTPTR_HEX_VALUE_FORMAT "  %" PRIdPTR "\n", cpu.gprName(id), value, value);
    }
    for (auto id = MacroAssembler::firstSPRegister(); id <= MacroAssembler::lastSPRegister(); id = nextID(id)) {
        intptr_t value = static_cast<intptr_t>(cpu.spr(id));
        INDENT; out.printf("    %6s: " INTPTR_HEX_VALUE_FORMAT "  %" PRIdPTR "\n", cpu.sprName(id), value, value);
    }
    #undef INTPTR_HEX_VALUE_FORMAT

    for (auto id = MacroAssembler::firstFPRegister(); id <= MacroAssembler::lastFPRegister(); id = nextID(id)) {
        uint64_t u = bitwise_cast<uint64_t>(cpu.fpr(id));
        double d = cpu.fpr(id);
        INDENT; out.printf("    %6s: 0x%016" PRIx64 "  %.13g\n", cpu.fprName(id), u, d);
    }

    INDENT; out.print("}\n");
#undef INDENT

}

void printPCRegister(PrintStream& out, Context& context)
{
    auto cpu = context.probeContext.cpu;
    void* value = cpu.pc();
    out.printf("pc:<%p %" PRIdPTR ">", value, bitwise_cast<intptr_t>(value));
}

void printRegisterID(PrintStream& out, Context& context)
{
    RegisterID regID = context.data.as<RegisterID>();
    const char* name = CPUState::gprName(regID);
    intptr_t value = context.probeContext.gpr(regID);
    out.printf("%s:<%p %" PRIdPTR ">", name, bitwise_cast<void*>(value), value);
}

void printFPRegisterID(PrintStream& out, Context& context)
{
    FPRegisterID regID = context.data.as<FPRegisterID>();
    const char* name = CPUState::fprName(regID);
    double value = context.probeContext.fpr(regID);
    out.printf("%s:<0x%016" PRIx64 " %.13g>", name, bitwise_cast<uint64_t>(value), value);
}

void printAddress(PrintStream& out, Context& context)
{
    MacroAssembler::Address address = context.data.as<MacroAssembler::Address>();
    RegisterID regID = address.base;
    const char* name = CPUState::gprName(regID);
    intptr_t value = context.probeContext.gpr(regID);
    out.printf("Address{base:%s:<%p %" PRIdPTR ">, offset:<0x%x %d>", name, bitwise_cast<void*>(value), value, address.offset, address.offset);
}

void printMemory(PrintStream& out, Context& context)
{
    const Memory& memory = context.data.as<Memory>();

    uint8_t* ptr = nullptr;
    switch (memory.addressType) {
    case Memory::AddressType::Address: {
        ptr = reinterpret_cast<uint8_t*>(context.probeContext.gpr(memory.u.address.base));
        ptr += memory.u.address.offset;
        break;
    }
    case Memory::AddressType::AbsoluteAddress: {
        ptr = reinterpret_cast<uint8_t*>(const_cast<void*>(memory.u.absoluteAddress.m_ptr));
        break;
    }
    }

    if (memory.dumpStyle == Memory::SingleWordDump) {
        if (memory.numBytes == sizeof(int8_t)) {
            auto p = reinterpret_cast<int8_t*>(ptr);
            out.printf("%p:<0x%02x %d>", p, *p, *p);
            return;
        }
        if (memory.numBytes == sizeof(int16_t)) {
            auto p = bitwise_cast<int16_t*>(ptr);
            out.printf("%p:<0x%04x %d>", p, *p, *p);
            return;
        }
        if (memory.numBytes == sizeof(int32_t)) {
            auto p = bitwise_cast<int32_t*>(ptr);
            out.printf("%p:<0x%08x %d>", p, *p, *p);
            return;
        }
        if (memory.numBytes == sizeof(int64_t)) {
            auto p = bitwise_cast<int64_t*>(ptr);
            out.printf("%p:<0x%016" PRIx64 " %" PRId64 ">", p, *p, *p);
            return;
        }
        // Else, unknown word size. Fall thru and dump in the generic way.
    }

    // Generic dump: dump rows of 16 bytes in 4 byte groupings.
    size_t numBytes = memory.numBytes;
    for (size_t i = 0; i < numBytes; i++) {
        if (!(i % 16))
            out.printf("%p: ", &ptr[i]);
        else if (!(i % 4))
            out.printf(" ");

        out.printf("%02x", ptr[i]);

        if (i % 16 == 15)
            out.print("\n");
    }
    if (numBytes % 16 < 15)
        out.print("\n");
}

void printCallback(Probe::Context& probeContext)
{
    auto& out = WTF::dataFile();
    PrintRecordList& list = *probeContext.arg<PrintRecordList*>();
    for (size_t i = 0; i < list.size(); i++) {
        auto& record = list[i];
        Context context(probeContext, record.data);
        record.printer(out, context);
    }
}

} // namespace Printer
} // namespace JSC

#endif // ENABLE(MASM_PROBE)
#endif // ENABLE(ASSEMBLER)
