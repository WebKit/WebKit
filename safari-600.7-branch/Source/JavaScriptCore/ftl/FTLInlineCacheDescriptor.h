/*
 * Copyright (C) 2013, 2014 Apple Inc. All rights reserved.
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

#ifndef FTLInlineCacheDescriptor_h
#define FTLInlineCacheDescriptor_h

#if ENABLE(FTL_JIT)

#include "CodeOrigin.h"
#include "JITInlineCacheGenerator.h"
#include "MacroAssembler.h"
#include <wtf/text/StringImpl.h>

namespace JSC { namespace FTL {

class InlineCacheDescriptor {
public:
    InlineCacheDescriptor() { }
    
    InlineCacheDescriptor(unsigned stackmapID, CodeOrigin codeOrigin, StringImpl* uid)
        : m_stackmapID(stackmapID)
        , m_codeOrigin(codeOrigin)
        , m_uid(uid)
    {
    }
    
    unsigned stackmapID() const { return m_stackmapID; }
    CodeOrigin codeOrigin() const { return m_codeOrigin; }
    StringImpl* uid() const { return m_uid; }
    
private:
    unsigned m_stackmapID;
    CodeOrigin m_codeOrigin;
    StringImpl* m_uid;
    
public:
    Vector<MacroAssembler::Jump> m_slowPathDone;
};

class GetByIdDescriptor : public InlineCacheDescriptor {
public:
    GetByIdDescriptor() { }
    
    GetByIdDescriptor(unsigned stackmapID, CodeOrigin codeOrigin, StringImpl* uid)
        : InlineCacheDescriptor(stackmapID, codeOrigin, uid)
    {
    }
    
    Vector<JITGetByIdGenerator> m_generators;
};

class PutByIdDescriptor : public InlineCacheDescriptor {
public:
    PutByIdDescriptor() { }
    
    PutByIdDescriptor(
        unsigned stackmapID, CodeOrigin codeOrigin, StringImpl* uid,
        ECMAMode ecmaMode, PutKind putKind)
        : InlineCacheDescriptor(stackmapID, codeOrigin, uid)
        , m_ecmaMode(ecmaMode)
        , m_putKind(putKind)
    {
    }
    
    Vector<JITPutByIdGenerator> m_generators;
    
    ECMAMode ecmaMode() const { return m_ecmaMode; }
    PutKind putKind() const { return m_putKind; }

private:
    ECMAMode m_ecmaMode;
    PutKind m_putKind;
};

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

#endif // FTLInlineCacheDescriptor_h

