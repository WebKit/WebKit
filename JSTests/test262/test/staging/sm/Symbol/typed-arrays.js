/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*---
includes: [sm/non262.js, sm/non262-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
// Symbol-to-number type conversions involving typed arrays.

for (var T of [Uint8Array, Uint8ClampedArray, Int16Array, Float32Array]) {
    // Typed array constructors convert symbols using ToNumber(), which throws.
    assertThrowsInstanceOf(() => new T([Symbol("a")]), TypeError);

    // Assignment does the same.
    var arr = new T([1]);
    assertThrowsInstanceOf(() => { arr[0] = Symbol.iterator; }, TypeError);
    assert.sameValue(arr[0], 1);
}

