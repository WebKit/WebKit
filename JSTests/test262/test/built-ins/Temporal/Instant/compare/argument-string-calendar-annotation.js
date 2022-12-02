// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.instant.compare
description: Various forms of calendar annotation; critical flag has no effect
features: [Temporal]
---*/

const tests = [
  ["1970-01-01T00:00Z[u-ca=iso8601]", "without time zone"],
  ["1970-01-01T00:00Z[UTC][u-ca=gregory]", "with time zone"],
  ["1970-01-01T00:00Z[!u-ca=hebrew]", "with ! and no time zone"],
  ["1970-01-01T00:00Z[UTC][!u-ca=chinese]", "with ! and time zone"],
  ["1970-01-01T00:00Z[u-ca=discord]", "annotation is ignored"],
  ["1970-01-01T00:00Z[!u-ca=discord]", "annotation with ! is ignored"],
  ["1970-01-01T00:00Z[u-ca=iso8601][u-ca=discord]", "two annotations are ignored"],
  ["1970-01-01T00:00Z[u-ca=iso8601][!u-ca=discord]", "two annotations are ignored even with !"],
];

tests.forEach(([arg, description]) => {
  const result = Temporal.Instant.compare(arg, arg);

  assert.sameValue(
    result,
    0,
    `calendar annotation (${description})`
  );
});
