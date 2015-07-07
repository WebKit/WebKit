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

#ifndef FTLJITCode_h
#define FTLJITCode_h

#if ENABLE(FTL_JIT)

#include "DFGCommonData.h"
#include "FTLDataSection.h"
#include "FTLOSRExit.h"
#include "FTLStackMaps.h"
#include "FTLUnwindInfo.h"
#include "JITCode.h"
#include "LLVMAPI.h"
#include <wtf/RefCountedArray.h>

namespace JSC { namespace FTL {

class JITCode : public JSC::JITCode {
public:
    JITCode();
    ~JITCode();
    
    CodePtr addressForCall(VM&, ExecutableBase*, ArityCheckMode, RegisterPreservationMode) override;
    void* executableAddressAtOffset(size_t offset) override;
    void* dataAddressAtOffset(size_t offset) override;
    unsigned offsetOf(void* pointerIntoCode) override;
    size_t size() override;
    bool contains(void*) override;
    
    void initializeExitThunks(CodeRef);
    void addHandle(PassRefPtr<ExecutableMemoryHandle>);
    void addDataSection(PassRefPtr<DataSection>);
    void initializeArityCheckEntrypoint(CodeRef);
    void initializeAddressForCall(CodePtr);
    
    const Vector<RefPtr<ExecutableMemoryHandle>>& handles() const { return m_handles; }
    const Vector<RefPtr<DataSection>>& dataSections() const { return m_dataSections; }
    
    CodePtr exitThunks();
    
    JITCode* ftl();
    DFG::CommonData* dfgCommon();
    
    DFG::CommonData common;
    SegmentedVector<OSRExit, 8> osrExit;
    StackMaps stackmaps;
    UnwindInfo unwindInfo;
    
private:
    Vector<RefPtr<DataSection>> m_dataSections;
    Vector<RefPtr<ExecutableMemoryHandle>> m_handles;
    CodePtr m_addressForCall;
    CodeRef m_arityCheckEntrypoint;
    CodeRef m_exitThunks;
};

} } // namespace JSC::FTL

#endif // ENABLE(FLT_JIT)

#endif // FTLJITCode_h

