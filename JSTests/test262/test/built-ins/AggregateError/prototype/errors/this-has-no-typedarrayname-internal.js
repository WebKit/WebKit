// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-get-aggregate-error.prototype.errors
description: >
  Throws a TypeError exception when `this` does not have a [[AggregateErrorData]]
  internal slot
info: |
  get AggregateError.prototype.errors

  1. Let E be the this value.
  2. If Type(E) is not Object, throw a TypeError exception.
  3. If E does not have an [[ErrorData]] internal slot, throw a TypeError exception.
  4. If E does not have an [[AggregateErrors]] internal slot, throw a TypeError exception.
  5. Return ! CreateArrayFromList(E.[[AggregateErrors]]).
features: [AggregateError]
---*/

var getter = Object.getOwnPropertyDescriptor(
  AggregateError.prototype, 'errors'
).get;

assert.throws(TypeError, function() {
  getter.call(new Error());
}, 'this is an instance of Error, no [[AggregateErrors]]');

assert.throws(TypeError, function() {
  getter.call(AggregateError);
}, 'AggregateError does not have an [[AggregateErrors]] internal');
