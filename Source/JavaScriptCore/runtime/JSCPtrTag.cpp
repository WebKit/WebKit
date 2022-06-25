/*
 * Copyright (C) 2018-2022 Apple Inc. All rights reserved.
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
#include "JSCPtrTag.h"

#include "JSCConfig.h"

#if ENABLE(JIT_CAGE)
#include <machine/cpu_capabilities.h>
#include <sys/mman.h>
#include <sys/types.h>
#endif

namespace JSC {

#if CPU(ARM64E) && (ENABLE(PTRTAG_DEBUGGING) || ENABLE(DISASSEMBLER))

const char* ptrTagName(PtrTag tag)
{
#define RETURN_PTRTAG_NAME(_tagName, calleeType, callerType) case _tagName: return #_tagName;
    switch (static_cast<unsigned>(tag)) {
        FOR_EACH_JSC_PTRTAG(RETURN_PTRTAG_NAME)
    }
#undef RETURN_PTRTAG_NAME
    return nullptr; // Matching tag not found.
}

#if ENABLE(PTRTAG_DEBUGGING)
static const char* tagForPtr(const void* ptr)
{
#define RETURN_NAME_IF_TAG_MATCHES(tag, calleeType, callerType) \
    if (callerType != PtrTagCallerType::JIT || calleeType != PtrTagCalleeType::Native) { \
        if (ptr == WTF::tagCodePtrImpl<WTF::PtrTagAction::NoAssert, JSC::tag>(removeCodePtrTag(ptr))) \
            return #tag; \
    }
    FOR_EACH_JSC_PTRTAG(RETURN_NAME_IF_TAG_MATCHES)
#undef RETURN_NAME_IF_TAG_MATCHES
    return nullptr; // Matching tag not found.
}

void initializePtrTagLookup()
{
    WTF::PtrTagLookup& lookup = g_jscConfig.ptrTagLookupRecord;
    lookup.initialize(tagForPtr, ptrTagName);
    WTF::registerPtrTagLookup(&lookup);
}
#endif // ENABLE(PTRTAG_DEBUGGING)
#endif // CPU(ARM64E) && (ENABLE(PTRTAG_DEBUGGING) || ENABLE(DISASSEMBLER))

#if CPU(ARM64E)

PtrTagCallerType callerType(PtrTag tag)
{
#define RETURN_PTRTAG_TYPE(_tagName, calleeType, callerType) case _tagName: return callerType;
    switch (tag) {
        FOR_EACH_JSC_PTRTAG(RETURN_PTRTAG_TYPE)
    default:
        return PtrTagCallerType::Native;
    }
#undef RETURN_PTRTAG_TYPE
    return PtrTagCallerType::Native;
}

PtrTagCalleeType calleeType(PtrTag tag)
{
#define RETURN_PTRTAG_TYPE(_tagName, calleeType, callerType) case _tagName: return calleeType;
    switch (tag) {
        FOR_EACH_JSC_PTRTAG(RETURN_PTRTAG_TYPE)
    default:
        return PtrTagCalleeType::Native;
    }
#undef RETURN_PTRTAG_TYPE
    return PtrTagCalleeType::Native;
}

#endif

} // namespace JSC
