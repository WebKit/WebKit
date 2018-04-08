// Copyright (C) 2018 Shilpi Jain and Michael Ficarra. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-array.prototype.flatten
description: >
    null or undefined should throw TypeError Exception
features: [Array.prototype.flatten]
---*/

assert.throws(TypeError, function() {
  [].flatten.call(null);
}, 'null value');

assert.throws(TypeError, function() {
  [].flatten.call();
}, 'missing');

assert.throws(TypeError, function() {
  [].flatten.call(void 0);
}, 'undefined');
