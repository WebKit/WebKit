/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Cameron Zwarich <cwzwarich@uwaterloo.ca>
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
#include "Opcode.h"

#include <stdlib.h>
#include "CodeBlock.h"
#include "Machine.h"

using namespace std;

namespace KJS {

#if SAMPLING_TOOL_ENABLED || DUMP_OPCODE_STATS

static const char* opcodeNames[] = {
    "load             ",
    "new_object       ",
    "new_array        ",
    "new_regexp       ",
    "mov              ",
    
    "not              ",
    "eq               ",
    "neq              ",
    "stricteq         ",
    "nstricteq        ",
    "less             ",
    "lesseq           ",
    
    "pre_inc          ",
    "pre_dec          ",
    "post_inc         ",
    "post_dec         ",
    "to_jsnumber      ",
    "negate           ",
    "add              ",
    "mul              ",
    "div              ",
    "mod              ",
    "sub              ",
    
    "lshift           ",
    "rshift           ",
    "urshift          ",
    "bitand           ",
    "bitxor           ",
    "bitor            ",
    "bitnot           ",
    
    "instanceof       ",
    "typeof           ",
    "in               ",

    "resolve          ",
    "resolve_skip     ",
    "get_scoped_var   ",
    "put_scoped_var   ",
    "resolve_base     ",
    "resolve_with_base",
    "resolve_func     ",
    "get_by_id        ",
    "put_by_id        ",
    "del_by_id        ",
    "get_by_val       ",
    "put_by_val       ",
    "del_by_val       ",
    "put_by_index     ",
    "put_getter       ",
    "put_setter       ",

    "jmp              ",
    "jtrue            ",
    "jfalse           ",
    "jless            ",
    "jnless           ",
    "jmp_scopes       ",
    "loop             ",
    "loop_if_true     ",
    "loop_if_less     ",
    "switch_imm       ",
    "switch_char      ",
    "switch_string    ",

    "new_func         ",
    "new_func_exp     ",
    "call             ",
    "call_eval        ",
    "ret              ",

    "construct        ",

    "get_pnames       ",
    "next_pname       ",

    "push_scope       ",
    "pop_scope        ",

    "catch            ",
    "throw            ",
    "new_error        ",

    "jsr              ",
    "sret             ",

    "debug            ",

    "end              "
};

#endif

#if SAMPLING_TOOL_ENABLED

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

static inline unsigned hertz2us(unsigned hertz)
{
    return 1000000 / hertz;
}

void SamplingTool::run()
{
    while (m_running) {
        usleep(hertz2us(m_hertz));

        m_totalSamples++;

        CodeBlock* codeBlock = m_recordedCodeBlock;
        Instruction* vPC = m_recordedVPC;

        if (codeBlock && vPC) {
            ScopeSampleRecord* record = m_scopeSampleMap->get(codeBlock->ownerNode);
            if (record)
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
    pthread_create(&m_samplingThread, 0, threadStartFunc, this);
}

void SamplingTool::stop()
{
    ASSERT(m_running);
    m_running = false;
    pthread_join(m_samplingThread, 0);
}

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
    const LineCountInfo *leftLineCount = reinterpret_cast<const LineCountInfo *>(left);
    const LineCountInfo *rightLineCount = reinterpret_cast<const LineCountInfo *>(right);

    return (leftLineCount->line > rightLineCount->line) ? 1 : (leftLineCount->line < rightLineCount->line) ? -1 : 0;
}

static int compareOpcodeIndicesSampling(const void* left, const void* right)
{
    const OpcodeSampleInfo *leftSampleInfo = reinterpret_cast<const OpcodeSampleInfo *>(left);
    const OpcodeSampleInfo *rightSampleInfo = reinterpret_cast<const OpcodeSampleInfo *>(right);

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
    ScopeSampleRecord* codeBlockSamples[scopeCount];
    ScopeSampleRecordMap::iterator iter = m_scopeSampleMap->begin();
    for (int i=0; i < scopeCount; ++i, ++iter) {
        codeBlockSamples[i] = iter->second;
        totalCodeBlockSamples += codeBlockSamples[i]->m_totalCount;
    }
    mergesort(codeBlockSamples, scopeCount, sizeof(ScopeSampleRecord*), compareScopeSampleRecords);

    // (2) Print data from 'codeBlockSamples' array, calculate 'totalOpcodeSamples', populate 'opcodeSampleCounts' array.

    long long totalOpcodeSamples = 0;
    long long opcodeSampleCounts[numOpcodeIDs] = { 0 };

    fprintf(stdout, "\nBlock sampling results\n\n"); 
    fprintf(stdout, "Total blocks sampled (total samples): %lld (%lld)\n\n", totalCodeBlockSamples, m_totalSamples);

    for (int i=0; i < scopeCount; i++) {
        ScopeSampleRecord *record = codeBlockSamples[i];
        CodeBlock* codeBlock = record->m_codeBlock;

        double totalPercent = (record->m_totalCount * 100.0)/m_totalSamples;
        double blockPercent = (record->m_totalCount * 100.0)/totalCodeBlockSamples;

        if ((blockPercent >= 1) && codeBlock) {
            Instruction* code = codeBlock->instructions.begin();
            fprintf(stdout, "#%d: %s:%d: sampled %d times - %.3f%% (%.3f%%)\n", i+1, record->m_scope->sourceURL().UTF8String().c_str(), codeBlock->lineNumberForVPC(code), record->m_totalCount, blockPercent, totalPercent);
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
                LineCountInfo lineCountInfo[linesCount];
                int lineno = 0;
                for (HashMap<unsigned,unsigned>::iterator iter = lineCounts.begin(); iter != lineCounts.end(); ++iter, ++lineno) {
                    lineCountInfo[lineno].line = iter->first;
                    lineCountInfo[lineno].count = iter->second;
                }
                mergesort(lineCountInfo, linesCount, sizeof(LineCountInfo), compareLineCountInfoSampling);
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
    mergesort(opcodeSampleInfo, numOpcodeIDs, sizeof(OpcodeSampleInfo), compareOpcodeIndicesSampling);

    // (4) Print Opcode sampling results.
    
    fprintf(stdout, "\nOpcode sampling results\n\n"); 
    
    fprintf(stdout, "Total opcodes sampled (total samples): %lld (%lld)\n\n", totalOpcodeSamples, m_totalSamples);
    fprintf(stdout, "Opcodes in order:\n\n");
    for (int i = 0; i < numOpcodeIDs; ++i) {
        long long count = opcodeSampleCounts[i];
        fprintf(stdout, "%s:\t%6lld\t%.3f%%\t(%.3f%%)\n", opcodeNames[i], count, ((double)count * 100)/totalOpcodeSamples, ((double)count * 100)/m_totalSamples);    
    }
    fprintf(stdout, "\n");
    fprintf(stdout, "Opcodes by sample count:\n\n");
    for (int i = 0; i < numOpcodeIDs; ++i) {
        OpcodeID opcode = opcodeSampleInfo[i].opcode;
        long long count = opcodeSampleInfo[i].count;
        fprintf(stdout, "%s:\t%6lld\t%.3f%%\t(%.3f%%)\n", opcodeNames[opcode], count, ((double)count * 100)/totalOpcodeSamples, ((double)count * 100)/m_totalSamples);    
    }
    fprintf(stdout, "\n");
}

#endif


#if DUMP_OPCODE_STATS

long long OpcodeStats::opcodeCounts[numOpcodeIDs];
long long OpcodeStats::opcodePairCounts[numOpcodeIDs][numOpcodeIDs];
int OpcodeStats::lastOpcode = -1;

static OpcodeStats logger;

OpcodeStats::OpcodeStats()
{
    for (int i = 0; i < numOpcodeIDs; ++i)
        opcodeCounts[i] = 0;
    
    for (int i = 0; i < numOpcodeIDs; ++i)
        for (int j = 0; j < numOpcodeIDs; ++j)
            opcodePairCounts[i][j] = 0;
}

static int compareOpcodeIndices(const void* left, const void* right)
{
    long long leftValue = OpcodeStats::opcodeCounts[*(int*) left];
    long long rightValue = OpcodeStats::opcodeCounts[*(int*) right];
    
    if (leftValue < rightValue)
        return 1;
    else if (leftValue > rightValue)
        return -1;
    else
        return 0;
}

static int compareOpcodePairIndices(const void* left, const void* right)
{
    pair<int, int> leftPair = *(pair<int, int>*) left;
    long long leftValue = OpcodeStats::opcodePairCounts[leftPair.first][leftPair.second];
    pair<int, int> rightPair = *(pair<int, int>*) right;
    long long rightValue = OpcodeStats::opcodePairCounts[rightPair.first][rightPair.second];
    
    if (leftValue < rightValue)
        return 1;
    else if (leftValue > rightValue)
        return -1;
    else
        return 0;
}

OpcodeStats::~OpcodeStats()
{
    long long totalInstructions = 0;
    for (int i = 0; i < numOpcodeIDs; ++i)
        totalInstructions += opcodeCounts[i];
    
    long long totalInstructionPairs = 0;
    for (int i = 0; i < numOpcodeIDs; ++i)
        for (int j = 0; j < numOpcodeIDs; ++j)
            totalInstructionPairs += opcodePairCounts[i][j];

    int sortedIndices[numOpcodeIDs];    
    for (int i = 0; i < numOpcodeIDs; ++i)
        sortedIndices[i] = i;
    mergesort(sortedIndices, numOpcodeIDs, sizeof(int), compareOpcodeIndices);
    
    pair<int, int> sortedPairIndices[numOpcodeIDs * numOpcodeIDs];
    pair<int, int>* currentPairIndex = sortedPairIndices;
    for (int i = 0; i < numOpcodeIDs; ++i)
        for (int j = 0; j < numOpcodeIDs; ++j)
            *(currentPairIndex++) = make_pair(i, j);
    mergesort(sortedPairIndices, numOpcodeIDs * numOpcodeIDs, sizeof(pair<int, int>), compareOpcodePairIndices);
    
    printf("\nExecuted opcode statistics\n"); 
    
    printf("Total instructions executed: %lld\n\n", totalInstructions);

    printf("All opcodes by frequency:\n\n");

    for (int i = 0; i < numOpcodeIDs; ++i) {
        int index = sortedIndices[i];
        printf("%s: %lld - %.2f%%\n", opcodeNames[index], opcodeCounts[index], ((double) opcodeCounts[index]) / ((double) totalInstructions) * 100.0);    
    }
    
    printf("\n");
    printf("2-opcode sequences by frequency: %lld\n\n", totalInstructions);
    
    for (int i = 0; i < numOpcodeIDs * numOpcodeIDs; ++i) {
        pair<int, int> indexPair = sortedPairIndices[i];
        long long count = opcodePairCounts[indexPair.first][indexPair.second];
        
        if (!count)
            break;
        
        printf("%s %s: %lld %.2f%%\n", opcodeNames[indexPair.first], opcodeNames[indexPair.second], count, ((double) count) / ((double) totalInstructionPairs) * 100.0);
    }
    
    printf("\n");
    printf("Most common opcodes and sequences:\n");

    for (int i = 0; i < numOpcodeIDs; ++i) {
        int index = sortedIndices[i];
        long long opcodeCount = opcodeCounts[index];
        double opcodeProportion = ((double) opcodeCount) / ((double) totalInstructions);
        if (opcodeProportion < 0.0001)
            break;
        printf("\n%s: %lld - %.2f%%\n", opcodeNames[index], opcodeCount, opcodeProportion * 100.0);

        for (int j = 0; j < numOpcodeIDs * numOpcodeIDs; ++j) {
            pair<int, int> indexPair = sortedPairIndices[j];
            long long pairCount = opcodePairCounts[indexPair.first][indexPair.second];
            double pairProportion = ((double) pairCount) / ((double) totalInstructionPairs);
        
            if (!pairCount || pairProportion < 0.0001 || pairProportion < opcodeProportion / 100)
                break;

            if (indexPair.first != index && indexPair.second != index)
                continue;

            printf("    %s %s: %lld - %.2f%%\n", opcodeNames[indexPair.first], opcodeNames[indexPair.second], pairCount, pairProportion * 100.0);
        }
        
    }
    printf("\n");
}

void OpcodeStats::recordInstruction(int opcode)
{
    opcodeCounts[opcode]++;
    
    if (lastOpcode != -1)
        opcodePairCounts[lastOpcode][opcode]++;
    
    lastOpcode = opcode;
}

void OpcodeStats::resetLastInstruction()
{
    lastOpcode = -1;
}

#endif

} // namespace WTF
