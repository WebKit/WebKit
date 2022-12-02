// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.from
description: Various forms of calendar annotation; critical flag has no effect
features: [Temporal]
includes: [temporalHelpers.js]
---*/

const tests = [
  ["2019-12-15T15:23[u-ca=iso8601]", "without time zone"],
  ["2019-12-15T15:23[UTC][u-ca=iso8601]", "with time zone"],
  ["2019-12-15T15:23[!u-ca=iso8601]", "with ! and no time zone"],
  ["2019-12-15T15:23[UTC][!u-ca=iso8601]", "with ! and time zone"],
  ["2019-12-15T15:23[u-ca=iso8601][u-ca=discord]", "second annotation ignored"],
  ["2019-12-15T15:23[u-ca=iso8601][!u-ca=discord]", "second annotation ignored even with !"],
];

tests.forEach(([arg, description]) => {
  const result = Temporal.PlainYearMonth.from(arg);

  TemporalHelpers.assertPlainYearMonth(
    result,
    2019, 12, "M12",
    `calendar annotation (${description})`
  );
});
