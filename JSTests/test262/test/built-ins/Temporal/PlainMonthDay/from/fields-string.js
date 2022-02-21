// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainmonthday.from
description: Basic tests for PlainMonthDay.from(string).
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const tests = [
  "10-01",
  "1001",
  "1965-10-01",
  "1976-10-01T152330.1+00:00",
  "19761001T15:23:30.1+00:00",
  "1976-10-01T15:23:30.1+0000",
  "1976-10-01T152330.1+0000",
  "19761001T15:23:30.1+0000",
  "19761001T152330.1+00:00",
  "19761001T152330.1+0000",
  "+001976-10-01T152330.1+00:00",
  "+0019761001T15:23:30.1+00:00",
  "+001976-10-01T15:23:30.1+0000",
  "+001976-10-01T152330.1+0000",
  "+0019761001T15:23:30.1+0000",
  "+0019761001T152330.1+00:00",
  "+0019761001T152330.1+0000",
  "1976-10-01T15:23:00",
  "1976-10-01T15:23",
  "1976-10-01T15",
  "1976-10-01",
  "--10-01",
  "--1001",
  1001,
];

for (const argument of tests) {
  const plainMonthDay = Temporal.PlainMonthDay.from(argument);
  assert.notSameValue(plainMonthDay, argument, `from ${argument} converts`);
  TemporalHelpers.assertPlainMonthDay(plainMonthDay, "M10", 1, `from ${argument}`);
  assert.sameValue(plainMonthDay.calendar.id, "iso8601", `from ${argument} calendar`);
}

assert.throws(RangeError, () => Temporal.PlainMonthDay.from("11-18junk"), "no junk at end of string");

