// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.withplaindate
description: PlainDate calendar is preserved when both calendars have the same id
features: [Temporal]
includes: [temporalHelpers.js]
---*/

const cal1 = {
  toString() { return "this is a string"; },
};
const cal2 = {
  id: 'thisisnotiso',
  era() { return undefined; },
  eraYear() { return undefined; },
  toString() { return "this is a string"; },
  year() { return 2008; },
  month() { return 9; },
  monthCode() { return "M09"; },
  day() { return 6; }
};
const pdt = new Temporal.PlainDateTime(1995, 12, 7, 3, 24, 30, 0, 0, 0, cal1);
const pd = new Temporal.PlainDate(2010, 11, 12, cal2);
const shifted = pdt.withPlainDate(pd);

TemporalHelpers.assertPlainDateTime(
  shifted,
  2008, 9, "M09", 6, 3, 24, 30, 0, 0, 0,
  "calendar is changed with same id (1)"
  // Testing of era and eraYear should only be coded under intl402
);

assert.sameValue(
  shifted.calendar,
  cal2,
  "calendar is changed with same id (2)"
);
