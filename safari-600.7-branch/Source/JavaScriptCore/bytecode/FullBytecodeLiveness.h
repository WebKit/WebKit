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

#ifndef FullBytecodeLiveness_h
#define FullBytecodeLiveness_h

#include <wtf/FastBitVector.h>

namespace JSC {

class BytecodeLivenessAnalysis;

typedef HashMap<unsigned, FastBitVector, WTF::IntHash<unsigned>, WTF::UnsignedWithZeroKeyHashTraits<unsigned>> BytecodeToBitmapMap;

class FullBytecodeLiveness {
public:
    FullBytecodeLiveness() : m_codeBlock(0) { }
    
    // We say "out" to refer to the bitvector that contains raw results for a bytecode
    // instruction.
    const FastBitVector& getOut(unsigned bytecodeIndex) const
    {
        BytecodeToBitmapMap::const_iterator iter = m_map.find(bytecodeIndex);
        ASSERT(iter != m_map.end());
        return iter->value;
    }
    
    bool operandIsLive(int operand, unsigned bytecodeIndex) const
    {
        return operandIsAlwaysLive(m_codeBlock, operand) || operandThatIsNotAlwaysLiveIsLive(m_codeBlock, getOut(bytecodeIndex), operand);
    }
    
    FastBitVector getLiveness(unsigned bytecodeIndex) const
    {
        return getLivenessInfo(m_codeBlock, getOut(bytecodeIndex));
    }
    
private:
    friend class BytecodeLivenessAnalysis;
    
    CodeBlock* m_codeBlock;
    BytecodeToBitmapMap m_map;
};

} // namespace JSC

#endif // FullBytecodeLiveness_h

