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

using namespace std;

namespace JSC {

#if ENABLE(BYTECODE_SAMPLING) || ENABLE(CODEBLOCK_SAMPLING) || ENABLE(BYTECODE_STATS)

const char* const bytecodeNames[] = {
#define BYTECODE_NAME_ENTRY(bytecode) #bytecode,
    FOR_EACH_BYTECODE_ID(BYTECODE_NAME_ENTRY)
#undef BYTECODE_NAME_ENTRY
};

#endif

#if ENABLE(BYTECODE_STATS)

long long BytecodeStats::bytecodeCounts[numBytecodeIDs];
long long BytecodeStats::bytecodePairCounts[numBytecodeIDs][numBytecodeIDs];
int BytecodeStats::lastBytecode = -1;

static BytecodeStats logger;

BytecodeStats::BytecodeStats()
{
    for (int i = 0; i < numBytecodeIDs; ++i)
        bytecodeCounts[i] = 0;
    
    for (int i = 0; i < numBytecodeIDs; ++i)
        for (int j = 0; j < numBytecodeIDs; ++j)
            bytecodePairCounts[i][j] = 0;
}

static int compareBytecodeIndices(const void* left, const void* right)
{
    long long leftValue = BytecodeStats::bytecodeCounts[*(int*) left];
    long long rightValue = BytecodeStats::bytecodeCounts[*(int*) right];
    
    if (leftValue < rightValue)
        return 1;
    else if (leftValue > rightValue)
        return -1;
    else
        return 0;
}

static int compareBytecodePairIndices(const void* left, const void* right)
{
    pair<int, int> leftPair = *(pair<int, int>*) left;
    long long leftValue = BytecodeStats::bytecodePairCounts[leftPair.first][leftPair.second];
    pair<int, int> rightPair = *(pair<int, int>*) right;
    long long rightValue = BytecodeStats::bytecodePairCounts[rightPair.first][rightPair.second];
    
    if (leftValue < rightValue)
        return 1;
    else if (leftValue > rightValue)
        return -1;
    else
        return 0;
}

BytecodeStats::~BytecodeStats()
{
    long long totalInstructions = 0;
    for (int i = 0; i < numBytecodeIDs; ++i)
        totalInstructions += bytecodeCounts[i];
    
    long long totalInstructionPairs = 0;
    for (int i = 0; i < numBytecodeIDs; ++i)
        for (int j = 0; j < numBytecodeIDs; ++j)
            totalInstructionPairs += bytecodePairCounts[i][j];

    int sortedIndices[numBytecodeIDs];    
    for (int i = 0; i < numBytecodeIDs; ++i)
        sortedIndices[i] = i;
    qsort(sortedIndices, numBytecodeIDs, sizeof(int), compareBytecodeIndices);
    
    pair<int, int> sortedPairIndices[numBytecodeIDs * numBytecodeIDs];
    pair<int, int>* currentPairIndex = sortedPairIndices;
    for (int i = 0; i < numBytecodeIDs; ++i)
        for (int j = 0; j < numBytecodeIDs; ++j)
            *(currentPairIndex++) = make_pair(i, j);
    qsort(sortedPairIndices, numBytecodeIDs * numBytecodeIDs, sizeof(pair<int, int>), compareBytecodePairIndices);
    
    printf("\nExecuted bytecode statistics\n"); 
    
    printf("Total instructions executed: %lld\n\n", totalInstructions);

    printf("All bytecodes by frequency:\n\n");

    for (int i = 0; i < numBytecodeIDs; ++i) {
        int index = sortedIndices[i];
        printf("%s:%s %lld - %.2f%%\n", bytecodeNames[index], padBytecodeName((BytecodeID)index, 28), bytecodeCounts[index], ((double) bytecodeCounts[index]) / ((double) totalInstructions) * 100.0);    
    }
    
    printf("\n");
    printf("2-bytecode sequences by frequency: %lld\n\n", totalInstructions);
    
    for (int i = 0; i < numBytecodeIDs * numBytecodeIDs; ++i) {
        pair<int, int> indexPair = sortedPairIndices[i];
        long long count = bytecodePairCounts[indexPair.first][indexPair.second];
        
        if (!count)
            break;
        
        printf("%s%s %s:%s %lld %.2f%%\n", bytecodeNames[indexPair.first], padBytecodeName((BytecodeID)indexPair.first, 28), bytecodeNames[indexPair.second], padBytecodeName((BytecodeID)indexPair.second, 28), count, ((double) count) / ((double) totalInstructionPairs) * 100.0);
    }
    
    printf("\n");
    printf("Most common bytecodes and sequences:\n");

    for (int i = 0; i < numBytecodeIDs; ++i) {
        int index = sortedIndices[i];
        long long bytecodeCount = bytecodeCounts[index];
        double bytecodeProportion = ((double) bytecodeCount) / ((double) totalInstructions);
        if (bytecodeProportion < 0.0001)
            break;
        printf("\n%s:%s %lld - %.2f%%\n", bytecodeNames[index], padBytecodeName((BytecodeID)index, 28), bytecodeCount, bytecodeProportion * 100.0);

        for (int j = 0; j < numBytecodeIDs * numBytecodeIDs; ++j) {
            pair<int, int> indexPair = sortedPairIndices[j];
            long long pairCount = bytecodePairCounts[indexPair.first][indexPair.second];
            double pairProportion = ((double) pairCount) / ((double) totalInstructionPairs);
        
            if (!pairCount || pairProportion < 0.0001 || pairProportion < bytecodeProportion / 100)
                break;

            if (indexPair.first != index && indexPair.second != index)
                continue;

            printf("    %s%s %s:%s %lld - %.2f%%\n", bytecodeNames[indexPair.first], padBytecodeName((BytecodeID)indexPair.first, 28), bytecodeNames[indexPair.second], padBytecodeName((BytecodeID)indexPair.second, 28), pairCount, pairProportion * 100.0);
        }
        
    }
    printf("\n");
}

void BytecodeStats::recordInstruction(int bytecode)
{
    bytecodeCounts[bytecode]++;
    
    if (lastBytecode != -1)
        bytecodePairCounts[lastBytecode][bytecode]++;
    
    lastBytecode = bytecode;
}

void BytecodeStats::resetLastInstruction()
{
    lastBytecode = -1;
}

#endif

} // namespace JSC
