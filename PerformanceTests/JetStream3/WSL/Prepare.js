/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
"use strict";

let prepare = (() => {
    let standardProgram;
    return function(origin, lineNumberOffset, text) {
        if (!standardProgram) {
            standardProgram = new Program();
            let firstLineOfStandardLibrary = 28; // See StandardLibrary.js.
            parse(standardProgram, "/internal/stdlib", "native", firstLineOfStandardLibrary - 1, standardLibrary);
        }
        
        let program = cloneProgram(standardProgram);
        if (arguments.length) {
            parse(program, origin, "user", lineNumberOffset, text);
            program = programWithUnnecessaryThingsRemoved(program);
        }
        
        foldConstexprs(program);
        let nameResolver = createNameResolver(program);
        resolveNamesInTypes(program, nameResolver);
        resolveNamesInProtocols(program, nameResolver);
        resolveTypeDefsInTypes(program);
        resolveTypeDefsInProtocols(program);
        checkRecursiveTypes(program);
        synthesizeStructAccessors(program);
        synthesizeEnumFunctions(program);
        resolveNamesInFunctions(program, nameResolver);
        resolveTypeDefsInFunctions(program);
        
        flattenProtocolExtends(program);
        check(program);
        checkLiteralTypes(program);
        resolveProperties(program);
        findHighZombies(program);
        checkLiteralTypes(program);
        checkProgramWrapped(program);
        checkReturns(program);
        checkUnreachableCode(program);
        checkLoops(program);
        checkRecursion(program);
        checkProgramWrapped(program);
        findHighZombies(program);
        inline(program);
        
        return program;
    };
})();

