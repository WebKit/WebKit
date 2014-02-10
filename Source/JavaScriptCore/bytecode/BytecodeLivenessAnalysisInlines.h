/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef BytecodeLivenessAnalysisInlines_h
#define BytecodeLivenessAnalysisInlines_h

#include "BytecodeLivenessAnalysis.h"
#include "CodeBlock.h"
#include "Operations.h"

namespace JSC {

inline bool operandIsAlwaysLive(CodeBlock* codeBlock, int operand)
{
    if (VirtualRegister(operand).isArgument())
        return true;
    return operand <= codeBlock->captureStart() && operand > codeBlock->captureEnd();
}

inline bool operandThatIsNotAlwaysLiveIsLive(CodeBlock* codeBlock, const FastBitVector& out, int operand)
{
    VirtualRegister virtualReg(operand);
    if (virtualReg.offset() > codeBlock->captureStart())
        return out.get(virtualReg.toLocal());
    size_t index = virtualReg.toLocal() - codeBlock->captureCount();
    if (index >= out.numBits())
        return false;
    return out.get(index);
}

inline bool operandIsLive(CodeBlock* codeBlock, const FastBitVector& out, int operand)
{
    return operandIsAlwaysLive(codeBlock, operand) || operandThatIsNotAlwaysLiveIsLive(codeBlock, out, operand);
}

} // namespace JSC

#endif // BytecodeLivenessAnalysisInlines_h

