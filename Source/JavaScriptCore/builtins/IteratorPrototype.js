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

@overriddenName="[Symbol.iterator]"
function symbolIteratorGetter()
{
    "use strict";

    return this;
}

// https://tc39.es/proposal-iterator-helpers/#sec-iteratorprototype.map
function map(mapper)
{
    "use strict";

    if (!@isObject(this))
        @throwTypeError("Iterator.prototype.map requires |this| be an object");

    if (!@isCallable(mapper))
        @throwTypeError("Iterator.prototype.map requires a function argument");

    var iterated = this;
    var nextMethod = iterated.next;
    var counter = 0;
    var closure = function () {
        var step = nextMethod.@call(iterated);
        if (step.done)
            return { done: true };
        var value = step.value;
        try {
            var mapped = mapper.@call(@undefined, value, counter++);
            // SUSPENDED-YEILD
            @putByIdDirectPrivate(this,  "generatorState",  0b01);
            return { done: false, value: mapped };
        } catch (error) {
            try {
                iterated.return();
            } catch {
                // Ignore the error
            }
            throw error;
        }
    };

    return new @IteratorHelper(iterated, closure);
}

// https://tc39.es/proposal-iterator-helpers/#sec-iteratorprototype.filter
function filter(predicate)
{
    "use strict";

    if (!@isObject(this))
        @throwTypeError("Iterator.prototype.filter requires |this| be an object");

    if (!@isCallable(predicate))
        @throwTypeError("Iterator.prototype.filter requires a function argument");

    var iterated = this;
    var nextMethod = iterated.next;
    var counter = 0;
    var closure = function () {
        while(true) {
            var step = nextMethod.@call(iterated);
            if (step.done)
                return { done: true };
            var value = step.value;
            try {
                var selected = predicate.@call(@undefined, value, counter++);
                // SUSPENDED-YEILD
                @putByIdDirectPrivate(this,  "generatorState",  0b01);
                if (selected)
                    return { done: false, value: value };
            } catch (error) {
                try {
                    iterated.return();
                } catch {
                    // Ignore the error
                }
                throw error;
            }
        }
    };

    return new @IteratorHelper(iterated, closure);
}
