/*
 * Copyright (C) 2013, 2015 Apple Inc. All rights reserved.
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

#include "B3OpaqueByproducts.h"
#include "DFGCommonData.h"
#include "FTLDataSection.h"
#include "FTLLazySlowPath.h"
#include "FTLOSRExit.h"
#include "FTLStackMaps.h"
#include "FTLUnwindInfo.h"
#include "JITCode.h"
#include "LLVMAPI.h"
#include <wtf/RefCountedArray.h>

#if OS(DARWIN)
#define SECTION_NAME_PREFIX "__"
#elif OS(LINUX)
#define SECTION_NAME_PREFIX "."
#else
#error "Unsupported platform"
#endif

#define SECTION_NAME(NAME) (SECTION_NAME_PREFIX NAME)

namespace JSC {

class TrackedReferences;

namespace FTL {

class JITCode : public JSC::JITCode {
public:
    JITCode();
    ~JITCode();

    CodePtr addressForCall(ArityCheckMode) override;
    void* executableAddressAtOffset(size_t offset) override;
    void* dataAddressAtOffset(size_t offset) override;
    unsigned offsetOf(void* pointerIntoCode) override;
    size_t size() override;
    bool contains(void*) override;

#if FTL_USES_B3
    void initializeB3Code(CodeRef);
    void initializeB3Byproducts(std::unique_ptr<B3::OpaqueByproducts>);
#else
    void initializeExitThunks(CodeRef);
    void addHandle(PassRefPtr<ExecutableMemoryHandle>);
    void addDataSection(PassRefPtr<DataSection>);
#endif
    void initializeAddressForCall(CodePtr);
    void initializeArityCheckEntrypoint(CodeRef);
    
    void validateReferences(const TrackedReferences&) override;

    RegisterSet liveRegistersToPreserveAtExceptionHandlingCallSite(CodeBlock*, CallSiteIndex) override;

    Optional<CodeOrigin> findPC(CodeBlock*, void* pc) override;

#if FTL_USES_B3
    CodeRef b3Code() const { return m_b3Code; }
#else
    const Vector<RefPtr<ExecutableMemoryHandle>>& handles() const { return m_handles; }
    const Vector<RefPtr<DataSection>>& dataSections() const { return m_dataSections; }
    
    CodePtr exitThunks();
#endif
    
    JITCode* ftl() override;
    DFG::CommonData* dfgCommon() override;
    static ptrdiff_t commonDataOffset() { return OBJECT_OFFSETOF(JITCode, common); }
    
    DFG::CommonData common;
    SegmentedVector<OSRExit, 8> osrExit;
    SegmentedVector<OSRExitDescriptor, 8> osrExitDescriptors;
#if !FTL_USES_B3
    StackMaps stackmaps;
#endif // !FTL_USES_B3
    Vector<std::unique_ptr<LazySlowPath>> lazySlowPaths;
    int32_t osrExitFromGenericUnwindStackSpillSlot;
    
private:
    CodePtr m_addressForCall;
#if FTL_USES_B3
    CodeRef m_b3Code;
    std::unique_ptr<B3::OpaqueByproducts> m_b3Byproducts;
#else
    Vector<RefPtr<DataSection>> m_dataSections;
    Vector<RefPtr<ExecutableMemoryHandle>> m_handles;
#endif
    CodeRef m_arityCheckEntrypoint;
#if !FTL_USES_B3
    CodeRef m_exitThunks;
#endif
};

} } // namespace JSC::FTL

#endif // ENABLE(FLT_JIT)

#endif // FTLJITCode_h

