/*
 * Copyright (C) 2015-2020 Apple Inc. All rights reserved.
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
#include "B3LowerMacros.h"

#if ENABLE(B3_JIT)

#include "AllowMacroScratchRegisterUsage.h"
#include "B3AtomicValue.h"
#include "B3BasicBlockInlines.h"
#include "B3BlockInsertionSet.h"
#include "B3CCallValue.h"
#include "B3CaseCollectionInlines.h"
#include "B3CheckValue.h"
#include "B3ConstPtrValue.h"
#include "B3FenceValue.h"
#include "B3InsertionSetInlines.h"
#include "B3MemoryValueInlines.h"
#include "B3PatchpointValue.h"
#include "B3PhaseScope.h"
#include "B3StackmapGenerationParams.h"
#include "B3SwitchValue.h"
#include "B3UpsilonValue.h"
#include "B3UseCounts.h"
#include "B3ValueInlines.h"
#include "B3WasmRefTypeCheckValue.h"
#include "B3WasmStructGetValue.h"
#include "B3WasmStructNewValue.h"
#include "B3WasmStructSetValue.h"
#include "CCallHelpers.h"
#include "GPRInfo.h"
#include "JSCJSValueInlines.h"
#include "JSCell.h"
#include "JSObject.h"
#include "JSWebAssemblyStruct.h"
#include "LinkBuffer.h"
#include "MarkedSpace.h"
#include "WasmExceptionType.h"
#include "WasmFaultSignalHandler.h"
#include "WasmOperations.h"
#include "WasmThunks.h"
#include "WasmTypeDefinition.h"
#include "WebAssemblyFunctionBase.h"
#include "WebAssemblyGCStructure.h"
#include <cmath>
#include <numeric>
#include <wtf/BitVector.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC { namespace B3 {

namespace {

class LowerMacros {
public:
    LowerMacros(Procedure& proc)
        : m_proc(proc)
        , m_blockInsertionSet(proc)
        , m_insertionSet(proc)
        , m_useCounts(proc)
    {
    }

    bool run()
    {
        RELEASE_ASSERT(!m_proc.hasQuirks());
        
        for (BasicBlock* block : m_proc) {
            m_block = block;
            processCurrentBlock();
        }
        m_changed |= m_blockInsertionSet.execute();
        if (m_changed) {
            m_proc.resetReachability();
            m_proc.invalidateCFG();
        }
        
        // This indicates that we've 
        m_proc.setHasQuirks(true);
        
        return m_changed;
    }
    
private:
    template <class Fn>
    void replaceWithBinaryCall(Fn &&function)
    {
        Value* functionAddress = m_insertionSet.insert<ConstPtrValue>(m_index, m_origin, tagCFunction<OperationPtrTag>(function));
        Value* result = m_insertionSet.insert<CCallValue>(m_index, m_value->type(), m_origin, Effects::none(), functionAddress, m_value->child(0), m_value->child(1));
        m_value->replaceWithIdentity(result);
        m_changed = true;
    }

    void processCurrentBlock()
    {
        for (m_index = 0; m_index < m_block->size(); ++m_index) {
            m_value = m_block->at(m_index);
            m_origin = m_value->origin();
            switch (m_value->opcode()) {
            case Mod: {
                if (m_value->isChill()) {
                    if (isARM64()) {
                        BasicBlock* before = m_blockInsertionSet.splitForward(m_block, m_index, &m_insertionSet);
                        BasicBlock* zeroDenCase = m_blockInsertionSet.insertBefore(m_block);
                        BasicBlock* normalModCase = m_blockInsertionSet.insertBefore(m_block);

                        before->replaceLastWithNew<Value>(m_proc, Branch, m_origin, m_value->child(1));
                        before->setSuccessors(
                            FrequentedBlock(normalModCase, FrequencyClass::Normal),
                            FrequentedBlock(zeroDenCase, FrequencyClass::Rare));

                        Value* divResult = normalModCase->appendNew<Value>(m_proc, chill(Div), m_origin, m_value->child(0), m_value->child(1));
                        Value* multipliedBack = normalModCase->appendNew<Value>(m_proc, Mul, m_origin, divResult, m_value->child(1));
                        Value* result = normalModCase->appendNew<Value>(m_proc, Sub, m_origin, m_value->child(0), multipliedBack);
                        UpsilonValue* normalResult = normalModCase->appendNew<UpsilonValue>(m_proc, m_origin, result);
                        normalModCase->appendNew<Value>(m_proc, Jump, m_origin);
                        normalModCase->setSuccessors(FrequentedBlock(m_block));

                        UpsilonValue* zeroResult = zeroDenCase->appendNew<UpsilonValue>(
                            m_proc, m_origin,
                            zeroDenCase->appendIntConstant(m_proc, m_value, 0));
                        zeroDenCase->appendNew<Value>(m_proc, Jump, m_origin);
                        zeroDenCase->setSuccessors(FrequentedBlock(m_block));

                        Value* phi = m_insertionSet.insert<Value>(m_index, Phi, m_value->type(), m_origin);
                        normalResult->setPhi(phi);
                        zeroResult->setPhi(phi);
                        m_value->replaceWithIdentity(phi);
                        before->updatePredecessorsAfter();
                        m_changed = true;
                    } else
                        makeDivisionChill(Mod);
                    break;
                }
                
                if (m_value->type() == Double) {
                    Value* functionAddress = m_insertionSet.insert<ConstPtrValue>(m_index, m_origin, tagCFunction<OperationPtrTag>(Math::fmodDouble));
                    Value* result = m_insertionSet.insert<CCallValue>(m_index, Double, m_origin,
                        Effects::none(),
                        functionAddress,
                        m_value->child(0),
                        m_value->child(1));
                    m_value->replaceWithIdentity(result);
                    m_changed = true;
                } else if (m_value->type() == Float) {
                    Value* numeratorAsDouble = m_insertionSet.insert<Value>(m_index, FloatToDouble, m_origin, m_value->child(0));
                    Value* denominatorAsDouble = m_insertionSet.insert<Value>(m_index, FloatToDouble, m_origin, m_value->child(1));
                    Value* functionAddress = m_insertionSet.insert<ConstPtrValue>(m_index, m_origin, tagCFunction<OperationPtrTag>(Math::fmodDouble));
                    Value* doubleMod = m_insertionSet.insert<CCallValue>(m_index, Double, m_origin,
                        Effects::none(),
                        functionAddress,
                        numeratorAsDouble,
                        denominatorAsDouble);
                    Value* result = m_insertionSet.insert<Value>(m_index, DoubleToFloat, m_origin, doubleMod);
                    m_value->replaceWithIdentity(result);
                    m_changed = true;
                } else if constexpr (isARM_THUMB2()) {
                    if (m_value->type() == Int64)
                        replaceWithBinaryCall(Math::i64_rem_s);
                    else
                        replaceWithBinaryCall(Math::i32_rem_s);
                } else if (isARM64()) {
                    Value* divResult = m_insertionSet.insert<Value>(m_index, chill(Div), m_origin, m_value->child(0), m_value->child(1));
                    Value* multipliedBack = m_insertionSet.insert<Value>(m_index, Mul, m_origin, divResult, m_value->child(1));
                    Value* result = m_insertionSet.insert<Value>(m_index, Sub, m_origin, m_value->child(0), multipliedBack);
                    m_value->replaceWithIdentity(result);
                    m_changed = true;
                }
                break;
            }

            case UMod: {
                if constexpr (isARM_THUMB2()) {
                    if (m_value->child(0)->type() == Int64)
                        replaceWithBinaryCall(Math::i64_rem_u);
                    else
                        replaceWithBinaryCall(Math::i32_rem_u);
                    break;
                }
                if (isARM64()) {
                    Value* divResult = m_insertionSet.insert<Value>(m_index, UDiv, m_origin, m_value->child(0), m_value->child(1));
                    Value* multipliedBack = m_insertionSet.insert<Value>(m_index, Mul, m_origin, divResult, m_value->child(1));
                    Value* result = m_insertionSet.insert<Value>(m_index, Sub, m_origin, m_value->child(0), multipliedBack);
                    m_value->replaceWithIdentity(result);
                    m_changed = true;
                }
                break;
            }

            case UDiv: {
                if constexpr (!isARM_THUMB2())
                    break;
                if (m_value->type() == Int64)
                    replaceWithBinaryCall(Math::i64_div_u);
                else
                    replaceWithBinaryCall(Math::i32_div_u);
                break;
            }
            case FMax:
            case FMin: {
                if (isX86() || isARM_THUMB2()) {
                    bool isMax = m_value->opcode() == FMax;

                    Value* a = m_value->child(0);
                    Value* b = m_value->child(1);

                    Value* isEqualValue = m_insertionSet.insert<Value>(
                        m_index, Equal, m_origin, a, b);

                    BasicBlock* before = m_blockInsertionSet.splitForward(m_block, m_index, &m_insertionSet);

                    BasicBlock* isEqual = m_blockInsertionSet.insertBefore(m_block);
                    BasicBlock* notEqual = m_blockInsertionSet.insertBefore(m_block);
                    BasicBlock* isLessThan = m_blockInsertionSet.insertBefore(m_block);
                    BasicBlock* notLessThan = m_blockInsertionSet.insertBefore(m_block);
                    BasicBlock* isGreaterThan = m_blockInsertionSet.insertBefore(m_block);
                    BasicBlock* isNaN = m_blockInsertionSet.insertBefore(m_block);

                    before->replaceLastWithNew<Value>(m_proc, Branch, m_origin, isEqualValue);
                    before->setSuccessors(FrequentedBlock(isEqual), FrequentedBlock(notEqual));

                    Value* lessThanValue = notEqual->appendNew<Value>(m_proc, LessThan, m_origin, a, b);
                    notEqual->appendNew<Value>(m_proc, Branch, m_origin, lessThanValue);
                    notEqual->setSuccessors(FrequentedBlock(isLessThan), FrequentedBlock(notLessThan));

                    Value* greaterThanValue = notLessThan->appendNew<Value>(m_proc, GreaterThan, m_origin, a, b);
                    notLessThan->appendNew<Value>(m_proc, Branch, m_origin, greaterThanValue);
                    notLessThan->setSuccessors(FrequentedBlock(isGreaterThan), FrequentedBlock(isNaN));

                    UpsilonValue* isLessThanResult = isLessThan->appendNew<UpsilonValue>(
                        m_proc, m_origin, isMax ? b : a);
                    isLessThan->appendNew<Value>(m_proc, Jump, m_origin);
                    isLessThan->setSuccessors(FrequentedBlock(m_block));

                    UpsilonValue* isGreaterThanResult = isGreaterThan->appendNew<UpsilonValue>(
                        m_proc, m_origin, isMax ? a : b);
                    isGreaterThan->appendNew<Value>(m_proc, Jump, m_origin);
                    isGreaterThan->setSuccessors(FrequentedBlock(m_block));

                    UpsilonValue* isEqualResult = isEqual->appendNew<UpsilonValue>(
                        m_proc, m_origin, isEqual->appendNew<Value>(m_proc, isMax ? BitAnd : BitOr, m_origin, a, b));
                    isEqual->appendNew<Value>(m_proc, Jump, m_origin);
                    isEqual->setSuccessors(FrequentedBlock(m_block));

                    UpsilonValue* isNaNResult = isNaN->appendNew<UpsilonValue>(
                        m_proc, m_origin, isNaN->appendNew<Value>(m_proc, Add, m_origin, a, b));
                    isNaN->appendNew<Value>(m_proc, Jump, m_origin);
                    isNaN->setSuccessors(FrequentedBlock(m_block));

                    Value* phi = m_insertionSet.insert<Value>(
                        m_index, Phi, m_value->type(), m_origin);
                    isLessThanResult->setPhi(phi);
                    isGreaterThanResult->setPhi(phi);
                    isEqualResult->setPhi(phi);
                    isNaNResult->setPhi(phi);

                    m_value->replaceWithIdentity(phi);
                    before->updatePredecessorsAfter();
                    m_changed = true;
                }
                break;
            }

            case Div: {
                if (m_value->isChill())
                    makeDivisionChill(Div);
                else if (isARM_THUMB2() && (m_value->type() == Int64 || m_value->type() == Int32)) {
                    BasicBlock* before = m_blockInsertionSet.splitForward(m_block, m_index);
                    before->replaceLastWithNew<Value>(m_proc, Nop, m_origin);
                    Value* result = callDivModHelper(before, Div, m_value->child(0), m_value->child(1));
                    before->appendNew<Value>(m_proc, Jump, m_origin);
                    before->setSuccessors(FrequentedBlock(m_block));
                    m_value->replaceWithIdentity(result);
                    m_changed = true;
                }
                break;
            }

            case Switch: {
                SwitchValue* switchValue = m_value->as<SwitchValue>();
                Vector<SwitchCase> cases;
                for (SwitchCase switchCase : switchValue->cases(m_block))
                    cases.append(switchCase);
                std::ranges::sort(cases, { }, &SwitchCase::caseValue);
                FrequentedBlock fallThrough = m_block->fallThrough();
                m_block->values().removeLast();
                recursivelyBuildSwitch(cases, fallThrough, 0, false, cases.size(), m_block);
                m_proc.deleteValue(switchValue);
                m_block->updatePredecessorsAfter();
                m_changed = true;
                break;
            }
                
            case Depend: {
                if (isX86()) {
                    // Create a load-load fence. This codegens to nothing on X86. We use it to tell the
                    // compiler not to block load motion.
                    FenceValue* fence = m_insertionSet.insert<FenceValue>(m_index, m_origin);
                    fence->read = HeapRange();
                    fence->write = HeapRange::top();
                    
                    // Kill the Depend, which should unlock a bunch of code simplification.
                    m_value->replaceWithBottom(m_insertionSet, m_index);
                    
                    m_changed = true;
                }
                break;
            }

            case AtomicWeakCAS:
            case AtomicStrongCAS: {
                AtomicValue* atomic = m_value->as<AtomicValue>();
                Width width = atomic->accessWidth();
                
                if (isCanonicalWidth(width))
                    break;
                
                Value* expectedValue = atomic->child(0);
                
                if (!isX86()) {
                    // On ARM, the load part of the CAS does a load with zero extension. Therefore, we need
                    // to zero-extend the input.
                    Value* maskedExpectedValue = m_insertionSet.insert<Value>(
                        m_index, BitAnd, m_origin, expectedValue,
                        m_insertionSet.insertIntConstant(m_index, expectedValue, mask(width)));
                    
                    atomic->child(0) = maskedExpectedValue;
                    m_changed = true;
                }
                
                if (atomic->opcode() == AtomicStrongCAS) {
                    Value* newValue = m_insertionSet.insert<Value>(
                        m_index, signExtendOpcode(width), m_origin,
                        m_insertionSet.insertClone(m_index, atomic));
                    
                    atomic->replaceWithIdentity(newValue);
                    m_changed = true;
                }

                break;
            }
                
            case AtomicXchgAdd:
            case AtomicXchgAnd:
            case AtomicXchgOr:
            case AtomicXchgSub:
            case AtomicXchgXor:
            case AtomicXchg: {
                // On X86, these may actually return garbage in the high bits. On ARM64, these sorta
                // zero-extend their high bits, except that the high bits might get polluted by high
                // bits in the operand. So, either way, we need to throw a sign-extend on these
                // things.
                
                if (isX86()) {
                    if (m_value->opcode() == AtomicXchgSub && m_useCounts.numUses(m_value)) {
                        // On x86, xchgadd is better than xchgsub if it has any users.
                        m_value->setOpcodeUnsafely(AtomicXchgAdd);
                        m_value->child(0) = m_insertionSet.insert<Value>(
                            m_index, Neg, m_origin, m_value->child(0));
                    }
                    
                    bool exempt = false;
                    switch (m_value->opcode()) {
                    case AtomicXchgAnd:
                    case AtomicXchgOr:
                    case AtomicXchgSub:
                    case AtomicXchgXor:
                        exempt = true;
                        break;
                    default:
                        break;
                    }
                    if (exempt)
                        break;
                }

                if (isARM64_LSE()) {
                    if (m_value->opcode() == AtomicXchgSub) {
                        m_value->setOpcodeUnsafely(AtomicXchgAdd);
                        m_value->child(0) = m_insertionSet.insert<Value>(
                            m_index, Neg, m_origin, m_value->child(0));
                    }
                }
                
                AtomicValue* atomic = m_value->as<AtomicValue>();
                Width width = atomic->accessWidth();
                
                if (isCanonicalWidth(width))
                    break;
                
                Value* newValue = m_insertionSet.insert<Value>(
                    m_index, signExtendOpcode(width), m_origin,
                    m_insertionSet.insertClone(m_index, atomic));
                
                atomic->replaceWithIdentity(newValue);
                m_changed = true;
                break;
            }
                
            case Load8Z:
            case Load16Z: {
                if (isX86())
                    break;
                
                MemoryValue* memory = m_value->as<MemoryValue>();
                if (!memory->hasFence())
                    break;
                
                // Sub-width load-acq on ARM64 always sign extends.
                Value* newLoad = m_insertionSet.insertClone(m_index, memory);
                newLoad->setOpcodeUnsafely(memory->opcode() == Load8Z ? Load8S : Load16S);
                
                Value* newValue = m_insertionSet.insert<Value>(
                    m_index, BitAnd, m_origin, newLoad,
                    m_insertionSet.insertIntConstant(
                        m_index, m_origin, Int32, mask(memory->accessWidth())));

                m_value->replaceWithIdentity(newValue);
                m_changed = true;
                break;
            }

            case VectorPopcnt: {
                if (!isX86())
                    break;
                ASSERT(m_value->as<SIMDValue>()->simdLane() == SIMDLane::i8x16);

                // x86_64 does not natively support vector lanewise popcount, so we emulate it using multiple
                // masks.

                v128_t bottomNibbleConst;
                v128_t popcntConst;
                bottomNibbleConst.u64x2[0] = 0x0f0f0f0f0f0f0f0f;
                bottomNibbleConst.u64x2[1] = 0x0f0f0f0f0f0f0f0f;
                popcntConst.u64x2[0] = 0x0302020102010100;
                popcntConst.u64x2[1] = 0x0403030203020201;
                Value* bottomNibbleMask = m_insertionSet.insert<Const128Value>(m_index, m_origin, bottomNibbleConst);
                Value* popcntMask = m_insertionSet.insert<Const128Value>(m_index, m_origin, popcntConst);

                Value* four = m_insertionSet.insert<Const32Value>(m_index, m_origin, 4);
                Value* v = m_value->child(0);
                Value* upper = m_insertionSet.insert<SIMDValue>(m_index, m_origin, VectorAndnot, B3::V128, SIMDLane::v128, SIMDSignMode::None, v, bottomNibbleMask);
                Value* lower = m_insertionSet.insert<SIMDValue>(m_index, m_origin, VectorAnd, B3::V128, SIMDLane::v128, SIMDSignMode::None, v, bottomNibbleMask);
                upper = m_insertionSet.insert<SIMDValue>(m_index, m_origin, VectorShr, B3::V128, SIMDLane::i16x8, SIMDSignMode::Unsigned, upper, four);
                lower = m_insertionSet.insert<SIMDValue>(m_index, m_origin, VectorSwizzle, B3::V128, SIMDLane::i8x16, SIMDSignMode::None, popcntMask, lower);
                upper = m_insertionSet.insert<SIMDValue>(m_index, m_origin, VectorSwizzle, B3::V128, SIMDLane::i8x16, SIMDSignMode::None, popcntMask, upper);
                Value* result = m_insertionSet.insert<SIMDValue>(m_index, m_origin, VectorAdd, B3::V128, SIMDLane::i8x16, SIMDSignMode::None, upper, lower);
                m_value->replaceWithIdentity(result);
                m_changed = true;
                break;
            }

            case VectorNot: {
                if (!isX86())
                    break;
                // x86_64 has no vector bitwise NOT instruction, so we expand vxv.not v into vxv.xor -1, v
                // here to give B3/Air a chance to optimize out repeated usage of the mask.
                v128_t mask;
                mask.u64x2[0] = 0xffffffffffffffff;
                mask.u64x2[1] = 0xffffffffffffffff;
                Value* ones = m_insertionSet.insert<Const128Value>(m_index, m_origin, mask);
                Value* result = m_insertionSet.insert<SIMDValue>(m_index, m_origin, VectorXor, B3::V128, SIMDLane::v128, SIMDSignMode::None, ones, m_value->child(0));
                m_value->replaceWithIdentity(result);
                m_changed = true;
                break;
            }

            case VectorNeg: {
                if (!isX86())
                    break;
                // x86_64 has no vector negate instruction. For integer vectors, we can replicate negation by
                // subtracting from zero. For floating-point vectors, we need to toggle the sign using packed
                // XOR.
                SIMDValue* value = m_value->as<SIMDValue>();
                switch (value->simdLane()) {
                case SIMDLane::i8x16:
                case SIMDLane::i16x8:
                case SIMDLane::i32x4:
                case SIMDLane::i64x2: {
                    Value* zero = m_insertionSet.insert<Const128Value>(m_index, m_origin, v128_t());
                    Value* result = m_insertionSet.insert<SIMDValue>(m_index, m_origin, VectorSub, B3::V128, value->simdInfo(), zero, m_value->child(0));
                    m_value->replaceWithIdentity(result);
                    m_changed = true;
                    break;
                }
                case SIMDLane::f32x4: {
                    Value* topBit = m_insertionSet.insert<Const32Value>(m_index, m_origin, 0x80000000u);
                    Value* floatMask = m_insertionSet.insert<Value>(m_index, BitwiseCast, m_origin, topBit);
                    Value* vectorMask = m_insertionSet.insert<SIMDValue>(m_index, m_origin, VectorSplat, B3::V128, SIMDLane::f32x4, SIMDSignMode::None, floatMask);
                    Value* result = m_insertionSet.insert<SIMDValue>(m_index, m_origin, VectorXor, B3::V128, SIMDLane::v128, SIMDSignMode::None, m_value->child(0), vectorMask);
                    m_value->replaceWithIdentity(result);
                    m_changed = true;
                    break;
                }
                case SIMDLane::f64x2: {
                    Value* topBit = m_insertionSet.insert<Const64Value>(m_index, m_origin, 0x8000000000000000ull);
                    Value* doubleMask = m_insertionSet.insert<Value>(m_index, BitwiseCast, m_origin, topBit);
                    Value* vectorMask = m_insertionSet.insert<SIMDValue>(m_index, m_origin, VectorSplat, B3::V128, SIMDLane::f64x2, SIMDSignMode::None, doubleMask);
                    Value* result = m_insertionSet.insert<SIMDValue>(m_index, m_origin, VectorXor, B3::V128, SIMDLane::v128, SIMDSignMode::None, m_value->child(0), vectorMask);
                    m_value->replaceWithIdentity(result);
                    m_changed = true;
                    break;
                }
                default:
                    RELEASE_ASSERT_NOT_REACHED();
                }
                break;
            }

            case VectorNotEqual:
                if (isX86())
                    invertedComparisonByXor(VectorEqual, m_value->child(0), m_value->child(1));
                break;
            case VectorAbove:
                if (isX86())
                    invertedComparisonByXor(VectorBelowOrEqual, m_value->child(0), m_value->child(1));
                break;
            case VectorBelow:
                if (isX86())
                    invertedComparisonByXor(VectorAboveOrEqual, m_value->child(0), m_value->child(1));
                break;
            case VectorGreaterThanOrEqual:
                if (isX86() && m_value->as<SIMDValue>()->simdLane() == SIMDLane::i64x2) {
                    // Note: rhs and lhs are reversed here, we are semantically negating LessThan. GreaterThan is
                    // just better supported on AVX.
                    invertedComparisonByXor(VectorGreaterThan, m_value->child(1), m_value->child(0));
                }
                break;
            case VectorLessThanOrEqual:
                if (isX86() && m_value->as<SIMDValue>()->simdLane() == SIMDLane::i64x2)
                    invertedComparisonByXor(VectorGreaterThan, m_value->child(0), m_value->child(1));
                break;
            case VectorShr:
            case VectorShl: {
                if constexpr (!isARM64())
                    break;
                SIMDValue* value = m_value->as<SIMDValue>();
                SIMDLane lane = value->simdLane();

                int32_t mask = (elementByteSize(lane) * CHAR_BIT) - 1;
                Value* shiftAmount = m_insertionSet.insert<Value>(m_index, BitAnd, m_origin, value->child(1), m_insertionSet.insertIntConstant(m_index, m_origin, Int32, mask));
                if (value->opcode() == VectorShr) {
                    // ARM64 doesn't have a version of this instruction for right shift. Instead, if the input to
                    // left shift is negative, it's a right shift by the absolute value of that amount.
                    shiftAmount = m_insertionSet.insert<Value>(m_index, Neg, m_origin, shiftAmount);
                }
                Value* shiftVector = m_insertionSet.insert<SIMDValue>(m_index, m_origin, VectorSplat, B3::V128, SIMDLane::i8x16, SIMDSignMode::None, shiftAmount);
                Value* result = m_insertionSet.insert<SIMDValue>(m_index, m_origin, VectorShiftByVector, B3::V128, value->simdInfo(), value->child(0), shiftVector);
                m_value->replaceWithIdentity(result);
                m_changed = true;
                break;
            }

            case WasmStructGet: {
                WasmStructGetValue* structGet = m_value->as<WasmStructGetValue>();
                Value* structPtr = structGet->child(0);
                SUPPRESS_UNCOUNTED_LOCAL const Wasm::StructType* structType = structGet->structType();
                Wasm::StructFieldCount fieldIndex = structGet->fieldIndex();
                auto fieldType = structType->field(fieldIndex).type;
                bool canTrap = structGet->kind().traps();
                HeapRange range = structGet->range();
                Mutability mutability = structGet->mutability();

                int32_t fieldOffset = JSWebAssemblyStruct::offsetOfData() + structType->offsetOfFieldInPayload(fieldIndex);

                auto wrapTrapping = [&](auto input) -> B3::Kind {
                    if (canTrap)
                        return trapping(input);
                    return input;
                };

                Value* result;
                if (fieldType.is<Wasm::PackedType>()) {
                    switch (fieldType.as<Wasm::PackedType>()) {
                    case Wasm::PackedType::I8:
                        result = m_insertionSet.insert<MemoryValue>(m_index, wrapTrapping(Load8Z), Int32, m_origin, structPtr, fieldOffset, range);
                        break;
                    case Wasm::PackedType::I16:
                        result = m_insertionSet.insert<MemoryValue>(m_index, wrapTrapping(Load16Z), Int32, m_origin, structPtr, fieldOffset, range);
                        break;
                    }
                } else {
                    ASSERT(fieldType.is<Wasm::Type>());
                    auto unpacked = fieldType.unpacked();
                    Type b3Type;
                    switch (unpacked.kind) {
                    case Wasm::TypeKind::I32:
                        b3Type = Int32;
                        break;
                    case Wasm::TypeKind::I64:
                        b3Type = Int64;
                        break;
                    case Wasm::TypeKind::F32:
                        b3Type = Float;
                        break;
                    case Wasm::TypeKind::F64:
                        b3Type = Double;
                        break;
                    case Wasm::TypeKind::V128:
                        b3Type = V128;
                        break;
                    default:
                        // Reference types are stored as Int64 (pointer-sized)
                        b3Type = Int64;
                        break;
                    }
                    result = m_insertionSet.insert<MemoryValue>(m_index, wrapTrapping(Load), b3Type, m_origin, structPtr, fieldOffset, range);
                }

                result->as<MemoryValue>()->setReadsMutability(mutability);
                m_value->replaceWithIdentity(result);
                m_changed = true;
                break;
            }

            case WasmStructSet: {
                WasmStructSetValue* structSet = m_value->as<WasmStructSetValue>();
                Value* structPtr = structSet->child(0);
                Value* value = structSet->child(1);
                SUPPRESS_UNCOUNTED_LOCAL const Wasm::StructType* structType = structSet->structType();
                Wasm::StructFieldCount fieldIndex = structSet->fieldIndex();
                auto fieldType = structType->field(fieldIndex).type;
                bool canTrap = structSet->kind().traps();
                HeapRange range = structSet->range();

                int32_t fieldOffset = JSWebAssemblyStruct::offsetOfData() + structType->offsetOfFieldInPayload(fieldIndex);

                auto wrapTrapping = [&](auto input) -> B3::Kind {
                    if (canTrap)
                        return trapping(input);
                    return input;
                };

                if (fieldType.is<Wasm::PackedType>()) {
                    switch (fieldType.as<Wasm::PackedType>()) {
                    case Wasm::PackedType::I8:
                        m_insertionSet.insert<MemoryValue>(m_index, wrapTrapping(Store8), m_origin, value, structPtr, fieldOffset, range);
                        break;
                    case Wasm::PackedType::I16:
                        m_insertionSet.insert<MemoryValue>(m_index, wrapTrapping(Store16), m_origin, value, structPtr, fieldOffset, range);
                        break;
                    }
                } else
                    m_insertionSet.insert<MemoryValue>(m_index, wrapTrapping(Store), m_origin, value, structPtr, fieldOffset, range);

                m_value->replaceWithNop();
                m_changed = true;
                break;
            }

            case WasmStructNew: {
                WasmStructNewValue* structNew = m_value->as<WasmStructNewValue>();
                Value* instance = structNew->instance();
                Value* structureID = structNew->structureID();
                SUPPRESS_UNCOUNTED_LOCAL const Wasm::StructType* structType = structNew->structType();
                uint32_t typeIndex = structNew->typeIndex();
                auto rtt = structNew->rtt();
                int32_t allocatorsBaseOffset = structNew->allocatorsBaseOffset();

                size_t allocationSize = JSWebAssemblyStruct::allocationSize(structType->instancePayloadSize());

                static_assert(!(MarkedSpace::sizeStep & (MarkedSpace::sizeStep - 1)), "MarkedSpace::sizeStep must be a power of two.");
                unsigned stepShift = getLSBSet(MarkedSpace::sizeStep);
                size_t sizeClass = (allocationSize + MarkedSpace::sizeStep - 1) >> stepShift;
                bool useFastPath = (sizeClass <= (MarkedSpace::largeCutoff >> stepShift));

                BasicBlock* before = m_blockInsertionSet.splitForward(m_block, m_index, &m_insertionSet);
                BasicBlock* slowPath = m_blockInsertionSet.insertBefore(m_block);

                UpsilonValue* fastUpsilon = nullptr;
                if (useFastPath) {
                    BasicBlock* fastPathContinuation = m_blockInsertionSet.insertBefore(m_block);

                    // Replace the Jump added by splitForward with Nop so we can add our own control flow
                    before->replaceLastWithNew<Value>(m_proc, Nop, m_origin);

                    // The Instance constructor initializes all the allocators on creation, thus it is never nullptr.
                    int32_t allocatorOffset = allocatorsBaseOffset + static_cast<int32_t>(sizeClass * sizeof(Allocator));
                    Value* allocator = before->appendNew<MemoryValue>(m_proc, Load, pointerType(), m_origin, instance, allocatorOffset);

                    PatchpointValue* patchpoint = before->appendNew<PatchpointValue>(m_proc, pointerType(), m_origin);
                    if (isARM64()) {
                        // emitAllocateWithNonNullAllocator uses the scratch registers on ARM.
                        patchpoint->clobber(RegisterSetBuilder::macroClobberedGPRs());
                    }
                    patchpoint->effects.terminal = true;
                    patchpoint->appendSomeRegisterWithClobber(allocator);
                    patchpoint->numGPScratchRegisters++;
                    patchpoint->resultConstraints = { ValueRep::SomeEarlyRegister };

                    patchpoint->setGenerator([=](CCallHelpers& jit, const StackmapGenerationParams& params) {
                        AllowMacroScratchRegisterUsage allowScratch(jit);
                        CCallHelpers::JumpList jumpToSlowPath;

                        GPRReg allocatorGPR = params[1].gpr();

                        // We use a patchpoint to emit the allocation path because whenever we mess with
                        // allocation paths, we already reason about them at the machine code level. We know
                        // exactly what instruction sequence we want. We're confident that no compiler
                        // optimization could make this code better. So, it's best to have the code in
                        // AssemblyHelpers::emitAllocate(). That way, the same optimized path is shared by
                        // all of the compiler tiers.
                        jit.emitAllocateWithNonNullAllocator(
                            params[0].gpr(), JITAllocator::variableNonNull(), allocatorGPR, params.gpScratch(0),
                            jumpToSlowPath, CCallHelpers::SlowAllocationResult::UndefinedBehavior);

                        CCallHelpers::Jump jumpToSuccess;
                        if (!params.fallsThroughToSuccessor(0))
                            jumpToSuccess = jit.jump();

                        Vector<Box<CCallHelpers::Label>> labels = params.successorLabels();

                        params.addLatePath([=](CCallHelpers& jit) {
                            jumpToSlowPath.linkTo(*labels[1], &jit);
                            if (jumpToSuccess.isSet())
                                jumpToSuccess.linkTo(*labels[0], &jit);
                        });
                    });

                    before->setSuccessors({ fastPathContinuation, FrequencyClass::Normal }, { slowPath, FrequencyClass::Rare });

                    // Header initialization happens in fastPathContinuation, not in fastPath
                    Value* cell = patchpoint;
                    Value* typeInfo = fastPathContinuation->appendNew<Const32Value>(m_proc, m_origin, JSWebAssemblyStruct::typeInfoBlob().blob());
                    fastPathContinuation->appendNew<MemoryValue>(m_proc, Store, m_origin, structureID, cell, static_cast<int32_t>(JSCell::structureIDOffset()));
                    fastPathContinuation->appendNew<MemoryValue>(m_proc, Store, m_origin, typeInfo, cell, static_cast<int32_t>(JSCell::indexingTypeAndMiscOffset()));
                    fastPathContinuation->appendNew<MemoryValue>(m_proc, Store, m_origin, fastPathContinuation->appendIntConstant(m_proc, m_origin, pointerType(), 0), cell, static_cast<int32_t>(JSObject::butterflyOffset()));
                    fastPathContinuation->appendNew<MemoryValue>(m_proc, Store, m_origin, fastPathContinuation->appendIntConstant(m_proc, m_origin, pointerType(), std::bit_cast<uintptr_t>(rtt.ptr())), cell, static_cast<int32_t>(WebAssemblyGCObjectBase::offsetOfRTT()));

                    fastUpsilon = fastPathContinuation->appendNew<UpsilonValue>(m_proc, m_origin, cell);
                    fastPathContinuation->appendNew<Value>(m_proc, Jump, m_origin);
                    fastPathContinuation->setSuccessors(m_block);
                } else {
                    // Just redirect the Jump added by splitForward to slowPath
                    before->setSuccessors(slowPath);
                }

                Value* slowFunctionAddress = slowPath->appendNew<ConstPtrValue>(m_proc, m_origin, tagCFunction<OperationPtrTag>(Wasm::operationWasmStructNewEmpty));
                Value* typeIndexValue = slowPath->appendNew<Const32Value>(m_proc, m_origin, typeIndex);
                Value* slowResult = slowPath->appendNew<CCallValue>(m_proc, Int64, m_origin, Effects::forCall(), slowFunctionAddress, instance, typeIndexValue);

                // Null check for slow path result
                Value* isNull = slowPath->appendNew<Value>(m_proc, Equal, m_origin, slowResult, slowPath->appendNew<Const64Value>(m_proc, m_origin, JSValue::encode(jsNull())));
                CheckValue* check = slowPath->appendNew<CheckValue>(m_proc, Check, m_origin, isNull);
                check->setGenerator([=](CCallHelpers& jit, const StackmapGenerationParams&) {
                    jit.move(CCallHelpers::TrustedImm32(static_cast<uint32_t>(Wasm::ExceptionType::BadStructNew)), GPRInfo::argumentGPR1);
                    jit.nearCallThunk(CodeLocationLabel<JITThunkPtrTag>(Wasm::Thunks::singleton().stub(Wasm::throwExceptionFromOMGThunkGenerator).code()));
                });

                UpsilonValue* slowUpsilon = slowPath->appendNew<UpsilonValue>(m_proc, m_origin, slowResult);
                slowPath->appendNew<Value>(m_proc, Jump, m_origin);
                slowPath->setSuccessors(m_block);

                Value* phi = m_insertionSet.insert<Value>(m_index, Phi, pointerType(), m_origin);
                if (fastUpsilon)
                    fastUpsilon->setPhi(phi);
                slowUpsilon->setPhi(phi);

                m_value->replaceWithIdentity(phi);
                before->updatePredecessorsAfter();
                m_changed = true;
                break;
            }

#if USE(JSVALUE64)
            case WasmRefCast:
            case WasmRefTest: {
                WasmRefTypeCheckValue* typeCheck = m_value->as<WasmRefTypeCheckValue>();
                // FIXME: In most of cases, we do not need to have split. We could split only when necessary.
                BasicBlock* before = m_blockInsertionSet.splitForward(m_block, m_index, &m_insertionSet);
                BasicBlock* continuation = m_block;
                m_value->replaceWithIdentity(emitRefTestOrCast(typeCheck, before, continuation));
                before->updatePredecessorsAfter();
                m_changed = true;
                break;
            }
#endif

            default:
                break;
            }
        }
        m_insertionSet.execute(m_block);
    }

    void invertedComparisonByXor(Opcode opcodeToBeInverted, Value* lhs, Value* rhs)
    {
        v128_t allOnes;
        allOnes.u64x2[0] = 0xffffffffffffffff;
        allOnes.u64x2[1] = 0xffffffffffffffff;
        Value* allOnesConst = m_insertionSet.insert<Const128Value>(m_index, m_origin, allOnes);
        Value* compareResult = m_insertionSet.insert<SIMDValue>(m_index, m_origin, opcodeToBeInverted, B3::V128, m_value->as<SIMDValue>()->simdInfo(), lhs, rhs);
        Value* result = m_insertionSet.insert<SIMDValue>(m_index, m_origin, VectorXor, B3::V128, SIMDLane::v128, SIMDSignMode::None, compareResult, allOnesConst);
        m_value->replaceWithIdentity(result);
        m_changed = true;
    }

#if USE(JSVALUE32_64)
    Value* callDivModHelper(BasicBlock* block, Opcode nonChillOpcode, Value* num, Value* den)
    {
        Type type = num->type();
        Value* functionAddress;
        if (nonChillOpcode == Div) {
            if (m_value->type() == Int64)
                functionAddress = block->appendNew<ConstPtrValue>(m_proc, m_origin, tagCFunction<OperationPtrTag>(Math::i64_div_s));
            else
                functionAddress = block->appendNew<ConstPtrValue>(m_proc, m_origin, tagCFunction<OperationPtrTag>(Math::i32_div_s));
        } else {
            if (m_value->type() == Int64)
                functionAddress = block->appendNew<ConstPtrValue>(m_proc, m_origin, tagCFunction<OperationPtrTag>(Math::i64_rem_s));
            else
                functionAddress = block->appendNew<ConstPtrValue>(m_proc, m_origin, tagCFunction<OperationPtrTag>(Math::i32_rem_s));
        }
        return block->appendNew<CCallValue>(m_proc, type, m_origin, Effects::none(), functionAddress, num, den);
    }
#else
    Value* callDivModHelper(BasicBlock*, Opcode, Value*, Value*)
    {
        RELEASE_ASSERT_NOT_REACHED();
    }
#endif
    void makeDivisionChill(Opcode nonChillOpcode)
    {
        ASSERT(nonChillOpcode == Div || nonChillOpcode == Mod);

        // ARM supports this instruction natively.
        if (isARM64())
            return;

        // We implement "res = Div<Chill>/Mod<Chill>(num, den)" as follows:
        //
        //     if (den + 1 <=_unsigned 1) {
        //         if (!den) {
        //             res = 0;
        //             goto done;
        //         }
        //         if (num == -2147483648) {
        //             res = isDiv ? num : 0;
        //             goto done;
        //         }
        //     }
        //     res = num (/ or %) dev;
        // done:
        m_changed = true;

        Value* num = m_value->child(0);
        Value* den = m_value->child(1);

        Value* one = m_insertionSet.insertIntConstant(m_index, m_value, 1);
        Value* isDenOK = m_insertionSet.insert<Value>(
            m_index, Above, m_origin,
            m_insertionSet.insert<Value>(m_index, Add, m_origin, den, one),
            one);

        BasicBlock* before = m_blockInsertionSet.splitForward(m_block, m_index, &m_insertionSet);

        BasicBlock* normalDivCase = m_blockInsertionSet.insertBefore(m_block);
        BasicBlock* shadyDenCase = m_blockInsertionSet.insertBefore(m_block);
        BasicBlock* zeroDenCase = m_blockInsertionSet.insertBefore(m_block);
        BasicBlock* neg1DenCase = m_blockInsertionSet.insertBefore(m_block);
        BasicBlock* intMinCase = m_blockInsertionSet.insertBefore(m_block);

        before->replaceLastWithNew<Value>(m_proc, Branch, m_origin, isDenOK);
        before->setSuccessors(
            FrequentedBlock(normalDivCase, FrequencyClass::Normal),
            FrequentedBlock(shadyDenCase, FrequencyClass::Rare));

        Value* innerResult;
        if (isARM_THUMB2() && (m_value->type() == Int64 || m_value->type() == Int32))
            innerResult = callDivModHelper(normalDivCase, nonChillOpcode, num, den);
        else
            innerResult = normalDivCase->appendNew<Value>(m_proc, nonChillOpcode, m_origin, num, den);
        UpsilonValue* normalResult = normalDivCase->appendNew<UpsilonValue>(
            m_proc, m_origin,
            innerResult);
        normalDivCase->appendNew<Value>(m_proc, Jump, m_origin);
        normalDivCase->setSuccessors(FrequentedBlock(m_block));

        shadyDenCase->appendNew<Value>(m_proc, Branch, m_origin, den);
        shadyDenCase->setSuccessors(
            FrequentedBlock(neg1DenCase, FrequencyClass::Normal),
            FrequentedBlock(zeroDenCase, FrequencyClass::Rare));

        UpsilonValue* zeroResult = zeroDenCase->appendNew<UpsilonValue>(
            m_proc, m_origin,
            zeroDenCase->appendIntConstant(m_proc, m_value, 0));
        zeroDenCase->appendNew<Value>(m_proc, Jump, m_origin);
        zeroDenCase->setSuccessors(FrequentedBlock(m_block));

        int64_t badNumeratorConst = 0;
        switch (m_value->type().kind()) {
        case Int32:
            badNumeratorConst = std::numeric_limits<int32_t>::min();
            break;
        case Int64:
            badNumeratorConst = std::numeric_limits<int64_t>::min();
            break;
        default:
            ASSERT_NOT_REACHED();
            badNumeratorConst = 0;
        }

        Value* badNumerator =
            neg1DenCase->appendIntConstant(m_proc, m_value, badNumeratorConst);

        neg1DenCase->appendNew<Value>(
            m_proc, Branch, m_origin,
            neg1DenCase->appendNew<Value>(
                m_proc, Equal, m_origin, num, badNumerator));
        neg1DenCase->setSuccessors(
            FrequentedBlock(intMinCase, FrequencyClass::Rare),
            FrequentedBlock(normalDivCase, FrequencyClass::Normal));

        Value* intMinResult = nonChillOpcode == Div ? badNumerator : intMinCase->appendIntConstant(m_proc, m_value, 0);
        UpsilonValue* intMinResultUpsilon = intMinCase->appendNew<UpsilonValue>(
            m_proc, m_origin, intMinResult);
        intMinCase->appendNew<Value>(m_proc, Jump, m_origin);
        intMinCase->setSuccessors(FrequentedBlock(m_block));

        Value* phi = m_insertionSet.insert<Value>(
            m_index, Phi, m_value->type(), m_origin);
        normalResult->setPhi(phi);
        zeroResult->setPhi(phi);
        intMinResultUpsilon->setPhi(phi);

        m_value->replaceWithIdentity(phi);
        before->updatePredecessorsAfter();
    }

    void recursivelyBuildSwitch(
        const Vector<SwitchCase>& cases, FrequentedBlock fallThrough, unsigned start, bool hardStart,
        unsigned end, BasicBlock* before)
    {
        Value* child = m_value->child(0);
        Type type = child->type();
        
        // It's a good idea to use a table-based switch in some cases: the number of cases has to be
        // large enough and they have to be dense enough. This could probably be improved a lot. For
        // example, we could still use a jump table in cases where the inputs are sparse so long as we
        // shift off the uninteresting bits. On the other hand, it's not clear that this would
        // actually be any better than what we have done here and it's not clear that it would be
        // better than a binary switch.
        const unsigned minCasesForTable = 7;
        const unsigned densityLimit = 4;
        if (end - start >= minCasesForTable) {
            int64_t firstValue = cases[start].caseValue();
            int64_t lastValue = cases[end - 1].caseValue();
            if ((lastValue - firstValue + 1) / (end - start) < densityLimit) {
                size_t tableSize = lastValue - firstValue + 1 + 1; // + 1 for fallthrough
                Value* index = before->appendNew<Value>(m_proc, Sub, m_origin, child, before->appendIntConstant(m_proc, m_origin, type, firstValue));
                Value* fallthroughIndex = before->appendIntConstant(m_proc, m_origin, type, tableSize - 1);
                index = before->appendNew<B3::Value>(m_proc, Select, m_origin, before->appendNew<Value>(m_proc, AboveEqual, m_origin, index, fallthroughIndex), fallthroughIndex, index);

                if (index->type() != pointerType() && index->type() == Int32)
                    index = before->appendNew<Value>(m_proc, ZExt32, m_origin, index);

                using JumpTableCodePtr = CodePtr<JSSwitchPtrTag>;
                JumpTableCodePtr* jumpTable = static_cast<JumpTableCodePtr*>(m_proc.addDataSection(sizeof(JumpTableCodePtr) * tableSize));
                auto* tableValue = before->appendIntConstant(m_proc, m_origin, pointerType(), std::bit_cast<uintptr_t>(jumpTable));
                auto* shifted = before->appendNew<Value>(m_proc, Shl, m_origin, index, before->appendIntConstant(m_proc, m_origin, Int32, getLSBSet(sizeof(JumpTableCodePtr))));
                auto* address = before->appendNew<Value>(m_proc, Add, pointerType(), m_origin, shifted, tableValue);
                auto* load = before->appendNew<MemoryValue>(m_proc, Load, pointerType(), m_origin, address);
                load->setControlDependent(false);
                load->setReadsMutability(B3::Mutability::Immutable);
                PatchpointValue* patchpoint = before->appendNew<PatchpointValue>(m_proc, Void, m_origin, cloningForbidden(Patchpoint));

                patchpoint->effects = Effects();
                patchpoint->effects.terminal = true;
                patchpoint->appendSomeRegister(load);
                // Technically, we don't have to clobber macro registers on X86_64. This is probably OK though.
                patchpoint->clobber(RegisterSetBuilder::macroClobberedGPRs());

                before->clearSuccessors();
                BitVector handledIndices;
                for (unsigned i = start; i < end; ++i) {
                    FrequentedBlock block = cases[i].target();
                    int64_t value = cases[i].caseValue();
                    before->appendSuccessor(block);
                    size_t index = value - firstValue;
                    ASSERT(!handledIndices.get(index));
                    handledIndices.set(index);
                }
                before->appendSuccessor(fallThrough);

                patchpoint->setGenerator(
                    [=](CCallHelpers& jit, const StackmapGenerationParams& params) {
                        AllowMacroScratchRegisterUsage allowScratch(jit);

                        GPRReg target = params[0].gpr();
                        jit.farJump(target, JSSwitchPtrTag);

                        // These labels are guaranteed to be populated before either late paths or
                        // link tasks run.
                        Vector<Box<CCallHelpers::Label>> labels = params.successorLabels();

                        jit.addLinkTask(
                            [=] (LinkBuffer& linkBuffer) {
                                JumpTableCodePtr fallThrough = linkBuffer.locationOf<JSSwitchPtrTag>(*labels.last());
                                for (unsigned i = 0; i < tableSize; ++i)
                                    jumpTable[i] = fallThrough;
                                unsigned labelIndex = 0;
                                for (unsigned tableIndex : handledIndices)
                                    jumpTable[tableIndex] = linkBuffer.locationOf<JSSwitchPtrTag>(*labels[labelIndex++]);
                            });
                    });
                return;
            }
        }
        
        // See comments in jit/BinarySwitch.cpp for a justification of this algorithm. The only
        // thing we do differently is that we don't use randomness.

        const unsigned leafThreshold = 3;

        unsigned size = end - start;

        if (size <= leafThreshold) {
            bool allConsecutive = false;

            if ((hardStart || (start && cases[start - 1].caseValue() == cases[start].caseValue() - 1))
                && end < cases.size()
                && cases[end - 1].caseValue() == cases[end].caseValue() - 1) {
                allConsecutive = true;
                for (unsigned i = 0; i < size - 1; ++i) {
                    if (cases[start + i].caseValue() + 1 != cases[start + i + 1].caseValue()) {
                        allConsecutive = false;
                        break;
                    }
                }
            }

            unsigned limit = allConsecutive ? size - 1 : size;
            
            for (unsigned i = 0; i < limit; ++i) {
                BasicBlock* nextCheck = m_blockInsertionSet.insertAfter(m_block);
                before->appendNew<Value>(
                    m_proc, Branch, m_origin,
                    before->appendNew<Value>(
                        m_proc, Equal, m_origin, child,
                        before->appendIntConstant(
                            m_proc, m_origin, type,
                            cases[start + i].caseValue())));
                before->setSuccessors(cases[start + i].target(), FrequentedBlock(nextCheck));

                before = nextCheck;
            }

            before->appendNew<Value>(m_proc, Jump, m_origin);
            if (allConsecutive)
                before->setSuccessors(cases[end - 1].target());
            else
                before->setSuccessors(fallThrough);
            return;
        }

        unsigned medianIndex = std::midpoint(start, end);

        BasicBlock* left = m_blockInsertionSet.insertAfter(m_block);
        BasicBlock* right = m_blockInsertionSet.insertAfter(m_block);

        before->appendNew<Value>(
            m_proc, Branch, m_origin,
            before->appendNew<Value>(
                m_proc, LessThan, m_origin, child,
                before->appendIntConstant(
                    m_proc, m_origin, type,
                    cases[medianIndex].caseValue())));
        before->setSuccessors(FrequentedBlock(left), FrequentedBlock(right));

        recursivelyBuildSwitch(cases, fallThrough, start, hardStart, medianIndex, left);
        recursivelyBuildSwitch(cases, fallThrough, medianIndex, true, end, right);
    }

#if USE(JSVALUE64)
    Value* emitRefTestOrCast(WasmRefTypeCheckValue* typeCheck, BasicBlock* before, BasicBlock* continuation)
    {
        enum class CastKind { Cast, Test };

        // CastKind::Test, reference, allowNull, heapType, shouldNegate, result
        Value* value = typeCheck->child(0);
        int32_t toHeapType = typeCheck->targetHeapType();
        bool allowNull = typeCheck->allowNull();
        bool referenceIsNullable = typeCheck->referenceIsNullable();
        bool definitelyIsCellOrNull = typeCheck->definitelyIsCellOrNull();
        bool definitelyIsWasmGCObjectOrNull = typeCheck->definitelyIsWasmGCObjectOrNull();
        SUPPRESS_UNCOUNTED_LOCAL const Wasm::RTT* targetRTT = typeCheck->targetRTT();
        bool isCast = typeCheck->kind().opcode() == WasmRefCast;
        CastKind castKind = isCast ? CastKind::Cast : CastKind::Test;
        bool shouldNegate = typeCheck->shouldNegate();
        Value* result = nullptr;

        if (isCast)
            result = value;

        BasicBlock* trueBlock = nullptr;
        BasicBlock* falseBlock = nullptr;
        if (!isCast) {
            trueBlock = m_proc.addBlock();
            falseBlock = m_proc.addBlock();
        }

        auto castFailure = [=](CCallHelpers& jit, const StackmapGenerationParams&) {
            jit.move(CCallHelpers::TrustedImm32(static_cast<uint32_t>(Wasm::ExceptionType::CastFailure)), GPRInfo::argumentGPR1);
            jit.nearCallThunk(CodeLocationLabel<JITThunkPtrTag>(Wasm::Thunks::singleton().stub(Wasm::throwExceptionFromOMGThunkGenerator).code()));
        };

        auto castAccessOffset = [&] -> std::optional<ptrdiff_t> {
            if (!isCast)
                return std::nullopt;

            if (allowNull)
                return std::nullopt;

            if (Wasm::typeIndexIsType(static_cast<Wasm::TypeIndex>(toHeapType)))
                return std::nullopt;

            if (targetRTT->kind() == Wasm::RTTKind::Function)
                return WebAssemblyFunctionBase::offsetOfRTT();

            if (!definitelyIsCellOrNull)
                return std::nullopt;
            if (!definitelyIsWasmGCObjectOrNull)
                return JSCell::typeInfoTypeOffset();
            return JSCell::structureIDOffset();
        };

        bool canTrap = false;
        auto wrapTrapping = [&](auto input) -> B3::Kind {
            if (canTrap) {
                canTrap = false;
                return trapping(input);
            }
            return input;
        };

        // Ensure reference nullness agrees with heap type.
        before->replaceLastWithNew<Value>(m_proc, Nop, m_origin);
        before->clearSuccessors();
        auto* currentBlock = before;

        auto constant = [&](TypeKind type, uint64_t bits) -> Value* {
            switch (type) {
            case Int32:
                return currentBlock->appendNew<Const32Value>(m_proc, m_origin, bits);
            case Int64:
                return currentBlock->appendNew<Const64Value>(m_proc, m_origin, bits);
            default:
                RELEASE_ASSERT_NOT_REACHED();
                return nullptr;
            }
        };

        auto emitCheckOrBranchForCast = [&](CastKind kind, Value* condition, const auto& generator, BasicBlock* falseBlock) {
            if (kind == CastKind::Cast) {
                CheckValue* check = currentBlock->appendNew<CheckValue>(m_proc, Check, m_origin, condition);
                check->setGenerator(generator);
            } else {
                ASSERT(falseBlock);
                BasicBlock* success = m_proc.addBlock();
                currentBlock->appendNewControlValue(m_proc, B3::Branch, m_origin, condition, FrequentedBlock(falseBlock), FrequentedBlock(success));
                falseBlock->addPredecessor(currentBlock);
                success->addPredecessor(currentBlock);
                currentBlock = success;
            }
        };

        {
            BasicBlock* nullCase = m_proc.addBlock();
            BasicBlock* nonNullCase = m_proc.addBlock();

            Value* isNull = nullptr;
            if (referenceIsNullable) {
                if (auto offset = castAccessOffset(); offset && offset.value() <= Wasm::maxAcceptableOffsetForNullReference()) {
                    isNull = constant(Int32, 0);
                    canTrap = true;
                } else
                    isNull = currentBlock->appendNew<Value>(m_proc, Equal, m_origin, value, constant(Int64, JSValue::encode(jsNull())));
            } else
                isNull = constant(Int32, 0);

            currentBlock->appendNewControlValue(m_proc, B3::Branch, m_origin, isNull, FrequentedBlock(nullCase), FrequentedBlock(nonNullCase));
            nullCase->addPredecessor(currentBlock);
            nonNullCase->addPredecessor(currentBlock);

            currentBlock = nullCase;
            if (isCast) {
                if (!allowNull) {
                    B3::PatchpointValue* throwException = currentBlock->appendNew<B3::PatchpointValue>(m_proc, B3::Void, m_origin);
                    throwException->setGenerator(castFailure);
                }
                currentBlock->appendNewControlValue(m_proc, Jump, m_origin, continuation);
                continuation->addPredecessor(currentBlock);
            } else {
                BasicBlock* nextBlock;
                if (!allowNull)
                    nextBlock = falseBlock;
                else
                    nextBlock = trueBlock;
                currentBlock->appendNewControlValue(m_proc, Jump, m_origin, nextBlock);
                nextBlock->addPredecessor(currentBlock);
            }

            currentBlock = nonNullCase;
        }

        if (Wasm::typeIndexIsType(static_cast<Wasm::TypeIndex>(toHeapType))) {
            switch (static_cast<Wasm::TypeKind>(toHeapType)) {
            case Wasm::TypeKind::Funcref:
            case Wasm::TypeKind::Externref:
            case Wasm::TypeKind::Anyref:
            case Wasm::TypeKind::Exnref:
                // Casts to these types cannot fail as they are the top types of their respective hierarchies, and static type-checking does not allow cross-hierarchy casts.
                break;
            case Wasm::TypeKind::Noneref:
            case Wasm::TypeKind::Nofuncref:
            case Wasm::TypeKind::Noexternref:
            case Wasm::TypeKind::Noexnref:
                // Casts to any bottom type should always fail.
                if (isCast) {
                    B3::PatchpointValue* throwException = currentBlock->appendNew<B3::PatchpointValue>(m_proc, B3::Void, m_origin);
                    throwException->setGenerator(castFailure);
                } else {
                    currentBlock->appendNewControlValue(m_proc, Jump, m_origin, falseBlock);
                    falseBlock->addPredecessor(currentBlock);
                    currentBlock = m_proc.addBlock();
                }
                break;
            case Wasm::TypeKind::Eqref: {
                auto nop = [] (CCallHelpers&, const B3::StackmapGenerationParams&) { };
                BasicBlock* endBlock = isCast ? continuation : trueBlock;
                BasicBlock* checkObject = m_proc.addBlock();

                // The eqref case chains together checks for i31, array, and struct with disjunctions so the control flow is more complicated, and requires some extra basic blocks to be created.
                emitCheckOrBranchForCast(CastKind::Test, currentBlock->appendNew<Value>(m_proc, Below, m_origin, value, constant(Int64, JSValue::NumberTag)), nop, checkObject);
                Value* untagged = currentBlock->appendNew<Value>(m_proc, Trunc, m_origin, value);
                emitCheckOrBranchForCast(CastKind::Test, currentBlock->appendNew<Value>(m_proc, GreaterThan, m_origin, untagged, constant(Int32, Wasm::maxI31ref)), nop, checkObject);
                emitCheckOrBranchForCast(CastKind::Test, currentBlock->appendNew<Value>(m_proc, LessThan, m_origin, untagged, constant(Int32, Wasm::minI31ref)), nop, checkObject);
                currentBlock->appendNewControlValue(m_proc, Jump, m_origin, endBlock);
                checkObject->addPredecessor(currentBlock);
                endBlock->addPredecessor(currentBlock);

                currentBlock = checkObject;
                if (!definitelyIsCellOrNull)
                    emitCheckOrBranchForCast(castKind, currentBlock->appendNew<Value>(m_proc, BitAnd, m_origin, value, constant(Int64, JSValue::NotCellMask)), castFailure, falseBlock);
                if (!definitelyIsWasmGCObjectOrNull) {
                    auto* jsType = currentBlock->appendNew<MemoryValue>(m_proc, Load8Z, Int32, m_origin, value, safeCast<int32_t>(JSCell::typeInfoTypeOffset()));

                    emitCheckOrBranchForCast(castKind, currentBlock->appendNew<Value>(m_proc, NotEqual, m_origin, jsType, constant(Int32, JSType::WebAssemblyGCObjectType)), castFailure, falseBlock);
                }
                break;
            }
            case Wasm::TypeKind::I31ref: {
                emitCheckOrBranchForCast(castKind, currentBlock->appendNew<Value>(m_proc, Below, m_origin, value, constant(Int64, JSValue::NumberTag)), castFailure, falseBlock);
                Value* untagged = currentBlock->appendNew<Value>(m_proc, Trunc, m_origin, value);
                emitCheckOrBranchForCast(castKind, currentBlock->appendNew<Value>(m_proc, GreaterThan, m_origin, untagged, constant(Int32, Wasm::maxI31ref)), castFailure, falseBlock);
                emitCheckOrBranchForCast(castKind, currentBlock->appendNew<Value>(m_proc, LessThan, m_origin, untagged, constant(Int32, Wasm::minI31ref)), castFailure, falseBlock);
                break;
            }
            case Wasm::TypeKind::Arrayref:
            case Wasm::TypeKind::Structref: {
                if (!definitelyIsCellOrNull)
                    emitCheckOrBranchForCast(castKind, currentBlock->appendNew<Value>(m_proc, BitAnd, m_origin, value, constant(Int64, JSValue::NotCellMask)), castFailure, falseBlock);
                if (!definitelyIsWasmGCObjectOrNull) {
                    auto* jsType = currentBlock->appendNew<MemoryValue>(m_proc, Load8Z, Int32, m_origin, value, safeCast<int32_t>(JSCell::typeInfoTypeOffset()));

                    emitCheckOrBranchForCast(castKind, currentBlock->appendNew<Value>(m_proc, NotEqual, m_origin, jsType, constant(Int32, JSType::WebAssemblyGCObjectType)), castFailure, falseBlock);
                }
                Value* rtt = currentBlock->appendNew<MemoryValue>(m_proc, B3::Load, pointerType(), m_origin, value, safeCast<int32_t>(WebAssemblyGCObjectBase::offsetOfRTT()));
                auto* kind = currentBlock->appendNew<MemoryValue>(m_proc, Load8Z, m_origin, rtt, safeCast<int32_t>(Wasm::RTT::offsetOfKind()));
                kind->setControlDependent(false);

                emitCheckOrBranchForCast(castKind, currentBlock->appendNew<Value>(m_proc, NotEqual, m_origin, kind, constant(Int32, static_cast<uint8_t>(static_cast<Wasm::TypeKind>(toHeapType) == Wasm::TypeKind::Arrayref ? Wasm::RTTKind::Array : Wasm::RTTKind::Struct))), castFailure, falseBlock);
                break;
            }
            default:
                RELEASE_ASSERT_NOT_REACHED();
            }
        } else {
            ([&] {
                MemoryValue* rtt;
                auto* targetRTTPointer = constant(Int64, std::bit_cast<uintptr_t>(targetRTT));
                if (targetRTT->kind() == Wasm::RTTKind::Function)
                    rtt = currentBlock->appendNew<MemoryValue>(m_proc, wrapTrapping(B3::Load), Int64, m_origin, value, safeCast<int32_t>(WebAssemblyFunctionBase::offsetOfRTT()));
                else {
                    // The cell check is only needed for non-functions, as the typechecker does not allow non-Cell values for funcref casts.
                    if (!definitelyIsCellOrNull)
                        emitCheckOrBranchForCast(castKind, currentBlock->appendNew<Value>(m_proc, BitAnd, m_origin, value, constant(Int64, JSValue::NotCellMask)), castFailure, falseBlock);

                    if (!definitelyIsWasmGCObjectOrNull) {
                        auto* jsType = currentBlock->appendNew<MemoryValue>(m_proc, wrapTrapping(Load8Z), Int32, m_origin, value, safeCast<int32_t>(JSCell::typeInfoTypeOffset()));
                        emitCheckOrBranchForCast(castKind, currentBlock->appendNew<Value>(m_proc, NotEqual, m_origin, jsType, constant(Int32, JSType::WebAssemblyGCObjectType)), castFailure, falseBlock);
                    }

                    rtt = currentBlock->appendNew<MemoryValue>(m_proc, B3::Load, pointerType(), m_origin, value, safeCast<int32_t>(WebAssemblyGCObjectBase::offsetOfRTT()));
                    if (targetRTT->isFinalType()) {
                        // If signature is final type and pointer equality failed, this value must not be a subtype.
                        emitCheckOrBranchForCast(castKind, currentBlock->appendNew<Value>(m_proc, NotEqual, m_origin, rtt, targetRTTPointer), castFailure, falseBlock);
                        return;
                    }

                    if (targetRTT->displaySizeExcludingThis() < Wasm::RTT::inlinedDisplaySize) {
                        auto* pointer = currentBlock->appendNew<MemoryValue>(m_proc, B3::Load, Int64, m_origin, rtt, safeCast<int32_t>(Wasm::RTT::offsetOfData() + targetRTT->displaySizeExcludingThis() * sizeof(RefPtr<const Wasm::RTT>)));
                        pointer->setReadsMutability(B3::Mutability::Immutable);
                        pointer->setControlDependent(false);

                        emitCheckOrBranchForCast(castKind, currentBlock->appendNew<Value>(m_proc, NotEqual, m_origin, pointer, targetRTTPointer), castFailure, falseBlock);
                        return;
                    }
                }

                BasicBlock* equalBlock;
                if (isCast)
                    equalBlock = continuation;
                else
                    equalBlock = trueBlock;
                BasicBlock* slowPath = m_proc.addBlock();
                currentBlock->appendNewControlValue(m_proc, B3::Branch, m_origin, currentBlock->appendNew<Value>(m_proc, Equal, m_origin, rtt, targetRTTPointer), FrequentedBlock(equalBlock), FrequentedBlock(slowPath));
                equalBlock->addPredecessor(currentBlock);
                slowPath->addPredecessor(currentBlock);

                currentBlock = slowPath;
                if (targetRTT->isFinalType()) {
                    // If signature is final type and pointer equality failed, this value must not be a subtype.
                    emitCheckOrBranchForCast(castKind, constant(Int32, 1), castFailure, falseBlock);
                } else {
                    auto* displaySizeExcludingThis = currentBlock->appendNew<MemoryValue>(m_proc, B3::Load, Int32, m_origin, rtt, safeCast<int32_t>(Wasm::RTT::offsetOfDisplaySizeExcludingThis()));
                    displaySizeExcludingThis->setControlDependent(false);

                    emitCheckOrBranchForCast(castKind, currentBlock->appendNew<Value>(m_proc, BelowEqual, m_origin, displaySizeExcludingThis, constant(Int32, targetRTT->displaySizeExcludingThis())), castFailure, falseBlock);

                    auto* pointer = currentBlock->appendNew<MemoryValue>(m_proc, B3::Load, Int64, m_origin, rtt, safeCast<int32_t>(Wasm::RTT::offsetOfData() + targetRTT->displaySizeExcludingThis() * sizeof(RefPtr<const Wasm::RTT>)));
                    pointer->setReadsMutability(B3::Mutability::Immutable);
                    pointer->setControlDependent(false);

                    emitCheckOrBranchForCast(castKind, currentBlock->appendNew<Value>(m_proc, NotEqual, m_origin, pointer, targetRTTPointer), castFailure, falseBlock);
                }
            }());
        }

        if (isCast) {
            currentBlock->appendNewControlValue(m_proc, Jump, m_origin, continuation);
            continuation->addPredecessor(currentBlock);
            currentBlock = continuation;
        } else {
            currentBlock->appendNewControlValue(m_proc, Jump, m_origin, trueBlock);
            trueBlock->addPredecessor(currentBlock);
            currentBlock = trueBlock;
            UpsilonValue* trueUpsilon = currentBlock->appendNew<UpsilonValue>(m_proc, m_origin, constant(B3::Int32, shouldNegate ? 0 : 1));
            currentBlock->appendNewControlValue(m_proc, Jump, m_origin, continuation);
            continuation->addPredecessor(currentBlock);

            currentBlock = falseBlock;
            UpsilonValue* falseUpsilon = currentBlock->appendNew<UpsilonValue>(m_proc, m_origin, constant(B3::Int32, shouldNegate ? 1 : 0));
            currentBlock->appendNewControlValue(m_proc, Jump, m_origin, continuation);
            continuation->addPredecessor(currentBlock);

            currentBlock = continuation;
            Value* phi = m_insertionSet.insert<Value>(m_index, Phi, m_value->type(), m_origin);
            trueUpsilon->setPhi(phi);
            falseUpsilon->setPhi(phi);
            result = phi;
        }

        return result;
    }
#endif

    Procedure& m_proc;
    BlockInsertionSet m_blockInsertionSet;
    InsertionSet m_insertionSet;
    UseCounts m_useCounts;
    BasicBlock* m_block;
    unsigned m_index;
    Value* m_value;
    Origin m_origin;
    bool m_changed { false };
};

} // anonymous namespace

bool lowerMacros(Procedure& proc)
{
    PhaseScope phaseScope(proc, "B3::lowerMacros"_s);
    LowerMacros lowerMacros(proc);
    return lowerMacros.run();
}

} } // namespace JSC::B3

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(B3_JIT)
