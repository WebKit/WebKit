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
#include "B3LowerMacros.h"

#if ENABLE(B3_JIT)

#include "B3BasicBlockInlines.h"
#include "B3BlockInsertionSet.h"
#include "B3ControlValue.h"
#include "B3InsertionSetInlines.h"
#include "B3PhaseScope.h"
#include "B3ProcedureInlines.h"
#include "B3UpsilonValue.h"
#include "B3ValueInlines.h"

namespace JSC { namespace B3 {

namespace {

class LowerMacros {
public:
    LowerMacros(Procedure& proc)
        : m_proc(proc)
        , m_blockInsertionSet(proc)
        , m_insertionSet(proc)
    {
    }

    bool run()
    {
        for (BasicBlock* block : m_proc) {
            m_block = block;
            processCurrentBlock();
        }
        m_changed |= m_blockInsertionSet.execute();
        if (m_changed)
            m_proc.resetReachability();
        return m_changed;
    }
    
private:
    void processCurrentBlock()
    {
        for (m_index = 0; m_index < m_block->size(); ++m_index) {
            m_value = m_block->at(m_index);
            switch (m_value->opcode()) {
            case ChillDiv: {
                // ARM supports this instruction natively.
                if (isARM64())
                    break;

                m_changed = true;

                // We implement "res = ChillDiv(num, den)" as follows:
                //
                //     if (den + 1 <=_unsigned 1) {
                //         if (!den) {
                //             res = 0;
                //             goto done;
                //         }
                //         if (num == -2147483648) {
                //             res = num;
                //             goto done;
                //         }
                //     }
                //     res = num / dev;
                // done:

                Value* num = m_value->child(0);
                Value* den = m_value->child(1);

                Value* one =
                    m_insertionSet.insertIntConstant(m_index, m_value, 1);
                Value* isDenOK = m_insertionSet.insert<Value>(
                    m_index, Above, m_value->origin(),
                    m_insertionSet.insert<Value>(m_index, Add, m_value->origin(), den, one),
                    one);

                BasicBlock* before =
                    m_blockInsertionSet.splitForward(m_block, m_index, &m_insertionSet);

                BasicBlock* normalDivCase = m_blockInsertionSet.insertBefore(m_block);
                BasicBlock* shadyDenCase = m_blockInsertionSet.insertBefore(m_block);
                BasicBlock* zeroDenCase = m_blockInsertionSet.insertBefore(m_block);
                BasicBlock* neg1DenCase = m_blockInsertionSet.insertBefore(m_block);
                BasicBlock* intMinCase = m_blockInsertionSet.insertBefore(m_block);

                before->replaceLastWithNew<ControlValue>(
                    m_proc, Branch, m_value->origin(), isDenOK,
                    FrequentedBlock(normalDivCase, FrequencyClass::Normal),
                    FrequentedBlock(shadyDenCase, FrequencyClass::Rare));

                UpsilonValue* normalResult = normalDivCase->appendNew<UpsilonValue>(
                    m_proc, m_value->origin(),
                    normalDivCase->appendNew<Value>(m_proc, Div, m_value->origin(), num, den));
                normalDivCase->appendNew<ControlValue>(
                    m_proc, Jump, m_value->origin(), FrequentedBlock(m_block));

                shadyDenCase->appendNew<ControlValue>(
                    m_proc, Branch, m_value->origin(), den,
                    FrequentedBlock(neg1DenCase, FrequencyClass::Normal),
                    FrequentedBlock(zeroDenCase, FrequencyClass::Rare));

                UpsilonValue* zeroResult = zeroDenCase->appendNew<UpsilonValue>(
                    m_proc, m_value->origin(),
                    zeroDenCase->appendIntConstant(m_proc, m_value, 0));
                zeroDenCase->appendNew<ControlValue>(
                    m_proc, Jump, m_value->origin(), FrequentedBlock(m_block));

                int64_t badNumeratorConst;
                switch (m_value->type()) {
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

                neg1DenCase->appendNew<ControlValue>(
                    m_proc, Branch, m_value->origin(),
                    neg1DenCase->appendNew<Value>(
                        m_proc, Equal, m_value->origin(), num, badNumerator),
                    FrequentedBlock(intMinCase, FrequencyClass::Rare),
                    FrequentedBlock(normalDivCase, FrequencyClass::Normal));

                UpsilonValue* intMinResult = intMinCase->appendNew<UpsilonValue>(
                    m_proc, m_value->origin(), badNumerator);
                intMinCase->appendNew<ControlValue>(
                    m_proc, Jump, m_value->origin(), FrequentedBlock(m_block));

                Value* phi = m_insertionSet.insert<Value>(
                    m_index, Phi, m_value->type(), m_value->origin());
                normalResult->setPhi(phi);
                zeroResult->setPhi(phi);
                intMinResult->setPhi(phi);

                m_value->replaceWithIdentity(phi);
                before->updatePredecessorsAfter();
                break;
            }

            // FIXME: Implement Switch.
            // https://bugs.webkit.org/show_bug.cgi?id=151115

            default:
                break;
            }
        }
        m_insertionSet.execute(m_block);
    }
    
    Procedure& m_proc;
    BlockInsertionSet m_blockInsertionSet;
    InsertionSet m_insertionSet;
    BasicBlock* m_block;
    unsigned m_index;
    Value* m_value;
    bool m_changed { false };
};

bool lowerMacrosImpl(Procedure& proc)
{
    LowerMacros lowerMacros(proc);
    return lowerMacros.run();
}

} // anonymous namespace

bool lowerMacros(Procedure& proc)
{
    PhaseScope phaseScope(proc, "lowerMacros");
    bool result = lowerMacrosImpl(proc);
    if (shouldValidateIR())
        RELEASE_ASSERT(!lowerMacrosImpl(proc));
    return result;
}

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)

