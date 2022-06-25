// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.dateadd
description: Temporal.Calendar.prototype.dateAdd should throw from ToTemporalDate.
info: |
  ...
  4. Set date to ? ToTemporalDate(date).
features: [Temporal, arrow-function]
---*/
let cal = new Temporal.Calendar("iso8601");

assert.throws(RangeError,
    () => cal.dateAdd("invalid date string", new Temporal.Duration(1)),
    'cal.dateAdd("invalid date string", new Temporal.Duration(1)) throws a RangeError exception');
