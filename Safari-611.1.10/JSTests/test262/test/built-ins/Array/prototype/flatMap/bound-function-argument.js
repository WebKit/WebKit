// Copyright (C) 2018 Shilpi Jain and Michael Ficarra. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-array.prototype.flatMap
description: >
    Behavior when given a bound function
includes: [compareArray.js]
features: [Array.prototype.flatMap]
---*/

var a = [0, 0];
assert.compareArray(a.flatMap(function() {
  return this;
}.bind([1, 2])), [1, 2, 1, 2]);
