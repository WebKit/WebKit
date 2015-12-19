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
#include "B3ReduceStrength.h"

#if ENABLE(B3_JIT)

#include "B3BasicBlockInlines.h"
#include "B3ControlValue.h"
#include "B3Dominators.h"
#include "B3IndexSet.h"
#include "B3InsertionSetInlines.h"
#include "B3MemoryValue.h"
#include "B3PhaseScope.h"
#include "B3PhiChildren.h"
#include "B3ProcedureInlines.h"
#include "B3UpsilonValue.h"
#include "B3UseCounts.h"
#include "B3ValueKey.h"
#include "B3ValueInlines.h"
#include <wtf/GraphNodeWorklist.h>
#include <wtf/HashMap.h>

namespace JSC { namespace B3 {

namespace {

// The goal of this phase is to:
//
// - Replace operations with less expensive variants. This includes constant folding and classic
//   strength reductions like turning Mul(x, 1 << k) into Shl(x, k).
//
// - Reassociate constant operations. For example, Load(Add(x, c)) is turned into Load(x, offset = c)
//   and Add(Add(x, c), d) is turned into Add(x, c + d).
//
// - Canonicalize operations. There are some cases where it's not at all obvious which kind of
//   operation is less expensive, but it's useful for subsequent phases - particularly LowerToAir -
//   to have only one way of representing things.
//
// This phase runs to fixpoint. Therefore, the canonicalizations must be designed to be monotonic.
// For example, if we had a canonicalization that said that Add(x, -c) should be Sub(x, c) and
// another canonicalization that said that Sub(x, d) should be Add(x, -d), then this phase would end
// up running forever. We don't want that.
//
// Therefore, we need to prioritize certain canonical forms over others. Naively, we want strength
// reduction to reduce the number of values, and so a form involving fewer total values is more
// canonical. But we might break this, for example when reducing strength of Mul(x, 9). This could be
// better written as Add(Shl(x, 3), x), which also happens to be representable using a single
// instruction on x86.
//
// Here are some of the rules we have:
//
// Canonical form of logical not: BitXor(value, 1). We may have to avoid using this form if we don't
// know for sure that 'value' is 0-or-1 (i.e. returnsBool). In that case we fall back on
// Equal(value, 0).
//
// Canonical form of commutative operations: if the operation involves a constant, the constant must
// come second. Add(x, constant) is canonical, while Add(constant, x) is not. If there are no
// constants then the canonical form involves the lower-indexed value first. Given Add(x, y), it's
// canonical if x->index() <= y->index().

bool verbose = false;

class ReduceStrength {
public:
    ReduceStrength(Procedure& proc)
        : m_proc(proc)
        , m_insertionSet(proc)
    {
    }

    bool run()
    {
        bool result = false;
        bool first = true;
        unsigned index = 0;
        do {
            m_changed = false;
            m_changedCFG = false;
            ++index;

            if (first)
                first = false;
            else if (verbose) {
                dataLog("B3 after iteration #", index - 1, " of reduceStrength:\n");
                dataLog(m_proc);
            }

            m_proc.resetValueOwners();
            m_dominators = &m_proc.dominators(); // Recompute if necessary.
            m_pureValues.clear();

            for (BasicBlock* block : m_proc.blocksInPreOrder()) {
                m_block = block;
                
                for (m_index = 0; m_index < block->size(); ++m_index) {
                    m_value = m_block->at(m_index);
                    m_value->performSubstitution();
                    
                    reduceValueStrength();
                    replaceIfRedundant();
                }
                m_insertionSet.execute(m_block);
            }

            simplifyCFG();

            if (m_changedCFG) {
                m_proc.resetReachability();
                m_proc.invalidateCFG();
                m_changed = true;
            }

            killDeadCode();
            simplifySSA();
            
            result |= m_changed;
        } while (m_changed);
        return result;
    }
    
private:
    void reduceValueStrength()
    {
        switch (m_value->opcode()) {
        case Add:
            handleCommutativity();

            // Turn this: Add(Add(value, constant1), constant2)
            // Into this: Add(value, constant1 + constant2)
            if (m_value->child(0)->opcode() == Add && isInt(m_value->type())) {
                Value* newSum = m_value->child(1)->addConstant(m_proc, m_value->child(0)->child(1));
                if (newSum) {
                    m_insertionSet.insertValue(m_index, newSum);
                    m_value->child(0) = m_value->child(0)->child(0);
                    m_value->child(1) = newSum;
                    m_changed = true;
                }
            }

            // Turn this: Add(constant1, constant2)
            // Into this: constant1 + constant2
            if (Value* constantAdd = m_value->child(0)->addConstant(m_proc, m_value->child(1))) {
                replaceWithNewValue(constantAdd);
                break;
            }

            // Turn this: Integer Add(value, value)
            // Into this: Shl(value, 1)
            // This is a useful canonicalization. It's not meant to be a strength reduction.
            if (m_value->isInteger() && m_value->child(0) == m_value->child(1)) {
                replaceWithNewValue(
                    m_proc.add<Value>(
                        Shl, m_value->origin(), m_value->child(0),
                        m_insertionSet.insert<Const32Value>(m_index, m_value->origin(), 1)));
                break;
            }

            // Turn this: Add(value, zero)
            // Into an Identity.
            //
            // Addition is subtle with doubles. Zero is not the neutral value, negative zero is:
            //    0 + 0 = 0
            //    0 + -0 = 0
            //    -0 + 0 = 0
            //    -0 + -0 = -0
            if (m_value->child(1)->isInt(0) || m_value->child(1)->isNegativeZero()) {
                m_value->replaceWithIdentity(m_value->child(0));
                m_changed = true;
                break;
            }

            // Turn this: Integer Add(Sub(0, value), -1)
            // Into this: BitXor(value, -1)
            if (m_value->isInteger()
                && m_value->child(0)->opcode() == Sub
                && m_value->child(1)->isInt(-1)
                && m_value->child(0)->child(0)->isInt(0)) {
                replaceWithNewValue(m_proc.add<Value>(BitXor, m_value->origin(), m_value->child(0)->child(1), m_value->child(1)));
                break;
            }

            break;

        case Sub:
            // Turn this: Sub(constant1, constant2)
            // Into this: constant1 - constant2
            if (Value* constantSub = m_value->child(0)->subConstant(m_proc, m_value->child(1))) {
                replaceWithNewValue(constantSub);
                break;
            }

            // Turn this: Sub(value, constant)
            // Into this: Add(value, -constant)
            if (isInt(m_value->type())) {
                if (Value* negatedConstant = m_value->child(1)->negConstant(m_proc)) {
                    m_insertionSet.insertValue(m_index, negatedConstant);
                    replaceWithNew<Value>(
                        Add, m_value->origin(), m_value->child(0), negatedConstant);
                    break;
                }
            }

            break;

        case Mul:
            handleCommutativity();

            // Turn this: Mul(constant1, constant2)
            // Into this: constant1 * constant2
            if (Value* value = m_value->child(0)->mulConstant(m_proc, m_value->child(1))) {
                replaceWithNewValue(value);
                break;
            }

            if (m_value->child(1)->hasInt()) {
                int64_t factor = m_value->child(1)->asInt();

                // Turn this: Mul(value, 0)
                // Into this: 0
                // Note that we don't do this for doubles because that's wrong. For example, -1 * 0
                // and 1 * 0 yield different results.
                if (!factor) {
                    m_value->replaceWithIdentity(m_value->child(1));
                    m_changed = true;
                    break;
                }

                // Turn this: Mul(value, 1)
                // Into this: value
                if (factor == 1) {
                    m_value->replaceWithIdentity(m_value->child(0));
                    m_changed = true;
                    break;
                }

                // Turn this: Mul(value, -1)
                // Into this: Sub(0, value)
                if (factor == -1) {
                    replaceWithNewValue(
                        m_proc.add<Value>(
                            Sub, m_value->origin(),
                            m_insertionSet.insertIntConstant(m_index, m_value, 0),
                            m_value->child(0)));
                    break;
                }
                
                // Turn this: Mul(value, constant)
                // Into this: Shl(value, log2(constant))
                if (hasOneBitSet(factor)) {
                    unsigned shiftAmount = WTF::fastLog2(static_cast<uint64_t>(factor));
                    replaceWithNewValue(
                        m_proc.add<Value>(
                            Shl, m_value->origin(), m_value->child(0),
                            m_insertionSet.insert<Const32Value>(
                                m_index, m_value->origin(), shiftAmount)));
                    break;
                }
            } else if (m_value->child(1)->hasDouble()) {
                double factor = m_value->child(1)->asDouble();

                // Turn this: Mul(value, 1)
                // Into this: value
                if (factor == 1) {
                    m_value->replaceWithIdentity(m_value->child(0));
                    break;
                }
            }

            break;

        case Div:
        case ChillDiv:
            // Turn this: Div(constant1, constant2)
            // Into this: constant1 / constant2
            // Note that this uses ChillDiv semantics. That's fine, because the rules for Div
            // are strictly weaker: it has corner cases where it's allowed to do anything it
            // likes.
            replaceWithNewValue(m_value->child(0)->divConstant(m_proc, m_value->child(1)));
            break;

        case Mod:
        case ChillMod:
            // Turn this: Mod(constant1, constant2)
            // Into this: constant1 / constant2
            // Note that this uses ChillMod semantics.
            replaceWithNewValue(m_value->child(0)->modConstant(m_proc, m_value->child(1)));
            break;

        case BitAnd:
            handleCommutativity();

            // Turn this: BitAnd(constant1, constant2)
            // Into this: constant1 & constant2
            if (Value* constantBitAnd = m_value->child(0)->bitAndConstant(m_proc, m_value->child(1))) {
                replaceWithNewValue(constantBitAnd);
                break;
            }

            // Turn this: BitAnd(BitAnd(value, constant1), constant2)
            // Into this: BitAnd(value, constant1 & constant2).
            if (m_value->child(0)->opcode() == BitAnd) {
                Value* newConstant = m_value->child(1)->bitAndConstant(m_proc, m_value->child(0)->child(1));
                if (newConstant) {
                    m_insertionSet.insertValue(m_index, newConstant);
                    m_value->child(0) = m_value->child(0)->child(0);
                    m_value->child(1) = newConstant;
                    m_changed = true;
                }
            }

            // Turn this: BitAnd(valueX, valueX)
            // Into this: valueX.
            if (m_value->child(0) == m_value->child(1)) {
                m_value->replaceWithIdentity(m_value->child(0));
                m_changed = true;
                break;
            }

            // Turn this: BitAnd(value, zero-constant)
            // Into this: zero-constant.
            if (m_value->child(1)->isInt(0)) {
                m_value->replaceWithIdentity(m_value->child(1));
                m_changed = true;
                break;
            }

            // Turn this: BitAnd(value, all-ones)
            // Into this: value.
            if ((m_value->type() == Int64 && m_value->child(1)->isInt(0xffffffffffffffff))
                || (m_value->type() == Int32 && m_value->child(1)->isInt(0xffffffff))) {
                m_value->replaceWithIdentity(m_value->child(0));
                m_changed = true;
                break;
            }

            // Turn this: BitAnd(64-bit value, 32 ones)
            // Into this: ZExt32(Trunc(64-bit value))
            if (m_value->child(1)->isInt64(0xffffffffllu)) {
                Value* newValue = m_insertionSet.insert<Value>(
                    m_index, ZExt32, m_value->origin(),
                    m_insertionSet.insert<Value>(m_index, Trunc, m_value->origin(), m_value->child(0)));
                m_value->replaceWithIdentity(newValue);
                m_changed = true;
                break;
            }

            // Turn this: BitAnd(SExt8(value), mask) where (mask & 0xffffff00) == 0
            // Into this: BitAnd(value, mask)
            if (m_value->child(0)->opcode() == SExt8 && m_value->child(1)->hasInt32()
                && !(m_value->child(1)->asInt32() & 0xffffff00)) {
                m_value->child(0) = m_value->child(0)->child(0);
                m_changed = true;
            }

            // Turn this: BitAnd(SExt16(value), mask) where (mask & 0xffff0000) == 0
            // Into this: BitAnd(value, mask)
            if (m_value->child(0)->opcode() == SExt16 && m_value->child(1)->hasInt32()
                && !(m_value->child(1)->asInt32() & 0xffff0000)) {
                m_value->child(0) = m_value->child(0)->child(0);
                m_changed = true;
            }

            // Turn this: BitAnd(SExt32(value), mask) where (mask & 0xffffffff00000000) == 0
            // Into this: BitAnd(ZExt32(value), mask)
            if (m_value->child(0)->opcode() == SExt32 && m_value->child(1)->hasInt32()
                && !(m_value->child(1)->asInt32() & 0xffffffff00000000llu)) {
                m_value->child(0) = m_insertionSet.insert<Value>(
                    m_index, ZExt32, m_value->origin(),
                    m_value->child(0)->child(0), m_value->child(0)->child(1));
                m_changed = true;
            }
            break;

        case BitOr:
            handleCommutativity();

            // Turn this: BitOr(constant1, constant2)
            // Into this: constant1 | constant2
            if (Value* constantBitOr = m_value->child(0)->bitOrConstant(m_proc, m_value->child(1))) {
                replaceWithNewValue(constantBitOr);
                break;
            }

            // Turn this: BitOr(BitOr(value, constant1), constant2)
            // Into this: BitOr(value, constant1 & constant2).
            if (m_value->child(0)->opcode() == BitOr) {
                Value* newConstant = m_value->child(1)->bitOrConstant(m_proc, m_value->child(0)->child(1));
                if (newConstant) {
                    m_insertionSet.insertValue(m_index, newConstant);
                    m_value->child(0) = m_value->child(0)->child(0);
                    m_value->child(1) = newConstant;
                    m_changed = true;
                }
            }

            // Turn this: BitOr(valueX, valueX)
            // Into this: valueX.
            if (m_value->child(0) == m_value->child(1)) {
                m_value->replaceWithIdentity(m_value->child(0));
                m_changed = true;
                break;
            }

            // Turn this: BitOr(value, zero-constant)
            // Into this: value.
            if (m_value->child(1)->isInt(0)) {
                m_value->replaceWithIdentity(m_value->child(0));
                m_changed = true;
                break;
            }

            // Turn this: BitOr(value, all-ones)
            // Into this: all-ones.
            if ((m_value->type() == Int64 && m_value->child(1)->isInt(0xffffffffffffffff))
                || (m_value->type() == Int32 && m_value->child(1)->isInt(0xffffffff))) {
                m_value->replaceWithIdentity(m_value->child(1));
                m_changed = true;
                break;
            }

            break;

        case BitXor:
            handleCommutativity();

            // Turn this: BitXor(constant1, constant2)
            // Into this: constant1 ^ constant2
            if (Value* constantBitXor = m_value->child(0)->bitXorConstant(m_proc, m_value->child(1))) {
                replaceWithNewValue(constantBitXor);
                break;
            }

            // Turn this: BitXor(BitXor(value, constant1), constant2)
            // Into this: BitXor(value, constant1 ^ constant2).
            if (m_value->child(0)->opcode() == BitXor) {
                Value* newConstant = m_value->child(1)->bitXorConstant(m_proc, m_value->child(0)->child(1));
                if (newConstant) {
                    m_insertionSet.insertValue(m_index, newConstant);
                    m_value->child(0) = m_value->child(0)->child(0);
                    m_value->child(1) = newConstant;
                    m_changed = true;
                }
            }

            // Turn this: BitXor(compare, 1)
            // Into this: invertedCompare
            if (m_value->child(1)->isInt32(1)) {
                if (Value* invertedCompare = m_value->child(0)->invertedCompare(m_proc)) {
                    replaceWithNewValue(invertedCompare);
                    break;
                }
            }

            // Turn this: BitXor(valueX, valueX)
            // Into this: zero-constant.
            if (m_value->child(0) == m_value->child(1)) {
                replaceWithNewValue(m_proc.addIntConstant(m_value, 0));
                break;
            }

            // Turn this: BitXor(value, zero-constant)
            // Into this: value.
            if (m_value->child(1)->isInt(0)) {
                m_value->replaceWithIdentity(m_value->child(0));
                m_changed = true;
                break;
            }

            break;

        case Shl:
            // Turn this: Shl(constant1, constant2)
            // Into this: constant1 << constant2
            if (Value* constant = m_value->child(0)->shlConstant(m_proc, m_value->child(1))) {
                replaceWithNewValue(constant);
                break;
            }

            if (handleShiftByZero())
                break;

            break;

        case SShr:
            // Turn this: SShr(constant1, constant2)
            // Into this: constant1 >> constant2
            if (Value* constant = m_value->child(0)->sShrConstant(m_proc, m_value->child(1))) {
                replaceWithNewValue(constant);
                break;
            }

            if (handleShiftByZero())
                break;

            break;

        case ZShr:
            // Turn this: ZShr(constant1, constant2)
            // Into this: (unsigned)constant1 >> constant2
            if (Value* constant = m_value->child(0)->zShrConstant(m_proc, m_value->child(1))) {
                replaceWithNewValue(constant);
                break;
            }

            if (handleShiftByZero())
                break;

            break;

        case Abs:
            // Turn this: Abs(constant)
            // Into this: fabs<value->type()>(constant)
            if (Value* constant = m_value->child(0)->absConstant(m_proc)) {
                replaceWithNewValue(constant);
                break;
            }

            // Turn this: Abs(Abs(value))
            // Into this: Abs(value)
            if (m_value->child(0)->opcode() == Abs) {
                m_value->replaceWithIdentity(m_value->child(0));
                break;
            }

            // Turn this: Abs(BitwiseCast(value))
            // Into this: BitwiseCast(And(value, mask-top-bit))
            if (m_value->child(0)->opcode() == BitwiseCast) {
                Value* mask;
                if (m_value->type() == Double)
                    mask = m_insertionSet.insert<Const64Value>(m_index, m_value->origin(), ~(1l << 63));
                else
                    mask = m_insertionSet.insert<Const32Value>(m_index, m_value->origin(), ~(1l << 31));

                Value* bitAnd = m_insertionSet.insert<Value>(m_index, BitAnd, m_value->origin(),
                    m_value->child(0)->child(0),
                    mask);
                Value* cast = m_insertionSet.insert<Value>(m_index, BitwiseCast, m_value->origin(), bitAnd);
                m_value->replaceWithIdentity(cast);
                break;
            }
            break;

        case Ceil:
            // Turn this: Ceil(constant)
            // Into this: ceil<value->type()>(constant)
            if (Value* constant = m_value->child(0)->ceilConstant(m_proc)) {
                replaceWithNewValue(constant);
                break;
            }

            // Turn this: Ceil(Ceil(value))
            // Into this: Ceil(value)
            if (m_value->child(0)->opcode() == Ceil) {
                m_value->replaceWithIdentity(m_value->child(0));
                break;
            }

            // Turn this: Ceil(IToD(value))
            // Into this: IToD(value)
            //
            // That works for Int64 because both ARM64 and x86_64
            // perform rounding when converting a 64bit integer to double.
            if (m_value->child(0)->opcode() == IToD) {
                m_value->replaceWithIdentity(m_value->child(0));
                break;
            }
            break;

        case Sqrt:
            // Turn this: Sqrt(constant)
            // Into this: sqrt<value->type()>(constant)
            if (Value* constant = m_value->child(0)->sqrtConstant(m_proc)) {
                replaceWithNewValue(constant);
                break;
            }
            break;

        case BitwiseCast:
            // Turn this: BitwiseCast(constant)
            // Into this: bitwise_cast<value->type()>(constant)
            if (Value* constant = m_value->child(0)->bitwiseCastConstant(m_proc)) {
                replaceWithNewValue(constant);
                break;
            }

            // Turn this: BitwiseCast(BitwiseCast(value))
            // Into this: value
            if (m_value->child(0)->opcode() == BitwiseCast) {
                m_value->replaceWithIdentity(m_value->child(0)->child(0));
                m_changed = true;
                break;
            }
            break;

        case SExt8:
            // Turn this: SExt8(constant)
            // Into this: static_cast<int8_t>(constant)
            if (m_value->child(0)->hasInt32()) {
                int32_t result = static_cast<int8_t>(m_value->child(0)->asInt32());
                replaceWithNewValue(m_proc.addIntConstant(m_value, result));
                break;
            }

            // Turn this: SExt8(SExt8(value))
            //   or this: SExt8(SExt16(value))
            // Into this: SExt8(value)
            if (m_value->child(0)->opcode() == SExt8 || m_value->child(0)->opcode() == SExt16) {
                m_value->child(0) = m_value->child(0)->child(0);
                m_changed = true;
            }

            if (m_value->child(0)->opcode() == BitAnd && m_value->child(0)->child(1)->hasInt32()) {
                Value* input = m_value->child(0)->child(0);
                int32_t mask = m_value->child(0)->child(1)->asInt32();
                
                // Turn this: SExt8(BitAnd(input, mask)) where (mask & 0xff) == 0xff
                // Into this: SExt8(input)
                if ((mask & 0xff) == 0xff) {
                    m_value->child(0) = input;
                    m_changed = true;
                    break;
                }
                
                // Turn this: SExt8(BitAnd(input, mask)) where (mask & 0x80) == 0
                // Into this: BitAnd(input, const & 0x7f)
                if (!(mask & 0x80)) {
                    replaceWithNewValue(
                        m_proc.add<Value>(
                            BitAnd, m_value->origin(), input,
                            m_insertionSet.insert<Const32Value>(
                                m_index, m_value->origin(), mask & 0x7f)));
                    break;
                }
            }
            break;

        case SExt16:
            // Turn this: SExt16(constant)
            // Into this: static_cast<int16_t>(constant)
            if (m_value->child(0)->hasInt32()) {
                int32_t result = static_cast<int16_t>(m_value->child(0)->asInt32());
                replaceWithNewValue(m_proc.addIntConstant(m_value, result));
                break;
            }

            // Turn this: SExt16(SExt16(value))
            // Into this: SExt16(value)
            if (m_value->child(0)->opcode() == SExt16) {
                m_value->child(0) = m_value->child(0)->child(0);
                m_changed = true;
            }

            // Turn this: SExt16(SExt8(value))
            // Into this: SExt8(value)
            if (m_value->child(0)->opcode() == SExt8) {
                m_value->replaceWithIdentity(m_value->child(0));
                m_changed = true;
                break;
            }

            if (m_value->child(0)->opcode() == BitAnd && m_value->child(0)->child(1)->hasInt32()) {
                Value* input = m_value->child(0)->child(0);
                int32_t mask = m_value->child(0)->child(1)->asInt32();
                
                // Turn this: SExt16(BitAnd(input, mask)) where (mask & 0xffff) == 0xffff
                // Into this: SExt16(input)
                if ((mask & 0xffff) == 0xffff) {
                    m_value->child(0) = input;
                    m_changed = true;
                    break;
                }
                
                // Turn this: SExt16(BitAnd(input, mask)) where (mask & 0x8000) == 0
                // Into this: BitAnd(input, const & 0x7fff)
                if (!(mask & 0x8000)) {
                    replaceWithNewValue(
                        m_proc.add<Value>(
                            BitAnd, m_value->origin(), input,
                            m_insertionSet.insert<Const32Value>(
                                m_index, m_value->origin(), mask & 0x7fff)));
                    break;
                }
            }
            break;

        case SExt32:
            // Turn this: SExt32(constant)
            // Into this: static_cast<int64_t>(constant)
            if (m_value->child(0)->hasInt32()) {
                replaceWithNewValue(m_proc.addIntConstant(m_value, m_value->child(0)->asInt32()));
                break;
            }

            // Turn this: SExt32(BitAnd(input, mask)) where (mask & 0x80000000) == 0
            // Into this: ZExt32(BitAnd(input, mask))
            if (m_value->child(0)->opcode() == BitAnd && m_value->child(0)->child(1)->hasInt32()
                && !(m_value->child(0)->child(1)->asInt32() & 0x80000000)) {
                replaceWithNewValue(
                    m_proc.add<Value>(
                        ZExt32, m_value->origin(), m_value->child(0)));
                break;
            }
            break;

        case ZExt32:
            // Turn this: ZExt32(constant)
            // Into this: static_cast<uint64_t>(static_cast<uint32_t>(constant))
            if (m_value->child(0)->hasInt32()) {
                replaceWithNewValue(
                    m_proc.addIntConstant(
                        m_value,
                        static_cast<uint64_t>(static_cast<uint32_t>(m_value->child(0)->asInt32()))));
                break;
            }
            break;

        case Trunc:
            // Turn this: Trunc(constant)
            // Into this: static_cast<int32_t>(constant)
            if (m_value->child(0)->hasInt64()) {
                replaceWithNewValue(
                    m_proc.addIntConstant(m_value, static_cast<int32_t>(m_value->child(0)->asInt64())));
                break;
            }

            // Turn this: Trunc(SExt32(value)) or Trunc(ZExt32(value))
            // Into this: value
            if (m_value->child(0)->opcode() == SExt32 || m_value->child(0)->opcode() == ZExt32) {
                m_value->replaceWithIdentity(m_value->child(0)->child(0));
                m_changed = true;
                break;
            }
            break;

        case FloatToDouble:
            // Turn this: FloatToDouble(constant)
            // Into this: ConstDouble(constant)
            if (Value* constant = m_value->child(0)->floatToDoubleConstant(m_proc)) {
                replaceWithNewValue(constant);
                break;
            }
            break;

        case DoubleToFloat:
            // Turn this: DoubleToFloat(FloatToDouble(value))
            // Into this: value
            if (m_value->child(0)->opcode() == FloatToDouble) {
                m_value->replaceWithIdentity(m_value->child(0)->child(0));
                m_changed = true;
                break;
            }

            // Turn this: DoubleToFloat(constant)
            // Into this: ConstFloat(constant)
            if (Value* constant = m_value->child(0)->doubleToFloatConstant(m_proc)) {
                replaceWithNewValue(constant);
                break;
            }
            break;

        case Select:
            // Turn this: Select(constant, a, b)
            // Into this: constant ? a : b
            if (m_value->child(0)->hasInt32()) {
                m_value->replaceWithIdentity(
                    m_value->child(0)->asInt32() ? m_value->child(1) : m_value->child(2));
                m_changed = true;
                break;
            }

            // Turn this: Select(Equal(x, 0), a, b)
            // Into this: Select(x, b, a)
            if (m_value->child(0)->opcode() == Equal && m_value->child(0)->child(1)->isInt(0)) {
                m_value->child(0) = m_value->child(0)->child(0);
                std::swap(m_value->child(1), m_value->child(2));
                m_changed = true;
                break;
            }

            // Turn this: Select(BitXor(bool, 1), a, b)
            // Into this: Select(bool, b, a)
            if (m_value->child(0)->opcode() == BitXor
                && m_value->child(0)->child(1)->isInt32(1)
                && m_value->child(0)->child(0)->returnsBool()) {
                m_value->child(0) = m_value->child(0)->child(0);
                std::swap(m_value->child(1), m_value->child(2));
                m_changed = true;
                break;
            }

            // Turn this: Select(BitAnd(bool, xyz1), a, b)
            // Into this: Select(bool, a, b)
            if (m_value->child(0)->opcode() == BitAnd
                && m_value->child(0)->child(1)->hasInt()
                && m_value->child(0)->child(1)->asInt() & 1
                && m_value->child(0)->child(0)->returnsBool()) {
                m_value->child(0) = m_value->child(0)->child(0);
                m_changed = true;
                break;
            }

            // Turn this: Select(stuff, x, x)
            // Into this: x
            if (m_value->child(1) == m_value->child(2)) {
                m_value->replaceWithIdentity(m_value->child(1));
                m_changed = true;
                break;
            }
            break;

        case Load8Z:
        case Load8S:
        case Load16Z:
        case Load16S:
        case Load:
        case Store8:
        case Store16:
        case Store: {
            Value* address = m_value->lastChild();
            MemoryValue* memory = m_value->as<MemoryValue>();

            // Turn this: Load(Add(address, offset1), offset = offset2)
            // Into this: Load(address, offset = offset1 + offset2)
            //
            // Also turns this: Store(value, Add(address, offset1), offset = offset2)
            // Into this: Store(value, address, offset = offset1 + offset2)
            if (address->opcode() == Add && address->child(1)->hasIntPtr()) {
                intptr_t offset = address->child(1)->asIntPtr();
                if (!sumOverflows<intptr_t>(offset, memory->offset())) {
                    offset += memory->offset();
                    int32_t smallOffset = static_cast<int32_t>(offset);
                    if (smallOffset == offset) {
                        address = address->child(0);
                        memory->lastChild() = address;
                        memory->setOffset(smallOffset);
                        m_changed = true;
                    }
                }
            }

            // Turn this: Load(constant1, offset = constant2)
            // Into this: Load(constant1 + constant2)
            //
            // This is a fun canonicalization. It purely regresses naively generated code. We rely
            // on constant materialization to be smart enough to materialize this constant the smart
            // way. We want this canonicalization because we want to know if two memory accesses see
            // the same address.
            if (memory->offset()) {
                if (Value* newAddress = address->addConstant(m_proc, memory->offset())) {
                    m_insertionSet.insertValue(m_index, newAddress);
                    address = newAddress;
                    memory->lastChild() = newAddress;
                    memory->setOffset(0);
                    m_changed = true;
                }
            }
            
            break;
        }

        case CCall:
            // Turn this: Call(fmod, constant1, constant2)
            // Into this: fcall-constant(constant1, constant2)
            if (m_value->type() == Double
                && m_value->numChildren() == 3
                && m_value->child(0)->isIntPtr(reinterpret_cast<intptr_t>(fmod))
                && m_value->child(1)->type() == Double
                && m_value->child(2)->type() == Double) {
                replaceWithNewValue(m_value->child(1)->modConstant(m_proc, m_value->child(2)));
            }
            break;

        case Equal:
            handleCommutativity();

            // Turn this: Equal(bool, 0)
            // Into this: BitXor(bool, 1)
            if (m_value->child(0)->returnsBool() && m_value->child(1)->isInt32(0)) {
                replaceWithNew<Value>(
                    BitXor, m_value->origin(), m_value->child(0),
                    m_insertionSet.insert<Const32Value>(m_index, m_value->origin(), 1));
                break;
            }
            
            // Turn this Equal(bool, 1)
            // Into this: bool
            if (m_value->child(0)->returnsBool() && m_value->child(1)->isInt32(1)) {
                m_value->replaceWithIdentity(m_value->child(0));
                m_changed = true;
                break;
            }

            // Turn this: Equal(const1, const2)
            // Into this: const1 == const2
            replaceWithNewValue(
                m_proc.addBoolConstant(
                    m_value->origin(),
                    m_value->child(0)->equalConstant(m_value->child(1))));
            break;
            
        case NotEqual:
            handleCommutativity();

            if (m_value->child(0)->returnsBool()) {
                // Turn this: NotEqual(bool, 0)
                // Into this: bool
                if (m_value->child(1)->isInt32(0)) {
                    m_value->replaceWithIdentity(m_value->child(0));
                    m_changed = true;
                    break;
                }
                
                // Turn this: NotEqual(bool, 1)
                // Into this: Equal(bool, 0)
                if (m_value->child(1)->isInt32(1)) {
                    replaceWithNew<Value>(
                        Equal, m_value->origin(), m_value->child(0), m_value->child(1));
                    break;
                }
            }

            // Turn this: NotEqual(const1, const2)
            // Into this: const1 != const2
            replaceWithNewValue(
                m_proc.addBoolConstant(
                    m_value->origin(),
                    m_value->child(0)->notEqualConstant(m_value->child(1))));
            break;

        case LessThan:
            // FIXME: We could do a better job of canonicalizing integer comparisons.
            // https://bugs.webkit.org/show_bug.cgi?id=150958

            replaceWithNewValue(
                m_proc.addBoolConstant(
                    m_value->origin(),
                    m_value->child(0)->lessThanConstant(m_value->child(1))));
            break;

        case GreaterThan:
            replaceWithNewValue(
                m_proc.addBoolConstant(
                    m_value->origin(),
                    m_value->child(0)->greaterThanConstant(m_value->child(1))));
            break;

        case LessEqual:
            replaceWithNewValue(
                m_proc.addBoolConstant(
                    m_value->origin(),
                    m_value->child(0)->lessEqualConstant(m_value->child(1))));
            break;

        case GreaterEqual:
            replaceWithNewValue(
                m_proc.addBoolConstant(
                    m_value->origin(),
                    m_value->child(0)->greaterEqualConstant(m_value->child(1))));
            break;

        case Above:
            replaceWithNewValue(
                m_proc.addBoolConstant(
                    m_value->origin(),
                    m_value->child(0)->aboveConstant(m_value->child(1))));
            break;

        case Below:
            replaceWithNewValue(
                m_proc.addBoolConstant(
                    m_value->origin(),
                    m_value->child(0)->belowConstant(m_value->child(1))));
            break;

        case AboveEqual:
            replaceWithNewValue(
                m_proc.addBoolConstant(
                    m_value->origin(),
                    m_value->child(0)->aboveEqualConstant(m_value->child(1))));
            break;

        case BelowEqual:
            replaceWithNewValue(
                m_proc.addBoolConstant(
                    m_value->origin(),
                    m_value->child(0)->belowEqualConstant(m_value->child(1))));
            break;

        case EqualOrUnordered:
            handleCommutativity();

            // Turn this: Equal(const1, const2)
            // Into this: isunordered(const1, const2) || const1 == const2.
            // Turn this: Equal(value, const_NaN)
            // Into this: 1.
            replaceWithNewValue(
                m_proc.addBoolConstant(
                    m_value->origin(),
                    m_value->child(1)->equalOrUnorderedConstant(m_value->child(0))));
            break;

        case CheckAdd:
            if (replaceWithNewValue(m_value->child(0)->checkAddConstant(m_proc, m_value->child(1))))
                break;

            handleCommutativity();
            
            if (m_value->child(1)->isInt(0)) {
                m_value->replaceWithIdentity(m_value->child(0));
                m_changed = true;
                break;
            }
            break;

        case CheckSub:
            if (replaceWithNewValue(m_value->child(0)->checkSubConstant(m_proc, m_value->child(1))))
                break;

            if (m_value->child(1)->isInt(0)) {
                m_value->replaceWithIdentity(m_value->child(0));
                m_changed = true;
                break;
            }

            if (Value* negatedConstant = m_value->child(1)->checkNegConstant(m_proc)) {
                m_insertionSet.insertValue(m_index, negatedConstant);
                m_value->as<CheckValue>()->convertToAdd();
                m_value->child(1) = negatedConstant;
                m_changed = true;
                break;
            }
            break;

        case CheckMul:
            if (replaceWithNewValue(m_value->child(0)->checkMulConstant(m_proc, m_value->child(1))))
                break;

            handleCommutativity();

            if (m_value->child(1)->hasInt()) {
                bool modified = true;
                switch (m_value->child(1)->asInt()) {
                case 0:
                    replaceWithNewValue(m_proc.addIntConstant(m_value, 0));
                    break;
                case 1:
                    m_value->replaceWithIdentity(m_value->child(0));
                    m_changed = true;
                    break;
                case 2:
                    m_value->as<CheckValue>()->convertToAdd();
                    m_value->child(1) = m_value->child(0);
                    m_changed = true;
                    break;
                default:
                    modified = false;
                    break;
                }
                if (modified)
                    break;
            }
            break;

        case Check:
            if (m_value->child(0)->isLikeZero()) {
                m_value->replaceWithNop();
                m_changed = true;
                break;
            }

            if (m_value->child(0)->opcode() == NotEqual && m_value->child(0)->child(1)->isInt(0)) {
                m_value->child(0) = m_value->child(0)->child(0);
                m_changed = true;
                break;
            }
            break;

        case Branch: {
            ControlValue* branch = m_value->as<ControlValue>();

            // Turn this: Branch(NotEqual(x, 0))
            // Into this: Branch(x)
            if (branch->child(0)->opcode() == NotEqual && branch->child(0)->child(1)->isInt(0)) {
                branch->child(0) = branch->child(0)->child(0);
                m_changed = true;
            }

            // Turn this: Branch(Equal(x, 0), then, else)
            // Into this: Branch(x, else, then)
            if (branch->child(0)->opcode() == Equal && branch->child(0)->child(1)->isInt(0)) {
                branch->child(0) = branch->child(0)->child(0);
                std::swap(branch->taken(), branch->notTaken());
                m_changed = true;
            }
            
            // Turn this: Branch(BitXor(bool, 1), then, else)
            // Into this: Branch(bool, else, then)
            if (branch->child(0)->opcode() == BitXor
                && branch->child(0)->child(1)->isInt32(1)
                && branch->child(0)->child(0)->returnsBool()) {
                branch->child(0) = branch->child(0)->child(0);
                std::swap(branch->taken(), branch->notTaken());
                m_changed = true;
            }

            // Turn this: Branch(BitAnd(bool, xyb1), then, else)
            // Into this: Branch(bool, then, else)
            if (branch->child(0)->opcode() == BitAnd
                && branch->child(0)->child(1)->hasInt()
                && branch->child(0)->child(1)->asInt() & 1
                && branch->child(0)->child(0)->returnsBool()) {
                branch->child(0) = branch->child(0)->child(0);
                m_changed = true;
            }

            TriState triState = branch->child(0)->asTriState();

            // Turn this: Branch(0, then, else)
            // Into this: Jump(else)
            if (triState == FalseTriState) {
                branch->taken().block()->removePredecessor(m_block);
                branch->convertToJump(branch->notTaken().block());
                m_changedCFG = true;
                break;
            }

            // Turn this: Branch(not 0, then, else)
            // Into this: Jump(then)
            if (triState == TrueTriState) {
                branch->notTaken().block()->removePredecessor(m_block);
                branch->convertToJump(branch->taken().block());
                m_changedCFG = true;
                break;
            }
            
            break;
        }
            
        default:
            break;
        }
    }

    // Turn this: Add(constant, value)
    // Into this: Add(value, constant)
    //
    // Also:
    // Turn this: Add(value1, value2)
    // Into this: Add(value2, value1)
    // If we decide that value2 coming first is the canonical ordering.
    void handleCommutativity()
    {
        // Note that we have commutative operations that take more than two children. Those operations may
        // commute their first two children while leaving the rest unaffected.
        ASSERT(m_value->numChildren() >= 2);
        
        // Leave it alone if the right child is a constant.
        if (m_value->child(1)->isConstant())
            return;
        
        if (m_value->child(0)->isConstant()) {
            std::swap(m_value->child(0), m_value->child(1));
            m_changed = true;
            return;
        }

        // Sort the operands. This is an important canonicalization. We use the index instead of
        // the address to make this at least slightly deterministic.
        if (m_value->child(0)->index() > m_value->child(1)->index()) {
            std::swap(m_value->child(0), m_value->child(1));
            m_changed = true;
            return;
        }
    }

    template<typename ValueType, typename... Arguments>
    void replaceWithNew(Arguments... arguments)
    {
        replaceWithNewValue(m_proc.add<ValueType>(arguments...));
    }

    bool replaceWithNewValue(Value* newValue)
    {
        if (!newValue)
            return false;
        m_insertionSet.insertValue(m_index, newValue);
        m_value->replaceWithIdentity(newValue);
        m_changed = true;
        return true;
    }

    bool handleShiftByZero()
    {
        // Shift anything by zero is identity.
        if (m_value->child(1)->isInt(0)) {
            m_value->replaceWithIdentity(m_value->child(0));
            m_changed = true;
            return true;
        }
        return false;
    }

    void replaceIfRedundant()
    {
        // This does a very simple pure dominator-based CSE. In the future we could add load elimination.
        // Note that if we add load elimination, we should do it by directly matching load and store
        // instructions instead of using the ValueKey functionality or doing DFG HeapLocation-like
        // things.

        // Don't bother with identities. We kill those anyway.
        if (m_value->opcode() == Identity)
            return;

        ValueKey key = m_value->key();
        if (!key)
            return;
        
        Vector<Value*, 1>& matches = m_pureValues.add(key, Vector<Value*, 1>()).iterator->value;

        // Replace this value with whichever value dominates us.
        for (Value* match : matches) {
            if (m_dominators->dominates(match->owner, m_value->owner)) {
                m_value->replaceWithIdentity(match);
                m_changed = true;
                return;
            }
        }

        matches.append(m_value);
    }

    void simplifyCFG()
    {
        if (verbose) {
            dataLog("Before simplifyCFG:\n");
            dataLog(m_proc);
        }
        
        // We have three easy simplification rules:
        //
        // 1) If a successor is a block that just jumps to another block, then jump directly to
        //    that block.
        //
        // 2) If all successors are the same and the operation has no effects, then use a jump
        //    instead.
        //
        // 3) If you jump to a block that is not you and has one predecessor, then merge.
        //
        // Note that because of the first rule, this phase may introduce critical edges. That's fine.
        // If you need broken critical edges, then you have to break them yourself.

        // Note that this relies on predecessors being at least conservatively correct. It's fine for
        // predecessors to mention a block that isn't actually a predecessor. It's *not* fine for a
        // predecessor to be omitted. We assert as much in the loop. In practice, we precisely preserve
        // predecessors during strength reduction since that minimizes the total number of fixpoint
        // iterations needed to kill a lot of code.

        for (BasicBlock* block : m_proc) {
            if (verbose)
                dataLog("Considering block ", *block, ":\n");

            checkPredecessorValidity();

            // We don't care about blocks that don't have successors.
            if (!block->numSuccessors())
                continue;

            // First check if any of the successors of this block can be forwarded over.
            for (BasicBlock*& successor : block->successorBlocks()) {
                if (successor != block
                    && successor->size() == 1
                    && successor->last()->opcode() == Jump) {
                    BasicBlock* newSuccessor = successor->successorBlock(0);
                    if (newSuccessor != successor) {
                        if (verbose) {
                            dataLog(
                                "Replacing ", pointerDump(block), "->", pointerDump(successor),
                                " with ", pointerDump(block), "->", pointerDump(newSuccessor),
                                "\n");
                        }
                        // Note that we do not do replacePredecessor() because the block we're
                        // skipping will still have newSuccessor as its successor.
                        newSuccessor->addPredecessor(block);
                        successor = newSuccessor;
                        m_changedCFG = true;
                    }
                }
            }

            // Now check if the block's terminal can be replaced with a jump.
            if (block->numSuccessors() > 1) {
                // The terminal must not have weird effects.
                Effects effects = block->last()->effects();
                effects.terminal = false;
                if (!effects.mustExecute()) {
                    // All of the successors must be the same.
                    bool allSame = true;
                    BasicBlock* firstSuccessor = block->successorBlock(0);
                    for (unsigned i = 1; i < block->numSuccessors(); ++i) {
                        if (block->successorBlock(i) != firstSuccessor) {
                            allSame = false;
                            break;
                        }
                    }
                    if (allSame) {
                        if (verbose) {
                            dataLog(
                                "Changing ", pointerDump(block), "'s terminal to a Jump.\n");
                        }
                        block->last()->as<ControlValue>()->convertToJump(firstSuccessor);
                        m_changedCFG = true;
                    }
                }
            }

            // Finally handle jumps to a block with one predecessor.
            if (block->numSuccessors() == 1) {
                BasicBlock* successor = block->successorBlock(0);
                if (successor != block && successor->numPredecessors() == 1) {
                    RELEASE_ASSERT(successor->predecessor(0) == block);
                    
                    // We can merge the two blocks, because the predecessor only jumps to the successor
                    // and the successor is only reachable from the predecessor.
                    
                    // Remove the terminal.
                    Value* value = block->values().takeLast();
                    Origin jumpOrigin = value->origin();
                    RELEASE_ASSERT(value->as<ControlValue>());
                    m_proc.deleteValue(value);
                    
                    // Append the full contents of the successor to the predecessor.
                    block->values().appendVector(successor->values());
                    
                    // Make sure that the successor has nothing left in it. Make sure that the block
                    // has a terminal so that nobody chokes when they look at it.
                    successor->values().resize(0);
                    successor->appendNew<ControlValue>(m_proc, Oops, jumpOrigin);
                    
                    // Ensure that predecessors of block's new successors know what's up.
                    for (BasicBlock* newSuccessor : block->successorBlocks())
                        newSuccessor->replacePredecessor(successor, block);

                    if (verbose) {
                        dataLog(
                            "Merged ", pointerDump(block), "->", pointerDump(successor), "\n");
                    }
                    
                    m_changedCFG = true;
                }
            }
        }

        if (m_changedCFG && verbose) {
            dataLog("B3 after simplifyCFG:\n");
            dataLog(m_proc);
        }
    }

    void checkPredecessorValidity()
    {
        if (!shouldValidateIRAtEachPhase())
            return;

        for (BasicBlock* block : m_proc) {
            for (BasicBlock* successor : block->successorBlocks())
                RELEASE_ASSERT(successor->containsPredecessor(block));
        }
    }

    void killDeadCode()
    {
        GraphNodeWorklist<Value*, IndexSet<Value>> worklist;
        Vector<UpsilonValue*, 64> upsilons;
        for (BasicBlock* block : m_proc) {
            for (Value* value : *block) {
                Effects effects = value->effects();
                // We don't care about SSA Effects, since we model them more accurately than the
                // effects() method does.
                effects.writesSSAState = false;
                effects.readsSSAState = false;
                if (effects.mustExecute())
                    worklist.push(value);
                if (UpsilonValue* upsilon = value->as<UpsilonValue>())
                    upsilons.append(upsilon);
            }
        }
        for (;;) {
            while (Value* value = worklist.pop()) {
                for (Value* child : value->children())
                    worklist.push(child);
            }
            
            bool didPush = false;
            for (size_t upsilonIndex = 0; upsilonIndex < upsilons.size(); ++upsilonIndex) {
                UpsilonValue* upsilon = upsilons[upsilonIndex];
                if (worklist.saw(upsilon->phi())) {
                    worklist.push(upsilon);
                    upsilons[upsilonIndex--] = upsilons.last();
                    upsilons.takeLast();
                    didPush = true;
                }
            }
            if (!didPush)
                break;
        }

        for (BasicBlock* block : m_proc) {
            size_t sourceIndex = 0;
            size_t targetIndex = 0;
            while (sourceIndex < block->size()) {
                Value* value = block->at(sourceIndex++);
                if (worklist.saw(value))
                    block->at(targetIndex++) = value;
                else {
                    m_proc.deleteValue(value);
                    
                    // It's not entirely clear if this is needed. I think it makes sense to have
                    // this force a rerun of the fixpoint for now, since that will make it easier
                    // to do peephole optimizations: removing dead code will make the peephole
                    // easier to spot.
                    m_changed = true;
                }
            }
            block->values().resize(targetIndex);
        }
    }

    void simplifySSA()
    {
        // This runs Aycock and Horspool's algorithm on our Phi functions [1]. For most CFG patterns,
        // this can take a suboptimal arrangement of Phi functions and make it optimal, as if you had
        // run Cytron, Ferrante, Rosen, Wegman, and Zadeck. It's only suboptimal for irreducible
        // CFGs. In practice, that doesn't matter, since we expect clients of B3 to run their own SSA
        // conversion before lowering to B3, and in the case of the DFG, that conversion uses Cytron
        // et al. In that context, this algorithm is intended to simplify Phi functions that were
        // made redundant by prior CFG simplification. But according to Aycock and Horspool's paper,
        // this algorithm is good enough that a B3 client could just give us maximal Phi's (i.e. Phi
        // for each variable at each basic block) and we will make them optimal.
        // [1] http://pages.cpsc.ucalgary.ca/~aycock/papers/ssa.ps

        // Aycock and Horspool prescribe two rules that are to be run to fixpoint:
        //
        // 1) If all of the Phi's children are the same (i.e. it's one child referenced from one or
        //    more Upsilons), then replace all uses of the Phi with the one child.
        //
        // 2) If all of the Phi's children are either the Phi itself or exactly one other child, then
        //    replace all uses of the Phi with the one other child.
        //
        // Rule (2) subsumes rule (1), so we can just run (2). We only run one fixpoint iteration
        // here. This premise is that in common cases, this will only find optimization opportunities
        // as a result of CFG simplification and usually CFG simplification will only do one round
        // of block merging per ReduceStrength fixpoint iteration, so it's OK for this to only do one
        // round of Phi merging - since Phis are the value analogue of blocks.

        PhiChildren phiChildren(m_proc);

        for (Value* phi : phiChildren.phis()) {
            Value* otherChild = nullptr;
            bool ok = true;
            for (Value* child : phiChildren[phi].values()) {
                if (child == phi)
                    continue;
                if (child == otherChild)
                    continue;
                if (!otherChild) {
                    otherChild = child;
                    continue;
                }
                ok = false;
                break;
            }
            if (!ok)
                continue;
            if (!otherChild) {
                // Wow, this would be super weird. It probably won't happen, except that things could
                // get weird as a consequence of stepwise simplifications in the strength reduction
                // fixpoint.
                continue;
            }
            
            // Turn the Phi into an Identity and turn the Upsilons into Nops.
            m_changed = true;
            for (Value* upsilon : phiChildren[phi])
                upsilon->replaceWithNop();
            phi->replaceWithIdentity(otherChild);
        }
    }

    Procedure& m_proc;
    InsertionSet m_insertionSet;
    BasicBlock* m_block { nullptr };
    unsigned m_index { 0 };
    Value* m_value { nullptr };
    Dominators* m_dominators { nullptr };
    HashMap<ValueKey, Vector<Value*, 1>> m_pureValues;
    bool m_changed { false };
    bool m_changedCFG { false };
};

} // anonymous namespace

bool reduceStrength(Procedure& proc)
{
    PhaseScope phaseScope(proc, "reduceStrength");
    ReduceStrength reduceStrength(proc);
    return reduceStrength.run();
}

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)

