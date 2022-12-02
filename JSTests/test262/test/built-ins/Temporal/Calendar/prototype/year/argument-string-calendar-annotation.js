// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.year
description: Various forms of calendar annotation; critical flag has no effect
features: [Temporal]
---*/

const tests = [
  ["2000-05-02[u-ca=iso8601]", "without time or time zone"],
  ["2000-05-02[UTC][u-ca=iso8601]", "with time zone and no time"],
  ["2000-05-02T15:23[u-ca=iso8601]", "without time zone"],
  ["2000-05-02T15:23[UTC][u-ca=iso8601]", "with time zone"],
  ["2000-05-02T15:23[!u-ca=iso8601]", "with ! and no time zone"],
  ["2000-05-02T15:23[UTC][!u-ca=iso8601]", "with ! and time zone"],
  ["2000-05-02T15:23[u-ca=iso8601][u-ca=discord]", "second annotation ignored"],
  ["2000-05-02T15:23[u-ca=iso8601][!u-ca=discord]", "second annotation ignored even with !"],
];

const instance = new Temporal.Calendar("iso8601");

tests.forEach(([arg, description]) => {
  const result = instance.year(arg);

  assert.sameValue(
    result,
    2000,
    `calendar annotation (${description})`
  );
});
