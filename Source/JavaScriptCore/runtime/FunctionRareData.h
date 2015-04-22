/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#ifndef FunctionRareData_h
#define FunctionRareData_h

#include "JSCell.h"
#include "ObjectAllocationProfile.h"
#include "Watchpoint.h"

namespace JSC {

class JSGlobalObject;
class LLIntOffsetsExtractor;
namespace DFG {
class SpeculativeJIT;
class JITCompiler;
}

class FunctionRareData : public JSCell {
    friend class JIT;
    friend class DFG::SpeculativeJIT;
    friend class DFG::JITCompiler;
    friend class VM;
    
public:
    typedef JSCell Base;
    static const unsigned StructureFlags = StructureIsImmortal | Base::StructureFlags;

    static FunctionRareData* create(VM&, JSObject* prototype, size_t inlineCapacity);

    static const bool needsDestruction = true;
    static void destroy(JSCell*);

    static Structure* createStructure(VM&, JSGlobalObject*, JSValue prototype);

    static void visitChildren(JSCell*, SlotVisitor&);

    DECLARE_INFO;

    static inline ptrdiff_t offsetOfAllocationProfile()
    {
        return OBJECT_OFFSETOF(FunctionRareData, m_allocationProfile);
    }

    ObjectAllocationProfile* allocationProfile()
    {
        return &m_allocationProfile;
    }

    Structure* allocationStructure() { return m_allocationProfile.structure(); }

    InlineWatchpointSet& allocationProfileWatchpointSet()
    {
        return m_allocationProfileWatchpoint;
    }

    void clear(const char* reason);

    void initialize(VM&, JSObject* prototype, size_t inlineCapacity);

    bool isInitialized() { return !m_allocationProfile.isNull(); }

protected:
    FunctionRareData(VM&);
    ~FunctionRareData();

    void finishCreation(VM&, JSObject* prototype, size_t inlineCapacity);
    using Base::finishCreation;

private:

    friend class LLIntOffsetsExtractor;

    ObjectAllocationProfile m_allocationProfile;
    InlineWatchpointSet m_allocationProfileWatchpoint;
};

} // namespace JSC

#endif // FunctionRareData_h
