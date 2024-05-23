/*
 * Copyright (C) 2017 Yusuke Suzuki <utatane.tea@gmail.com>.
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

// We keep this function small very carefully to encourage inlining.
@linkTimeConstant
function setIteratorNext(storage, entry, kind)
{
    "use strict";
    var value;
    var tableKeyEntry = @setEntryNext(storage, entry);
    var done = tableKeyEntry.length === 0;
    if (!done) {
        var newTable = tableKeyEntry[0];
        if (newTable !== storage)
            @putSetIteratorInternalField(this, @setIteratorFieldStorage, newTable);
        @putSetIteratorInternalField(this, @setIteratorFieldEntry, tableKeyEntry[2] + 1);
        value = tableKeyEntry[1];
        if (kind === @iterationKindEntries)
            value = [ value, value ]
    } else
        @putSetIteratorInternalField(this, @setIteratorFieldStorage, @orderedHashTableSentinel);
    return { value, done };
}

function next()
{
    "use strict";

    if (!@isSetIterator(this))
        @throwTypeError("%SetIteratorPrototype%.next requires that |this| be a Set Iterator instance");

    var storage = @getSetIteratorInternalField(this, @setIteratorFieldStorage);
    var entry = @getSetIteratorInternalField(this, @setIteratorFieldEntry);
    var kind = @getSetIteratorInternalField(this, @setIteratorFieldKind);
    return @setIteratorNext.@call(this, storage, entry, kind);
}
