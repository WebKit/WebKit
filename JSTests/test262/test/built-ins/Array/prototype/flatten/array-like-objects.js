// Copyright (C) 2018 Shilpi Jain and Michael Ficarra. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-array.prototype.flatten
description: >
    array-like objects can be flattened
includes: [compareArray.js]
features: [Array.prototype.flatten]
---*/

function getArgumentsObject() {
  return arguments;
}

var a = getArgumentsObject([1], [2]);
var actual = [].flatten.call(a);
assert.compareArray(actual, [1, 2], 'arguments objects');

var a = {
  length: 1,
  0: [1],
};
var actual = [].flatten.call(a);
assert.compareArray(actual, [1], 'array-like objects');

var a = {
  length: undefined,
  0: [1],
};
var actual = [].flatten.call(a);
assert.compareArray(actual, [], 'array-like objects; undefined length');
