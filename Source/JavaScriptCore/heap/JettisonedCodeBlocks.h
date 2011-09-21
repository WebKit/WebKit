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

#ifndef JettisonedCodeBlocks_h
#define JettisonedCodeBlocks_h

#include <wtf/FastAllocBase.h>
#include <wtf/HashMap.h>
#include <wtf/PassOwnPtr.h>

namespace JSC {

class CodeBlock;
class SlotVisitor;

class JettisonedCodeBlocks {
    WTF_MAKE_FAST_ALLOCATED; // Only malloc'd in ConservativeRoots
public:
    JettisonedCodeBlocks();
    
    ~JettisonedCodeBlocks();
    
    void addCodeBlock(PassOwnPtr<CodeBlock>);
    
    void clearMarks();
    
    void mark(void* candidateCodeBlock)
    {
        // We have to check for 0 and -1 because those are used by the HashMap as markers.
        uintptr_t value = reinterpret_cast<uintptr_t>(candidateCodeBlock);
        
        // This checks for both of those nasty cases in one go.
        // 0 + 1 = 1
        // -1 + 1 = 0
        if (value + 1 <= 1)
            return;
        
        HashMap<CodeBlock*, bool>::iterator iter = m_map.find(static_cast<CodeBlock*>(candidateCodeBlock));
        if (iter == m_map.end())
            return;
        iter->second = true;
    }
    
    void deleteUnmarkedCodeBlocks();
    
    void traceCodeBlocks(SlotVisitor&);

private:
    // It would be great to use an OwnPtr<CodeBlock> here but that would
    // almost certainly not work.
    HashMap<CodeBlock*, bool> m_map;
};

} // namespace JSC

#endif // JettisonedCodeBlocks_h

