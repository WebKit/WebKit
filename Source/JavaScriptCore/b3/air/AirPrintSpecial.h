/*
 * Copyright (C) 2017-2023 Apple Inc. All rights reserved.
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

#if ENABLE(B3_JIT)

#include "AirInst.h"
#include "AirSpecial.h"
#include "MacroAssemblerPrinter.h"
#include <wtf/SequesteredMalloc.h>
#include <wtf/TZoneMalloc.h>

namespace JSC {

class CCallHelpers;

namespace Printer {

typedef Vector<B3::Air::Arg> ArgList;

template<typename T, typename U>
concept IsSameOrReference = std::same_as<T, U> || std::same_as<T, U&>;
    
template<typename T, typename... Arguments>
    requires (IsSameOrReference<T, B3::Air::Tmp> || IsSameOrReference<T, Reg>)
inline void appendAirArg(B3::Air::Inst& inst, T&& arg)
{
    inst.args.append(std::forward<T>(arg));
}

template<typename T, typename... Arguments>
    requires (!IsSameOrReference<T, B3::Air::Tmp> && !IsSameOrReference<T, Reg>)
inline void appendAirArg(B3::Air::Inst&, T&&, int = 0) { }

inline void appendAirArgs(B3::Air::Inst&) { }

template<typename T, typename... Arguments>
inline void appendAirArgs(B3::Air::Inst& inst, T&& t, Arguments&&... others)
{
    appendAirArg(inst, std::forward<T>(t));
    appendAirArgs(inst, std::forward<Arguments>(others)...);
}

[[noreturn]] void printAirArg(PrintStream&, Context&);

// Printer<Arg&> is only a place-holder which PrintSpecial::generate() will later
// replace with a Printer for a register, constant, etc. as appropriate. 
template<>
struct Printer<B3::Air::Tmp> : public PrintRecord {
    Printer(B3::Air::Tmp&)
        : PrintRecord(printAirArg)
    { }
};
    
template<>
struct Printer<Reg> : public PrintRecord {
    Printer(Reg&)
        : PrintRecord(printAirArg)
    { }
};

} // namespace Printer

namespace B3 { namespace Air {

class PrintSpecial final : public Special {
    WTF_MAKE_SEQUESTERED_ARENA_ALLOCATED(PrintSpecial);
public:
    PrintSpecial(Printer::PrintRecordList*);
    ~PrintSpecial() final;
    
    // You cannot use this register to pass arguments. It just so happens that this register is not
    // used for arguments in the C calling convention. By the way, this is the only thing that causes
    // this special to be specific to C calls.
    static constexpr GPRReg scratchRegister = GPRInfo::nonArgGPR0;
    
private:
    void forEachArg(Inst&, const ScopedLambda<Inst::EachArgCallback>&) final;
    bool isValid(Inst&) final;
    bool admitsStack(Inst&, unsigned argIndex) final;
    bool admitsExtendedOffsetAddr(Inst&, unsigned) final;
    void reportUsedRegisters(Inst&, const RegisterSet&) final;
    MacroAssembler::Jump generate(Inst&, CCallHelpers&, GenerationContext&) final;
    RegisterSet extraEarlyClobberedRegs(Inst&) final;
    RegisterSet extraClobberedRegs(Inst&) final;
    
    void dumpImpl(PrintStream&) const final;
    void deepDumpImpl(PrintStream&) const final;
    
    static constexpr unsigned specialArgOffset = 0;
    static constexpr unsigned numSpecialArgs = 1;
    static constexpr unsigned calleeArgOffset = numSpecialArgs;
    static constexpr unsigned numCalleeArgs = 1;
    static constexpr unsigned returnGPArgOffset = numSpecialArgs + numCalleeArgs;
    static constexpr unsigned numReturnGPArgs = 2;
    static constexpr unsigned returnFPArgOffset = numSpecialArgs + numCalleeArgs + numReturnGPArgs;
    static constexpr unsigned numReturnFPArgs = 1;
    static constexpr unsigned argArgOffset =
    numSpecialArgs + numCalleeArgs + numReturnGPArgs + numReturnFPArgs;
    
    Printer::PrintRecordList* m_printRecordList;
};

} } } // namespace JSC::B3::Air

#endif // ENABLE(B3_JIT)
