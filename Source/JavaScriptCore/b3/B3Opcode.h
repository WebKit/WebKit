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

#ifndef B3Opcode_h
#define B3Opcode_h

#if ENABLE(B3_JIT)

#include "B3Type.h"
#include <wtf/Optional.h>
#include <wtf/StdLibExtras.h>

namespace JSC { namespace B3 {

enum Opcode : int16_t {
    // A no-op that returns Void, useful for when you want to remove a value.
    Nop,
    
    // Polymorphic identity, usable with any value type.
    Identity,

    // Constants. Use the ConstValue* classes. Constants exist in the control flow, so that we can
    // reason about where we would construct them. Large constants are expensive to create.
    Const32,
    Const64,
    ConstDouble,
    ConstFloat,

    // The magical stack slot. This is viewed as executing at the top of the program regardless of
    // where in control flow you put it. Each instance of a StackSlot Value gets a disjoint range of
    // stack memory. Use the StackSlotValue class.
    StackSlot,

    // The magical argument register. This is viewed as executing at the top of the program
    // regardless of where in control flow you put it, and the compiler takes care to ensure that we
    // don't clobber the value by register allocation or calls (either by saving the argument to the
    // stack or preserving it in a callee-save register). Use the ArgumentRegValue class. The return
    // type is either pointer() (for GPRs) or Double (for FPRs).
    ArgumentReg,

    // The frame pointer. You can put this anywhere in control flow but it will always yield the
    // frame pointer, with a caveat: if our compiler changes the frame pointer temporarily for some
    // silly reason, the FramePointer intrinsic will return where the frame pointer *should* be not
    // where it happens to be right now.
    FramePointer,

    // Polymorphic math, usable with any value type.
    Add,
    Sub,
    Mul,
    Div, // All bets are off as to what will happen when you execute this for -2^31/-1 and x/0.
    Mod, // All bets are off as to what will happen when you execute this for -2^31%-1 and x%0.

    // Integer math.
    ChillDiv, // doesn't trap ever, behaves like JS (x/y)|0.
    ChillMod, // doesn't trap ever, behaves like JS (x%y)|0.
    BitAnd,
    BitOr,
    BitXor,
    Shl,
    SShr, // Arithmetic Shift.
    ZShr, // Logical Shift.
    Clz, // Count leading zeros.

    // Double math.
    Sqrt,

    // Casts and such.
    // Bitwise Cast of Double->Int64 or Int64->Double
    BitwiseCast,
    // Takes and returns Int32:
    SExt8,
    SExt16,
    // Takes Int32 and returns Int64:
    SExt32,
    ZExt32,
    // Takes Int64 and returns Int32:
    Trunc,
    // Takes ints and returns Double:
    IToD,
    // Takes Double and returns Int32:
    DToI32,
    // Convert between double and float.
    FloatToDouble,
    DoubleToFloat,

    // Polymorphic comparisons, usable with any value type. Returns int32 0 or 1. Note that "Not"
    // is just Equal(x, 0), and "ToBoolean" is just NotEqual(x, 0).
    Equal,
    NotEqual,
    LessThan,
    GreaterThan,
    LessEqual,
    GreaterEqual,

    // Integer comparisons. Returns int32 0 or 1.
    Above,
    Below,
    AboveEqual,
    BelowEqual,

    // SSA form of conditional move. The first child is evaluated for truthiness. If true, the second child
    // is returned. Otherwise, the third child is returned.
    Select,

    // Memory loads. Opcode indicates how we load and the loaded type. These use MemoryValue.
    // These return Int32:
    Load8Z,
    Load8S,
    Load16Z,
    Load16S,
    // This returns whatever the return type is:
    Load,

    // Memory stores. Opcode indicates how the value is stored. These use MemoryValue.
    // These take an Int32 value:
    Store8,
    Store16,
    // This is a polymorphic store for Int32, Int64, Float, and Double.
    Store,

    // This is a regular ordinary C function call, using the system C calling convention. Make sure
    // that the arguments are passed using the right types. The first argument is the callee.
    CCall,

    // This is a patchpoint. Use the PatchpointValue class. This is viewed as behaving like a call,
    // but only emits code via a code generation callback. That callback gets to emit code inline.
    // You can pass a stackmap along with constraints on how each stackmap argument must be passed.
    // It's legal to request that a stackmap argument is in some register and it's legal to request
    // that a stackmap argument is at some offset from the top of the argument passing area on the
    // stack.
    Patchpoint,

    // Checked math. Use the CheckValue class. Like a Patchpoint, this takes a code generation
    // callback. That callback gets to emit some code after the epilogue, and gets to link the jump
    // from the check, and the choice of registers. You also get to supply a stackmap. Note that you
    // are not allowed to jump back into the mainline code from your slow path, since the compiler
    // will assume that the execution of these instructions proves that overflow didn't happen. For
    // example, if you have two CheckAdd's:
    //
    // a = CheckAdd(x, y)
    // b = CheckAdd(x, y)
    //
    // Then it's valid to change this to:
    //
    // a = CheckAdd(x, y)
    // b = Identity(a)
    //
    // This is valid regardless of the callbacks used by the two CheckAdds. They may have different
    // callbacks. Yet, this transformation is valid even if they are different because we know that
    // after the first CheckAdd executes, the second CheckAdd could not have possibly taken slow
    // path. Therefore, the second CheckAdd's callback is irrelevant.
    //
    // Note that the first two children of these operations have ValueRep's, both as input constraints and
    // in the reps provided to the generator. The output constraints could be anything, and should not be
    // inspected for meaning. If you want to capture the values of the inputs, use stackmap arguments.
    CheckAdd,
    CheckSub,
    CheckMul,

    // Check that side-exits. Use the CheckValue class. Like CheckAdd and friends, this has a
    // stackmap with a generation callback. This takes an int argument that this branches on, with
    // full branch fusion in the instruction selector. A true value jumps to the generator's slow
    // path. Note that the predicate child is has both an input and output ValueRep. The input constraint
    // must be Any, and the output could be anything.
    Check,

    // SSA support, in the style of DFG SSA.
    Upsilon, // This uses the UpsilonValue class.
    Phi,

    // Jump. Uses the ControlValue class.
    Jump,
    
    // Polymorphic branch, usable with any integer type. Branches if not equal to zero. Uses the
    // ControlValue class, with the 0-index successor being the true successor.
    Branch,

    // Switch. Switches over either Int32 or Int64. Uses the SwitchValue class.
    Switch,

    // Return. Note that B3 procedures don't know their return type, so this can just return any
    // type. Uses the ControlValue class.
    Return,

    // This is a terminal that indicates that we will never get here. Uses the ControlValue class.
    Oops
};

inline bool isCheckMath(Opcode opcode)
{
    switch (opcode) {
    case CheckAdd:
    case CheckSub:
    case CheckMul:
        return true;
    default:
        return false;
    }
}

Optional<Opcode> invertedCompare(Opcode, Type);

inline Opcode constPtrOpcode()
{
    if (is64Bit())
        return Const64;
    return Const32;
}

inline bool isConstant(Opcode opcode)
{
    switch (opcode) {
    case Const32:
    case Const64:
    case ConstDouble:
    case ConstFloat:
        return true;
    default:
        return false;
    }
}

} } // namespace JSC::B3

namespace WTF {

class PrintStream;

void printInternal(PrintStream&, JSC::B3::Opcode);

} // namespace WTF

#endif // ENABLE(B3_JIT)

#endif // B3Opcode_h

