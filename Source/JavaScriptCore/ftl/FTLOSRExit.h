/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef FTLOSRExit_h
#define FTLOSRExit_h

#include <wtf/Platform.h>

#if ENABLE(FTL_JIT)

#include "CodeOrigin.h"
#include "DFGExitProfile.h"
#include "DFGOSRExitBase.h"
#include "FTLAbbreviations.h"
#include "FTLExitArgumentList.h"
#include "FTLExitValue.h"
#include "FTLFormattedValue.h"
#include "MethodOfGettingAValueProfile.h"
#include "Operands.h"
#include "ValueProfile.h"
#include "VirtualRegister.h"

namespace JSC { namespace FTL {

// Tracks one OSR exit site within the FTL JIT. OSR exit in FTL works by deconstructing
// the crazy that is OSR down to simple SSA CFG primitives that any compiler backend
// (including of course LLVM) can grok and do meaningful things to. Except for
// watchpoint-based exits, which haven't yet been implemented (see webkit.org/b/113647),
// an exit is just a conditional branch in the emitted code where one destination is the
// continuation and the other is a basic block that performs a no-return tail-call to an
// exit thunk. This thunk takes as its arguments the live non-constant
// not-already-accounted-for bytecode state. To appreciate how this works consider the
// following JavaScript program, and its lowering down to LLVM IR including the relevant
// exits:
//
// function foo(o) {
//     var a = o.a; // predicted int
//     var b = o.b;
//     var c = o.c; // NB this is dead
//     a = a | 5; // our example OSR exit: need to check if a is an int
//     return a + b;
// }
//
// Just consider the "a | 5". In the DFG IR, this looks like:
//
// BitOr(Check:Int32:@a, Int32:5)
//
// Where @a is the node for the GetLocal node that gets the value of the 'a' variable.
// Conceptually, this node can be further broken down to the following (note that this
// particular lowering never actually happens - we skip this step and go straight to
// LLVM IR - but it's still useful to see this):
//
// exitIf(@a is not int32);
// continuation;
//
// Where 'exitIf()' is a function that will exit if the argument is true, and
// 'continuation' is the stuff that we will do after the exitIf() check. (Note that
// FTL refers to 'exitIf()' as 'speculate()', which is in line with DFG terminology.)
// This then gets broken down to the following LLVM IR, assuming that %0 is the LLVM
// value corresponding to variable 'a', and %1 is the LLVM value for variable 'b':
//
//   %2 = ... // the predictate corresponding to '@a is not int32'
//   br i1 %2, label %3, label %4
// ; <label>:3
//   call void exitThunk1(%0, %1) // pass 'a' and 'b', since they're both live-in-bytecode
//   unreachable
// ; <label>:4
//   ... // code for the continuation
//
// Where 'exitThunk1' is the IR to get the exit thunk for *this* OSR exit. Each OSR
// exit will appear to LLVM to have a distinct exit thunk.
//
// Note that this didn't have to pass '5', 'o', or 'c' to the exit thunk. 5 is a
// constant and the DFG already knows that, and can already tell the OSR exit machinery
// what that contant is and which bytecode variables (if any) it needs to be dropped
// into. This is conveyed to the exit statically, via the OSRExit data structure below.
// See the code for ExitValue for details. 'o' is an argument, and arguments are always
// "flushed" - if you never assign them then their values are still in the argument
// stack slots, and if you do assign them then we eagerly store them into those slots.
// 'c' is dead in bytecode, and the DFG knows this; we statically tell the exit thunk
// that it's dead and don't have to pass anything. The exit thunk will "initialize" its
// value to Undefined.
//
// This approach to OSR exit has a number of virtues:
//
// - It is an entirely unsurprising representation for a compiler that already groks
//   CFG-like IRs for C-like languages. All existing analyses and transformations just
//   work.
//
// - It lends itself naturally to modern approaches to code motion. For example, you
//   could sink operations from above the exit to below it, if you just duplicate the
//   operation into the OSR exit block. This is both legal and desirable. It works
//   because the backend sees the OSR exit block as being no different than any other,
//   and LLVM already supports sinking if it sees that a value is only partially used.
//   Hence there exists a value that dominates the exit but is only used by the exit
//   thunk and not by the continuation, sinking ought to kick in for that value.
//   Hoisting operations from below it to above it is also possible, for similar
//   reasons.
//
// - The no-return tail-call to the OSR exit thunk can be subjected to specialized
//   code-size reduction optimizations, though this is optional. For example, instead
//   of actually emitting a call along with all that goes with it (like placing the
//   arguments into argument position), the backend could choose to simply inform us
//   where it had placed the arguments and expect the callee (i.e. the exit thunk) to
//   figure it out from there. It could also tell us what we need to do to pop stack,
//   although again, it doesn't have to; it could just emit that code normally. Though
//   we don't support this yet, we could; the only thing that would change on our end
//   is that we'd need feedback from the backend about the location of the arguments
//   and a description of the things that need to be done to pop stack. This would
//   involve switching the m_values array to use something more akin to ValueRecovery
//   rather than the current ExitValue, albeit possibly with some hacks to better
//   understand the kinds of places where the LLVM backend would put values.
//
// - It could be extended to allow the backend to do its own exit hoisting, by using
//   intrinsics (or meta-data, or something) to inform the backend that it's safe to
//   make the predicate passed to 'exitIf()' more truthy.
//
// - It could be extended to support watchpoints (see webkit.org/b/113647) by making
//   the predicate passed to 'exitIf()' be an intrinsic that the backend knows to be
//   true at compile-time. The backend could then turn the conditional branch into a
//   replaceable jump, much like the DFG does.

struct OSRExit : public DFG::OSRExitBase {
    OSRExit(
        ExitKind, ValueFormat profileValueFormat, MethodOfGettingAValueProfile,
        CodeOrigin, CodeOrigin originForProfile,
        unsigned numberOfArguments, unsigned numberOfLocals);
    
    MacroAssemblerCodeRef m_code;
    
    // The first argument to the exit call may be a value we wish to profile.
    // If that's the case, the format will be not Invalid and we'll have a
    // method of getting a value profile. Note that all of the ExitArgument's
    // are already aware of this possible off-by-one, so there is no need to
    // correct them.
    ValueFormat m_profileValueFormat;
    MethodOfGettingAValueProfile m_valueProfile;
    
    // Offset within the exit stubs of the stub for this exit.
    unsigned m_patchableCodeOffset;
    
    Operands<ExitValue> m_values;
    
    uint32_t m_stackmapID;
    
    CodeLocationJump codeLocationForRepatch(CodeBlock* ftlCodeBlock) const;
    
    bool considerAddingAsFrequentExitSite(CodeBlock* profiledCodeBlock)
    {
        return OSRExitBase::considerAddingAsFrequentExitSite(profiledCodeBlock, ExitFromFTL);
    }
};

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

#endif // FTLOSRExit_h

