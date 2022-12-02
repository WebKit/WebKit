// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.from
description: Various forms of calendar annotation; critical flag has no effect
features: [Temporal]
---*/

const tests = [
  ["1970-01-01T00:00[UTC][u-ca=iso8601]", "without !"],
  ["1970-01-01T00:00[UTC][!u-ca=iso8601]", "with !"],
  ["1970-01-01T00:00[UTC][u-ca=iso8601][u-ca=discord]", "second annotation ignored"],
  ["1970-01-01T00:00[UTC][u-ca=iso8601][!u-ca=discord]", "second annotation ignored even with !"],
];

tests.forEach(([arg, description]) => {
  const result = Temporal.ZonedDateTime.from(arg);

  assert.sameValue(
    result.calendar.toString(),
    "iso8601",
    `calendar annotation (${description})`
  );
});
