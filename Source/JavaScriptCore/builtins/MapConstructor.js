/*
 * Copyright (C) 2023 Devin Rousso <webkit@devinrousso.com>.
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

function groupBy(items, callback)
{
    "use strict";

    if (@isUndefinedOrNull(items))
        @throwTypeError("Map.groupBy requires that the first argument not be null or undefined");

    if (!@isCallable(callback))
        @throwTypeError("Map.groupBy requires that the second argument must be a function");

    var iteratorMethod = items.@@iterator;
    if (!@isCallable(iteratorMethod))
        @throwTypeError("Map.groupBy requires that the property of the first argument, items[Symbol.iterator] be a function");

    var groups = new @Map;
    var k = 0;

    var iterator = iteratorMethod.@call(items);
    // Since for-of loop once more looks up the @@iterator property of a given iterable,
    // it could be observable if the user defines a getter for @@iterator.
    // To avoid this situation, we define a wrapper object that @@iterator just returns a given iterator.
    var wrapper = {
        @@iterator: function () { return iterator; }
    };
    for (var item of wrapper) {
        var key = callback.@call(@undefined, item, k);
        var group = groups.@get(key);
        if (!group) {
            group = [];
            groups.@set(key, group);
        }
        @putByValDirect(group, group.length, item);
        ++k;
    }
    return groups;
}
