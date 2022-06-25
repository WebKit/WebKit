// Copyright (C) 2018 Amal Hussein. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-atomics.notify
description: >
  Throws a TypeError if typedArray arg is not an Int32Array
info: |
  Atomics.notify( typedArray, index, count )

  1.Let buffer be ? ValidateSharedIntegerTypedArray(typedArray, true).
    ...
      5.If onlyInt32 is true, then
        If typeName is not "Int32Array", throw a TypeError exception.
features: [Atomics, Float32Array, Float64Array, Int8Array, TypedArray, Uint16Array, Uint8Array, Uint8ClampedArray]
---*/

const poisoned = {
  valueOf: function() {
    throw new Test262Error('should not evaluate this code');
  }
};

assert.throws(TypeError, function() {
  const view = new Float64Array(
    new SharedArrayBuffer(Float64Array.BYTES_PER_ELEMENT * 8)
  );
  Atomics.notify(view, poisoned, poisoned);
}, '`const view = new Float64Array( new SharedArrayBuffer(Float64Array.BYTES_PER_ELEMENT * 8) ); Atomics.notify(view, poisoned, poisoned)` throws TypeError');

assert.throws(TypeError, function() {
  const view = new Float32Array(
    new SharedArrayBuffer(Float32Array.BYTES_PER_ELEMENT * 4)
  );
  Atomics.notify(view, poisoned, poisoned);
}, '`const view = new Float32Array( new SharedArrayBuffer(Float32Array.BYTES_PER_ELEMENT * 4) ); Atomics.notify(view, poisoned, poisoned)` throws TypeError');

assert.throws(TypeError, function() {
  const view = new Int16Array(
    new SharedArrayBuffer(Int16Array.BYTES_PER_ELEMENT * 2)
  );
  Atomics.notify(view, poisoned, poisoned);
}, '`const view = new Int16Array( new SharedArrayBuffer(Int16Array.BYTES_PER_ELEMENT * 2) ); Atomics.notify(view, poisoned, poisoned)` throws TypeError');

assert.throws(TypeError, function() {
  const view = new Int8Array(
    new SharedArrayBuffer(Int8Array.BYTES_PER_ELEMENT)
  );
  Atomics.notify(view, poisoned, poisoned);
}, '`const view = new Int8Array( new SharedArrayBuffer(Int8Array.BYTES_PER_ELEMENT) ); Atomics.notify(view, poisoned, poisoned)` throws TypeError');

assert.throws(TypeError, function() {
  const view = new Uint32Array(
    new SharedArrayBuffer(Uint32Array.BYTES_PER_ELEMENT * 4)
  );
  Atomics.notify(new Uint32Array(),  poisoned, poisoned);
}, '`const view = new Uint32Array( new SharedArrayBuffer(Uint32Array.BYTES_PER_ELEMENT * 4) ); Atomics.notify(new Uint32Array(), poisoned, poisoned)` throws TypeError');

assert.throws(TypeError, function() {
  const view = new Uint16Array(
    new SharedArrayBuffer(Uint16Array.BYTES_PER_ELEMENT * 2)
  );
  Atomics.notify(view, poisoned, poisoned);
}, '`const view = new Uint16Array( new SharedArrayBuffer(Uint16Array.BYTES_PER_ELEMENT * 2) ); Atomics.notify(view, poisoned, poisoned)` throws TypeError');

assert.throws(TypeError, function() {
  const view = new Uint8Array(
    new SharedArrayBuffer(Uint8Array.BYTES_PER_ELEMENT)
  );
  Atomics.notify(view, poisoned, poisoned);
}, '`const view = new Uint8Array( new SharedArrayBuffer(Uint8Array.BYTES_PER_ELEMENT) ); Atomics.notify(view, poisoned, poisoned)` throws TypeError');

assert.throws(TypeError, function() {
  const view = new Uint8ClampedArray(
    new SharedArrayBuffer(Uint8ClampedArray.BYTES_PER_ELEMENT)
  );
  Atomics.notify(view, poisoned, poisoned);
}, '`const view = new Uint8ClampedArray( new SharedArrayBuffer(Uint8ClampedArray.BYTES_PER_ELEMENT) ); Atomics.notify(view, poisoned, poisoned)` throws TypeError');
