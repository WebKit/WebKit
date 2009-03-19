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
#include "JSGlobalObject.h"
#include "RegisterFile.h"
#include "WRECFunctors.h"
#include "WRECParser.h"
#include "pcre_internal.h"

using namespace WTF;

namespace JSC { namespace WREC {

CompiledRegExp Generator::compileRegExp(JSGlobalData* globalData, const UString& pattern, unsigned* numSubpatterns_ptr, const char** error_ptr, RefPtr<ExecutablePool>& pool, bool ignoreCase, bool multiline)
{
    if (pattern.size() > MAX_PATTERN_SIZE) {
        *error_ptr = "regular expression too large";
        return 0;
    }

    Parser parser(pattern, ignoreCase, multiline);
    Generator& generator = parser.generator();
    MacroAssembler::JumpList failures;
    MacroAssembler::Jump endOfInput;

    generator.generateEnter();
    generator.generateSaveIndex();

    Label beginPattern(&generator);
    parser.parsePattern(failures);
    generator.generateReturnSuccess();

    failures.link(&generator);
    generator.generateIncrementIndex(&endOfInput);
    parser.parsePattern(failures);
    generator.generateReturnSuccess();

    failures.link(&generator);
    generator.generateIncrementIndex();
    generator.generateJumpIfNotEndOfInput(beginPattern);
    
    endOfInput.link(&generator);
    generator.generateReturnFailure();

    if (parser.error()) {
        *error_ptr = parser.syntaxError(); // NULL in the case of patterns that WREC doesn't support yet.
        return 0;
    }

    *numSubpatterns_ptr = parser.numSubpatterns();
    pool = globalData->executableAllocator.poolForSize(generator.size());
    return reinterpret_cast<CompiledRegExp>(generator.copyCode(pool.get()));
}

} } // namespace JSC::WREC

#endif // ENABLE(WREC)
