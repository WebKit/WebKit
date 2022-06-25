// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-date.prototype.setdate
description: Type coercion of provided argument
info: |
  1. Let t be LocalTime(? thisTimeValue(this value)).
  2. Let dt be ? ToNumber(date).
  3. Let newDate be MakeDate(MakeDay(YearFromTime(t), MonthFromTime(t), dt),
     TimeWithinDay(t)).
  4. Let u be TimeClip(UTC(newDate)).
  5. Set the [[DateValue]] internal slot of this Date object to u.
  6. Return u.
---*/

var date = new Date(2016, 6);
var callCount = 0;
var arg = {
  valueOf: function() {
    args = arguments;
    thisValue = this;
    callCount += 1;
    return 2;
  }
};
var args, thisValue, returnValue;

returnValue = date.setDate(arg);

assert.sameValue(callCount, 1, 'invoked exactly once');
assert.sameValue(args.length, 0, 'invoked without arguments');
assert.sameValue(thisValue, arg, '"this" value');
assert.sameValue(
  returnValue, new Date(2016, 6, 2).getTime(), 'application of specified value'
);

returnValue = date.setDate(null);

assert.sameValue(returnValue, new Date(2016, 5, 30).getTime(), 'null');

returnValue = date.setDate(true);

assert.sameValue(returnValue, new Date(2016, 5, 1).getTime(), 'true');

returnValue = date.setDate(false);

assert.sameValue(returnValue, new Date(2016, 4, 31).getTime(), 'false');

returnValue = date.setDate('   +00200.000E-0002	');

assert.sameValue(returnValue, new Date(2016, 4, 2).getTime(), 'string');

returnValue = date.setDate();

assert.sameValue(returnValue, NaN, 'undefined');
