// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainmonthday.prototype.equals
description: Various forms of calendar annotation; critical flag has no effect
features: [Temporal]
---*/

const tests = [
  ["1976-05-02T15:23[u-ca=iso8601]", "without time zone"],
  ["1976-05-02T15:23[UTC][u-ca=iso8601]", "with time zone"],
  ["1976-05-02T15:23[!u-ca=iso8601]", "with ! and no time zone"],
  ["1976-05-02T15:23[UTC][!u-ca=iso8601]", "with ! and time zone"],
  ["1976-05-02T15:23[u-ca=iso8601][u-ca=discord]", "second annotation ignored"],
  ["1976-05-02T15:23[u-ca=iso8601][!u-ca=discord]", "second annotation ignored even with !"],
];

const instance = new Temporal.PlainMonthDay(5, 2);

tests.forEach(([arg, description]) => {
  const result = instance.equals(arg);

  assert.sameValue(
    result,
    true,
    `calendar annotation (${description})`
  );
});
