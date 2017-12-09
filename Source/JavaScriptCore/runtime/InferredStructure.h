/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#pragma once

#include "InferredStructureWatchpoint.h"
#include "JSCell.h"
#include "VM.h"

namespace JSC {

class InferredType;

class InferredStructure final : public JSCell {
public:
    typedef JSCell Base;
    
    template<typename CellType>
    static IsoSubspace* subspaceFor(VM& vm)
    {
        return &vm.inferredStructureSpace;
    }
    
    static InferredStructure* create(VM&, GCDeferralContext&, InferredType* parent, Structure*);
    
    static const bool needsDestruction = true;
    static void destroy(JSCell*);
    
    static const unsigned StructureFlags = StructureIsImmortal | Base::StructureFlags;

    static Structure* createStructure(VM&, JSGlobalObject*, JSValue prototype);
    
    static void visitChildren(JSCell*, SlotVisitor&);

    DECLARE_INFO;
    
    InferredType* parent() const { return m_parent.get(); }
    Structure* structure() const { return m_structure.get(); }
    
    void finalizeUnconditionally(VM&);

private:
    InferredStructure(VM&, InferredType* parent, Structure*);
    
    void finishCreation(VM&, Structure*);
    
    WriteBarrier<InferredType> m_parent;
    WriteBarrier<Structure> m_structure;
    
    friend class InferredStructureWatchpoint;
    InferredStructureWatchpoint m_watchpoint;
};

} // namespace JSC

