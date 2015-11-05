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
#include "B3Value.h"

#if ENABLE(B3_JIT)

#include "B3ArgumentRegValue.h"
#include "B3CCallValue.h"
#include "B3ControlValue.h"
#include "B3MemoryValue.h"
#include "B3StackSlotValue.h"
#include "B3UpsilonValue.h"
#include "B3ValueInlines.h"
#include <wtf/CommaPrinter.h>
#include <wtf/StringPrintStream.h>

namespace JSC { namespace B3 {

const char* const Value::dumpPrefix = "@";

Value::~Value()
{
}

void Value::replaceWithIdentity(Value* value)
{
    // This is a bit crazy. It does an in-place replacement of whatever Value subclass this is with
    // a plain Identity Value. We first collect all of the information we need, then we destruct the
    // previous value in place, and then we construct the Identity Value in place.

    unsigned index = m_index;
    Type type = m_type;
    Origin origin = m_origin;
    BasicBlock* owner = this->owner;

    RELEASE_ASSERT(type == value->type());

    this->Value::~Value();

    new (this) Value(index, Identity, type, origin, value);

    this->owner = owner;
}

void Value::replaceWithNop()
{
    unsigned index = m_index;
    Origin origin = m_origin;
    BasicBlock* owner = this->owner;

    this->Value::~Value();

    new (this) Value(index, Nop, Void, origin);

    this->owner = owner;
}

void Value::dump(PrintStream& out) const
{
    out.print(dumpPrefix, m_index);
}

void Value::dumpChildren(CommaPrinter& comma, PrintStream& out) const
{
    for (Value* child : children())
        out.print(comma, pointerDump(child));
}

void Value::deepDump(PrintStream& out) const
{
    out.print(m_type, " ", *this, " = ", m_opcode);

    out.print("(");
    CommaPrinter comma;
    dumpChildren(comma, out);

    if (m_origin)
        out.print(comma, m_origin);

    dumpMeta(comma, out);

    {
        CString string = toCString(effects());
        if (string.length())
            out.print(comma, string);
    }

    out.print(")");
}

Value* Value::negConstant(Procedure&) const
{
    return nullptr;
}

Value* Value::addConstant(Procedure&, int32_t) const
{
    return nullptr;
}

Value* Value::addConstant(Procedure&, Value*) const
{
    return nullptr;
}

Value* Value::subConstant(Procedure&, Value*) const
{
    return nullptr;
}

Value* Value::bitAndConstant(Procedure&, Value*) const
{
    return nullptr;
}

Value* Value::bitOrConstant(Procedure&, Value*) const
{
    return nullptr;
}

Value* Value::bitXorConstant(Procedure&, Value*) const
{
    return nullptr;
}

Value* Value::shlConstant(Procedure&, Value*) const
{
    return nullptr;
}

Value* Value::sShrConstant(Procedure&, Value*) const
{
    return nullptr;
}

Value* Value::zShrConstant(Procedure&, Value*) const
{
    return nullptr;
}

Value* Value::equalConstant(Procedure&, Value*) const
{
    return nullptr;
}

Value* Value::notEqualConstant(Procedure&, Value*) const
{
    return nullptr;
}

bool Value::returnsBool() const
{
    if (type() != Int32)
        return false;
    switch (opcode()) {
    case Const32:
        return asInt32() == 0 || asInt32() == 1;
    case BitAnd:
        return child(1)->isInt32(1);
    case Equal:
    case NotEqual:
    case LessThan:
    case GreaterThan:
    case LessEqual:
    case GreaterEqual:
    case Above:
    case Below:
    case AboveEqual:
    case BelowEqual:
        return true;
    case Phi:
        // FIXME: We should have a story here.
        // https://bugs.webkit.org/show_bug.cgi?id=150725
        return false;
    default:
        return false;
    }
}

TriState Value::asTriState() const
{
    switch (opcode()) {
    case Const32:
        return triState(!!asInt32());
    case Const64:
        return triState(!!asInt64());
    case ConstDouble:
        // Use "!= 0" to really emphasize what this mean with respect to NaN and such.
        return triState(asDouble() != 0);
    default:
        return MixedTriState;
    }
}

Effects Value::effects() const
{
    Effects result;
    switch (opcode()) {
    case Nop:
    case Identity:
    case Const32:
    case Const64:
    case ConstDouble:
    case StackSlot:
    case ArgumentReg:
    case FramePointer:
    case Add:
    case Sub:
    case Mul:
    case ChillDiv:
    case Mod:
    case BitAnd:
    case BitOr:
    case BitXor:
    case Shl:
    case SShr:
    case ZShr:
    case SExt8:
    case SExt16:
    case SExt32:
    case ZExt32:
    case Trunc:
    case FRound:
    case IToD:
    case DToI32:
    case Equal:
    case NotEqual:
    case LessThan:
    case GreaterThan:
    case LessEqual:
    case GreaterEqual:
    case Above:
    case Below:
    case AboveEqual:
    case BelowEqual:
        break;
    case Div:
        result.controlDependent = true;
        break;
    case Load8Z:
    case Load8S:
    case Load16Z:
    case Load16S:
    case LoadFloat:
    case Load:
        result.reads = as<MemoryValue>()->range();
        result.controlDependent = true;
        break;
    case Store8:
    case Store16:
    case StoreFloat:
    case Store:
        result.writes = as<MemoryValue>()->range();
        result.controlDependent = true;
        break;
    case CCall:
        result = as<CCallValue>()->effects;
        break;
    case Patchpoint:
        result = as<PatchpointValue>()->effects;
        break;
    case CheckAdd:
    case CheckSub:
    case CheckMul:
    case Check:
        result.exitsSideways = true;
        break;
    case Upsilon:
        result.writesSSAState = true;
        break;
    case Phi:
        result.readsSSAState = true;
        break;
    case Jump:
    case Branch:
    case Switch:
    case Return:
    case Oops:
        result.terminal = true;
        break;
    }
    return result;
}

void Value::performSubstitution()
{
    for (Value*& child : children()) {
        while (child->opcode() == Identity)
            child = child->child(0);
    }
}

void Value::dumpMeta(CommaPrinter&, PrintStream&) const
{
}

#if !ASSERT_DISABLED
void Value::checkOpcode(Opcode opcode)
{
    ASSERT(!ArgumentRegValue::accepts(opcode));
    ASSERT(!CCallValue::accepts(opcode));
    ASSERT(!CheckValue::accepts(opcode));
    ASSERT(!Const32Value::accepts(opcode));
    ASSERT(!Const64Value::accepts(opcode));
    ASSERT(!ConstDoubleValue::accepts(opcode));
    ASSERT(!ControlValue::accepts(opcode));
    ASSERT(!MemoryValue::accepts(opcode));
    ASSERT(!PatchpointValue::accepts(opcode));
    ASSERT(!StackSlotValue::accepts(opcode));
    ASSERT(!UpsilonValue::accepts(opcode));
}
#endif // !ASSERT_DISABLED

Type Value::typeFor(Opcode opcode, Value* firstChild)
{
    switch (opcode) {
    case Identity:
    case Add:
    case Sub:
    case Mul:
    case Div:
    case ChillDiv:
    case Mod:
    case BitAnd:
    case BitOr:
    case BitXor:
    case Shl:
    case SShr:
    case ZShr:
    case CheckAdd:
    case CheckSub:
    case CheckMul:
        return firstChild->type();
    case FramePointer:
        return pointerType();
    case SExt8:
    case SExt16:
    case Trunc:
    case DToI32:
    case Equal:
    case NotEqual:
    case LessThan:
    case GreaterThan:
    case LessEqual:
    case GreaterEqual:
    case Above:
    case Below:
    case AboveEqual:
    case BelowEqual:
        return Int32;
    case SExt32:
    case ZExt32:
        return Int64;
    case FRound:
    case IToD:
        return Double;
    case Nop:
        return Void;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)
