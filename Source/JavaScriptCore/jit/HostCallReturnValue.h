/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#ifndef HostCallReturnValue_h
#define HostCallReturnValue_h

#include "JSValue.h"
#include "MacroAssemblerCodeRef.h"
#include <wtf/Platform.h>

// Unfortunately this only works on GCC-like compilers. And it's currently only used
// by LLInt and DFG, which also are restricted to GCC-like compilers. We should
// probably fix that at some point.
#if COMPILER(GCC)

#if CALLING_CONVENTION_IS_STDCALL
#define HOST_CALL_RETURN_VALUE_OPTION CDECL
#else
#define HOST_CALL_RETURN_VALUE_OPTION
#endif

namespace JSC {

extern "C" EncodedJSValue HOST_CALL_RETURN_VALUE_OPTION getHostCallReturnValue();

// This is a public declaration only to convince CLANG not to elide it.
extern "C" EncodedJSValue HOST_CALL_RETURN_VALUE_OPTION getHostCallReturnValueWithExecState(ExecState*);

inline void initializeHostCallReturnValue()
{
    getHostCallReturnValueWithExecState(0);
}

}

#else // COMPILER(GCC)

namespace JSC {
inline void initializeHostCallReturnValue() { }
}

#endif // COMPILER(GCC)

#endif // HostCallReturnValue_h

