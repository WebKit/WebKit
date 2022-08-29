/*
 * Copyright (C) 2016 Caitlin Potter <caitp@igalia.com>.
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

@linkTimeConstant
function asyncFunctionResumeOnFulfilled(value, generator)
{
    "use strict";

    return @asyncFunctionResume(generator, value, @GeneratorResumeModeNormal);
}

@linkTimeConstant
function asyncFunctionResumeOnRejected(error, generator)
{
    "use strict";

    return @asyncFunctionResume(generator, error, @GeneratorResumeModeThrow);
}

@linkTimeConstant
function asyncFunctionResume(generator, sentValue, resumeMode)
{
    "use strict";

    var state = @getGeneratorInternalField(generator, @generatorFieldState);

    try {
        @putGeneratorInternalField(generator, @generatorFieldState, @GeneratorStateExecuting);
        var value = @getGeneratorInternalField(generator, @generatorFieldNext).@call(@getGeneratorInternalField(generator, @generatorFieldThis), generator, state, sentValue, resumeMode, @getGeneratorInternalField(generator, @generatorFieldFrame));
        if (@getGeneratorInternalField(generator, @generatorFieldState) === @GeneratorStateExecuting)
            return @resolvePromiseWithFirstResolvingFunctionCallCheck(@getGeneratorInternalField(generator, @generatorFieldContext), value);
    } catch (error) {
        return @rejectPromiseWithFirstResolvingFunctionCallCheck(@getGeneratorInternalField(generator, @generatorFieldContext), error);
    }

    return @resolveWithoutPromiseForAsyncAwait(value, @asyncFunctionResumeOnFulfilled, @asyncFunctionResumeOnRejected, generator);
}
