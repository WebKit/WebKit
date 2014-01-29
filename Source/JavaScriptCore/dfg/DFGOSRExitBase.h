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

#ifndef DFGOSRExitBase_h
#define DFGOSRExitBase_h

#include <wtf/Platform.h>

#if ENABLE(DFG_JIT)

#include "CodeOrigin.h"
#include "DFGExitProfile.h"

namespace JSC { namespace DFG {

struct BasicBlock;
struct Node;

// Provides for the OSR exit profiling functionality that is common between the DFG
// and the FTL.

struct OSRExitBase {
    OSRExitBase(ExitKind kind, CodeOrigin origin, CodeOrigin originForProfile)
        : m_kind(kind)
        , m_count(0)
        , m_codeOrigin(origin)
        , m_codeOriginForExitProfile(originForProfile)
    {
        ASSERT(m_codeOrigin.isSet());
        ASSERT(m_codeOriginForExitProfile.isSet());
    }
    
    ExitKind m_kind;
    uint32_t m_count;
    
    CodeOrigin m_codeOrigin;
    CodeOrigin m_codeOriginForExitProfile;

protected:
    bool considerAddingAsFrequentExitSite(CodeBlock* profiledCodeBlock, ExitingJITType jitType)
    {
        if (!m_count)
            return false;
        return considerAddingAsFrequentExitSiteSlow(profiledCodeBlock, jitType);
    }

private:
    bool considerAddingAsFrequentExitSiteSlow(CodeBlock* profiledCodeBlock, ExitingJITType);
};

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

#endif // DFGOSRExitBase_h

