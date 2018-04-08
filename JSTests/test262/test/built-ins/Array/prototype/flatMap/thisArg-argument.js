// Copyright (C) 2018 Shilpi Jain and Michael Ficarra. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-array.prototype.flatMap
description: >
    Behavior when thisArg is provided
    Array.prototype.flatMap ( mapperFunction [ , thisArg ] )
includes: [compareArray.js]
flags: [onlyStrict]
features: [Array.prototype.flatMap]
---*/

var a = {};

assert(compareArray([1].flatMap(function() {
  return [this];
}, "TestString"), ["TestString"]));
assert(compareArray([1].flatMap(function() {
  return [this];
}, 1), [1]));
assert(compareArray([1].flatMap(function() {
  return [this];
}, null), [null]));
assert(compareArray([1].flatMap(function() {
  return [this];
}, true), [true]));
assert(compareArray([1].flatMap(function() {
  return [this];
}, a), [a]));
assert(compareArray([1].flatMap(function() {
  return [this];
}, void 0), [undefined]));
