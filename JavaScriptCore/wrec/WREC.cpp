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

CompiledRegExp compileRegExp(Interpreter* interpreter, const UString& pattern, unsigned* numSubpatterns_ptr, const char** error_ptr, bool ignoreCase, bool multiline)
{
    // TODO: better error messages
    if (pattern.size() > MaxPatternSize) {
        *error_ptr = "regular expression too large";
        return 0;
    }

    X86Assembler assembler(interpreter->assemblerBuffer());
    Parser parser(pattern, ignoreCase, multiline, assembler);
    
    __ emitConvertToFastCall();
    // (0) Setup:
    //     Preserve regs & initialize outputRegister.
    __ pushl_r(Generator::outputRegister);
    __ pushl_r(Generator::currentValueRegister);
    // push pos onto the stack, both to preserve and as a parameter available to parseDisjunction
    __ pushl_r(Generator::currentPositionRegister);
    // load output pointer
    __ movl_mr(16
#if COMPILER(MSVC)
                    + 3 * sizeof(void*)
#endif
                    , X86::esp, Generator::outputRegister);
    
    // restart point on match fail.
    Generator::JmpDst nextLabel = __ label();

    // (1) Parse Disjunction:
    
    //     Parsing the disjunction should fully consume the pattern.
    JmpSrcVector failures;
    parser.parseDisjunction(failures);
    if (parser.isEndOfPattern()) {
        parser.setError(Parser::Error_malformedPattern);
    }
    if (parser.error()) {
        // TODO: better error messages
        *error_ptr = "TODO: better error messages";
        return 0;
    }

    // (2) Success:
    //     Set return value & pop registers from the stack.

    __ testl_rr(Generator::outputRegister, Generator::outputRegister);
    Generator::JmpSrc noOutput = __ emitUnlinkedJe();

    __ movl_rm(Generator::currentPositionRegister, 4, Generator::outputRegister);
    __ popl_r(X86::eax);
    __ movl_rm(X86::eax, Generator::outputRegister);
    __ popl_r(Generator::currentValueRegister);
    __ popl_r(Generator::outputRegister);
    __ ret();
    
    __ link(noOutput, __ label());
    
    __ popl_r(X86::eax);
    __ movl_rm(X86::eax, Generator::outputRegister);
    __ popl_r(Generator::currentValueRegister);
    __ popl_r(Generator::outputRegister);
    __ ret();

    // (3) Failure:
    //     All fails link to here.  Progress the start point & if it is within scope, loop.
    //     Otherwise, return fail value.
    Generator::JmpDst here = __ label();
    for (unsigned i = 0; i < failures.size(); ++i)
        __ link(failures[i], here);
    failures.clear();

    __ movl_mr(X86::esp, Generator::currentPositionRegister);
    __ addl_i8r(1, Generator::currentPositionRegister);
    __ movl_rm(Generator::currentPositionRegister, X86::esp);
    __ cmpl_rr(Generator::lengthRegister, Generator::currentPositionRegister);
    __ link(__ emitUnlinkedJle(), nextLabel);

    __ addl_i8r(4, X86::esp);

    __ movl_i32r(-1, X86::eax);
    __ popl_r(Generator::currentValueRegister);
    __ popl_r(Generator::outputRegister);
    __ ret();

    *numSubpatterns_ptr = parser.numSubpatterns();

    void* code = __ executableCopy();
    ASSERT(code);
    return reinterpret_cast<CompiledRegExp>(code);
}

} } // namespace JSC::WREC

#endif // ENABLE(WREC)
