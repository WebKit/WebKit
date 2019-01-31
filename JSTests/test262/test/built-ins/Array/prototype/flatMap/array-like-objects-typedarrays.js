// Copyright (C) 2018 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-array.prototype.flatMap
description: >
  array-like objects can be flattened (typedArrays)
info: |
  Array.prototype.flatMap ( mapperFunction [ , thisArg ] )

  1. Let O be ? ToObject(this value).
  2. Let sourceLen be ? ToLength(? Get(O, "length")).
  ...
  5. Let A be ? ArraySpeciesCreate(O, 0).
  ...

  ArraySpeciesCreate ( originalArray, length )

  3. Let isArray be ? IsArray(originalArray).
  4. If isArray is false, return ? ArrayCreate(length).
  5. Let C be ? Get(originalArray, "constructor").
includes: [compareArray.js]
features: [Array.prototype.flatMap, Int32Array]
---*/

function same(e) {
  return e;
}

var ta;
var actual;

ta = new Int32Array([1, 0, 42]);

Object.defineProperty(ta, 'constructor', {
  get() { throw "it should not object the typedarray ctor"; }
});
actual = [].flatMap.call(ta, same);
assert.compareArray(actual, [1, 0, 42], 'compare returned array');
assert.sameValue(Object.getPrototypeOf(actual), Array.prototype, 'returned object is an array #1');
assert.sameValue(actual instanceof Int32Array, false, 'returned object is not an instance of Int32Array #1');

ta = new Int32Array(0);

Object.defineProperty(ta, 'constructor', {
  get() { throw "it should not object the typedarray ctor"; }
});
actual = [].flatMap.call(ta, same);
assert.compareArray(actual, [], 'compare returned empty array');
assert.sameValue(Object.getPrototypeOf(actual), Array.prototype, 'returned object is an array #2');
assert.sameValue(actual instanceof Int32Array, false, 'returned object is not an instance of Int32Array #2');
