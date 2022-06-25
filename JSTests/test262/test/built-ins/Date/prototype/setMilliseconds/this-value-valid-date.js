// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-date.prototype.setmilliseconds
description: Return value for valid dates
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
var returnValue, expected;

returnValue = date.setMilliseconds(333);

expected = new Date(2016, 6, 1, 0, 0, 0, 333).getTime();
assert.sameValue(
  returnValue, expected, 'within unit boundary (return value)'
);
assert.sameValue(
  date.getTime(), expected, 'within unit boundary ([[DateValue]] slot)'
);

returnValue = date.setMilliseconds(-1);

expected = new Date(2016, 5, 30, 23, 59, 59, 999).getTime();
assert.sameValue(
  returnValue, expected, 'before time unit boundary (return value)'
);
assert.sameValue(
  date.getTime(), expected, 'before time unit boundary ([[DateValue]] slot)'
);

returnValue = date.setMilliseconds(1000);

expected = new Date(2016, 6, 1).getTime();
assert.sameValue(
  returnValue, expected, 'after time unit boundary (return value)'
);
assert.sameValue(
  date.getTime(), expected, 'after time unit boundary ([[DateValue]] slot)'
);
