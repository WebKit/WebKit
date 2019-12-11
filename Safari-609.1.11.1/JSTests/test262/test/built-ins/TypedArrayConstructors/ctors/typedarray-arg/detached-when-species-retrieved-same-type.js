// Copyright (C) 2017 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-typedarray-typedarray
description: >
    When a TypedArray is created from another TypedArray with the same element-type
    and SpeciesConstructor detaches the source buffer, AllocateArrayBuffer is still
    executed.
info: |
    22.2.4.3 TypedArray ( typedArray )

    ...
    16. If IsSharedArrayBuffer(srcData) is false, then
        a. Let bufferConstructor be ? SpeciesConstructor(srcData, %ArrayBuffer%).
    ...
    18. If SameValue(elementType, srcType) is true, then
        a. Let data be ? CloneArrayBuffer(srcData, srcByteOffset, byteLength, bufferConstructor).
    ...

    24.1.1.4 CloneArrayBuffer ( srcBuffer, srcByteOffset, srcLength, cloneConstructor )

    ...
    3. Let targetBuffer be ? AllocateArrayBuffer(cloneConstructor, srcLength).
    4. If IsDetachedBuffer(srcBuffer) is true, throw a TypeError exception.
    ...

    24.1.1.1 AllocateArrayBuffer ( constructor, byteLength )

    1. Let obj be ? OrdinaryCreateFromConstructor(constructor, "%ArrayBufferPrototype%",
       « [[ArrayBufferData]], [[ArrayBufferByteLength]] »).
    ...
includes: [testTypedArray.js, detachArrayBuffer.js]
features: [TypedArray, Symbol.species]
---*/

testWithTypedArrayConstructors(function(TA) {
    var speciesCallCount = 0;
    var bufferConstructor = Object.defineProperty({}, Symbol.species, {
        get: function() {
            speciesCallCount += 1;
            $DETACHBUFFER(ta.buffer);
            return speciesConstructor;
        }
    });

    var prototypeCallCount = 0;
    var speciesConstructor = Object.defineProperty(function(){}.bind(), "prototype", {
        get: function() {
            prototypeCallCount += 1;
            return null;
        }
    });

    var ta = new TA(0);
    ta.buffer.constructor = bufferConstructor;

    assert.throws(TypeError, function() {
        new TA(ta);
    }, "TypeError thrown for detached source buffer");

    assert.sameValue(speciesCallCount, 1, "@@species getter called once");
    assert.sameValue(prototypeCallCount, 1, "prototype getter called once");
});
