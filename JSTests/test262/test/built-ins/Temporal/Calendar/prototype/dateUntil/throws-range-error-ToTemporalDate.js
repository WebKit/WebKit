// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.dateuntil
description: Temporal.Calendar.prototype.dateUntil throw RangeError on ToTemporalDate
info: |
  1. Let calendar be the this value.
  4. Set one to ? ToTemporalDate(one).
  5. Set two to ? ToTemporalDate(two).
features: [Temporal, arrow-function]
---*/
let cal = new Temporal.Calendar("iso8601");

assert.throws(RangeError, () => cal.dateUntil("2021-07-16", "invalide date"),
      'cal.dateUntil("2021-07-16", "invalide date") throws a RangeError exception');
assert.throws(RangeError, () => cal.dateUntil("invalide date", "2021-07-16"),
      'cal.dateUntil("invalide date", "2021-07-16") throws a RangeError exception');
