// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.tozoneddatetime
description: More than one time zone annotation is not syntactical
features: [Temporal]
---*/

const invalidStrings = [
  "00:00[UTC][UTC]",
  "T00:00[UTC][UTC]",
  "1970-01-01T00:00[UTC][UTC]",
  "1970-01-01T00:00[!UTC][UTC]",
  "1970-01-01T00:00[UTC][!UTC]",
  "1970-01-01T00:00[UTC][u-ca=iso8601][UTC]",
  "1970-01-01T00:00[UTC][foo=bar][UTC]",
];
const instance = new Temporal.PlainDate(2000, 5, 2);
invalidStrings.forEach((arg) => {
  assert.throws(
    RangeError,
    () => instance.toZonedDateTime({ plainTime: arg, timeZone: "UTC" }),
    `reject more than one time zone annotation: ${arg}`
  );
});
