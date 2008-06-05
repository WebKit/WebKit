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

#include "Opcode.h"

namespace KJS {

#if DUMP_OPCODE_STATS

unsigned OpcodeStats::opcodeCounts[numOpcodeIDs];

static OpcodeStats logger;

static const char* opcodeNames[] = {
    "load",
    "new_object",
    "new_array",
    "new_regexp",
    "mov",
    
    "not",
    "eq",
    "neq",
    "stricteq",
    "nstricteq",
    "less",
    "lesseq",
    
    "pre_inc",
    "pre_dec",
    "post_inc",
    "post_dec",
    "to_jsnumber",
    "negate",
    "add",
    "mul",
    "div",
    "mod",
    "sub",
    
    "lshift",
    "rshift",
    "urshift",
    "bitand",
    "bitxor",
    "bitor",
    "bitnot",
    
    "instanceof",
    "typeof",
    "in",

    "resolve",
    "resolve_skip",
    "get_scoped_var",
    "put_scoped_var",
    "resolve_base",
    "resolve_with_base",
    "resolve_func",
    "get_by_id",
    "put_by_id",
    "del_by_id",
    "get_by_val",
    "put_by_val",
    "del_by_val",
    "put_by_index",
    "put_getter",
    "put_setter",

    "jmp",
    "jtrue",
    "jfalse",
    "jmp_scopes",

    "new_func",
    "new_func_exp",
    "call",
    "call_eval",
    "ret",

    "construct",

    "get_pnames",
    "next_pname",

    "push_scope",
    "pop_scope",

    "catch",
    "throw",
    "new_error",

    "jsr",
    "sret",

    "debug",

    "end"    
};

OpcodeStats::OpcodeStats()
{
    for (int i = 0; i < numOpcodeIDs; ++i)
        opcodeCounts[i] = 0;
}

OpcodeStats::~OpcodeStats()
{
    int totalInstructions = 0;
    int sortedIndices[numOpcodeIDs];
    
    for (int i = 0; i < numOpcodeIDs; ++i) {
        totalInstructions += opcodeCounts[i];
        sortedIndices[i] = i;
    }
        
    for (int i = 0; i < numOpcodeIDs - 1; ++i) {
        int max = i;
        
        for (int j = i + 1; j < numOpcodeIDs; ++j) {
            if (opcodeCounts[sortedIndices[j]] > opcodeCounts[sortedIndices[max]])
                max = j;
        }
        
        int temp = sortedIndices[i];
        sortedIndices[i] = sortedIndices[max];
        sortedIndices[max] = temp;
    }
 
    printf("\nExecuted opcode statistics:\n\n"); 
    
    printf("Total instructions executed: %d\n\n", totalInstructions);

    for (int i = 0; i < numOpcodeIDs; ++i) {
        int index = sortedIndices[i];
        printf("%s: %.2f%%\n", opcodeNames[index], ((double) opcodeCounts[index]) / ((double) totalInstructions) * 100.0);    
    }
    
    printf("\n");
}

void OpcodeStats::recordInstruction(int opcode)
{
    opcodeCounts[opcode]++;
}

#endif

} // namespace WTF
