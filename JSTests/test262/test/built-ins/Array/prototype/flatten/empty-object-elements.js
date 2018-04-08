// Copyright (C) 2018 Shilpi Jain and Michael Ficarra. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-array.prototype.flatten
description: >
    arrays with empty object elements
includes: [compareArray.js]
features: [Array.prototype.flatten]
---*/

var a = {},
  b = {};

assert.compareArray([a].flatten(), [a]);
assert.compareArray([a, [b]].flatten(), [a, b]);
assert.compareArray([
  [a], b
].flatten(), [a, b]);
assert.compareArray([
  [a],
  [b]
].flatten(), [a, b]);
