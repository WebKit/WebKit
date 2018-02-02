// Copyright (C) 2018 Shilpi Jain and Michael Ficarra. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-array.prototype.flatten
description: >
    Behavior when `constructor` property is neither an Object nor undefined
    - if IsConstructor(C) is false, throw a TypeError exception.
features: [Array.prototype.flatten]
---*/

var a = [];
a.constructor = null;
assert.throws(TypeError, function() {
  a.flatten();
}, 'null value');

var a = [];
a.constructor = 1;
assert.throws(TypeError, function() {
  a.flatten();
}, 'number value');

var a = [];
a.constructor = 'string';
assert.throws(TypeError, function() {
  a.flatten();
}, 'string value');

var a = [];
a.constructor = true;
assert.throws(TypeError, function() {
  a.flatten();
}, 'boolean value');
