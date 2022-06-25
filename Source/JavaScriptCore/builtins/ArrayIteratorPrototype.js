/*
 * Copyright (C) 2015 Yusuke Suzuki <utatane.tea@gmail.com>.
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

function next()
{
    "use strict";

    if (!@isArrayIterator(this))
        @throwTypeError("%ArrayIteratorPrototype%.next requires that |this| be an Array Iterator instance");

    var array = @getArrayIteratorInternalField(this, @arrayIteratorFieldIteratedObject);
    if (@isTypedArrayView(array) && @isDetached(array))
        @throwTypeError("Underlying ArrayBuffer has been detached from the view");

    var kind = @getArrayIteratorInternalField(this, @arrayIteratorFieldKind);
    return @arrayIteratorNextHelper.@call(this, array, kind);
}

// FIXME: We have to have this because we can't implement this all as one function and meet our inlining heuristics.
// Collectively, next and this have ~130 bytes of bytecodes but our limit is 120.
@globalPrivate
function arrayIteratorNextHelper(array, kind)
{
    "use strict";
    var done = true;
    var value;

    var index = @getArrayIteratorInternalField(this, @arrayIteratorFieldIndex);
    if (index !== -1) {
        var length = array.length >>> 0;
        if (index < length) {
            @putArrayIteratorInternalField(this, @arrayIteratorFieldIndex, index + 1);
            done = false;
            if (kind === @iterationKindKey)
                value = index;
            else {
                value = array[index];
                if (kind === @iterationKindEntries)
                    value = [index, value];
            }
        } else
            @putArrayIteratorInternalField(this, @arrayIteratorFieldIndex, -1);
    }

    return { value, done };
}
