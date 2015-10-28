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

#include "config.h"
#include "AirRegisterPriority.h"

#if ENABLE(B3_JIT)

#include "RegisterSet.h"

namespace JSC { namespace B3 { namespace Air {

const Vector<GPRReg>& gprsInPriorityOrder()
{
    static Vector<GPRReg>* result;
    static std::once_flag once;
    std::call_once(
        once,
        [] {
            result = new Vector<GPRReg>();
            RegisterSet all = RegisterSet::allGPRs();
            all.exclude(RegisterSet::stackRegisters());
            all.exclude(RegisterSet::reservedHardwareRegisters());
            RegisterSet calleeSave = RegisterSet::calleeSaveRegisters();
            all.forEach(
                [&] (Reg reg) {
                    if (!calleeSave.get(reg))
                        result->append(reg.gpr());
                });
            all.forEach(
                [&] (Reg reg) {
                    if (calleeSave.get(reg))
                        result->append(reg.gpr());
                });
        });
    return *result;
}

const Vector<FPRReg>& fprsInPriorityOrder()
{
    static Vector<FPRReg>* result;
    static std::once_flag once;
    std::call_once(
        once,
        [] {
            result = new Vector<FPRReg>();
            RegisterSet all = RegisterSet::allFPRs();
            RegisterSet calleeSave = RegisterSet::calleeSaveRegisters();
            all.forEach(
                [&] (Reg reg) {
                    if (!calleeSave.get(reg))
                        result->append(reg.fpr());
                });
            all.forEach(
                [&] (Reg reg) {
                    if (calleeSave.get(reg))
                        result->append(reg.fpr());
                });
        });
    return *result;
}

const Vector<Reg>& regsInPriorityOrder(Arg::Type type)
{
    static Vector<Reg>* result[Arg::numTypes];
    static std::once_flag once;
    std::call_once(
        once,
        [] {
            result[Arg::GP] = new Vector<Reg>();
            for (GPRReg reg : gprsInPriorityOrder())
                result[Arg::GP]->append(Reg(reg));
            result[Arg::FP] = new Vector<Reg>();
            for (FPRReg reg : fprsInPriorityOrder())
                result[Arg::FP]->append(Reg(reg));
        });
    return *result[type];
}

} } } // namespace JSC::B3::Air

#endif // ENABLE(B3_JIT)
