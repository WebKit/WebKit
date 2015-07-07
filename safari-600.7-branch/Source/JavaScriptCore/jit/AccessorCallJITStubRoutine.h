/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef AccessorCallJITStubRoutine_h
#define AccessorCallJITStubRoutine_h

#if ENABLE(JIT)

#include "GCAwareJITStubRoutine.h"

namespace JSC {

struct CallLinkInfo;

// JIT stub routine for use by JavaScript accessors. These will be making a JS
// call that requires inline caching. 

class AccessorCallJITStubRoutine : public GCAwareJITStubRoutine {
public:
    AccessorCallJITStubRoutine(
        const MacroAssemblerCodeRef&, VM&, std::unique_ptr<CallLinkInfo>);
    
    virtual ~AccessorCallJITStubRoutine();
    
    virtual bool visitWeak(RepatchBuffer&) override;
    
private:
    std::unique_ptr<CallLinkInfo> m_callLinkInfo;
};

} // namespace JSC

#endif // ENABLE(JIT)

#endif // AccessorCallJITStubRoutine_h

