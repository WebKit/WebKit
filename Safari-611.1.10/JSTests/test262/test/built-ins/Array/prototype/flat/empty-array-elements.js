// Copyright (C) 2018 Shilpi Jain and Michael Ficarra. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-array.prototype.flat
description: >
    arrays with empty arrays elements
includes: [compareArray.js]
features: [Array.prototype.flat]
---*/

var a = {};
assert.compareArray([].flat(), []);
assert.compareArray([
  [],
  []
].flat(), []);
assert.compareArray([
  [],
  [1]
].flat(), [1]);
assert.compareArray([
  [],
  [1, a]
].flat(), [1, a]);
