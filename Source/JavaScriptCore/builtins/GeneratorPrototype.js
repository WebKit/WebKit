/*
 * Copyright (C) 2015-2016 Yusuke Suzuki <utatane.tea@gmail.com>.
 * Copyright (C) 2016-2024 Apple Inc. All rights reserved.
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

// This implements https://tc39.es/ecma262/#sec-generatorresume and https://tc39.es/ecma262/#sec-generatorresumeabrupt
// with exception of [[GeneratorBrand]] check and handling of [[GeneratorState]] equal to *completed*.
@linkTimeConstant
function generatorResume(generator, state, value, resumeMode)
{
    "use strict";

    @assert(state !== @GeneratorStateCompleted);

    if (state === @GeneratorStateExecuting)
        @throwTypeError("Generator is executing");

    @putGeneratorInternalField(generator, @generatorFieldState, @GeneratorStateExecuting);

    try {
        var value = @getGeneratorInternalField(generator, @generatorFieldNext).@call(@getGeneratorInternalField(generator, @generatorFieldThis), generator, state, value, resumeMode, @getGeneratorInternalField(generator, @generatorFieldFrame));
    } catch (error) {
        @putGeneratorInternalField(generator, @generatorFieldState, @GeneratorStateCompleted);
        throw error;
    }

    var done = @getGeneratorInternalField(generator, @generatorFieldState) === @GeneratorStateExecuting;
    if (done)
        @putGeneratorInternalField(generator, @generatorFieldState, @GeneratorStateCompleted);

    return { value, done };
}

function next(value)
{
    "use strict";

    if (!@isGenerator(this))
        @throwTypeError("|this| should be a generator");

    var state = @getGeneratorInternalField(this, @generatorFieldState);
    if (state === @GeneratorStateCompleted)
        return { value: @undefined, done: true };

    return @generatorResume(this, state, value, @GeneratorResumeModeNormal);
}

function return(value)
{
    "use strict";

    if (!@isGenerator(this))
        @throwTypeError("|this| should be a generator");

    var state = @getGeneratorInternalField(this, @generatorFieldState);
    if (state === @GeneratorStateCompleted)
        return { value, done: true };

    return @generatorResume(this, state, value, @GeneratorResumeModeReturn);
}

function throw(exception)
{
    "use strict";

    if (!@isGenerator(this))
        @throwTypeError("|this| should be a generator");

    var state = @getGeneratorInternalField(this, @generatorFieldState);
    if (state === @GeneratorStateCompleted)
        throw exception;

    return @generatorResume(this, state, exception, @GeneratorResumeModeThrow);
}
