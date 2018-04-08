// Copyright (C) 2018 Shilpi Jain and Michael Ficarra. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-array.prototype.flatten
description: >
    using bound functions
includes: [compareArray.js]
features: [Array.prototype.flatten]
---*/

var a = [
  [0],
  [1]
];
var actual = [].flatten.bind(a)();

assert.compareArray(actual, [0, 1], 'bound flatten');
