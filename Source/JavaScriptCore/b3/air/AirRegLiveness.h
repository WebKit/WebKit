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
#include "AirInst.h"
#include "AirLiveness.h"
#include "RegisterSet.h"
#include <wtf/IndexMap.h>

namespace JSC { namespace B3 { namespace Air {

// Although we could trivially adapt Air::Liveness<> to work with Reg, this would not be so
// efficient. There is a small number of registers, so it's much better to use bitvectors for
// register liveness. This is a specialization of Liveness<> that uses bitvectors directly.
// This makes the code sufficiently different that it didn't make sense to try to share code.
class RegLiveness {
    struct Actions {
        Actions() = default;
        
        RegisterSet use;
        RegisterSet def;
    };
    
    typedef Vector<Actions, 0, UnsafeVectorOverflow> ActionsForBoundary;
    
public:
    typedef Reg Thing;
    
    RegLiveness(Code& code);
    ~RegLiveness();
    
    class LocalCalcBase {
    public:
        LocalCalcBase(BasicBlock* block)
            : m_block(block)
        {
        }
        
        const RegisterSet& live() const
        {
            return m_workset;
        }
        
        bool isLive(Reg reg) const
        {
            return m_workset.contains(reg, IgnoreVectors);
        }
        
    protected:
        BasicBlock* m_block;
        RegisterSet m_workset;
    };
    
    // This calculator has to be run in reverse.
    class LocalCalc : public LocalCalcBase {
    public:
        LocalCalc(RegLiveness& liveness, BasicBlock* block)
            : LocalCalcBase(block)
            , m_actions(liveness.m_actions[block])
        {
            m_workset = liveness.m_liveAtTail[block];
        }
        
        void execute(unsigned instIndex)
        {
            m_actions[instIndex + 1].def.forEach([&] (Reg r) {
                m_workset.remove(r);
            });
            m_workset.merge(m_actions[instIndex].use);
        }
        
    private:
        friend class RegLiveness;
        
        ActionsForBoundary& m_actions;
    };
    
    class LocalCalcForUnifiedTmpLiveness : public LocalCalcBase {
    public:
        LocalCalcForUnifiedTmpLiveness(UnifiedTmpLiveness& liveness, BasicBlock* block);
        
        void execute(unsigned instIndex);
        
    private:
        Code& m_code;
        UnifiedTmpLiveness::ActionsForBoundary& m_actions;
    };
    
    const RegisterSet& liveAtHead(BasicBlock* block) const
    {
        return m_liveAtHead[block];
    }
    
    const RegisterSet& liveAtTail(BasicBlock* block) const
    {
        return m_liveAtTail[block];
    }
    
private:
    IndexMap<BasicBlock*, RegisterSet> m_liveAtHead;
    IndexMap<BasicBlock*, RegisterSet> m_liveAtTail;
    IndexMap<BasicBlock*, ActionsForBoundary> m_actions;
};

} } } // namespace JSC::B3::Air

#endif // ENABLE(B3_JIT)

