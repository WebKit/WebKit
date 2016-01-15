/*
 * Copyright (C) 2015-2016 Apple Inc. All rights reserved.
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

#ifndef B3Compilation_h
#define B3Compilation_h

#if ENABLE(B3_JIT)

#include "MacroAssemblerCodeRef.h"
#include <wtf/FastMalloc.h>
#include <wtf/Noncopyable.h>

namespace JSC {

class VM;

namespace B3 {

class OpaqueByproducts;
class Procedure;

// This is a fool-proof API for compiling a Procedure to code and then running that code. You compile
// a Procedure using this API by doing:
//
// std::unique_ptr<Compilation> compilation = std::make_unique<Compilation>(vm, proc);
//
// Then you keep the Compilation object alive for as long as you want to be able to run the code. If
// this API feels too high-level, you can use B3::generate() directly.

class Compilation {
    WTF_MAKE_NONCOPYABLE(Compilation);
    WTF_MAKE_FAST_ALLOCATED;

public:
    JS_EXPORT_PRIVATE Compilation(VM&, Procedure&, unsigned optLevel = 1);

    // This constructor allows you to manually create a Compilation. It's currently only used by test
    // code. Probably best to keep it that way.
    JS_EXPORT_PRIVATE Compilation(MacroAssemblerCodeRef, std::unique_ptr<OpaqueByproducts>);
    
    JS_EXPORT_PRIVATE ~Compilation();

    MacroAssemblerCodePtr code() const { return m_codeRef.code(); }

private:
    MacroAssemblerCodeRef m_codeRef;
    std::unique_ptr<OpaqueByproducts> m_byproducts;
};

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)

#endif // B3Compilation_h

