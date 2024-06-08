/*
 * Copyright (C) 2024 Sosuke Suzuki <aosukeke@gmail.com>.
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

// https://tc39.es/proposal-iterator-helpers/#sec-%iteratorhelperprototype%.next
function next() 
{
    "use strict";

    if (!@isObject(this))
        @throwTypeError("%IteratorHelperPrototype%.next requires that |this| be an Object");

    var state = @getByIdDirectPrivate(this, "generatorState");

    var executing = state === 0b10;
    if (executing)
        @throwTypeError("Iterator is executing");

    var completed = state === 0b11;
    if (completed)
        return { value: @undefined, done: true };

    // EXECUTING
    @putByIdDirectPrivate(this, "generatorState", 0b10);

    var underlyingIterator = @getByIdDirectPrivate(this, "underlyingIterator");
    var nextMethod = @getByIdDirectPrivate(this, "nextMethod");
    var step = nextMethod.@call(this);
    if (step.done)
        // COMPLETED
        @putByIdDirectPrivate(this, "generatorState", 0b11);
    return step;
}

// https://tc39.es/proposal-iterator-helpers/#sec-%iteratorhelperprototype%.return
function return()
{
    "use strict";

    if (!@isObject(this) || !@isObject(@getByIdDirectPrivate(this, "underlyingIterator")))
        @throwTypeError("%IteratorHelperPrototype%.return requires |this| to be an Object");

    var state = @getByIdDirectPrivate(this, "generatorState");
    var completed = state === 0b11;
    if (!completed) {
        // COMPLETED
        @putByIdDirectPrivate(this, "generatorState", 0b11);

        var underlyingIterator = @getByIdDirectPrivate(this, "underlyingIterator");
        var returnMethod = underlyingIterator.return;

        if (@isUndefinedOrNull(returnMethod))
            return { value: @undefined, done: true };

        returnMethod.@call(underlyingIterator);
    }

    return { value: @undefined, done: true };
}

@linkTimeConstant
@constructor
function IteratorHelper(underlyingIterator, nextMethod) {
    "use strict";

    @putByIdDirectPrivate(this, "underlyingIterator", underlyingIterator);
    @putByIdDirectPrivate(this, "nextMethod", nextMethod);
    // SUSPENDED-START 00
    // SUSPENDED-YIELD 01
    // EXECUTING       10
    // COMPLETED       11
    @putByIdDirectPrivate(this, "generatorState", 0b00);
}
