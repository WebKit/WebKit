/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#ifndef AirCustom_h
#define AirCustom_h

#if ENABLE(B3_JIT)

#include "AirInst.h"
#include "AirSpecial.h"

namespace JSC { namespace B3 { namespace Air {

// This defines the behavior of custom instructions - i.e. those whose behavior cannot be
// described using AirOpcode.opcodes. If you define an opcode as "custom Foo" in that file, then
// you will need to create a "struct FooCustom" here that implements the custom behavior
// methods.
//
// The customizability granted by the custom instruction mechanism is strictly less than what
// you get using the Patch instruction and implementing a Special. However, that path requires
// allocating a Special object and ensuring that it's the first operand. For many instructions,
// that is not as convenient as using Custom, which makes the instruction look like any other
// instruction. Note that both of those extra powers of the Patch instruction happen because we
// special-case that instruction in many phases and analyses. Non-special-cased behaviors of
// Patch are implemented using the custom instruction mechanism.
//
// Specials are still more flexible if you need to list extra clobbered registers and you'd like
// that to be expressed as a bitvector rather than an arglist. They are also more flexible if
// you need to carry extra state around with the instruction. Also, Specials mean that you
// always have access to Code& even in methods that don't take a GenerationContext.

struct PatchCustom {
    template<typename Functor>
    static void forEachArg(Inst& inst, const Functor& functor)
    {
        // This is basically bogus, but it works for analyses that model Special as an
        // immediate.
        functor(inst.args[0], Arg::Use, Arg::GP, Arg::pointerWidth());
        
        inst.args[0].special()->forEachArg(inst, scopedLambda<Inst::EachArgCallback>(functor));
    }

    template<typename... Arguments>
    static bool isValidFormStatic(Arguments...)
    {
        return false;
    }

    static bool isValidForm(Inst& inst)
    {
        if (inst.args.size() < 1)
            return false;
        if (!inst.args[0].isSpecial())
            return false;
        return inst.args[0].special()->isValid(inst);
    }

    static bool admitsStack(Inst& inst, unsigned argIndex)
    {
        if (!argIndex)
            return false;
        return inst.args[0].special()->admitsStack(inst, argIndex);
    }

    static bool hasNonArgNonControlEffects(Inst& inst)
    {
        return inst.args[0].special()->hasNonArgNonControlEffects();
    }

    static CCallHelpers::Jump generate(
        Inst& inst, CCallHelpers& jit, GenerationContext& context)
    {
        return inst.args[0].special()->generate(inst, jit, context);
    }
};

} } } // namespace JSC::B3::Air

#endif // ENABLE(B3_JIT)

#endif // AirCustom_h

