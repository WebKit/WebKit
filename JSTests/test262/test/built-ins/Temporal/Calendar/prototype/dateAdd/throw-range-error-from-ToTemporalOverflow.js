// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.dateadd
description: Temporal.Calendar.prototype.dateAdd should throw from ToTemporalOverflow.
info: |
  7. Let overflow be ? ToTemporalOverflow(options).
features: [Temporal, arrow-function]
---*/
let cal = new Temporal.Calendar("iso8601");

assert.throws(RangeError,
    () => cal.dateAdd("2020-02-29", "PT1M", {overflow: "bad value"}),
    'cal.dateAdd("2020-02-29", "PT1M", {overflow: "bad value"}) throws a RangeError exception');
