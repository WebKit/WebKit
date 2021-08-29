// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.dateadd
description: Temporal.Calendar.prototype.dateAdd should throw from ToTemporalDuration.
info: |
  ...
  5. Set duration to ? ToTemporalDuration(duration).
features: [Temporal, arrow-function]
---*/
let cal = new Temporal.Calendar("iso8601");

assert.throws(RangeError,
    () => cal.dateAdd("2020-02-03", "invalid duration string"),
    'cal.dateAdd("2020-02-03", "invalid duration string") throws a RangeError exception');
