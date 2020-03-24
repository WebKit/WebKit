// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-get-aggregate-error.prototype.errors
description: Throws a TypeError exception when `this` is not Object
info: |
  get AggregateError.prototype.errors

  1. Let E be the this value.
  2. If Type(E) is not Object, throw a TypeError exception.
  ...
features: [AggregateError, Symbol]
---*/

var getter = Object.getOwnPropertyDescriptor(
  AggregateError.prototype, 'errors'
).get;

assert.throws(TypeError, function() {
  getter.call(undefined);
}, 'this is undefined');

assert.throws(TypeError, function() {
  getter.call(null);
}, 'this is null');

assert.throws(TypeError, function() {
  getter.call(42);
}, 'this is 42');

assert.throws(TypeError, function() {
  getter.call('1');
}, 'this is a string');

assert.throws(TypeError, function() {
  getter.call(true);
}, 'this is true');

assert.throws(TypeError, function() {
  getter.call(false);
}, 'this is false');

var s = Symbol('s');
assert.throws(TypeError, function() {
  getter.call(s);
}, 'this is a Symbol');
