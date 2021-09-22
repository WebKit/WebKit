// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-temporal.calendar.prototype.daysinyear
description: >
  Temporal.Calendar.prototype.daysInYear will take PlainDate and return
  the number of days in a year.
info: |
  5. Return 𝔽(! ISODaysInYear(temporalDateLike.[[ISOYear]])).
features: [Temporal]
---*/
let cal = new Temporal.Calendar("iso8601");

let d = new Temporal.PlainDate(1995, 7, 15);
assert.sameValue(cal.daysInYear(d), 365, 'cal.daysInYear(new Temporal.PlainDate(1995, 7, 15)) must return 365');

d = new Temporal.PlainDate(1996, 7, 15);
assert.sameValue(cal.daysInYear(d), 366, 'cal.daysInYear(new Temporal.PlainDate(1996, 7, 15)) must return 366');

d = new Temporal.PlainDate(1997, 7, 15);
assert.sameValue(cal.daysInYear(d), 365, 'cal.daysInYear(new Temporal.PlainDate(1997, 7, 15)) must return 365');

d = new Temporal.PlainDate(1998, 7, 15);
assert.sameValue(cal.daysInYear(d), 365, 'cal.daysInYear(new Temporal.PlainDate(1998, 7, 15)) must return 365');

d = new Temporal.PlainDate(1999, 7, 15);
assert.sameValue(cal.daysInYear(d), 365, 'cal.daysInYear(new Temporal.PlainDate(1999, 7, 15)) must return 365');

d = new Temporal.PlainDate(2000, 7, 15);
assert.sameValue(cal.daysInYear(d), 366, 'cal.daysInYear(new Temporal.PlainDate(2000, 7, 15)) must return 366');

d = new Temporal.PlainDate(2001, 7, 15);
assert.sameValue(cal.daysInYear(d), 365, 'cal.daysInYear(new Temporal.PlainDate(2001, 7, 15)) must return 365');

d = new Temporal.PlainDate(2002, 7, 15);
assert.sameValue(cal.daysInYear(d), 365, 'cal.daysInYear(new Temporal.PlainDate(2002, 7, 15)) must return 365');

d = new Temporal.PlainDate(2003, 7, 15);
assert.sameValue(cal.daysInYear(d), 365, 'cal.daysInYear(new Temporal.PlainDate(2003, 7, 15)) must return 365');

d = new Temporal.PlainDate(2004, 7, 15);
assert.sameValue(cal.daysInYear(d), 366, 'cal.daysInYear(new Temporal.PlainDate(2004, 7, 15)) must return 366');

d = new Temporal.PlainDate(2005, 7, 15);
assert.sameValue(cal.daysInYear(d), 365, 'cal.daysInYear(new Temporal.PlainDate(2005, 7, 15)) must return 365');
