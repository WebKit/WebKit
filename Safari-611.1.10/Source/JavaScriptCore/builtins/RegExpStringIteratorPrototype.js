/*
 * Copyright (C) 2019 Alexey Shvayka <shvaikalesh@gmail.com>.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

function next()
{
    "use strict";

    if (!@isObject(this))
        @throwTypeError("%RegExpStringIteratorPrototype%.next requires |this| to be an Object");

    var done = @getByIdDirectPrivate(this, "regExpStringIteratorDone");
    if (done === @undefined)
        @throwTypeError("%RegExpStringIteratorPrototype%.next requires |this| to be an RegExp String Iterator instance");

    if (done)
        return { value: @undefined, done: true };

    var regExp = @getByIdDirectPrivate(this, "regExpStringIteratorRegExp");
    var string = @getByIdDirectPrivate(this, "regExpStringIteratorString");
    var global = @getByIdDirectPrivate(this, "regExpStringIteratorGlobal");
    var fullUnicode = @getByIdDirectPrivate(this, "regExpStringIteratorUnicode");
    var match = @regExpExec(regExp, string);
    if (match === null) {
        @putByIdDirectPrivate(this, "regExpStringIteratorDone", true);
        return { value: @undefined, done: true };
    }

    if (global) {
        var matchStr = @toString(match[0]);
        if (matchStr === "") {
            var thisIndex = @toLength(regExp.lastIndex);
            regExp.lastIndex = @advanceStringIndex(string, thisIndex, fullUnicode);
        }
    } else
        @putByIdDirectPrivate(this, "regExpStringIteratorDone", true);

    return { value: match, done: false };
}
