/*
 * Copyright (C) 2015-2016 Yusuke Suzuki <utatane.tea@gmail.com>.
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

// 25.3.3.3 GeneratorResume ( generator, value )
// 25.3.3.4 GeneratorResumeAbrupt(generator, abruptCompletion)
@globalPrivate
function generatorResume(generator, state, generatorThis, sentValue, value, resumeMode)
{
    "use strict";

    var done = state === @GeneratorStateCompleted;
    if (!done) {
        try {
            @putByIdDirectPrivate(generator, "generatorState", @GeneratorStateExecuting);
            value = @getByIdDirectPrivate(generator, "generatorNext").@call(generatorThis, generator, state, sentValue, resumeMode, @getByIdDirectPrivate(generator, "generatorFrame"));
            if (@getByIdDirectPrivate(generator, "generatorState") === @GeneratorStateExecuting) {
                @putByIdDirectPrivate(generator, "generatorState", @GeneratorStateCompleted);
                done = true;
            }
        } catch (error) {
            @putByIdDirectPrivate(generator, "generatorState", @GeneratorStateCompleted);
            throw error;
        }
    }
    return { value, done };
}

function next(value)
{
    "use strict";

    var state = @getByIdDirectPrivate(this, "generatorState");
    if (typeof state !== "number")
        @throwTypeError("|this| should be a generator");

    if (state === @GeneratorStateExecuting)
        @throwTypeError("Generator is executing");

    return @generatorResume(this, state, @getByIdDirectPrivate(this, "generatorThis"), value, @undefined, @GeneratorResumeModeNormal);
}

function return(value)
{
    "use strict";

    var state = @getByIdDirectPrivate(this, "generatorState");
    if (typeof state !== "number")
        @throwTypeError("|this| should be a generator");

    if (state === @GeneratorStateExecuting)
        @throwTypeError("Generator is executing");

    return @generatorResume(this, state, @getByIdDirectPrivate(this, "generatorThis"), value, value, @GeneratorResumeModeReturn);
}

function throw(exception)
{
    "use strict";

    var state = @getByIdDirectPrivate(this, "generatorState");
    if (typeof state !== "number")
        @throwTypeError("|this| should be a generator");

    if (state === @GeneratorStateExecuting)
        @throwTypeError("Generator is executing");

    if (state === @GeneratorStateCompleted)
        throw exception;

    return @generatorResume(this, state, @getByIdDirectPrivate(this, "generatorThis"), exception, @undefined, @GeneratorResumeModeThrow);
}
