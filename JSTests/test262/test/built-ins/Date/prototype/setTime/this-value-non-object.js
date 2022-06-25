// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-date.prototype.settime
description: Behavior when "this" value is not an Object
info: |
  1. Let t be LocalTime(? thisTimeValue(this value)).

  The abstract operation thisTimeValue(value) performs the following steps:

  1. If Type(value) is Object and value has a [[DateValue]] internal slot, then
     a. Return value.[[DateValue]].
  2. Throw a TypeError exception.
features: [Symbol]
---*/

var setTime = Date.prototype.setTime;
var callCount = 0;
var arg = {
  valueOf: function() {
    callCount += 1;
    return 1;
  }
};
var symbol = Symbol();

assert.sameValue(typeof setTime, 'function');

assert.throws(TypeError, function() {
  setTime.call(0, arg);
}, 'number');

assert.throws(TypeError, function() {
  setTime.call(true, arg);
}, 'boolean');

assert.throws(TypeError, function() {
  setTime.call(null, arg);
}, 'null');

assert.throws(TypeError, function() {
  setTime.call(undefined, arg);
}, 'undefined');

assert.throws(TypeError, function() {
  setTime.call('', arg);
}, 'string');

assert.throws(TypeError, function() {
  setTime.call(symbol, arg);
}, 'symbol');

assert.sameValue(callCount, 0, 'validation precedes input coercion');
