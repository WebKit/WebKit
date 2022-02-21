// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.calendar.prototype.dayofweek
description: >
  Temporal.Calendar.prototype.dayOfWeek will take Temporal.PlainDate objects
  and return the day of week.
info: |
  5. Return 𝔽(! ToISODayOfWeek(temporalDate.[[ISOYear]], temporalDate.[[ISOMonth]], temporalDate.[[ISODay]])).
features: [Temporal]
---*/
let cal = new Temporal.Calendar("iso8601");

let d = new Temporal.PlainDate(1970, 1, 1);
assert.sameValue(4, cal.dayOfWeek(d), '4 must return the same value returned by cal.dayOfWeek(d)');
d = new Temporal.PlainDate(2021, 2, 15);
assert.sameValue(1, cal.dayOfWeek(d), '1 must return the same value returned by cal.dayOfWeek(d)');
d = new Temporal.PlainDate(2021, 8, 15);
assert.sameValue(7, cal.dayOfWeek(d), '7 must return the same value returned by cal.dayOfWeek(d)');
