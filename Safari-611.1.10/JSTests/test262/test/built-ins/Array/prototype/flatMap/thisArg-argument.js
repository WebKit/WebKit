// Copyright (C) 2018 Shilpi Jain and Michael Ficarra. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-array.prototype.flatMap
description: >
    Behavior when thisArg is provided
    Array.prototype.flatMap ( mapperFunction [ , thisArg ] )
flags: [onlyStrict]
includes: [compareArray.js]
features: [Array.prototype.flatMap]
---*/

var a = {};
var actual;

actual = [1].flatMap(function() {
  return [this];
}, "TestString");
assert.compareArray(actual, ["TestString"]);

actual = [1].flatMap(function() {
  return [this];
}, 1);
assert.compareArray(actual, [1]);

actual = [1].flatMap(function() {
  return [this];
}, null);
assert.compareArray(actual, [null]);

actual = [1].flatMap(function() {
  return [this];
}, true);
assert.compareArray(actual, [true]);

actual = [1].flatMap(function() {
  return [this];
}, a);
assert.compareArray(actual, [a]);

actual = [1].flatMap(function() {
  return [this];
}, void 0);
assert.compareArray(actual, [undefined]);

