/*
 * Copyright (C) 2015 Yusuke Suzuki <utatane.tea@gmail.com>.
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
function generatorResume(generator, sentValue, resumeMode)
{
    "use strict";

    // GeneratorState.
    const Completed = -1;
    const Executing = -2;

    // ResumeMode.
    const NormalMode = 0;
    const ReturnMode = 1;
    const ThrowMode = 2;

    let state = generator.@generatorState;
    let done = false;
    let value = @undefined;

    if (typeof state !== 'number')
        throw new @TypeError("|this| should be a generator");

    if (state === Executing)
        throw new @TypeError("Generator is executing");

    if (state === Completed) {
        if (resumeMode === ThrowMode)
            throw sentValue;

        done = true;
        if (resumeMode === ReturnMode)
            value = sentValue;
    } else {
        try {
            generator.@generatorState = Executing;
            value = generator.@generatorNext.@call(generator.@generatorThis, generator, state, sentValue, resumeMode);
            if (generator.@generatorState === Executing) {
                generator.@generatorState = Completed;
                done = true;
            }
        } catch (error) {
            generator.@generatorState = Completed;
            throw error;
        }
    }
    return { done, value };
}

function next(value)
{
    "use strict";

    return @generatorResume(this, value, /* NormalMode */ 0);
}

function return(value)
{
    "use strict";

    return @generatorResume(this, value, /* ReturnMode */ 1);
}

function throw(exception)
{
    "use strict";

    return @generatorResume(this, exception, /* ThrowMode */ 2);
}
