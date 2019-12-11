// Copyright (C) 2018 Shilpi Jain and Michael Ficarra. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-array.prototype.flat
description: >
    array-like objects can be flattened
includes: [compareArray.js]
features: [Array.prototype.flat]
---*/

function getArgumentsObject() {
  return arguments;
}

var a = getArgumentsObject([1], [2]);
var actual = [].flat.call(a);
assert.compareArray(actual, [1, 2], 'arguments objects');

a = {
  length: 1,
  0: [1],
};
actual = [].flat.call(a);
assert.compareArray(actual, [1], 'array-like objects');

a = {
  length: undefined,
  0: [1],
};
actual = [].flat.call(a);
assert.compareArray(actual, [], 'array-like objects; undefined length');
