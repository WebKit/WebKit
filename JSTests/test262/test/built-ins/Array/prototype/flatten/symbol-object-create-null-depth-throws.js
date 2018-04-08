// Copyright (C) 2018 Shilpi Jain and Michael Ficarra. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-array.prototype.flatten
description: >
    if the argument is a Symbol or Object null, it throws exception
features: [Array.prototype.flatten]
---*/

assert.throws(TypeError, function() {
  [].flatten(Symbol());
}, 'symbol value');

assert.throws(TypeError, function() {
  [].flatten(Object.create(null));
}, 'object create null');
