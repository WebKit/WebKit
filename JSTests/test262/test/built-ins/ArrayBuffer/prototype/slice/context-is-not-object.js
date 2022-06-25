// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-arraybuffer.prototype.slice
description: >
  Throws a TypeError if `this` is not an Object.
info: |
  ArrayBuffer.prototype.slice ( start, end )

  1. Let O be the this value.
  2. If Type(O) is not Object, throw a TypeError exception.
  ...
features: [Symbol]
---*/

assert.throws(TypeError, function() {
  ArrayBuffer.prototype.slice.call(undefined);
}, "`this` value is undefined");

assert.throws(TypeError, function() {
  ArrayBuffer.prototype.slice.call(null);
}, "`this` value is null");

assert.throws(TypeError, function() {
  ArrayBuffer.prototype.slice.call(true);
}, "`this` value is Boolean");

assert.throws(TypeError, function() {
  ArrayBuffer.prototype.slice.call("");
}, "`this` value is String");

assert.throws(TypeError, function() {
  ArrayBuffer.prototype.slice.call(Symbol());
}, "`this` value is Symbol");

assert.throws(TypeError, function() {
  ArrayBuffer.prototype.slice.call(1);
}, "`this` value is Number");
