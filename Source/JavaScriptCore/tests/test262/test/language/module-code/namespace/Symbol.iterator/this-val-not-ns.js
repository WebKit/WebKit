// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-@@iterator
es6id: 26.3.2
description: >
    Behavior when the `this` value is not a module namespace exotic object
info: |
    1. Let N be the this value.
    2. If N is not a module namespace exotic object, throw a TypeError
       exception.
flags: [module]
features: [Symbol.iterator]
---*/

import * as ns from './this-val-not-ns.js';

var iter = ns[Symbol.iterator];

assert.sameValue(typeof iter, 'function');

assert.throws(TypeError, function() {
  iter();
}, 'undefined');

assert.throws(TypeError, function() {
  iter.call({});
}, 'ordinary object');

assert.throws(TypeError, function() {
  iter.call([]);
}, 'Array exotic object');

assert.throws(TypeError, function() {
  iter.call(23);
}, 'number literal');

assert.throws(TypeError, function() {
  iter.call(null);
}, 'null');

assert.throws(TypeError, function() {
  iter.call('string literal');
}, 'string literal');

assert.throws(TypeError, function() {
  iter.call(Symbol.iterator);
}, 'symbol');
