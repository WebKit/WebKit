// Copyright (C) 2018 Shilpi Jain and Michael Ficarra. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-array.prototype.flatMap
description: >
    Behavior when array is depth more than 1
includes: [compareArray.js]
features: [Array.prototype.flatMap]
---*/

assert.compareArray([1, 2].flatMap(function(e) {
  return [e, e * 2];
}), [1, 2, 2, 4], 'array depth is 1');

var result = [1, 2, 3].flatMap(function(ele) {
  return [
    [ele * 2]
  ];
});
assert.sameValue(result.length, 3, 'array depth is more than 1 - length');
assert.compareArray(result[0], [2], 'array depth is more than 1 - 1st element');
assert.compareArray(result[1], [4], 'array depth is more than 1 - 2nd element');
assert.compareArray(result[2], [6], 'array depth is more than 1 - 3rd element');
