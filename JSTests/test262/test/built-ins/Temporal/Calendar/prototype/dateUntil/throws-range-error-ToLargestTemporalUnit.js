// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.dateuntil
description: Temporal.Calendar.prototype.dateUntil throw RangeError on ToLargestTemporalUnit with invalide or disallowed unit
info: |
  7. Let largestUnit be ? ToLargestTemporalUnit(options, « "hour", "minute", "second", "millisecond", "microsecond", "nanosecond" », "auto", "day").
features: [Temporal, arrow-function]
---*/
let cal = new Temporal.Calendar("iso8601");

["invalid", "hour", "minute", "second", "millisecond", "microsecond",
    "nanosecond"].forEach(function(largestUnit) {
  assert.throws(RangeError, () => cal.dateUntil("2021-07-16", "2022-03-04", {largestUnit}),
      'cal.dateUntil("2021-07-16", "2022-03-04", {largestUnit}) throws a RangeError exception');
});
