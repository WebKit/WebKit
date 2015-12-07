/*
 * Copyright (C) 2013-2015 Apple Inc. All rights reserved.
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

#ifndef FTLJSTailCall_h
#define FTLJSTailCall_h

#include "DFGCommon.h"

#if ENABLE(FTL_JIT) && !FTL_USES_B3

#include "B3PatchpointValue.h"
#include "DFGCommon.h"
#include "FTLExitValue.h"
#include "FTLJSCallBase.h"
#include "FTLStackmapArgumentList.h"

namespace JSC {

namespace DFG {
struct Node;
}

namespace FTL {

class JSTailCall : public JSCallBase {
public:
    JSTailCall(
        unsigned stackmapID,
        DFG::Node*, const Vector<ExitValue>& arguments);

    void emit(JITCode&, CCallHelpers&);

    unsigned stackmapID() const { return m_stackmapID; }

    unsigned estimatedSize() const { return m_estimatedSize; }

    unsigned numArguments() const { return m_arguments.size(); }

    bool operator<(const JSTailCall& other) const
    {
        return m_instructionOffset < other.m_instructionOffset;
    }
    
private:
    unsigned m_stackmapID;
    Vector<ExitValue> m_arguments;
    unsigned m_estimatedSize;

public:
    uint32_t m_instructionOffset;
};

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT) && !FTL_USES_B3

#endif // FTLJSTailCall_h

