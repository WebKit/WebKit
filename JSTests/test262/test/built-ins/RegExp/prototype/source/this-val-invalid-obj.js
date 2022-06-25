// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-get-regexp.prototype.source
description: A TypeError is thrown when the "this" value is an invalid Object
info: |
  1. Let R be the this value.
  2. If Type(R) is not Object, throw a TypeError exception.
  3. If R does not have an [[OriginalFlags]] internal slot, then
     a. If SameValue(R, %RegExpPrototype%) is true, return "(?:)".
     b. Otherwise, throw a TypeError exception.
---*/

var get = Object.getOwnPropertyDescriptor(RegExp.prototype, 'source').get;

assert.throws(TypeError, function() {
  get.call({});
}, 'ordinary object');

assert.throws(TypeError, function() {
  get.call([]);
}, 'array exotic object');

assert.throws(TypeError, function() {
  get.call(arguments);
}, 'arguments object');
