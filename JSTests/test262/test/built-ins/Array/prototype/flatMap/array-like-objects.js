// Copyright (C) 2018 Shilpi Jain and Michael Ficarra. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-array.prototype.flatMap
description: >
    array-like objects can be flattened
includes: [compareArray.js]
features: [Array.prototype.flatMap]
---*/

function getArgumentsObject() {
  return arguments;
}

function double(e) {
  return [e * 2];
}

var a = getArgumentsObject(1, 2);
var actual = [].flatMap.call(a, double);
assert.compareArray(actual, [2, 4], 'arguments objects');

var a = {
  length: 1,
  0: 1,
};
var actual = [].flatMap.call(a, double);
assert.compareArray(actual, [2], 'array-like objects');

var a = {
  length: void 0,
  0: 1,
};
var actual = [].flatMap.call(a, double);
assert.compareArray(actual, [], 'array-like objects; undefined length');
