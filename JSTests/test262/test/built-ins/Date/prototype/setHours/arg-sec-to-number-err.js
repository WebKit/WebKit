// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-date.prototype.sethours
description: Abrupt completion during type coercion of provided "sec"
info: |
  1. Let t be LocalTime(? thisTimeValue(this value)).
  2. Let dt be ? ToNumber(hour).
  3. If min is not specified, let m be MinFromTime(t); otherwise, let m be ?
     ToNumber(min).
  4. If sec is not specified, let s be SecFromTime(t); otherwise, let s be ?
     ToNumber(sec).
---*/

var date = new Date();
var callCount = 0;
var originalValue = date.getTime();
var obj = {
  valueOf: function() {
    throw new Test262Error();
  }
};
var counter = {
  valueOf: function() {
    callCount += 1;
  }
};

assert.throws(Test262Error, function() {
  date.setHours(0, 0, obj, counter);
});

assert.sameValue(date.getTime(), originalValue);
assert.sameValue(callCount, 0);
