// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.subtract
description: Using lower units in subtract() works
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const ym = Temporal.PlainYearMonth.from("2019-11");

const tests = [
  [{ days: 1 }, 2019, 11, "M11"],
  [{ hours: 1 }, 2019, 11, "M11"],
  [{ minutes: 1 }, 2019, 11, "M11"],
  [{ seconds: 1 }, 2019, 11, "M11"],
  [{ milliseconds: 1 }, 2019, 11, "M11"],
  [{ microseconds: 1 }, 2019, 11, "M11"],
  [{ nanoseconds: 1 }, 2019, 11, "M11"],
  [{ days: 29 }, 2019, 11, "M11"],
  [{ days: 30 }, 2019, 10, "M10"],
  [{ days: 60 }, 2019, 10, "M10"],
  [{ days: 61 }, 2019, 9, "M09"],
  [{ hours: 720 }, 2019, 10, "M10"],
  [{ minutes: 43200 }, 2019, 10, "M10"],
  [{ seconds: 2592000 }, 2019, 10, "M10"],
  [{ milliseconds: 2592000_000 }, 2019, 10, "M10"],
  [{ microseconds: 2592000_000_000 }, 2019, 10, "M10"],
  [{ nanoseconds: 2592000_000_000_000 }, 2019, 10, "M10"],
];

for (const [argument, ...expected] of tests) {
  TemporalHelpers.assertPlainYearMonth(ym.subtract(argument), ...expected, "no options");
  TemporalHelpers.assertPlainYearMonth(ym.subtract(argument, { overflow: "constrain" }), ...expected, "constrain");
  TemporalHelpers.assertPlainYearMonth(ym.subtract(argument, { overflow: "reject" }), ...expected, "reject");
}
