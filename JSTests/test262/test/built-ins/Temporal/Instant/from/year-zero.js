// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.instant.from
description: Negative zero, as an extended year, is rejected
features: [Temporal]
---*/

const invalidStrings = [
  "-000000-03-31T00:45Z",
  "-000000-03-31T01:45+01:00",
  "-000000-03-31T01:45:00+01:00[UTC]"
];

invalidStrings.forEach((str) => {
  assert.throws(
    RangeError,
    () => Temporal.Instant.from(str),
    "reject minus zero as extended year"
  );
});
