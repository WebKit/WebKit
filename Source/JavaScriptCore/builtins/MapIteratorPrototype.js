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
function mapIteratorNext(storage, entry, kind)
{
    "use strict";
    var value;
    var tableKeyValueEntry = @mapEntryNext(storage, entry);
    var done = tableKeyValueEntry.length === 0;
    if (!done) {
        var newTable = tableKeyValueEntry[0];
        if (newTable !== storage)
            @putMapIteratorInternalField(this, @mapIteratorFieldStorage, newTable);
        @putMapIteratorInternalField(this, @mapIteratorFieldEntry, tableKeyValueEntry[3] + 1);
        var key = tableKeyValueEntry[1];
        value = tableKeyValueEntry[2];
        if (kind === @iterationKindEntries)
            value = [ key, value ]
        else if (kind === @iterationKindKey)
            value = key;
    } else
        @putMapIteratorInternalField(this, @mapIteratorFieldStorage, @orderedHashTableSentinel);
    return { value, done };
}

function next()   
{
    "use strict";

    if (!@isMapIterator(this))
        @throwTypeError("%MapIteratorPrototype%.next requires that |this| be an Map Iterator instance");

    var storage = @getMapIteratorInternalField(this, @mapIteratorFieldStorage);
    var entry = @getMapIteratorInternalField(this, @mapIteratorFieldEntry);
    var kind = @getMapIteratorInternalField(this, @mapIteratorFieldKind);
    return @mapIteratorNext.@call(this, storage, entry, kind);
}
