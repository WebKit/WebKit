/*
 * Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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

#include "JITOperationList.h"
#include <wtf/PtrTag.h>

namespace JSC {

using PtrTag = WTF::PtrTag;

#define FOR_EACH_JSC_PTRTAG(v) \
    /* Native None */ \
    v(DOMJITFunctionPtrTag, PtrTagCalleeType::Native, PtrTagCallerType::None) \
    v(DisassemblyPtrTag, PtrTagCalleeType::Native, PtrTagCallerType::None) \
    v(PutValuePtrTag, PtrTagCalleeType::Native, PtrTagCallerType::None) \
    /* JIT None */ \
    v(B3CompilationPtrTag, PtrTagCalleeType::JIT, PtrTagCallerType::None) \
    v(ExecutableMemoryPtrTag, PtrTagCalleeType::JIT, PtrTagCallerType::None) \
    v(LinkBufferPtrTag, PtrTagCalleeType::JIT, PtrTagCallerType::None) \
    /* Native Native */ \
    v(BytecodePtrTag, PtrTagCalleeType::Native, PtrTagCallerType::Native) \
    v(JITProbePtrTag, PtrTagCalleeType::Native, PtrTagCallerType::Native) \
    v(JITProbeExecutorPtrTag, PtrTagCalleeType::Native, PtrTagCallerType::Native) \
    v(JITProbeStackInitializationFunctionPtrTag, PtrTagCalleeType::Native, PtrTagCallerType::Native) \
    v(SlowPathPtrTag, PtrTagCalleeType::Native, PtrTagCallerType::Native) \
    /* JIT Native */ \
    v(NativeToJITGatePtrTag, PtrTagCalleeType::JIT, PtrTagCallerType::Native) \
    /* Native JIT */ \
    v(HostFunctionPtrTag, PtrTagCalleeType::Native, PtrTagCallerType::JIT) \
    v(OperationPtrTag, PtrTagCalleeType::Native, PtrTagCallerType::JIT) \
    v(JITToNativeGatePtrTag, PtrTagCalleeType::Native, PtrTagCallerType::JIT) \
    /* JIT JIT */ \
    v(ExceptionHandlerPtrTag, PtrTagCalleeType::JIT, PtrTagCallerType::JIT) \
    v(JITThunkPtrTag, PtrTagCalleeType::JIT, PtrTagCallerType::JIT) \
    v(JITStubRoutinePtrTag, PtrTagCalleeType::JIT, PtrTagCallerType::JIT) \
    v(JSEntryPtrTag, PtrTagCalleeType::JIT, PtrTagCallerType::JIT) \
    v(JSInternalPtrTag, PtrTagCalleeType::JIT, PtrTagCallerType::JIT) \
    v(JSSwitchPtrTag, PtrTagCalleeType::JIT, PtrTagCallerType::JIT) \
    v(OSREntryPtrTag, PtrTagCalleeType::JIT, PtrTagCallerType::JIT) \
    v(OSRExitPtrTag, PtrTagCalleeType::JIT, PtrTagCallerType::JIT) \
    v(WasmEntryPtrTag, PtrTagCalleeType::JIT, PtrTagCallerType::JIT) \
    v(Yarr8BitPtrTag, PtrTagCalleeType::JIT, PtrTagCallerType::JIT) \
    v(Yarr16BitPtrTag, PtrTagCalleeType::JIT, PtrTagCallerType::JIT) \
    v(YarrMatchOnly8BitPtrTag, PtrTagCalleeType::JIT, PtrTagCallerType::JIT) \
    v(YarrMatchOnly16BitPtrTag, PtrTagCalleeType::JIT, PtrTagCallerType::JIT) \
    v(YarrBacktrackPtrTag, PtrTagCalleeType::JIT, PtrTagCallerType::JIT) \

#define JSC_DECLARE_PTRTAG(name, type, target) WTF_DECLARE_PTRTAG(name)

#define JSC_DECLARE_PTRTAG_TRAIT(passedTag, passedCalleeType, passedCallerType) \
    template<> struct PtrTagTraits<JSC::passedTag> { \
        static constexpr PtrTag tag = JSC::passedTag; \
        static constexpr PtrTagCalleeType calleeType = passedCalleeType; \
        static constexpr PtrTagCallerType callerType = passedCallerType; \
        static constexpr bool isSpecialized = passedCallerType == PtrTagCallerType::JIT;\
    \
        template<typename PtrType> \
        ALWAYS_INLINE static PtrType tagCodePtr(PtrType ptr) \
        { \
            if constexpr (!isSpecialized) \
                return WTF::tagNativeCodePtrImpl<tag>(ptr); \
            else \
                return JSC::tagJSCCodePtrImpl<tag, calleeType, callerType>(ptr); \
        } \
    \
        template<typename PtrType> \
        ALWAYS_INLINE static PtrType untagCodePtr(PtrType ptr) \
        { \
            if constexpr (!isSpecialized) \
                return WTF::untagNativeCodePtrImpl<tag>(ptr); \
            else \
                return JSC::untagJSCCodePtrImpl<tag, calleeType, callerType>(ptr); \
        } \
    };

#if COMPILER(MSVC)
#pragma warning(push)
#pragma warning(disable:4307)
#endif

FOR_EACH_JSC_PTRTAG(JSC_DECLARE_PTRTAG)

#if COMPILER(MSVC)
#pragma warning(pop)
#endif

template<PtrTag tag, PtrTagCalleeType calleeType, PtrTagCallerType callerType, typename PtrType>
ALWAYS_INLINE static PtrType tagJSCCodePtrImpl(PtrType ptr)
{
    static_assert(callerType == PtrTagCallerType::JIT);
    if constexpr (calleeType == PtrTagCalleeType::Native) {
        if constexpr (tag != JITToNativeGatePtrTag) {
            static_assert(tag == OperationPtrTag || tag == HostFunctionPtrTag);
            if constexpr (tag == OperationPtrTag)
                JITOperationList::instance().assertIsJITOperation(ptr);
            else if constexpr (tag == HostFunctionPtrTag)
                JITOperationList::instance().assertIsHostFunction(ptr);
        }
    }
    return WTF::tagNativeCodePtrImpl<tag>(ptr);
}

template<PtrTag tag, PtrTagCalleeType calleeType, PtrTagCallerType callerType, typename PtrType>
ALWAYS_INLINE static PtrType untagJSCCodePtrImpl(PtrType ptr)
{
    static_assert(callerType == PtrTagCallerType::JIT);
    if constexpr (calleeType == PtrTagCalleeType::Native) {
        if constexpr (tag != JITToNativeGatePtrTag) {
            static_assert(tag == OperationPtrTag || tag == HostFunctionPtrTag);
            if constexpr (tag == OperationPtrTag)
                JITOperationList::instance().assertIsJITOperation(ptr);
            else if constexpr (tag == HostFunctionPtrTag)
                JITOperationList::instance().assertIsHostFunction(ptr);
        }
    }
    return WTF::untagNativeCodePtrImpl<tag>(ptr);
}

#if CPU(ARM64E) && ENABLE(PTRTAG_DEBUGGING)
void initializePtrTagLookup();
#else
inline void initializePtrTagLookup() { }
#endif

} // namespace JSC
namespace WTF {

#if COMPILER(MSVC)
#pragma warning(push)
#pragma warning(disable:4307)
#endif

FOR_EACH_JSC_PTRTAG(JSC_DECLARE_PTRTAG_TRAIT)

#if COMPILER(MSVC)
#pragma warning(pop)
#endif

} // namespace WTF
