/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#ifndef LLIntData_h
#define LLIntData_h

#include "Opcode.h"
#include <wtf/Platform.h>

namespace JSC {

class JSGlobalData;
struct Instruction;

namespace LLInt {

#if ENABLE(LLINT)
class Data {
public:
    Data();
    ~Data();
    
    void performAssertions(JSGlobalData&);
    
    Instruction* exceptionInstructions()
    {
        return m_exceptionInstructions;
    }
    
    Opcode* opcodeMap()
    {
        return m_opcodeMap;
    }
private:
    Instruction* m_exceptionInstructions;
    Opcode* m_opcodeMap;
};
#else // ENABLE(LLINT)

#if COMPILER(CLANG)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
#endif

class Data {
public:
    void performAssertions(JSGlobalData&) { }

    Instruction* exceptionInstructions()
    {
        ASSERT_NOT_REACHED();
        return 0;
    }
    
    Opcode* opcodeMap()
    {
        ASSERT_NOT_REACHED();
        return 0;
    }
};

#if COMPILER(CLANG)
#pragma clang diagnostic pop
#endif

#endif // ENABLE(LLINT)

} } // namespace JSC::LLInt

#endif // LLIntData_h

