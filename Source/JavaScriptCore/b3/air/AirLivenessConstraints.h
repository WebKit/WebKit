/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(B3_JIT)

#include "AirBasicBlock.h"
#include "AirCode.h"
#include "AirInstInlines.h"
#include <wtf/IndexMap.h>

namespace JSC { namespace B3 { namespace Air {

template<typename Adapter>
class LivenessConstraints {
public:
    typedef Vector<unsigned, 4> ActionsList;
    
    struct Actions {
        Actions() { }
        
        ActionsList use;
        ActionsList def;
    };
    
    typedef Vector<Actions, 0, UnsafeVectorOverflow> ActionsForBoundary;
    
    LivenessConstraints(Code& code)
        : m_code(code)
        , m_actions(code.size())
    {
        for (BasicBlock* block : m_code) {
            ActionsForBoundary& actionsForBoundary = m_actions[block];
            actionsForBoundary.resize(block->size() + 1);
            
            for (size_t instIndex = block->size(); instIndex--;) {
                Inst& inst = block->at(instIndex);
                inst.forEach<typename Adapter::Thing>(
                    [&] (typename Adapter::Thing& thing, Arg::Role role, Bank bank, Width) {
                        if (!Adapter::acceptsBank(bank) || !Adapter::acceptsRole(role))
                            return;
                        
                        unsigned index = Adapter::valueToIndex(thing);
                        
                        if (Arg::isEarlyUse(role))
                            actionsForBoundary[instIndex].use.appendIfNotContains(index);
                        if (Arg::isEarlyDef(role))
                            actionsForBoundary[instIndex].def.appendIfNotContains(index);
                        if (Arg::isLateUse(role))
                            actionsForBoundary[instIndex + 1].use.appendIfNotContains(index);
                        if (Arg::isLateDef(role))
                            actionsForBoundary[instIndex + 1].def.appendIfNotContains(index);
                    });
            }
        }
    }
    
    Actions& at(BasicBlock* block, unsigned instBoundaryIndex)
    {
        return m_actions[block][instBoundaryIndex];
    }
    
private:
    Code& m_code;
    IndexMap<BasicBlock*, ActionsForBoundary> m_actions;
};

} } } // namespace JSC::B3::Air

#endif // ENABLE(B3_JIT)

