/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include "DFGCapabilities.h"

#include "CodeBlock.h"
#include "Interpreter.h"

namespace JSC { namespace DFG {

#if ENABLE(DFG_JIT)

template<bool (*canHandleOpcode)(OpcodeID)>
bool canHandleOpcodes(CodeBlock* codeBlock)
{
    Interpreter* interpreter = codeBlock->globalData()->interpreter;
    Instruction* instructionsBegin = codeBlock->instructions().begin();
    unsigned instructionCount = codeBlock->instructions().size();
    
    for (unsigned bytecodeOffset = 0; bytecodeOffset < instructionCount; ) {
        switch (interpreter->getOpcodeID(instructionsBegin[bytecodeOffset].u.opcode)) {
#define DEFINE_OP(opcode, length)           \
        case opcode:                        \
            if (!canHandleOpcode(opcode))  \
                return false;               \
            bytecodeOffset += length;       \
            break;
            FOR_EACH_OPCODE_ID(DEFINE_OP)
#undef DEFINE_OP
        default:
            ASSERT_NOT_REACHED();
            break;
        }
    }
    
    return true;
}

bool canCompileOpcodes(CodeBlock* codeBlock)
{
    return canHandleOpcodes<canCompileOpcode>(codeBlock);
}

bool canInlineOpcodes(CodeBlock* codeBlock)
{
    return canHandleOpcodes<canInlineOpcode>(codeBlock);
}

#endif

} } // namespace JSC::DFG

