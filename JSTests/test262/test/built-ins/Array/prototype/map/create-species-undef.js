// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-array.prototype.map
description: >
    An undefined value for the @@species constructor triggers the creation  of
    an Array exotic object
info: |
    [...]
    5. Let A be ? ArraySpeciesCreate(O, len).
    [...]

    9.4.2.3 ArraySpeciesCreate

    [...]
    5. Let C be ? Get(originalArray, "constructor").
    [...]
    7. If Type(C) is Object, then
       a. Let C be ? Get(C, @@species).
       b. If C is null, let C be undefined.
    8. If C is undefined, return ? ArrayCreate(length).
features: [Symbol.species]
---*/

var a = [];
var result;

a.constructor = {};
a.constructor[Symbol.species] = undefined;

result = a.map(function() {});

assert.sameValue(Object.getPrototypeOf(result), Array.prototype);
assert(Array.isArray(result), 'result is an Array exotic object');
