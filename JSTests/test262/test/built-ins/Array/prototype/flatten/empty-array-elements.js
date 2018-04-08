// Copyright (C) 2018 Shilpi Jain and Michael Ficarra. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-array.prototype.flatten
description: >
    arrays with empty arrays elements
includes: [compareArray.js]
features: [Array.prototype.flatten]
---*/

var a = {};
assert.compareArray([].flatten(), []);
assert.compareArray([
  [],
  []
].flatten(), []);
assert.compareArray([
  [],
  [1]
].flatten(), [1]);
assert.compareArray([
  [],
  [1, a]
].flatten(), [1, a]);
