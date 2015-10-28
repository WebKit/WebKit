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
#include "B3CheckSpecial.h"

#if ENABLE(B3_JIT)

#include "AirCode.h"
#include "AirGenerationContext.h"
#include "AirInstInlines.h"
#include "B3ValueInlines.h"

namespace JSC { namespace B3 {

using namespace Air;

namespace {

unsigned numB3Args(Inst& inst)
{
    switch (inst.origin->opcode()) {
    case CheckAdd:
    case CheckSub:
    case CheckMul:
        // ResCond, Tmp, Tmp
        return 3;
    case Check:
        return 1;
    default:
        RELEASE_ASSERT_NOT_REACHED();
        return 0;
    }
}

} // anonymous namespace

CheckSpecial::CheckSpecial(Air::Opcode opcode, unsigned numArgs)
    : m_checkOpcode(opcode)
    , m_numCheckArgs(numArgs)
{
    ASSERT(isTerminal(opcode));
}

CheckSpecial::~CheckSpecial()
{
}

Inst CheckSpecial::hiddenBranch(const Inst& inst) const
{
    Inst hiddenBranch(m_checkOpcode, inst.origin);
    hiddenBranch.args.reserveInitialCapacity(m_numCheckArgs);
    for (unsigned i = 0; i < m_numCheckArgs; ++i)
        hiddenBranch.args.append(inst.args[i + 1]);
    return hiddenBranch;
}

void CheckSpecial::forEachArg(Inst& inst, const ScopedLambda<Inst::EachArgCallback>& callback)
{
    hiddenBranch(inst).forEachArg(callback);
    forEachArgImpl(numB3Args(inst), m_numCheckArgs + 1, inst, callback);
}

bool CheckSpecial::isValid(Inst& inst)
{
    return hiddenBranch(inst).isValidForm()
        && isValidImpl(numB3Args(inst), m_numCheckArgs + 1, inst);
}

bool CheckSpecial::admitsStack(Inst& inst, unsigned argIndex)
{
    if (argIndex >= 1 && argIndex < 1 + m_numCheckArgs)
        return hiddenBranch(inst).admitsStack(argIndex - 1);
    return admitsStackImpl(numB3Args(inst), m_numCheckArgs + 1, inst, argIndex);
}

CCallHelpers::Jump CheckSpecial::generate(Inst& inst, CCallHelpers& jit, GenerationContext& context)
{
    CCallHelpers::Jump fail = hiddenBranch(inst).generate(jit, context);
    ASSERT(fail.isSet());

    Value* value = inst.origin;

    Vector<ValueRep> reps;
    if (isCheckMath(value->opcode())) {
        if (value->opcode() == CheckMul)
            reps.append(ValueRep());
        else if (value->opcode() == CheckSub && value->child(0)->isInt(0))
            reps.append(ValueRep::constant(0));
        else
            reps.append(repForArg(*context.code, inst.args[3]));
        reps.append(repForArg(*context.code, inst.args[2]));
    } else {
        ASSERT(value->opcode() == Check);
        reps.append(ValueRep::constant(1));
    }

    appendRepsImpl(context, m_numCheckArgs + 1, inst, reps);
    
    context.latePaths.append(
        createSharedTask<GenerationContext::LatePathFunction>(
            [=] (CCallHelpers& jit, GenerationContext&) {
                fail.link(&jit);
                
                Stackmap* stackmap = value->stackmap();
                ASSERT(stackmap);

                Stackmap::GenerationParams params;
                params.value = value;
                params.stackmap = stackmap;
                params.reps = reps;
                params.usedRegisters = stackmap->m_usedRegisters;

                stackmap->m_generator->run(jit, params);
            }));

    return CCallHelpers::Jump(); // As far as Air thinks, we are not a terminal.
}

void CheckSpecial::dumpImpl(PrintStream& out) const
{
    out.print(m_checkOpcode, "(", m_numCheckArgs, ")");
}

void CheckSpecial::deepDumpImpl(PrintStream& out) const
{
    out.print("B3::CheckValue lowered to ", m_checkOpcode, " with ", m_numCheckArgs, " args.");
}

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)
