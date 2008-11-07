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

#include "Nodes.h"
#include "Opcode.h"

namespace JSC {

    class CodeBlock;
    class ExecState;
    class Machine;
    class ScopeNode;
    struct Instruction;

    struct ScopeSampleRecord {
        ScopeSampleRecord(ScopeNode* scope)
            : m_scope(scope)
            , m_codeBlock(0)
            , m_sampleCount(0)
            , m_opcodeSampleCount(0)
            , m_samples(0)
            , m_size(0)
        {
        }
        
        ~ScopeSampleRecord()
        {
            if (m_samples)
                free(m_samples);
        }
        
        void sample(CodeBlock*, Instruction*);

        RefPtr<ScopeNode> m_scope;
        CodeBlock* m_codeBlock;
        int m_sampleCount;
        int m_opcodeSampleCount;
        int* m_samples;
        unsigned m_size;
    };

    typedef WTF::HashMap<ScopeNode*, ScopeSampleRecord*> ScopeSampleRecordMap;

    class SamplingTool {
    public:
        friend class CallRecord;
        friend class HostCallRecord;
        
#if ENABLE(OPCODE_SAMPLING)
        class CallRecord : Noncopyable {
        public:
            CallRecord(SamplingTool* samplingTool)
                : m_samplingTool(samplingTool)
                , m_savedSample(samplingTool->m_sample)
                , m_savedCodeBlock(samplingTool->m_codeBlock)
            {
            }

            ~CallRecord()
            {
                m_samplingTool->m_sample = m_savedSample;
                m_samplingTool->m_codeBlock = m_savedCodeBlock;
            }

        private:
            SamplingTool* m_samplingTool;
            intptr_t m_savedSample;
            CodeBlock* m_savedCodeBlock;
        };
        
        class HostCallRecord : public CallRecord {
        public:
            HostCallRecord(SamplingTool* samplingTool)
                : CallRecord(samplingTool)
            {
                samplingTool->m_sample |= 0x1;
            }
        };
#else
        class CallRecord : Noncopyable {
        public:
            CallRecord(SamplingTool*)
            {
            }
        };

        class HostCallRecord : public CallRecord {
        public:
            HostCallRecord(SamplingTool* samplingTool)
                : CallRecord(samplingTool)
            {
            }
        };
#endif        

        SamplingTool(Machine* machine)
            : m_machine(machine)
            , m_running(false)
            , m_codeBlock(0)
            , m_sample(0)
            , m_sampleCount(0)
            , m_opcodeSampleCount(0)
            , m_scopeSampleMap(new ScopeSampleRecordMap())
        {
            memset(m_opcodeSamples, 0, sizeof(m_opcodeSamples));
            memset(m_opcodeSamplesInCTIFunctions, 0, sizeof(m_opcodeSamplesInCTIFunctions));
        }

        ~SamplingTool()
        {
            deleteAllValues(*m_scopeSampleMap);
        }

        void start(unsigned hertz=10000);
        void stop();
        void dump(ExecState*);

        void notifyOfScope(ScopeNode* scope);

        void sample(CodeBlock* codeBlock, Instruction* vPC)
        {
            ASSERT(!(reinterpret_cast<intptr_t>(vPC) & 0x3));
            m_codeBlock = codeBlock;
            m_sample = reinterpret_cast<intptr_t>(vPC);
        }

        CodeBlock** codeBlockSlot() { return &m_codeBlock; }
        intptr_t* sampleSlot() { return &m_sample; }

        unsigned encodeSample(Instruction* vPC, bool inCTIFunction = false, bool inHostFunction = false)
        {
            ASSERT(!(reinterpret_cast<intptr_t>(vPC) & 0x3));
            return reinterpret_cast<intptr_t>(vPC) | (static_cast<intptr_t>(inCTIFunction) << 1) | static_cast<intptr_t>(inHostFunction);
        }

    private:
        class Sample {
        public:
            Sample(volatile intptr_t sample, CodeBlock* volatile codeBlock)
                : m_sample(sample)
                , m_codeBlock(codeBlock)
            {
            }
            
            bool isNull() { return !m_sample || !m_codeBlock; }
            CodeBlock* codeBlock() { return m_codeBlock; }
            Instruction* vPC() { return reinterpret_cast<Instruction*>(m_sample & ~0x3); }
            bool inHostFunction() { return m_sample & 0x1; }
            bool inCTIFunction() { return m_sample & 0x2; }

        private:
            intptr_t m_sample;
            CodeBlock* m_codeBlock;
        };
        
        static void* threadStartFunc(void*);
        void run();
        
        Machine* m_machine;
        
        // Sampling thread state.
        bool m_running;
        unsigned m_hertz;
        ThreadIdentifier m_samplingThread;

        // State tracked by the main thread, used by the sampling thread.
        CodeBlock* m_codeBlock;
        intptr_t m_sample;

        // Gathered sample data.
        long long m_sampleCount;
        long long m_opcodeSampleCount;
        unsigned m_opcodeSamples[numOpcodeIDs];
        unsigned m_opcodeSamplesInCTIFunctions[numOpcodeIDs];
        
        Mutex m_scopeSampleMapMutex;
        OwnPtr<ScopeSampleRecordMap> m_scopeSampleMap;
    };

} // namespace JSC

#endif // SamplingTool_h
