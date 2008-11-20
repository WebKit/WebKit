/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
#include "WREC.h"

#if ENABLE(WREC)

#include "CharacterClassConstructor.h"
#include "Interpreter.h"
#include "WRECFunctors.h"
#include "WRECParser.h"
#include "pcre_internal.h"

#define __ assembler. 

using namespace WTF;

namespace JSC { namespace WREC {

// This limit comes from the limit set in PCRE
static const int MaxPatternSize = (1 << 16);

#if COMPILER(MSVC)
// MSVC has 3 extra arguments on the stack because it doesn't use the register calling convention.
static const int outputParameter = 16 + sizeof(const UChar*) + sizeof(unsigned) + sizeof(unsigned);
#else
static const int outputParameter = 16;
#endif
                    
CompiledRegExp compileRegExp(Interpreter* interpreter, const UString& pattern, unsigned* numSubpatterns_ptr, const char** error_ptr, bool ignoreCase, bool multiline)
{
    if (pattern.size() > MaxPatternSize) {
        *error_ptr = "Regular expression too large.";
        return 0;
    }

    X86Assembler assembler(interpreter->assemblerBuffer());
    Parser parser(pattern, ignoreCase, multiline, assembler);
    
    // (0) Setup:
    //     Preserve callee save regs and initialize output register.
    __ convertToFastCall();
    __ pushl_r(Generator::output);
    __ pushl_r(Generator::character);
    __ pushl_r(Generator::index); // load index into TOS as an argument to the top-level disjunction.
    __ movl_mr(outputParameter, X86::esp, Generator::output);

#ifndef NDEBUG
    // ASSERT that the output register is not null.
    __ testl_rr(Generator::output, Generator::output);
    X86Assembler::JmpSrc outputNotNull = __ jne();
    __ int3();
    __ link(outputNotNull, __ label());
#endif
    
    // Restart point for top-level disjunction.
    Generator::JmpDst beginPattern = __ label();

    // (1) Parse Pattern:

    JmpSrcVector failures;
    if (!parser.parsePattern(failures)) {
        *error_ptr = "Regular expression malformed.";
        return 0;
    }

    // (2) Success:
    //     Set return value & pop registers from the stack.

    __ popl_r(X86::eax);
    __ movl_rm(X86::eax, Generator::output); // match begin
    __ movl_rm(Generator::index, 4, Generator::output); // match end
    __ popl_r(Generator::character);
    __ popl_r(Generator::output);
    __ ret();
    
    // (3) Failure:
    //     All top-level failures link to here.
    Generator::JmpDst failure = __ label();
    for (unsigned i = 0; i < failures.size(); ++i)
        __ link(failures[i], failure);
    failures.clear();

    // Move to the next input character and try again.
    __ movl_mr(X86::esp, Generator::index);
    __ addl_i8r(1, Generator::index);
    __ movl_rm(Generator::index, X86::esp);
    __ cmpl_rr(Generator::length, Generator::index);
    __ link(__ jle(), beginPattern);

    // No more input characters: return failure.
    __ addl_i8r(4, X86::esp);
    __ movl_i32r(-1, X86::eax);
    __ popl_r(Generator::character);
    __ popl_r(Generator::output);
    __ ret();

    *numSubpatterns_ptr = parser.numSubpatterns();

    void* code = __ executableCopy();
    ASSERT(code);
    return reinterpret_cast<CompiledRegExp>(code);
}

} } // namespace JSC::WREC

#endif // ENABLE(WREC)
