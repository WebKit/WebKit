// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.add
description: Using lower units in add() works
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const ym = Temporal.PlainYearMonth.from("2019-11");

const tests = [
  [{ days: 1 }, 2019, 11, "M11"],
  [{ days: 29 }, 2019, 11, "M11"],
  [{ hours: 1 }, 2019, 11, "M11"],
  [{ minutes: 1 }, 2019, 11, "M11"],
  [{ seconds: 1 }, 2019, 11, "M11"],
  [{ milliseconds: 1 }, 2019, 11, "M11"],
  [{ microseconds: 1 }, 2019, 11, "M11"],
  [{ nanoseconds: 1 }, 2019, 11, "M11"],
  [{ days: 30 }, 2019, 12, "M12"],
  [{ days: 31 }, 2019, 12, "M12"],
  [{ days: 60 }, 2019, 12, "M12"],
  [{ days: 61 }, 2020, 1, "M01"],
  [{ hours: 720 }, 2019, 12, "M12"],
  [{ minutes: 43200 }, 2019, 12, "M12"],
  [{ seconds: 2592000 }, 2019, 12, "M12"],
  [{ milliseconds: 2592000_000 }, 2019, 12, "M12"],
  [{ microseconds: 2592000_000_000 }, 2019, 12, "M12"],
  [{ nanoseconds: 2592000_000_000_000 }, 2019, 12, "M12"],
];

for (const [argument, ...expected] of tests) {
  TemporalHelpers.assertPlainYearMonth(ym.add(argument), ...expected, "no options");
  TemporalHelpers.assertPlainYearMonth(ym.add(argument, { overflow: "constrain" }), ...expected, "constrain");
  TemporalHelpers.assertPlainYearMonth(ym.add(argument, { overflow: "reject" }), ...expected, "reject");
}
