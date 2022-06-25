// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.instant.prototype.tozoneddatetime
description: Negative zero, as an extended year, is rejected
features: [Temporal, arrow-function]
---*/

const invalidStrings = [
  "-000000-10-31T17:45Z",
  "-000000-10-31T17:45+00:00[UTC]",
];
const instance = new Temporal.Instant(0n);
invalidStrings.forEach((timeZone) => {
  assert.throws(
    RangeError,
    () => instance.toZonedDateTime({ timeZone, calendar: "iso8601" }),
    "reject minus zero as extended year"
  );
  assert.throws(
    RangeError,
    () => instance.toZonedDateTime({ timeZone: { timeZone }, calendar: "iso8601" }),
    "reject minus zero as extended year (nested property)"
  );
});
