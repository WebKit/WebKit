// Copyright (C) 2021 Kevin Gibbons. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-date.prototype.setutcdate
description: Order of coercion of provided argument vs NaN check
info: |
  1. Let t be ? thisTimeValue(this value).
  2. Let dt be ? ToNumber(date).
  3. If t is NaN, return NaN.
  4. Let newDate be MakeDate(MakeDay(YearFromTime(t), MonthFromTime(t), dt), TimeWithinDay(t)).
  5. Let v be TimeClip(newDate).
  6. Set the [[DateValue]] internal slot of this Date object to v.
  7. Return v.
---*/

var date = new Date(NaN);
var callCount = 0;
var arg = {
  valueOf: function() {
    callCount += 1;
    return 0;
  }
};

var returnValue = date.setUTCDate(arg);

assert.sameValue(callCount, 1, 'ToNumber invoked exactly once');
assert.sameValue(returnValue, NaN, 'argument is ignored when `this` is an invalid date');
assert.sameValue(date.getTime(), NaN, 'argument is ignored when `this` is an invalid date');
