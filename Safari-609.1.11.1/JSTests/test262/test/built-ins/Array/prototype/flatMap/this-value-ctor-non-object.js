// Copyright (C) 2018 Shilpi Jain and Michael Ficarra. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-array.prototype.flatMap
description: >
  Assert behavior if this value has a custom non-object constructor property
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
  6. If IsConstructor(C) is true, then
    a. Let thisRealm be the current Realm Record.
    b. Let realmC be ? GetFunctionRealm(C).
    c. If thisRealm and realmC are not the same Realm Record, then
      i. If SameValue(C, realmC.[[Intrinsics]].[[%Array%]]) is true, set C to undefined.
  7. If Type(C) is Object, then
    a. Set C to ? Get(C, @@species).
    b. If C is null, set C to undefined.
  8. If C is undefined, return ? ArrayCreate(length).
  9. If IsConstructor(C) is false, throw a TypeError exception.
features: [Array.prototype.flatMap, Symbol]
includes: [compareArray.js]
---*/

assert.sameValue(typeof Array.prototype.flatMap, 'function');

var a = [];
var mapperFn = function() {};

a.constructor = null;
assert.throws(TypeError, function() {
  a.flatMap(mapperFn);
}, 'null value');

a = [];
a.constructor = 1;
assert.throws(TypeError, function() {
  a.flatMap(mapperFn);
}, 'number value');

a = [];
a.constructor = 'string';
assert.throws(TypeError, function() {
  a.flatMap(mapperFn);
}, 'string value');

a = [];
a.constructor = true;
assert.throws(TypeError, function() {
  a.flatMap(mapperFn);
}, 'boolean value');

a = [];
a.constructor = Symbol();
assert.throws(TypeError, function() {
  a.flatMap(mapperFn);
}, 'symbol value');

a = [];
a.constructor = undefined;
var actual = a.flatMap(mapperFn);
assert.compareArray(actual, [], 'undefined value does not throw');
assert.sameValue(Object.getPrototypeOf(actual), Array.prototype);
assert.notSameValue(actual, a, 'returns a new array');
