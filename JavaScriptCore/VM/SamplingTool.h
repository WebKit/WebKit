/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SamplingTool_h
#define SamplingTool_h

#include <wtf/Assertions.h>
#include <wtf/HashMap.h>
#include <wtf/Threading.h>

#include <nodes.h>
#include <Opcode.h>

namespace JSC {

    class ExecState;
    class ScopeNode;
    class CodeBlock;
    struct Instruction;

#if ENABLE(SAMPLING_TOOL)
    extern OpcodeID currentOpcodeID;
    extern unsigned inCalledCode;
#endif

    struct ScopeSampleRecord {
        RefPtr<ScopeNode> m_scope;
        CodeBlock* m_codeBlock;
        int m_totalCount;
        int* m_vpcCounts;
        unsigned m_size;
        
        ScopeSampleRecord(ScopeNode* scope)
            : m_scope(scope)
            , m_codeBlock(0)
            , m_totalCount(0)
            , m_vpcCounts(0)
            , m_size(0)
        {
        }
        
        ~ScopeSampleRecord()
        {
            if (m_vpcCounts)
                free(m_vpcCounts);
        }
        
        void sample(CodeBlock* codeBlock, Instruction* vPC);
    };

    typedef WTF::HashMap<ScopeNode*, ScopeSampleRecord*> ScopeSampleRecordMap;

    class SamplingTool {
    public:
        SamplingTool()
            : m_running(false)
            , m_recordedCodeBlock(0)
            , m_recordedVPC(0)
            , m_totalSamples(0)
            , m_scopeSampleMap(new ScopeSampleRecordMap())
        {
        }

        ~SamplingTool()
        {
            deleteAllValues(*m_scopeSampleMap);
        }

        void start(unsigned hertz=10000);
        void stop();
        void dump(ExecState*);

        void notifyOfScope(ScopeNode* scope);

        void sample(CodeBlock* recordedCodeBlock, Instruction* recordedVPC)
        {
            m_recordedCodeBlock = recordedCodeBlock;
            m_recordedVPC = recordedVPC;
        }

        void privateExecuteReturned()
        {
            m_recordedCodeBlock = 0;
            m_recordedVPC = 0;
#if ENABLE(SAMPLING_TOOL)
            currentOpcodeID = static_cast<OpcodeID>(-1);
#endif
        }
        
        void callingHostFunction()
        {
            m_recordedCodeBlock = 0;
            m_recordedVPC = 0;
#if ENABLE(SAMPLING_TOOL)
            currentOpcodeID = static_cast<OpcodeID>(-1);
#endif
        }

    private:
        static void* threadStartFunc(void*);
        void run();
        
        // Sampling thread state.
        bool m_running;
        unsigned m_hertz;
        ThreadIdentifier m_samplingThread;

        // State tracked by the main thread, used by the sampling thread.
        CodeBlock* m_recordedCodeBlock;
        Instruction* m_recordedVPC;

        // Gathered sample data.
        long long m_totalSamples;
        OwnPtr<ScopeSampleRecordMap> m_scopeSampleMap;
    };

// SCOPENODE_ / MACHINE_ macros for use from within member methods on ScopeNode / Machine respectively.
#if ENABLE(SAMPLING_TOOL)
#define SCOPENODE_SAMPLING_notifyOfScope(sampler) sampler->notifyOfScope(this)
#define MACHINE_SAMPLING_sample(codeBlock, vPC) m_sampler->sample(codeBlock, vPC)
#define MACHINE_SAMPLING_privateExecuteReturned() m_sampler->privateExecuteReturned()
#define MACHINE_SAMPLING_callingHostFunction() m_sampler->callingHostFunction()
#define CTI_MACHINE_SAMPLING_callingHostFunction() ARG_globalData->machine->m_sampler->callingHostFunction()
#else
#define SCOPENODE_SAMPLING_notifyOfScope(sampler)
#define MACHINE_SAMPLING_sample(codeBlock, vPC)
#define MACHINE_SAMPLING_privateExecuteReturned()
#define MACHINE_SAMPLING_callingHostFunction()
#define CTI_MACHINE_SAMPLING_callingHostFunction()
#endif

} // namespace JSC

#endif // SamplingTool_h
