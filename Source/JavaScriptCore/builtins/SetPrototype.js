/*
 * Copyright (C) 2016 Yusuke Suzuki <utatane.tea@gmail.com>.
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

function forEach(callback /*, thisArg */)
{
    "use strict";

    if (!@isSet(this))
        @throwTypeError("Set operation called on non-Set object");

    if (!@isCallable(callback))
        @throwTypeError("Set.prototype.forEach callback must be a function");

    var thisArg = @argument(1);
    var bucket = @setBucketHead(this);

    do {
        bucket = @setBucketNext(bucket);
        if (bucket === @sentinelSetBucket)
            break;
        var key = @setBucketKey(bucket);
        callback.@call(thisArg, key, key, this);
    } while (true);
}

// https://tc39.es/proposal-set-methods/#sec-getsetrecord (steps 1-7)
@linkTimeConstant
@alwaysInline
function getSetSizeAsInt(other)
{
    if (!@isObject(other))
        @throwTypeError("Set operation expects first argument to be an object");

    var size = @toNumber(other.size);
    if (size !== size) // is NaN?
        @throwTypeError("Set operation expects first argument to have non-NaN 'size' property");

    var sizeInt = @toIntegerOrInfinity(size);
    if (sizeInt < 0)
        @throwRangeError("Set operation expects first argument to have non-negative 'size' property");

    return sizeInt;
}

function union(other)
{
    "use strict";

    if (!@isSet(this))
        @throwTypeError("Set operation called on non-Set object");

    // Get Set Record
    var size = @getSetSizeAsInt(other); // unused but @getSetSizeAsInt call is observable

    var has = other.has;
    if (!@isCallable(has))
        @throwTypeError("Set.prototype.union expects other.has to be callable");

    var keys = other.keys;
    if (!@isCallable(keys))
        @throwTypeError("Set.prototype.union expects other.keys to be callable");

    var iterator = keys.@call(other, keys);
    var wrapper = {
        @@iterator: function () { return iterator; }
    };

    var result = @setClone(this);
    for (var key of wrapper)
        result.@add(key);

    return result;
}

function intersection(other)
{
    "use strict";

    if (!@isSet(this))
        @throwTypeError("Set operation called on non-Set object");

    // Get Set Record
    var size = @getSetSizeAsInt(other);

    var has = other.has;
    if (!@isCallable(has))
        @throwTypeError("Set.prototype.intersection expects other.has to be callable");

    var keys = other.keys;
    if (!@isCallable(keys))
        @throwTypeError("Set.prototype.intersection expects other.keys to be callable");

    var result = new @Set();
    if (this.@size <= size) {
        var bucket = @setBucketHead(this);

        do {
            bucket = @setBucketNext(bucket);
            if (bucket === @sentinelSetBucket)
                break;
            var key = @setBucketKey(bucket);
            if (has.@call(other, key))
                result.@add(key);
        } while (true);
    } else {
        var iterator = keys.@call(other, keys);
        var wrapper = {
            @@iterator: function () { return iterator; }
        };

        for (var key of wrapper) {
            if (this.@has(key))
                result.@add(key);
        }
    }

    return result;
}

function difference(other)
{
    "use strict";

    if (!@isSet(this))
        @throwTypeError("Set operation called on non-Set object");

    // Get Set Record
    var size = @getSetSizeAsInt(other);

    var has = other.has;
    if (!@isCallable(has))
        @throwTypeError("Set.prototype.difference expects other.has to be callable");

    var keys = other.keys;
    if (!@isCallable(keys))
        @throwTypeError("Set.prototype.difference expects other.keys to be callable");

    var result = @setClone(this);
    if (this.@size <= size) {
        var bucket = @setBucketHead(this);

        while (true) {
            bucket = @setBucketNext(bucket);
            if (bucket === @sentinelSetBucket)
                break;
            var key = @setBucketKey(bucket);
            if (has.@call(other, key))
                result.@delete(key);
        }
    } else {
        var iterator = keys.@call(other, keys);
        var wrapper = {
            @@iterator: function () { return iterator; }
        };

        for (var key of wrapper) {
            if (this.@has(key))
                result.@delete(key);
        }
    }

    return result;
}

function symmetricDifference(other)
{
    "use strict";

    if (!@isSet(this))
        @throwTypeError("Set operation called on non-Set object");

    // Get Set Record
    var size = @getSetSizeAsInt(other); // unused but @getSetSizeAsInt call is observable

    var has = other.has;
    if (!@isCallable(has))
        @throwTypeError("Set.prototype.symmetricDifference expects other.has to be callable");

    var keys = other.keys;
    if (!@isCallable(keys))
        @throwTypeError("Set.prototype.symmetricDifference expects other.keys to be callable");

    var iterator = keys.@call(other, keys);
    var wrapper = {
        @@iterator: function () { return iterator; }
    };

    var result = @setClone(this);
    for (var key of wrapper) {
        if (result.@has(key))
            result.@delete(key);
        else
            result.@add(key);
    }

    return result;
}

function isSubsetOf(other)
{
    "use strict";

    if (!@isSet(this))
        @throwTypeError("Set operation called on non-Set object");

    // Get Set Record
    var size = @getSetSizeAsInt(other);

    var has = other.has;
    if (!@isCallable(has))
        @throwTypeError("Set.prototype.isSubsetOf expects other.has to be callable");

    var keys = other.keys;
    if (!@isCallable(keys))
        @throwTypeError("Set.prototype.isSubsetOf expects other.keys to be callable");

    if (this.@size > size)
        return false;

    var bucket = @setBucketHead(this);

    do {
        bucket = @setBucketNext(bucket);
        if (bucket === @sentinelSetBucket)
            break;
        var key = @setBucketKey(bucket);
        if (!has.@call(other, key))
            return false;
    } while (true);

    return true;
}

function isSupersetOf(other)
{
    "use strict";

    if (!@isSet(this))
        @throwTypeError("Set operation called on non-Set object");

    // Get Set Record
    var size = @getSetSizeAsInt(other);

    var has = other.has;
    if (!@isCallable(has))
        @throwTypeError("Set.prototype.isSupersetOf expects other.has to be callable");

    var keys = other.keys;
    if (!@isCallable(keys))
        @throwTypeError("Set.prototype.isSupersetOf expects other.keys to be callable");

    if (this.@size < size)
        return false;

    var iterator = keys.@call(other, keys);
    var wrapper = {
        @@iterator: function () { return iterator; }
    };

    for (var key of wrapper) {
        if (!this.@has(key))
            return false;
    }
    return true;
}

function isDisjointFrom(other)
{
    "use strict";

    if (!@isSet(this))
        @throwTypeError("Set operation called on non-Set object");

    // Get Set Record
    var size = @getSetSizeAsInt(other);

    var has = other.has;
    if (!@isCallable(has))
        @throwTypeError("Set.prototype.isDisjointFrom expects other.has to be callable");

    var keys = other.keys;
    if (!@isCallable(keys))
        @throwTypeError("Set.prototype.isDisjointFrom expects other.keys to be callable");

    if (this.@size <= size) {
        var bucket = @setBucketHead(this);

        do {
            bucket = @setBucketNext(bucket);
            if (bucket === @sentinelSetBucket)
                break;
            var key = @setBucketKey(bucket);
            if (has.@call(other, key))
                return false;
        } while (true);
    } else {
        var iterator = keys.@call(other, keys);
        var wrapper = {
            @@iterator: function () { return iterator; }
        };

        for (var key of wrapper) {
            if (this.@has(key))
                return false;
        }
    }

    return true;
}
