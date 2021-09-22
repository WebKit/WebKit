// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.month
description: >
  Temporal.Calendar.prototype.month will take PlainYearMonth and return
  the value of the month.
info: |
  6. Return ! ISOMonth(temporalDateLike).
features: [Temporal]
---*/
let cal = new Temporal.Calendar("iso8601");

let yearMonth = new Temporal.PlainYearMonth(1999, 6);
assert.sameValue(cal.month(yearMonth), 6, 'cal.month(new Temporal.PlainYearMonth(1999, 6)) must return 6');
