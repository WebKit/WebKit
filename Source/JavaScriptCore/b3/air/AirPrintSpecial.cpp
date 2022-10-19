/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "AirPrintSpecial.h"

#if ENABLE(B3_JIT)

#include "CCallHelpers.h"
#include "MacroAssemblerPrinter.h"

namespace JSC { namespace B3 { namespace Air {

PrintSpecial::PrintSpecial(Printer::PrintRecordList* list)
    : m_printRecordList(list)
{
}

PrintSpecial::~PrintSpecial()
{
}

void PrintSpecial::forEachArg(Inst&, const ScopedLambda<Inst::EachArgCallback>&)
{
}

bool PrintSpecial::isValid(Inst&)
{
    return true;
}

bool PrintSpecial::admitsStack(Inst&, unsigned)
{
    return false;
}

bool PrintSpecial::admitsExtendedOffsetAddr(Inst&, unsigned)
{
    return false;
}

void PrintSpecial::reportUsedRegisters(Inst&, const RegisterSetBuilder&)
{
}

MacroAssembler::Jump PrintSpecial::generate(Inst& inst, CCallHelpers& jit, GenerationContext&)
{
    size_t currentArg = 1; // Skip the PrintSpecial arg.
    for (auto& term : *m_printRecordList) {
        if (term.printer == Printer::printAirArg) {
            const Arg& arg = inst.args[currentArg++];
            switch (arg.kind()) {
            case Arg::Tmp:
                term = Printer::Printer<MacroAssembler::RegisterID>(arg.gpr());
                break;
            case Arg::Addr:
            case Arg::ExtendedOffsetAddr:
                term = Printer::Printer<MacroAssembler::Address>(arg.asAddress());
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }
        }
    }
    jit.print(m_printRecordList);
    return CCallHelpers::Jump();
}

RegisterSetBuilder PrintSpecial::extraEarlyClobberedRegs(Inst&)
{
    return { };
}

RegisterSetBuilder PrintSpecial::extraClobberedRegs(Inst&)
{
    return { };
}

void PrintSpecial::dumpImpl(PrintStream& out) const
{
    out.print("Print");
}

void PrintSpecial::deepDumpImpl(PrintStream& out) const
{
    out.print("print for debugging logging.");
}

} } // namespace B3::Air

namespace Printer {

NO_RETURN void printAirArg(PrintStream&, Context&)
{
    // This function is only a placeholder to let PrintSpecial::generate() know that
    // the Printer needs to be replaced with one for a register, constant, etc. Hence,
    // this function should never be called.
    RELEASE_ASSERT_NOT_REACHED();
}

} // namespace Printer

} // namespace JSC

#endif // ENABLE(B3_JIT)
