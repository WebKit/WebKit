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

#if !PLATFORM(WIN_OS)
#include <unistd.h>
#endif

namespace JSC {

void ScopeSampleRecord::sample(CodeBlock* codeBlock, Instruction* vPC)
{
    if (!m_vpcCounts) {
        m_size = codeBlock->instructions.size();
        m_vpcCounts = static_cast<int*>(calloc(m_size, sizeof(int)));
        m_codeBlock = codeBlock;
    }

    unsigned codeOffset = vPC - codeBlock->instructions.begin();
    // Since we don't read and write codeBlock and vPC atomically, this check
    // can fail if we sample mid op_call / op_ret.
    if (codeOffset < m_size) {
        m_vpcCounts[codeOffset]++;
        m_totalCount++;
    }
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

        Sample sample(m_sample, m_codeBlock);
        ++m_sampleCount;

        if (sample.isNull())
            continue;

        if (!sample.inHostFunction()) {
            unsigned opcodeID = m_machine->getOpcodeID(sample.vPC()[0].u.opcode);

            ++m_opcodeSampleCount;
            ++m_opcodeSamples[opcodeID];

            if (sample.inCTIFunction())
                m_opcodeSamplesInCTIFunctions[opcodeID]++;
        }

#if ENABLE(CODEBLOCK_SAMPLING)
        ScopeSampleRecord* record = m_scopeSampleMap->get(sample.codeBlock()->ownerNode);
        ASSERT(record);
        record->sample(sample.codeBlock(), sample.vPC());
#endif
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

#if ENABLE(OPCODE_SAMPLING)

struct OpcodeSampleInfo {
    OpcodeID opcode;
    long long count;
    long long countInCTIFunctions;
};

struct LineCountInfo {
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
    if (m_sampleCount < 10)
        return;
    
    // (1) Build and sort 'opcodeSampleInfo' array.

    OpcodeSampleInfo opcodeSampleInfo[numOpcodeIDs];
    for (int i = 0; i < numOpcodeIDs; ++i) {
        opcodeSampleInfo[i].opcode = static_cast<OpcodeID>(i);
        opcodeSampleInfo[i].count = m_opcodeSamples[i];
        opcodeSampleInfo[i].countInCTIFunctions = m_opcodeSamplesInCTIFunctions[i];
    }

    qsort(opcodeSampleInfo, numOpcodeIDs, sizeof(OpcodeSampleInfo), compareOpcodeIndicesSampling);

    // (2) Print Opcode sampling results.

    printf("\nOpcode samples [*]\n");
    printf("                             sample   %% of       %% of     |   cti     cti %%\n");
    printf("opcode                       count     VM        total    |  count   of self\n");
    printf("-------------------------------------------------------   |  ----------------\n");

    for (int i = 0; i < numOpcodeIDs; ++i) {
        long long count = opcodeSampleInfo[i].count;
        if (!count)
            continue;

        OpcodeID opcode = opcodeSampleInfo[i].opcode;
        
        const char* opcodeName = opcodeNames[opcode];
        const char* opcodePadding = padOpcodeName(opcode, 28);
        double percentOfVM = (static_cast<double>(count) * 100) / m_opcodeSampleCount;
        double percentOfTotal = (static_cast<double>(count) * 100) / m_sampleCount;
        long long countInCTIFunctions = opcodeSampleInfo[i].countInCTIFunctions;
        double percentInCTIFunctions = (static_cast<double>(countInCTIFunctions) * 100) / count;
        fprintf(stdout, "%s:%s%-6lld %.3f%%\t%.3f%%\t  |   %-6lld %.3f%%\n", opcodeName, opcodePadding, count, percentOfVM, percentOfTotal, countInCTIFunctions, percentInCTIFunctions);
    }
    
    printf("\n[*] Samples inside host code are not charged to any Opcode.\n\n");
    printf("\tSamples inside VM:\t\t%lld / %lld (%.3f%%)\n", m_opcodeSampleCount, m_sampleCount, (static_cast<double>(m_opcodeSampleCount) * 100) / m_sampleCount);
    printf("\tSamples inside host code:\t%lld / %lld (%.3f%%)\n\n", m_sampleCount - m_opcodeSampleCount, m_sampleCount, (static_cast<double>(m_sampleCount - m_opcodeSampleCount) * 100) / m_sampleCount);
    printf("\tsample count:\tsamples inside this opcode\n");
    printf("\t%% of VM:\tsample count / all opcode samples\n");
    printf("\t%% of total:\tsample count / all samples\n");
    printf("\t--------------\n");
    printf("\tcti count:\tsamples inside a CTI function called by this opcode\n");
    printf("\tcti %% of self:\tcti count / sample count\n");
    
    // (3) Calculate 'codeBlockSampleCount', build and sort 'codeBlockSamples' array.

    int scopeCount = m_scopeSampleMap->size();
    long long codeBlockSampleCount = 0;
    Vector<ScopeSampleRecord*> codeBlockSamples(scopeCount);
    ScopeSampleRecordMap::iterator iter = m_scopeSampleMap->begin();
    for (int i = 0; i < scopeCount; ++i, ++iter) {
        codeBlockSamples[i] = iter->second;
        codeBlockSampleCount += codeBlockSamples[i]->m_totalCount;
    }

    qsort(codeBlockSamples.begin(), scopeCount, sizeof(ScopeSampleRecord*), compareScopeSampleRecords);

    // (4) Print data from 'codeBlockSamples' array.

    printf("\nCodeBlock samples [*]\n\n"); 

    for (int i = 0; i < scopeCount; ++i) {
        ScopeSampleRecord* record = codeBlockSamples[i];
        CodeBlock* codeBlock = record->m_codeBlock;

        double blockPercent = (record->m_totalCount * 100.0) / codeBlockSampleCount;

        if (blockPercent >= 1) {
            Instruction* code = codeBlock->instructions.begin();
            printf("#%d: %s:%d: %d / %lld (%.3f%%)\n", i + 1, record->m_scope->sourceURL().UTF8String().c_str(), codeBlock->lineNumberForVPC(code), record->m_totalCount, codeBlockSampleCount, blockPercent);
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

                qsort(lineCountInfo.begin(), linesCount, sizeof(LineCountInfo), compareLineCountInfoSampling);

                for (lineno = 0; lineno < linesCount; ++lineno) {
                    printf("    Line #%d has sample count %d.\n", lineCountInfo[lineno].line, lineCountInfo[lineno].count);
                }
                printf("\n");
            }
        }
    }

    printf("\n[*] Samples inside host code are charged to the calling Opcode.\n");
    printf("    Samples that fall on a call / return boundary are discarded.\n\n");
    printf("\tSamples discarded:\t\t%lld / %lld (%.3f%%)\n\n", m_sampleCount - codeBlockSampleCount, m_sampleCount, (static_cast<double>(m_sampleCount - codeBlockSampleCount) * 100) / m_sampleCount);
}

#else

void SamplingTool::dump(ExecState*)
{
}

#endif

} // namespace JSC
