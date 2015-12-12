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
#include "B3Validate.h"

#if ENABLE(B3_JIT)

#include "B3ArgumentRegValue.h"
#include "B3BasicBlockInlines.h"
#include "B3MemoryValue.h"
#include "B3Procedure.h"
#include "B3StackSlotValue.h"
#include "B3UpsilonValue.h"
#include "B3ValueInlines.h"
#include <wtf/HashSet.h>
#include <wtf/StringPrintStream.h>
#include <wtf/text/CString.h>

namespace JSC { namespace B3 {

namespace {

class Validater {
public:
    Validater(Procedure& procedure, const char* dumpBefore)
        : m_procedure(procedure)
        , m_dumpBefore(dumpBefore)
    {
    }

#define VALIDATE(condition, message) do {                               \
        if (condition)                                                  \
            break;                                                      \
        fail(__FILE__, __LINE__, WTF_PRETTY_FUNCTION, #condition, toCString message); \
    } while (false)

    void run()
    {
        HashSet<BasicBlock*> blocks;
        HashSet<Value*> valueInProc;
        HashMap<Value*, unsigned> valueInBlock;

        for (BasicBlock* block : m_procedure) {
            blocks.add(block);
            for (Value* value : *block)
                valueInBlock.add(value, 0).iterator->value++;
        }

        for (Value* value : m_procedure.values())
            valueInProc.add(value);

        for (Value* value : valueInProc)
            VALIDATE(valueInBlock.contains(value), ("At ", *value));
        for (auto& entry : valueInBlock) {
            VALIDATE(valueInProc.contains(entry.key), ("At ", *entry.key));
            VALIDATE(entry.value == 1, ("At ", *entry.key));
        }

        for (Value* value : valueInProc) {
            for (Value* child : value->children()) {
                VALIDATE(child, ("At ", *value));
                VALIDATE(valueInProc.contains(child), ("At ", *value, "->", pointerDump(child)));
            }
        }

        HashMap<BasicBlock*, HashSet<BasicBlock*>> allPredecessors;
        for (BasicBlock* block : blocks) {
            VALIDATE(block->size() >= 1, ("At ", *block));
            for (unsigned i = 0; i < block->size() - 1; ++i)
                VALIDATE(!ControlValue::accepts(block->at(i)->opcode()), ("At ", *block->at(i)));
            VALIDATE(ControlValue::accepts(block->last()->opcode()), ("At ", *block->last()));
            
            for (BasicBlock* successor : block->successorBlocks()) {
                allPredecessors.add(successor, HashSet<BasicBlock*>()).iterator->value.add(block);
                VALIDATE(
                    blocks.contains(successor), ("At ", *block, "->", pointerDump(successor)));
            }
        }

        // Note that this totally allows dead code.
        for (auto& entry : allPredecessors) {
            BasicBlock* successor = entry.key;
            HashSet<BasicBlock*>& predecessors = entry.value;
            VALIDATE(predecessors == successor->predecessors(), ("At ", *successor));
        }

        for (Value* value : m_procedure.values()) {
            for (Value* child : value->children())
                VALIDATE(child->type() != Void, ("At ", *value, "->", *child));
            switch (value->opcode()) {
            case Nop:
            case Jump:
            case Oops:
                VALIDATE(!value->numChildren(), ("At ", *value));
                VALIDATE(value->type() == Void, ("At ", *value));
                break;
            case Identity:
                VALIDATE(value->numChildren() == 1, ("At ", *value));
                VALIDATE(value->type() == value->child(0)->type(), ("At ", *value));
                VALIDATE(value->type() != Void, ("At ", *value));
                break;
            case Const32:
                VALIDATE(!value->numChildren(), ("At ", *value));
                VALIDATE(value->type() == Int32, ("At ", *value));
                break;
            case Const64:
                VALIDATE(!value->numChildren(), ("At ", *value));
                VALIDATE(value->type() == Int64, ("At ", *value));
                break;
            case ConstDouble:
                VALIDATE(!value->numChildren(), ("At ", *value));
                VALIDATE(value->type() == Double, ("At ", *value));
                break;
            case ConstFloat:
                VALIDATE(!value->numChildren(), ("At ", *value));
                VALIDATE(value->type() == Float, ("At ", *value));
                break;
            case StackSlot:
            case FramePointer:
                VALIDATE(!value->numChildren(), ("At ", *value));
                VALIDATE(value->type() == pointerType(), ("At ", *value));
                break;
            case ArgumentReg:
                VALIDATE(!value->numChildren(), ("At ", *value));
                VALIDATE(
                    (value->as<ArgumentRegValue>()->argumentReg().isGPR() ? pointerType() : Double)
                    == value->type(), ("At ", *value));
                break;
            case Add:
            case Sub:
            case Mul:
            case Div:
            case Mod:
            case BitAnd:
                VALIDATE(value->numChildren() == 2, ("At ", *value));
                VALIDATE(value->type() == value->child(0)->type(), ("At ", *value));
                VALIDATE(value->type() == value->child(1)->type(), ("At ", *value));
                VALIDATE(value->type() != Void, ("At ", *value));
                break;
            case ChillDiv:
            case ChillMod:
            case BitOr:
            case BitXor:
                VALIDATE(value->numChildren() == 2, ("At ", *value));
                VALIDATE(value->type() == value->child(0)->type(), ("At ", *value));
                VALIDATE(value->type() == value->child(1)->type(), ("At ", *value));
                VALIDATE(isInt(value->type()), ("At ", *value));
                break;
            case Shl:
            case SShr:
            case ZShr:
                VALIDATE(value->numChildren() == 2, ("At ", *value));
                VALIDATE(value->type() == value->child(0)->type(), ("At ", *value));
                VALIDATE(value->child(1)->type() == Int32, ("At ", *value));
                VALIDATE(isInt(value->type()), ("At ", *value));
                break;
            case BitwiseCast:
                VALIDATE(value->numChildren() == 1, ("At ", *value));
                VALIDATE(value->type() != value->child(0)->type(), ("At ", *value));
                VALIDATE(
                    (value->type() == Int64 && value->child(0)->type() == Double)
                    || (value->type() == Double && value->child(0)->type() == Int64)
                    || (value->type() == Float && value->child(0)->type() == Int32)
                    || (value->type() == Int32 && value->child(0)->type() == Float),
                    ("At ", *value));
                break;
            case SExt8:
            case SExt16:
                VALIDATE(value->numChildren() == 1, ("At ", *value));
                VALIDATE(value->child(0)->type() == Int32, ("At ", *value));
                VALIDATE(value->type() == Int32, ("At ", *value));
                break;
            case SExt32:
            case ZExt32:
                VALIDATE(value->numChildren() == 1, ("At ", *value));
                VALIDATE(value->child(0)->type() == Int32, ("At ", *value));
                VALIDATE(value->type() == Int64, ("At ", *value));
                break;
            case Clz:
                VALIDATE(value->numChildren() == 1, ("At ", *value));
                VALIDATE(isInt(value->child(0)->type()), ("At ", *value));
                VALIDATE(isInt(value->type()), ("At ", *value));
                break;
            case Trunc:
                VALIDATE(value->numChildren() == 1, ("At ", *value));
                VALIDATE(value->child(0)->type() == Int64, ("At ", *value));
                VALIDATE(value->type() == Int32, ("At ", *value));
                break;
            case Abs:
            case Sqrt:
                VALIDATE(value->numChildren() == 1, ("At ", *value));
                VALIDATE(isFloat(value->child(0)->type()), ("At ", *value));
                VALIDATE(isFloat(value->type()), ("At ", *value));
                break;
            case IToD:
                VALIDATE(value->numChildren() == 1, ("At ", *value));
                VALIDATE(isInt(value->child(0)->type()), ("At ", *value));
                VALIDATE(value->type() == Double, ("At ", *value));
                break;
            case DToI32:
                VALIDATE(value->numChildren() == 1, ("At ", *value));
                VALIDATE(value->child(0)->type() == Double, ("At ", *value));
                VALIDATE(value->type() == Int32, ("At ", *value));
                break;
            case FloatToDouble:
                VALIDATE(value->numChildren() == 1, ("At ", *value));
                VALIDATE(value->child(0)->type() == Float, ("At ", *value));
                VALIDATE(value->type() == Double, ("At ", *value));
                break;
            case DoubleToFloat:
                VALIDATE(value->numChildren() == 1, ("At ", *value));
                VALIDATE(value->child(0)->type() == Double, ("At ", *value));
                VALIDATE(value->type() == Float, ("At ", *value));
                break;
            case Equal:
            case NotEqual:
            case LessThan:
            case GreaterThan:
            case LessEqual:
            case GreaterEqual:
                VALIDATE(value->numChildren() == 2, ("At ", *value));
                VALIDATE(value->child(0)->type() == value->child(1)->type(), ("At ", *value));
                VALIDATE(value->type() == Int32, ("At ", *value));
                break;
            case Above:
            case Below:
            case AboveEqual:
            case BelowEqual:
                VALIDATE(value->numChildren() == 2, ("At ", *value));
                VALIDATE(value->child(0)->type() == value->child(1)->type(), ("At ", *value));
                VALIDATE(isInt(value->child(0)->type()), ("At ", *value));
                VALIDATE(value->type() == Int32, ("At ", *value));
                break;
            case Select:
                VALIDATE(value->numChildren() == 3, ("At ", *value));
                VALIDATE(isInt(value->child(0)->type()), ("At ", *value));
                VALIDATE(value->type() == value->child(1)->type(), ("At ", *value));
                VALIDATE(value->type() == value->child(2)->type(), ("At ", *value));
                break;
            case Load8Z:
            case Load8S:
            case Load16Z:
            case Load16S:
                VALIDATE(value->numChildren() == 1, ("At ", *value));
                VALIDATE(value->child(0)->type() == pointerType(), ("At ", *value));
                VALIDATE(value->type() == Int32, ("At ", *value));
                validateStackAccess(value);
                break;
            case Load:
                VALIDATE(value->numChildren() == 1, ("At ", *value));
                VALIDATE(value->child(0)->type() == pointerType(), ("At ", *value));
                VALIDATE(value->type() != Void, ("At ", *value));
                validateStackAccess(value);
                break;
            case Store8:
            case Store16:
                VALIDATE(value->numChildren() == 2, ("At ", *value));
                VALIDATE(value->child(0)->type() == Int32, ("At ", *value));
                VALIDATE(value->child(1)->type() == pointerType(), ("At ", *value));
                VALIDATE(value->type() == Void, ("At ", *value));
                validateStackAccess(value);
                break;
            case Store:
                VALIDATE(value->numChildren() == 2, ("At ", *value));
                VALIDATE(value->child(1)->type() == pointerType(), ("At ", *value));
                VALIDATE(value->type() == Void, ("At ", *value));
                validateStackAccess(value);
                break;
            case CCall:
                VALIDATE(value->numChildren() >= 1, ("At ", *value));
                VALIDATE(value->child(0)->type() == pointerType(), ("At ", *value));
                break;
            case Patchpoint:
                if (value->type() == Void)
                    VALIDATE(value->as<PatchpointValue>()->resultConstraint == ValueRep::WarmAny, ("At ", *value));
                else {
                    switch (value->as<PatchpointValue>()->resultConstraint.kind()) {
                    case ValueRep::WarmAny:
                    case ValueRep::SomeRegister:
                    case ValueRep::Register:
                    case ValueRep::StackArgument:
                        break;
                    default:
                        VALIDATE(false, ("At ", *value));
                        break;
                    }
                    
                    validateStackmapConstraint(value, ConstrainedValue(value, value->as<PatchpointValue>()->resultConstraint));
                }
                validateStackmap(value);
                break;
            case CheckAdd:
            case CheckSub:
            case CheckMul:
                VALIDATE(value->numChildren() >= 2, ("At ", *value));
                VALIDATE(isInt(value->child(0)->type()), ("At ", *value));
                VALIDATE(isInt(value->child(1)->type()), ("At ", *value));
                VALIDATE(value->as<StackmapValue>()->constrainedChild(0).rep() == ValueRep::WarmAny, ("At ", *value));
                VALIDATE(value->as<StackmapValue>()->constrainedChild(1).rep() == ValueRep::WarmAny, ("At ", *value));
                validateStackmap(value);
                break;
            case Check:
                VALIDATE(value->numChildren() >= 1, ("At ", *value));
                VALIDATE(isInt(value->child(0)->type()), ("At ", *value));
                VALIDATE(value->as<StackmapValue>()->constrainedChild(0).rep() == ValueRep::WarmAny, ("At ", *value));
                validateStackmap(value);
                break;
            case Upsilon:
                VALIDATE(value->numChildren() == 1, ("At ", *value));
                VALIDATE(value->as<UpsilonValue>()->phi(), ("At ", *value));
                VALIDATE(value->child(0)->type() == value->as<UpsilonValue>()->phi()->type(), ("At ", *value));
                VALIDATE(valueInProc.contains(value->as<UpsilonValue>()->phi()), ("At ", *value));
                break;
            case Phi:
                VALIDATE(!value->numChildren(), ("At ", *value));
                VALIDATE(value->type() != Void, ("At ", *value));
                break;
            case Return:
                VALIDATE(value->numChildren() == 1, ("At ", *value));
                VALIDATE(value->type() == Void, ("At ", *value));
                break;
            case Branch:
            case Switch:
                VALIDATE(value->numChildren() == 1, ("At ", *value));
                VALIDATE(isInt(value->child(0)->type()), ("At ", *value));
                VALIDATE(value->type() == Void, ("At ", *value));
                break;
            }
        }
    }

private:
    void validateStackmap(Value* value)
    {
        StackmapValue* stackmap = value->as<StackmapValue>();
        VALIDATE(stackmap, ("At ", *value));
        VALIDATE(stackmap->numChildren() >= stackmap->reps().size(), ("At ", *stackmap));
        for (ConstrainedValue child : stackmap->constrainedChildren())
            validateStackmapConstraint(stackmap, child);
    }
    
    void validateStackmapConstraint(Value* context, const ConstrainedValue& value)
    {
        switch (value.rep().kind()) {
        case ValueRep::WarmAny:
        case ValueRep::ColdAny:
        case ValueRep::LateColdAny:
        case ValueRep::SomeRegister:
        case ValueRep::StackArgument:
            break;
        case ValueRep::Register:
            if (value.rep().reg().isGPR())
                VALIDATE(isInt(value.value()->type()), ("At ", *context, ": ", value));
            else
                VALIDATE(isFloat(value.value()->type()), ("At ", *context, ": ", value));
            break;
        default:
            VALIDATE(false, ("At ", *context, ": ", value));
            break;
        }
    }

    void validateStackAccess(Value* value)
    {
        MemoryValue* memory = value->as<MemoryValue>();
        StackSlotValue* stack = value->lastChild()->as<StackSlotValue>();
        if (!stack)
            return;

        VALIDATE(memory->offset() >= 0, ("At ", *value));
        VALIDATE(memory->offset() + memory->accessByteSize() <= stack->byteSize(), ("At ", *value));
    }
    
    NO_RETURN_DUE_TO_CRASH void fail(
        const char* filename, int lineNumber, const char* function, const char* condition,
        CString message)
    {
        CString failureMessage;
        {
            StringPrintStream out;
            out.print("B3 VALIDATION FAILURE\n");
            out.print("    ", condition, " (", filename, ":", lineNumber, ")\n");
            out.print("    ", message, "\n");
            out.print("    After ", m_procedure.lastPhaseName(), "\n");
            failureMessage = out.toCString();
        }

        dataLog(failureMessage);
        if (m_dumpBefore) {
            dataLog("Before ", m_procedure.lastPhaseName(), ":\n");
            dataLog(m_dumpBefore);
        }
        dataLog("At time of failure:\n");
        dataLog(m_procedure);

        dataLog(failureMessage);
        WTFReportAssertionFailure(filename, lineNumber, function, condition);
        CRASH();
    }
    
    Procedure& m_procedure;
    const char* m_dumpBefore;
};

} // anonymous namespace

void validate(Procedure& procedure, const char* dumpBefore)
{
    Validater validater(procedure, dumpBefore);
    validater.run();
}

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)
