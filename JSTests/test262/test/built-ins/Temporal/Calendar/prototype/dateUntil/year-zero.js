// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.dateuntil
description: Negative zero, as extended year, is invalid
features: [Temporal]
---*/

const calendar = new Temporal.Calendar("iso8601");
const date = new Temporal.PlainDate(2000, 5, 2);
const bad = "-000000-03-14";

assert.throws(
  RangeError,
  () => calendar.dateUntil(bad, date),
  "cannot use minus zero as extended date (first argument)"
);

assert.throws(
  RangeError,
  () => calendar.dateUntil(date, bad),
  "cannot use minus zero as extended date (second argument)"
);
