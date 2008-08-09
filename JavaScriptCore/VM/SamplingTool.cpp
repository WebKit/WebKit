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

#include "config.h"
#include "SamplingTool.h"

#include "CodeBlock.h"
#include "Machine.h"
#include "Opcode.h"

namespace KJS {

void ScopeSampleRecord::sample(CodeBlock* codeBlock, Instruction* vPC)
{
    m_totalCount++;

    if (!m_vpcCounts) {
        m_size = codeBlock->instructions.size();
        m_vpcCounts = static_cast<int*>(calloc(m_size, sizeof(int)));
        m_codeBlock = codeBlock;
    }

    unsigned codeOffset = static_cast<unsigned>(reinterpret_cast<ptrdiff_t>(vPC) - reinterpret_cast<ptrdiff_t>(codeBlock->instructions.begin())) / sizeof(Instruction*);
    // This could occur if codeBlock & vPC are not consistent - e.g. sample mid op_call/op_ret.
    if (codeOffset < m_size)
        m_vpcCounts[codeOffset]++;
}

#if PLATFORM(WIN_OS)

static void sleepForMicroseconds(unsigned us)
{
    unsigned ms = us / 1000;
    if (us && !ms)
        ms = 1;
    Sleep(ms);
}

#else 

static void sleepForMicroseconds(unsigned us)
{
    usleep(us);
}

#endif

static inline unsigned hertz2us(unsigned hertz)
{
    return 1000000 / hertz;
}

void SamplingTool::run()
{
    while (m_running) {
        sleepForMicroseconds(hertz2us(m_hertz));

        m_totalSamples++;

        CodeBlock* codeBlock = m_recordedCodeBlock;
        Instruction* vPC = m_recordedVPC;

        if (codeBlock && vPC) {
            if (ScopeSampleRecord* record = m_scopeSampleMap->get(codeBlock->ownerNode))
                record->sample(codeBlock, vPC);
        }
    }
}

void* SamplingTool::threadStartFunc(void* samplingTool)
{
    reinterpret_cast<SamplingTool*>(samplingTool)->run();
    return 0;
}

void SamplingTool::notifyOfScope(ScopeNode* scope)
{
    m_scopeSampleMap->set(scope, new ScopeSampleRecord(scope));
}

void SamplingTool::start(unsigned hertz)
{
    ASSERT(!m_running);
    m_running = true;
    m_hertz = hertz;

    m_samplingThread = createThread(threadStartFunc, this, "JavaScriptCore::Sampler");
}

void SamplingTool::stop()
{
    ASSERT(m_running);
    m_running = false;
    waitForThreadCompletion(m_samplingThread, 0);
}

#if ENABLE(SAMPLING_TOOL)

struct OpcodeSampleInfo
{
    OpcodeID opcode;
    long long count;
};

struct LineCountInfo
{
    unsigned line;
    unsigned count;
};

static int compareLineCountInfoSampling(const void* left, const void* right)
{
    const LineCountInfo* leftLineCount = reinterpret_cast<const LineCountInfo*>(left);
    const LineCountInfo* rightLineCount = reinterpret_cast<const LineCountInfo*>(right);

    return (leftLineCount->line > rightLineCount->line) ? 1 : (leftLineCount->line < rightLineCount->line) ? -1 : 0;
}

static int compareOpcodeIndicesSampling(const void* left, const void* right)
{
    const OpcodeSampleInfo* leftSampleInfo = reinterpret_cast<const OpcodeSampleInfo*>(left);
    const OpcodeSampleInfo* rightSampleInfo = reinterpret_cast<const OpcodeSampleInfo*>(right);

    return (leftSampleInfo->count < rightSampleInfo->count) ? 1 : (leftSampleInfo->count > rightSampleInfo->count) ? -1 : 0;
}

static int compareScopeSampleRecords(const void* left, const void* right)
{
    const ScopeSampleRecord* const leftValue = *static_cast<const ScopeSampleRecord* const *>(left);
    const ScopeSampleRecord* const rightValue = *static_cast<const ScopeSampleRecord* const *>(right);

    return (leftValue->m_totalCount < rightValue->m_totalCount) ? 1 : (leftValue->m_totalCount > rightValue->m_totalCount) ? -1 : 0;
}

void SamplingTool::dump(ExecState* exec)
{
    // Tidies up SunSpider output by removing short scripts - such a small number of samples would likely not be useful anyhow.
    if (m_totalSamples < 10)
        return;
    
    // (1) Calculate 'totalCodeBlockSamples', build and sort 'codeBlockSamples' array.

    int scopeCount = m_scopeSampleMap->size();
    long long totalCodeBlockSamples = 0;
    Vector<ScopeSampleRecord*> codeBlockSamples(scopeCount);
    ScopeSampleRecordMap::iterator iter = m_scopeSampleMap->begin();
    for (int i = 0; i < scopeCount; ++i, ++iter) {
        codeBlockSamples[i] = iter->second;
        totalCodeBlockSamples += codeBlockSamples[i]->m_totalCount;
    }
#if HAVE(MERGESORT)
    mergesort(codeBlockSamples.begin(), scopeCount, sizeof(ScopeSampleRecord*), compareScopeSampleRecords);
#else
    qsort(codeBlockSamples.begin(), scopeCount, sizeof(ScopeSampleRecord*), compareScopeSampleRecords);
#endif

    // (2) Print data from 'codeBlockSamples' array, calculate 'totalOpcodeSamples', populate 'opcodeSampleCounts' array.

    long long totalOpcodeSamples = 0;
    long long opcodeSampleCounts[numOpcodeIDs] = { 0 };

    printf("\nBlock sampling results\n\n"); 
    printf("Total blocks sampled (total samples): %lld (%lld)\n\n", totalCodeBlockSamples, m_totalSamples);

    for (int i=0; i < scopeCount; i++) {
        ScopeSampleRecord* record = codeBlockSamples[i];
        CodeBlock* codeBlock = record->m_codeBlock;

        double totalPercent = (record->m_totalCount * 100.0)/m_totalSamples;
        double blockPercent = (record->m_totalCount * 100.0)/totalCodeBlockSamples;

        if ((blockPercent >= 1) && codeBlock) {
            Instruction* code = codeBlock->instructions.begin();
            printf("#%d: %s:%d: sampled %d times - %.3f%% (%.3f%%)\n", i + 1, record->m_scope->sourceURL().UTF8String().c_str(), codeBlock->lineNumberForVPC(code), record->m_totalCount, blockPercent, totalPercent);
            if (i < 10) {
                HashMap<unsigned,unsigned> lineCounts;
                codeBlock->dump(exec);
                for (unsigned op = 0; op < record->m_size; ++op) {
                    int count = record->m_vpcCounts[op];
                    if (count) {
                        printf("    [% 4d] has sample count: % 4d\n", op, count);
                        unsigned line = codeBlock->lineNumberForVPC(code+op);
                        lineCounts.set(line, (lineCounts.contains(line) ? lineCounts.get(line) : 0) + count);
                    }
                }
                printf("\n");
                int linesCount = lineCounts.size();
                Vector<LineCountInfo> lineCountInfo(linesCount);
                int lineno = 0;
                for (HashMap<unsigned,unsigned>::iterator iter = lineCounts.begin(); iter != lineCounts.end(); ++iter, ++lineno) {
                    lineCountInfo[lineno].line = iter->first;
                    lineCountInfo[lineno].count = iter->second;
                }
#if HAVE(MERGESORT)
                mergesort(lineCountInfo.begin(), linesCount, sizeof(LineCountInfo), compareLineCountInfoSampling);
#else
                qsort(lineCountInfo.begin(), linesCount, sizeof(LineCountInfo), compareLineCountInfoSampling);
#endif
                for (lineno = 0; lineno < linesCount; ++lineno) {
                    printf("    Line #%d has sample count %d.\n", lineCountInfo[lineno].line, lineCountInfo[lineno].count);
                }
                printf("\n");
            }
        }
        
        if (record->m_vpcCounts && codeBlock) {
            Instruction* instructions = codeBlock->instructions.begin();
            for (unsigned op = 0; op < record->m_size; ++op) {
                Opcode opcode = instructions[op].u.opcode;
                if (exec->machine()->isOpcode(opcode)) {
                    totalOpcodeSamples += record->m_vpcCounts[op];
                    opcodeSampleCounts[exec->machine()->getOpcodeID(opcode)] += record->m_vpcCounts[op];
                }
            }
        }
    }
    printf("\n");

    // (3) Build and sort 'opcodeSampleInfo' array.

    OpcodeSampleInfo opcodeSampleInfo[numOpcodeIDs];
    for (int i = 0; i < numOpcodeIDs; ++i) {
        opcodeSampleInfo[i].opcode = (OpcodeID)i;
        opcodeSampleInfo[i].count = opcodeSampleCounts[i];
    }
#if HAVE(MERGESORT)
    mergesort(opcodeSampleInfo, numOpcodeIDs, sizeof(OpcodeSampleInfo), compareOpcodeIndicesSampling);
#else
    qsort(opcodeSampleInfo, numOpcodeIDs, sizeof(OpcodeSampleInfo), compareOpcodeIndicesSampling);
#endif

    // (4) Print Opcode sampling results.
    
    printf("\nOpcode sampling results\n\n"); 
    
    printf("Total opcodes sampled (total samples): %lld (%lld)\n\n", totalOpcodeSamples, m_totalSamples);
    printf("Opcodes in order:\n\n");
    for (int i = 0; i < numOpcodeIDs; ++i) {
        long long count = opcodeSampleCounts[i];
        printf("%s:%s%6lld\t%.3f%%\t(%.3f%%)\n", opcodeNames[i], padOpcodeName(reinterpret_cast<OpcodeID>(i), 20), count, (static_cast<double>(count) * 100) / totalOpcodeSamples, (static_cast<double>(count) * 100) / m_totalSamples);    
    }
    printf("\n");
    printf("Opcodes by sample count:\n\n");
    for (int i = 0; i < numOpcodeIDs; ++i) {
        OpcodeID opcode = opcodeSampleInfo[i].opcode;
        long long count = opcodeSampleInfo[i].count;
        printf("%s:%s%6lld\t%.3f%%\t(%.3f%%)\n", opcodeNames[opcode], padOpcodeName(opcode, 20), count, (static_cast<double>(count) * 100) / totalOpcodeSamples, (static_cast<double>(count) * 100) / m_totalSamples);    
    }
    printf("\n");
}

#else

void SamplingTool::dump(ExecState*)
{
}

#endif

} // namespace KJS
