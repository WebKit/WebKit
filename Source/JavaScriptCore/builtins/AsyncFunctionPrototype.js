/*
 * Copyright (C) 2016 Caitlin Potter <caitp@igalia.com>.
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

function asyncFunctionResume(generator, sentValue, resumeMode)
{
    "use strict";
    let state = generator.@generatorState;
    let value = @undefined;

    if (state === @GeneratorStateCompleted || (resumeMode !== @GeneratorResumeModeNormal && resumeMode !== @GeneratorResumeModeThrow))
        throw new @TypeError("Async function illegally resumed");

    try {
        generator.@generatorState = @GeneratorStateExecuting;
        value = generator.@generatorNext.@call(generator.@generatorThis, generator, state, sentValue, resumeMode);
        if (generator.@generatorState === @GeneratorStateExecuting) {
            generator.@generatorState = @GeneratorStateCompleted;
            return @Promise.@resolve(value);
        }
    } catch (error) {
        generator.@generatorState = @GeneratorStateCompleted;
        return @Promise.@reject(error);
    }

    return @Promise.@resolve(value).@then(
        function(value) { return @asyncFunctionResume(generator, value, @GeneratorResumeModeNormal); },
        function(error) { return @asyncFunctionResume(generator, error, @GeneratorResumeModeThrow); });
}
