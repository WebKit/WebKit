/*
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
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

#include "AirCode.h"
#include "B3ArgumentRegValue.h"
#include "B3AtomicValue.h"
#include "B3Dominators.h"
#include "B3MemoryValue.h"
#include "B3Procedure.h"
#include "B3ProcedureInlines.h"
#include "B3SlotBaseValue.h"
#include "B3SwitchValue.h"
#include "B3UpsilonValue.h"
#include "B3ValueInlines.h"
#include "B3Variable.h"
#include "B3VariableValue.h"
#include "B3WasmBoundsCheckValue.h"
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
        HashMap<Value*, BasicBlock*> valueOwner;
        HashMap<Value*, unsigned> valueIndex;
        HashMap<Value*, Vector<std::optional<Type>>> extractions;

        for (unsigned tuple = 0; tuple < m_procedure.tuples().size(); ++tuple) {
            VALIDATE(m_procedure.tuples()[tuple].size(), ("In tuple ", tuple));
            for (unsigned i = 0; i < m_procedure.tuples()[tuple].size(); ++i)
                VALIDATE(m_procedure.tuples()[tuple][i].isNumeric(), ("In tuple ", tuple, " at index", i));
        }

        for (BasicBlock* block : m_procedure) {
            blocks.add(block);
            for (unsigned i = 0; i < block->size(); ++i) {
                Value* value = block->at(i);
                valueInBlock.add(value, 0).iterator->value++;
                valueOwner.add(value, block);
                valueIndex.add(value, i);
            }
        }

        for (Value* value : m_procedure.values())
            valueInProc.add(value);

        for (Value* value : valueInProc)
            VALIDATE(valueInBlock.contains(value), ("At ", *value));
        for (auto& entry : valueInBlock) {
            VALIDATE(valueInProc.contains(entry.key), ("At ", *entry.key));
            VALIDATE(entry.value == 1, ("At ", *entry.key));
        }

        // Compute dominators ourselves to avoid perturbing Procedure.
        Dominators dominators(m_procedure);

        for (Value* value : valueInProc) {
            for (Value* child : value->children()) {
                VALIDATE(child, ("At ", *value));
                VALIDATE(valueInProc.contains(child), ("At ", *value, "->", pointerDump(child)));
                if (valueOwner.get(child) == valueOwner.get(value))
                    VALIDATE(valueIndex.get(value) > valueIndex.get(child), ("At ", *value, "->", pointerDump(child)));
                else
                    VALIDATE(dominators.dominates(valueOwner.get(child), valueOwner.get(value)), ("at ", *value, "->", pointerDump(child)));
            }
        }

        HashMap<BasicBlock*, HashSet<BasicBlock*>> allPredecessors;
        for (BasicBlock* block : blocks) {
            VALIDATE(block->size() >= 1, ("At ", *block));
            for (unsigned i = 0; i < block->size() - 1; ++i)
                VALIDATE(!block->at(i)->effects().terminal, ("At ", *block->at(i)));
            VALIDATE(block->last()->effects().terminal, ("At ", *block->last()));
            
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
            case Fence:
                VALIDATE(!value->kind().hasExtraBits(), ("At ", *value));
                VALIDATE(!value->numChildren(), ("At ", *value));
                VALIDATE(value->type() == Void, ("At ", *value));
                break;
            case Identity:
            case Opaque:
                VALIDATE(!value->kind().hasExtraBits(), ("At ", *value));
                VALIDATE(value->numChildren() == 1, ("At ", *value));
                VALIDATE(value->type() == value->child(0)->type(), ("At ", *value));
                VALIDATE(value->type() != Void, ("At ", *value));
                break;
            case Const32:
                VALIDATE(!value->kind().hasExtraBits(), ("At ", *value));
                VALIDATE(!value->numChildren(), ("At ", *value));
                VALIDATE(value->type() == Int32, ("At ", *value));
                break;
            case Const64:
                VALIDATE(!value->kind().hasExtraBits(), ("At ", *value));
                VALIDATE(!value->numChildren(), ("At ", *value));
                VALIDATE(value->type() == Int64, ("At ", *value));
                break;
            case ConstDouble:
                VALIDATE(!value->kind().hasExtraBits(), ("At ", *value));
                VALIDATE(!value->numChildren(), ("At ", *value));
                VALIDATE(value->type() == Double, ("At ", *value));
                break;
            case ConstFloat:
                VALIDATE(!value->kind().hasExtraBits(), ("At ", *value));
                VALIDATE(!value->numChildren(), ("At ", *value));
                VALIDATE(value->type() == Float, ("At ", *value));
                break;
            case BottomTuple:
                VALIDATE(!value->kind().hasExtraBits(), ("At ", *value));
                VALIDATE(!value->numChildren(), ("At ", *value));
                VALIDATE(value->type().isTuple(), ("At ", *value));
                break;
            case Set:
                VALIDATE(!value->kind().hasExtraBits(), ("At ", *value));
                VALIDATE(value->numChildren() == 1, ("At ", *value));
                VALIDATE(value->child(0)->type() == value->as<VariableValue>()->variable()->type(), ("At ", *value));
                break;
            case Get:
                VALIDATE(!value->kind().hasExtraBits(), ("At ", *value));
                VALIDATE(!value->numChildren(), ("At ", *value));
                VALIDATE(value->type() == value->as<VariableValue>()->variable()->type(), ("At ", *value));
                break;
            case SlotBase:
            case FramePointer:
                VALIDATE(!value->kind().hasExtraBits(), ("At ", *value));
                VALIDATE(!value->numChildren(), ("At ", *value));
                VALIDATE(value->type() == pointerType(), ("At ", *value));
                break;
            case ArgumentReg:
                VALIDATE(!value->kind().hasExtraBits(), ("At ", *value));
                VALIDATE(!value->numChildren(), ("At ", *value));
                VALIDATE(
                    (value->as<ArgumentRegValue>()->argumentReg().isGPR() ? pointerType() : Double)
                    == value->type(), ("At ", *value));
                break;
            case Add:
            case Sub:
            case Mul:
            case Div:
            case UDiv:
            case Mod:
            case UMod:
            case BitAnd:
            case BitOr:
            case BitXor:
                VALIDATE(!value->kind().traps(), ("At ", *value));
                switch (value->opcode()) {
                case Div:
                case Mod:
                    if (value->isChill()) {
                        VALIDATE(value->opcode() == Div || value->opcode() == Mod, ("At ", *value));
                        VALIDATE(value->type().isInt(), ("At ", *value));
                    }
                    break;
                default:
                    VALIDATE(!value->kind().hasExtraBits(), ("At ", *value));
                    break;
                }
                VALIDATE(value->numChildren() == 2, ("At ", *value));
                VALIDATE(value->type() == value->child(0)->type(), ("At ", *value));
                VALIDATE(value->type() == value->child(1)->type(), ("At ", *value));
                VALIDATE(value->type().isNumeric(), ("At ", *value));
                break;
            case FMax:
            case FMin:
                VALIDATE(value->numChildren() == 2, ("At ", *value));
                VALIDATE(value->type() == value->child(0)->type(), ("At ", *value));
                VALIDATE(value->type() == value->child(1)->type(), ("At ", *value));
                VALIDATE(value->type() == Float || value->type() == Double, ("At ", *value));
                break;
            case Neg:
                VALIDATE(!value->kind().hasExtraBits(), ("At ", *value));
                VALIDATE(value->numChildren() == 1, ("At ", *value));
                VALIDATE(value->type() == value->child(0)->type(), ("At ", *value));
                VALIDATE(value->type().isNumeric(), ("At ", *value));
                break;
            case Shl:
            case SShr:
            case ZShr:
            case RotR:
                case RotL:
                VALIDATE(!value->kind().hasExtraBits(), ("At ", *value));
                VALIDATE(value->numChildren() == 2, ("At ", *value));
                VALIDATE(value->type() == value->child(0)->type(), ("At ", *value));
                VALIDATE(value->child(1)->type() == Int32, ("At ", *value));
                VALIDATE(value->type().isInt(), ("At ", *value));
                break;
            case BitwiseCast:
                VALIDATE(!value->kind().hasExtraBits(), ("At ", *value));
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
                VALIDATE(!value->kind().hasExtraBits(), ("At ", *value));
                VALIDATE(value->numChildren() == 1, ("At ", *value));
                VALIDATE(value->child(0)->type() == Int32, ("At ", *value));
                VALIDATE(value->type() == Int32, ("At ", *value));
                break;
            case SExt32:
            case ZExt32:
                VALIDATE(!value->kind().hasExtraBits(), ("At ", *value));
                VALIDATE(value->numChildren() == 1, ("At ", *value));
                VALIDATE(value->child(0)->type() == Int32, ("At ", *value));
                VALIDATE(value->type() == Int64, ("At ", *value));
                break;
            case Clz:
                VALIDATE(!value->kind().hasExtraBits(), ("At ", *value));
                VALIDATE(value->numChildren() == 1, ("At ", *value));
                VALIDATE(value->child(0)->type().isInt(), ("At ", *value));
                VALIDATE(value->type().isInt(), ("At ", *value));
                break;
            case Trunc:
                VALIDATE(!value->kind().hasExtraBits(), ("At ", *value));
                VALIDATE(value->numChildren() == 1, ("At ", *value));
                VALIDATE(
                    (value->type() == Int32 && value->child(0)->type() == Int64)
                    || (value->type() == Float && value->child(0)->type() == Double),
                    ("At ", *value));
                break;
            case Abs:
            case Ceil:
            case Floor:
            case Sqrt:
                VALIDATE(!value->kind().hasExtraBits(), ("At ", *value));
                VALIDATE(value->numChildren() == 1, ("At ", *value));
                VALIDATE(value->child(0)->type().isFloat(), ("At ", *value));
                VALIDATE(value->type().isFloat(), ("At ", *value));
                break;
            case IToD:
                VALIDATE(!value->kind().hasExtraBits(), ("At ", *value));
                VALIDATE(value->numChildren() == 1, ("At ", *value));
                VALIDATE(value->child(0)->type().isInt(), ("At ", *value));
                VALIDATE(value->type() == Double, ("At ", *value));
                break;
            case IToF:
                VALIDATE(!value->kind().hasExtraBits(), ("At ", *value));
                VALIDATE(value->numChildren() == 1, ("At ", *value));
                VALIDATE(value->child(0)->type().isInt(), ("At ", *value));
                VALIDATE(value->type() == Float, ("At ", *value));
                break;
            case FloatToDouble:
                VALIDATE(!value->kind().hasExtraBits(), ("At ", *value));
                VALIDATE(value->numChildren() == 1, ("At ", *value));
                VALIDATE(value->child(0)->type() == Float, ("At ", *value));
                VALIDATE(value->type() == Double, ("At ", *value));
                break;
            case DoubleToFloat:
                VALIDATE(!value->kind().hasExtraBits(), ("At ", *value));
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
                VALIDATE(!value->kind().hasExtraBits(), ("At ", *value));
                VALIDATE(value->numChildren() == 2, ("At ", *value));
                VALIDATE(value->child(0)->type() == value->child(1)->type(), ("At ", *value));
                VALIDATE(value->type() == Int32, ("At ", *value));
                break;
            case Above:
            case Below:
            case AboveEqual:
            case BelowEqual:
                VALIDATE(!value->kind().hasExtraBits(), ("At ", *value));
                VALIDATE(value->numChildren() == 2, ("At ", *value));
                VALIDATE(value->child(0)->type() == value->child(1)->type(), ("At ", *value));
                VALIDATE(value->child(0)->type().isInt(), ("At ", *value));
                VALIDATE(value->type() == Int32, ("At ", *value));
                break;
            case EqualOrUnordered:
                VALIDATE(!value->kind().hasExtraBits(), ("At ", *value));
                VALIDATE(value->numChildren() == 2, ("At ", *value));
                VALIDATE(value->child(0)->type() == value->child(1)->type(), ("At ", *value));
                VALIDATE(value->child(0)->type().isFloat(), ("At ", *value));
                VALIDATE(value->type() == Int32, ("At ", *value));
                break;
            case Select:
                VALIDATE(!value->kind().hasExtraBits(), ("At ", *value));
                VALIDATE(value->numChildren() == 3, ("At ", *value));
                VALIDATE(value->child(0)->type().isInt(), ("At ", *value));
                VALIDATE(value->type() == value->child(1)->type(), ("At ", *value));
                VALIDATE(value->type() == value->child(2)->type(), ("At ", *value));
                break;
            case Load8Z:
            case Load8S:
            case Load16Z:
            case Load16S:
                VALIDATE(!value->kind().isChill(), ("At ", *value));
                VALIDATE(value->numChildren() == 1, ("At ", *value));
                VALIDATE(value->child(0)->type() == pointerType(), ("At ", *value));
                VALIDATE(value->type() == Int32, ("At ", *value));
                validateFence(value);
                validateStackAccess(value);
                break;
            case Load:
                VALIDATE(!value->kind().isChill(), ("At ", *value));
                VALIDATE(value->numChildren() == 1, ("At ", *value));
                VALIDATE(value->child(0)->type() == pointerType(), ("At ", *value));
                VALIDATE(value->type().isNumeric(), ("At ", *value));
                validateFence(value);
                validateStackAccess(value);
                break;
            case Store8:
            case Store16:
                VALIDATE(!value->kind().isChill(), ("At ", *value));
                VALIDATE(value->numChildren() == 2, ("At ", *value));
                VALIDATE(value->child(0)->type() == Int32, ("At ", *value));
                VALIDATE(value->child(1)->type() == pointerType(), ("At ", *value));
                VALIDATE(value->type() == Void, ("At ", *value));
                validateFence(value);
                validateStackAccess(value);
                break;
            case Store:
                VALIDATE(!value->kind().isChill(), ("At ", *value));
                VALIDATE(value->numChildren() == 2, ("At ", *value));
                VALIDATE(value->child(1)->type() == pointerType(), ("At ", *value));
                VALIDATE(value->type() == Void, ("At ", *value));
                validateFence(value);
                validateStackAccess(value);
                break;
            case AtomicWeakCAS:
                VALIDATE(!value->kind().isChill(), ("At ", *value));
                VALIDATE(value->numChildren() == 3, ("At ", *value));
                VALIDATE(value->type() == Int32, ("At ", *value));
                VALIDATE(value->child(0)->type() == value->child(1)->type(), ("At ", *value));
                VALIDATE(value->child(0)->type().isInt(), ("At ", *value));
                VALIDATE(value->child(2)->type() == pointerType(), ("At ", *value));
                validateAtomic(value);
                validateStackAccess(value);
                break;
            case AtomicStrongCAS:
                VALIDATE(!value->kind().isChill(), ("At ", *value));
                VALIDATE(value->numChildren() == 3, ("At ", *value));
                VALIDATE(value->type() == value->child(0)->type(), ("At ", *value));
                VALIDATE(value->type() == value->child(1)->type(), ("At ", *value));
                VALIDATE(value->type().isInt(), ("At ", *value));
                VALIDATE(value->child(2)->type() == pointerType(), ("At ", *value));
                validateAtomic(value);
                validateStackAccess(value);
                break;
            case AtomicXchgAdd:
            case AtomicXchgAnd:
            case AtomicXchgOr:
            case AtomicXchgSub:
            case AtomicXchgXor:
            case AtomicXchg:
                VALIDATE(!value->kind().isChill(), ("At ", *value));
                VALIDATE(value->numChildren() == 2, ("At ", *value));
                VALIDATE(value->type() == value->child(0)->type(), ("At ", *value));
                VALIDATE(value->type().isInt(), ("At ", *value));
                VALIDATE(value->child(1)->type() == pointerType(), ("At ", *value));
                validateAtomic(value);
                validateStackAccess(value);
                break;
            case Depend:
                VALIDATE(!value->kind().hasExtraBits(), ("At ", *value));
                VALIDATE(value->numChildren() == 1, ("At ", *value));
                VALIDATE(value->type() == value->child(0)->type(), ("At ", *value));
                VALIDATE(value->type().isInt(), ("At ", *value));
                break;
            case WasmAddress:
                VALIDATE(!value->kind().hasExtraBits(), ("At ", *value));
                VALIDATE(value->numChildren() == 1, ("At ", *value));
                VALIDATE(value->child(0)->type() == pointerType(), ("At ", *value));
                VALIDATE(value->type() == pointerType(), ("At ", *value));
                break;
            case CCall:
                VALIDATE(!value->kind().hasExtraBits(), ("At ", *value));
                VALIDATE(value->numChildren() >= 1, ("At ", *value));
                VALIDATE(value->child(0)->type() == pointerType(), ("At ", *value));
                break;
            case Patchpoint:
                VALIDATE(!value->kind().hasExtraBits(), ("At ", *value));
                if (value->type() == Void) {
                    VALIDATE(value->as<PatchpointValue>()->resultConstraints.size() == 1, ("At ", *value));
                    VALIDATE(value->as<PatchpointValue>()->resultConstraints[0] == ValueRep::WarmAny, ("At ", *value));
                } else {
                    if (value->type().isNumeric()) {
                        VALIDATE(value->as<PatchpointValue>()->resultConstraints.size() == 1, ("At ", *value));
                        validateStackmapConstraint(value, ConstrainedValue(value, value->as<PatchpointValue>()->resultConstraints[0]), ConstraintRole::Def);
                    } else {
                        VALIDATE(m_procedure.isValidTuple(value->type()), ("At ", *value));
                        VALIDATE(value->as<PatchpointValue>()->resultConstraints.size() == m_procedure.tupleForType(value->type()).size(), ("At ", *value));
                        for (unsigned i = 0; i < value->as<PatchpointValue>()->resultConstraints.size(); ++i)
                            validateStackmapConstraint(value, ConstrainedValue(value, value->as<PatchpointValue>()->resultConstraints[i]), ConstraintRole::Def, i);
                    }
                }
                validateStackmap(value);
                break;
            case Extract: {
                VALIDATE(value->numChildren() == 1, ("At ", *value));
                VALIDATE(value->child(0)->type() == Tuple, ("At ", *value));
                VALIDATE(value->type().isNumeric(), ("At ", *value));
                break;
            }
            case CheckAdd:
            case CheckSub:
            case CheckMul:
                VALIDATE(!value->kind().hasExtraBits(), ("At ", *value));
                VALIDATE(value->numChildren() >= 2, ("At ", *value));
                VALIDATE(value->child(0)->type().isInt(), ("At ", *value));
                VALIDATE(value->child(1)->type().isInt(), ("At ", *value));
                VALIDATE(value->as<StackmapValue>()->constrainedChild(0).rep() == ValueRep::WarmAny, ("At ", *value));
                VALIDATE(value->as<StackmapValue>()->constrainedChild(1).rep() == ValueRep::WarmAny, ("At ", *value));
                validateStackmap(value);
                break;
            case Check:
                VALIDATE(!value->kind().hasExtraBits(), ("At ", *value));
                VALIDATE(value->numChildren() >= 1, ("At ", *value));
                VALIDATE(value->child(0)->type().isInt(), ("At ", *value));
                VALIDATE(value->as<StackmapValue>()->constrainedChild(0).rep() == ValueRep::WarmAny, ("At ", *value));
                validateStackmap(value);
                break;
            case WasmBoundsCheck:
                VALIDATE(!value->kind().hasExtraBits(), ("At ", *value));
                VALIDATE(value->numChildren() == 1, ("At ", *value));
                VALIDATE(value->child(0)->type() == Int32, ("At ", *value));
                switch (value->as<WasmBoundsCheckValue>()->boundsType()) {
                case WasmBoundsCheckValue::Type::Pinned:
                    VALIDATE(m_procedure.code().isPinned(value->as<WasmBoundsCheckValue>()->bounds().pinnedSize), ("At ", *value));
                    break;
                case WasmBoundsCheckValue::Type::Maximum:
                    break;
                }
                VALIDATE(m_procedure.code().wasmBoundsCheckGenerator(), ("At ", *value));
                break;
            case Upsilon:
                VALIDATE(!value->kind().hasExtraBits(), ("At ", *value));
                VALIDATE(value->numChildren() == 1, ("At ", *value));
                VALIDATE(value->as<UpsilonValue>()->phi(), ("At ", *value));
                VALIDATE(value->as<UpsilonValue>()->phi()->opcode() == Phi, ("At ", *value));
                VALIDATE(value->child(0)->type() != Void, ("At ", *value));
                VALIDATE(value->child(0)->type() == value->as<UpsilonValue>()->phi()->type(), ("At ", *value));
                VALIDATE(valueInProc.contains(value->as<UpsilonValue>()->phi()), ("At ", *value));
                break;
            case Phi:
                VALIDATE(!value->kind().hasExtraBits(), ("At ", *value));
                VALIDATE(!value->numChildren(), ("At ", *value));
                VALIDATE(value->type() != Void, ("At ", *value));
                break;
            case Jump:
                VALIDATE(!value->kind().hasExtraBits(), ("At ", *value));
                VALIDATE(!value->numChildren(), ("At ", *value));
                VALIDATE(value->type() == Void, ("At ", *value));
                VALIDATE(valueOwner.get(value)->numSuccessors() == 1, ("At ", *value));
                break;
            case Oops:
                VALIDATE(!value->kind().hasExtraBits(), ("At ", *value));
                VALIDATE(!value->numChildren(), ("At ", *value));
                VALIDATE(value->type() == Void, ("At ", *value));
                VALIDATE(!valueOwner.get(value)->numSuccessors(), ("At ", *value));
                break;
            case Return:
                VALIDATE(!value->kind().hasExtraBits(), ("At ", *value));
                VALIDATE(value->numChildren() <= 1, ("At ", *value));
                VALIDATE(value->type() == Void, ("At ", *value));
                VALIDATE(!valueOwner.get(value)->numSuccessors(), ("At ", *value));
                break;
            case Branch:
                VALIDATE(!value->kind().hasExtraBits(), ("At ", *value));
                VALIDATE(value->numChildren() == 1, ("At ", *value));
                VALIDATE(value->child(0)->type().isInt(), ("At ", *value));
                VALIDATE(value->type() == Void, ("At ", *value));
                VALIDATE(valueOwner.get(value)->numSuccessors() == 2, ("At ", *value));
                break;
            case Switch: {
                VALIDATE(!value->kind().hasExtraBits(), ("At ", *value));
                VALIDATE(value->numChildren() == 1, ("At ", *value));
                VALIDATE(value->child(0)->type().isInt(), ("At ", *value));
                VALIDATE(value->type() == Void, ("At ", *value));
                VALIDATE(value->as<SwitchValue>()->hasFallThrough(valueOwner.get(value)), ("At ", *value));
                // This validates the same thing as hasFallThrough, but more explicitly. We want to
                // make sure that if anyone tries to change the definition of hasFallThrough, they
                // will feel some pain here, since this is fundamental.
                VALIDATE(valueOwner.get(value)->numSuccessors() == value->as<SwitchValue>()->numCaseValues() + 1, ("At ", *value));
                
                // Check that there are no duplicate cases.
                Vector<int64_t> caseValues = value->as<SwitchValue>()->caseValues();
                std::sort(caseValues.begin(), caseValues.end());
                for (unsigned i = 1; i < caseValues.size(); ++i)
                    VALIDATE(caseValues[i - 1] != caseValues[i], ("At ", *value, ", caseValue = ", caseValues[i]));
                break;
            }
            case EntrySwitch:
                VALIDATE(!value->kind().hasExtraBits(), ("At ", *value));
                VALIDATE(!value->numChildren(), ("At ", *value));
                VALIDATE(value->type() == Void, ("At ", *value));
                VALIDATE(valueOwner.get(value)->numSuccessors() == m_procedure.numEntrypoints(), ("At ", *value));
                break;
            }

            VALIDATE(!(value->effects().writes && value->key()), ("At ", *value));
        }

        for (Variable* variable : m_procedure.variables())
            VALIDATE(variable->type() != Void, ("At ", *variable));

        for (BasicBlock* block : m_procedure) {
            // We expect the predecessor list to be de-duplicated.
            HashSet<BasicBlock*> predecessors;
            for (BasicBlock* predecessor : block->predecessors())
                predecessors.add(predecessor);
            VALIDATE(block->numPredecessors() == predecessors.size(), ("At ", *block));
        }

        validatePhisAreDominatedByUpsilons();
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
    
    enum class ConstraintRole {
        Use,
        Def
    };
    void validateStackmapConstraint(Value* context, const ConstrainedValue& value, ConstraintRole role = ConstraintRole::Use, unsigned tupleIndex = 0)
    {
        switch (value.rep().kind()) {
        case ValueRep::WarmAny:
        case ValueRep::SomeRegister:
        case ValueRep::StackArgument:
            break;
        case ValueRep::LateColdAny:
        case ValueRep::ColdAny:
            VALIDATE(role == ConstraintRole::Use, ("At ", *context, ": ", value));
            break;
        case ValueRep::SomeRegisterWithClobber:
            VALIDATE(role == ConstraintRole::Use, ("At ", *context, ": ", value));
            VALIDATE(context->as<PatchpointValue>(), ("At ", *context));
            break;
        case ValueRep::SomeEarlyRegister:
            VALIDATE(role == ConstraintRole::Def, ("At ", *context, ": ", value));
            break;
        case ValueRep::Register:
        case ValueRep::LateRegister:
        case ValueRep::SomeLateRegister:
            if (value.rep().kind() == ValueRep::LateRegister)
                VALIDATE(role == ConstraintRole::Use, ("At ", *context, ": ", value));
            if (value.rep().reg().isGPR()) {
                if (value.value()->type().isTuple())
                    VALIDATE(m_procedure.extractFromTuple(value.value()->type(), tupleIndex).isInt(), ("At ", *context, ": ", value));
                else
                    VALIDATE(value.value()->type().isInt(), ("At ", *context, ": ", value));
            } else {
                if (value.value()->type().isTuple())
                    VALIDATE(m_procedure.extractFromTuple(value.value()->type(), tupleIndex).isFloat(), ("At ", *context, ": ", value));
                else
                    VALIDATE(value.value()->type().isFloat(), ("At ", *context, ": ", value));
            }
            break;
        default:
            VALIDATE(false, ("At ", *context, ": ", value));
            break;
        }
    }
    
    void validateFence(Value* value)
    {
        MemoryValue* memory = value->as<MemoryValue>();
        if (memory->hasFence())
            VALIDATE(memory->accessBank() == GP, ("Fence at ", *memory));
    }
    
    void validateAtomic(Value* value)
    {
        AtomicValue* atomic = value->as<AtomicValue>();
        
        VALIDATE(bestType(GP, atomic->accessWidth()) == atomic->accessType(), ("At ", *value));
    }

    void validateStackAccess(Value* value)
    {
        MemoryValue* memory = value->as<MemoryValue>();
        SlotBaseValue* slotBase = value->lastChild()->as<SlotBaseValue>();
        if (!slotBase)
            return;

        VALIDATE(memory->offset() >= 0, ("At ", *value));
    }

    // A simple backwards analysis to check that we cannot reach a Phi without going through a corresponding Upsilon
    // We cannot use the dominator tree, since we are checking that each Phi is dominated by a the set of all of its upsilons, and not by a single node.
    void validatePhisAreDominatedByUpsilons()
    {
        bool changed = true;
        BitVector blocksToVisit;
        IndexMap<BasicBlock*, HashSet<Value*>> undominatedPhisAtTail(m_procedure.size());
        for (BasicBlock* block : m_procedure)
            blocksToVisit.set(block->index());
        while (changed) {
            changed = false;
            for (BasicBlock* block : m_procedure.blocksInPostOrder()) {
                if (!blocksToVisit.quickClear(block->index()))
                    continue;
                HashSet<Value*> undominatedPhis = undominatedPhisAtTail[block];
                for (unsigned index = block->size()-1; index--;) {
                    Value* value = block->at(index);
                    switch (value->opcode()) {
                    case Upsilon:
                        undominatedPhis.remove(value->as<UpsilonValue>()->phi());
                        break;
                    case Phi:
                        undominatedPhis.add(value);
                        break;
                    default:
                        break;
                    }
                }
                for (BasicBlock* predecessor : block->predecessors()) {
                    bool changedSet = false;
                    for (Value* phi : undominatedPhis)
                        changedSet |= undominatedPhisAtTail[predecessor].add(phi).isNewEntry;
                    if (changedSet) {
                        blocksToVisit.quickSet(predecessor->index());
                        changed = true;
                    }
                }
                if (!block->index())
                    VALIDATE(undominatedPhis.isEmpty(), ("Undominated phi at top of entry block: ", **undominatedPhis.begin()));
            }
        }
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
