/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#ifndef AirInstInlines_h
#define AirInstInlines_h

#if ENABLE(B3_JIT)

#include "AirInst.h"
#include "AirOpcodeUtils.h"
#include "AirSpecial.h"
#include "B3Stackmap.h"
#include "B3Value.h"

namespace JSC { namespace B3 { namespace Air {

template<typename T> struct ForEach;
template<> struct ForEach<Tmp> {
    template<typename Functor>
    static void forEach(Inst& inst, const Functor& functor)
    {
        inst.forEachTmp(functor);
    }
};
template<> struct ForEach<Arg> {
    template<typename Functor>
    static void forEach(Inst& inst, const Functor& functor)
    {
        inst.forEachArg(functor);
    }
};

template<typename Thing, typename Functor>
void Inst::forEach(const Functor& functor)
{
    ForEach<Thing>::forEach(*this, functor);
}

inline bool Inst::hasSpecial() const
{
    return args.size() && args[0].isSpecial();
}

inline const RegisterSet& Inst::extraClobberedRegs()
{
    ASSERT(hasSpecial());
    return args[0].special()->extraClobberedRegs(*this);
}

inline void Inst::reportUsedRegisters(const RegisterSet& usedRegisters)
{
    ASSERT(hasSpecial());
    args[0].special()->reportUsedRegisters(*this, usedRegisters);
}

inline bool isShiftValid(const Inst& inst)
{
#if CPU(X86) || CPU(X86_64)
    return inst.args[0] == Tmp(X86Registers::ecx);
#else
    UNUSED_PARAM(inst);
    return true;
#endif
}   

inline bool isLshift32Valid(const Inst& inst)
{
    return isShiftValid(inst);
}

} } } // namespace JSC::B3::Air

#endif // ENABLE(B3_JIT)

#endif // AirInstInlines_h

