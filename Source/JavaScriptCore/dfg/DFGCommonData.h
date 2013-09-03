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

#ifndef DFGCommonData_h
#define DFGCommonData_h

#include <wtf/Platform.h>

#if ENABLE(DFG_JIT)

#include "JSCell.h"
#include "ProfilerCompilation.h"
#include <wtf/Noncopyable.h>

namespace JSC {

class CodeBlock;
class Identifier;

namespace DFG {

struct Node;
struct Plan;

// CommonData holds the set of data that both DFG and FTL code blocks need to know
// about themselves.

struct WeakReferenceTransition {
    WeakReferenceTransition() { }
    
    WeakReferenceTransition(VM& vm, JSCell* owner, JSCell* codeOrigin, JSCell* from, JSCell* to)
        : m_from(vm, owner, from)
        , m_to(vm, owner, to)
    {
        if (!!codeOrigin)
            m_codeOrigin.set(vm, owner, codeOrigin);
    }
    
    WriteBarrier<JSCell> m_codeOrigin;
    WriteBarrier<JSCell> m_from;
    WriteBarrier<JSCell> m_to;
};
        
class CommonData {
    WTF_MAKE_NONCOPYABLE(CommonData);
public:
    CommonData() { }
    
    void notifyCompilingStructureTransition(Plan&, CodeBlock*, Node*);
    
    void shrinkToFit();

    Vector<Identifier> dfgIdentifiers;
    Vector<WeakReferenceTransition> transitions;
    Vector<WriteBarrier<JSCell> > weakReferences;
    
    RefPtr<Profiler::Compilation> compilation;
    bool livenessHasBeenProved; // Initialized and used on every GC.
    bool allTransitionsHaveBeenMarked; // Initialized and used on every GC.
};

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

#endif // DFGCommonData_h

