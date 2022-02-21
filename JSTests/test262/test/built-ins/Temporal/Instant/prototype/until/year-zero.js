// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.instant.prototype.until
description: Negative zero, as an extended year, is rejected
features: [Temporal]
---*/

const invalidStrings = [
    "-000000-03-30T00:45Z",
    "-000000-03-30T01:45+01:00",
    "-000000-03-30T01:45:00+01:00[UTC]"
];
const instance = new Temporal.Instant(0n);
invalidStrings.forEach((str) => {
  assert.throws(
    RangeError,
    () => instance.until(str),
    "reject minus zero as extended year"
  );
});
